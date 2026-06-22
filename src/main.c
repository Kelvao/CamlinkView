#define _XOPEN_SOURCE 700
/*
 * camlink-view — Elgato Camlink 4K viewer
 * SDL2 + V4L2 (mmap, NV12) + PipeWire audio
 *
 * Teclas: F=fullscreen | O=overlay | Q/Esc=sair
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "common.h"
#include "video.h"
#include "audio.h"
#include "renderer.h"
#include "overlay.h"
#include "stats.h"
#include "detect.h"
#include "controls.h"
#include "menu.h"

/* ── Signal ──────────────────────────────────────────────────────────────── */

static volatile sig_atomic_t g_quit = 0;

static void handle_signal (int sig) { (void)sig; g_quit = 1; }

static void
install_signals (void)
{
    struct sigaction sa = { .sa_handler = handle_signal };
    sigemptyset (&sa.sa_mask);
    sigaction (SIGINT,  &sa, NULL);
    sigaction (SIGTERM, &sa, NULL);
}

/* ── Forward declarations ───────────────────────────────────────────────── */
static void on_stream_ready (void *userdata);

/* ── App state ───────────────────────────────────────────────────────────── */

typedef struct {
    VideoDevice  video;
    Renderer     renderer;
    AudioState  *audio;
    Overlay     *overlay;
    Stats        stats;
    Controls     controls;
    Menu        *menu;
    bool         fullscreen;
    bool         show_overlay;
    bool         controls_ready;  /* true após controls_reapply — descarta frames antes */
} App;

/* ── Stream ready callback ───────────────────────────────────────────────── */

static void
on_stream_ready (void *userdata)
{
    App *app = userdata;
    /* Stream ativo — relê os valores reais do driver (FLAG_INACTIVE levantado).
     * Não reaplica nada — usa o que o driver reporta como verdade. */
    controls_refresh (&app->controls);
    app->controls_ready = true;
}

/* ── Main loop ───────────────────────────────────────────────────────────── */

static void
run (App *app)
{
    SDL_Event ev;

    app->stats.last_fps_ts = mono_ns ();
    app->stats.latency_ms  = 0.0;

    while (!g_quit) {

        /* ── SDL events ──────────────────────────────────────────────── */
        while (SDL_PollEvent (&ev)) {
            if (ev.type == SDL_QUIT) { g_quit = 1; break; }

            /* Menu consome o evento se estiver aberto */
            if (menu_handle_event (app->menu, &ev)) continue;

            if (ev.type != SDL_KEYDOWN) continue;
            switch (ev.key.keysym.sym) {
            case SDLK_q:
            case SDLK_ESCAPE: g_quit = 1;                                  break;
            case SDLK_f:
                app->fullscreen = !app->fullscreen;
                SDL_SetWindowFullscreen (app->renderer.window,
                    app->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                break;
            case SDLK_o:
                app->show_overlay = !app->show_overlay;
                break;
            case SDLK_m:
                menu_toggle (app->menu);
                break;
            }
        }

        /* ── Dequeue V4L2 frame ──────────────────────────────────────── */
        VideoFrame frame;
        bool fatal = false;
        if (!video_dequeue (&app->video, &frame, &fatal)) {
            if (fatal) break;
            continue;
        }

        /* Descarta frames capturados antes dos controles serem aplicados */
        if (!app->controls_ready) {
            video_enqueue (&app->video, frame.index);
            continue;
        }

        const uint64_t dequeue_ts = mono_ns ();

        /* ── Upload NV12 — planos Y e UV separados ───────────────────── */
        SDL_UpdateNVTexture (app->renderer.tex_frame, NULL,
            frame.y_plane,  app->video.width,
            frame.uv_plane, app->video.width);

        /* Re-enqueue imediatamente — minimiza starvation do buffer V4L2 */
        video_enqueue (&app->video, frame.index);

        /* ── Render ──────────────────────────────────────────────────── */
        int win_w, win_h;
        SDL_GetRendererOutputSize (app->renderer.renderer, &win_w, &win_h);
        const SDL_Rect dst = renderer_aspect_rect (
            win_w, win_h, app->video.width, app->video.height);

        SDL_SetRenderDrawColor (app->renderer.renderer, 0, 0, 0, 255);
        SDL_RenderClear  (app->renderer.renderer);
        SDL_RenderCopy   (app->renderer.renderer, app->renderer.tex_frame, NULL, &dst);

        /* ── Stats & overlay ─────────────────────────────────────────── */
        stats_update (&app->stats, (double)(mono_ns () - dequeue_ts) / 1e6);

        if (app->show_overlay)
            overlay_draw (app->overlay, app->renderer.renderer, dst, &app->stats);

        /* Menu sobrepõe tudo — renderiza por cima */
        menu_draw (app->menu, app->renderer.renderer, win_w, win_h);

        SDL_RenderPresent (app->renderer.renderer);
    }
}

/* ── CLI ─────────────────────────────────────────────────────────────────── */

static void
print_help (void)
{
    puts ("camlink-view [opções]\n"
          "\n"
          "  Sem argumentos: detecta a Camlink automaticamente.\n"
          "\n"
          "  --device=PATH       força dispositivo V4L2 (ex: /dev/video0)\n"
          "  --audio=SOURCE      força PipeWire source name\n"
          "  --resolution=WxH    ex: 1920x1080\n"
          "  --fps=N\n"
          "  --fullscreen\n"
          "  --overlay\n"
          "\n"
          "Teclas: F=fullscreen | O=overlay | M=menu | Q/Esc=sair\n"
          "Menu:   ↑↓=navega | ←→=ajusta | PgUp/Dn=passo grande\n"
          "        S=salva | R=reset | M/Esc=fecha");
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int
main (int argc, char **argv)
{
    App app = {
        .video = {
            .fd     = -1,
            .width  = DEFAULT_WIDTH,
            .height = DEFAULT_HEIGHT,
            .fps    = DEFAULT_FPS,
            .device = NULL,   /* NULL = será preenchido por auto_detect ou --device */
        },
        .fullscreen   = false,
        .show_overlay = false,
    };

    const char *audio_source = NULL;
    bool        auto_detect  = true;   /* false quando --device E --audio forem passados */

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strncmp (a, "--device=",     9))  app.video.device = a + 9;
        else if (!strncmp (a, "--audio=",      8))  audio_source     = a + 8;
        else if (!strncmp (a, "--fps=",        6))  app.video.fps    = atoi (a + 6);
        else if (!strncmp (a, "--resolution=", 13)) sscanf (a + 13, "%dx%d",
                                                        &app.video.width, &app.video.height);
        else if (!strncmp (a, "--width=",      8))  app.video.width  = atoi (a + 8);
        else if (!strncmp (a, "--height=",     9))  app.video.height = atoi (a + 9);
        else if (!strcmp  (a, "--fullscreen"))       app.fullscreen   = true;
        else if (!strcmp  (a, "--overlay"))          app.show_overlay = true;
        else if (!strcmp  (a, "--help"))             { print_help (); return 0; }
        else { fprintf (stderr, "Opção desconhecida: %s\n", a); return 1; }
    }

    /* Desativa auto_detect se ambos foram passados explicitamente */
    if (app.video.device && audio_source)
        auto_detect = false;

    /* Auto-detecção quando --device ou --audio não forem fornecidos */
    if (auto_detect) {
        printf ("Detectando Camlink 4K...\n");
        DetectResult det = detect_camlink ();
        detect_print (&det);
        printf ("\n");

        /* Só aplica resultado se o usuário não passou o parâmetro explicitamente */
        if (!app.video.device && det.video_device)
            app.video.device = det.video_device;
        if (!audio_source)
            audio_source = det.audio_source;  /* NULL = sem áudio, não-fatal */
    }

    /* Garante fallback de device se nada foi encontrado */
    if (!app.video.device)
        app.video.device = DEFAULT_DEVICE;

    printf ("camlink-view\n"
            "  device     : %s\n"
            "  audio      : %s\n"
            "  resolution : %dx%d @ %dfps\n",
            app.video.device,
            audio_source ? audio_source : "(none)",
            app.video.width, app.video.height, app.video.fps);

    install_signals ();

    /* Guarda dimensões pedidas antes do open para comparação */
    int req_w = app.video.width;
    int req_h = app.video.height;

    if (!renderer_init (&app.renderer, app.video.width, app.video.height))
        goto cleanup;

    video_set_on_stream_ready (&app.video, on_stream_ready, &app);
    if (!video_open (&app.video)) goto cleanup;

    /* Se o driver ajustou as dimensões, recria a textura com o tamanho real */
    if (app.video.width != req_w || app.video.height != req_h) {
        printf ("video : driver ajustou para %dx%d\n",
                app.video.width, app.video.height);
        if (!renderer_resize (&app.renderer, app.video.width, app.video.height))
            goto cleanup;
    }

    app.audio   = audio_create   (audio_source);             /* non-fatal */
    app.overlay = overlay_create (app.renderer.renderer);    /* non-fatal */

    /* Controles de imagem — lê valores diretamente do driver.
     * Não usa arquivo de config — o driver é a fonte de verdade. */
    controls_init (&app.controls, app.video.fd);

    app.menu = menu_create (app.renderer.renderer, &app.controls); /* non-fatal */

    printf ("Pronto. F=fullscreen | O=overlay | Q/Esc=sair\n\n");
    run (&app);

cleanup:
    menu_destroy     (app.menu);
    overlay_destroy  (app.overlay);
    audio_destroy    (app.audio);
    video_close      (&app.video);
    renderer_deinit  (&app.renderer);
    return 0;
}
