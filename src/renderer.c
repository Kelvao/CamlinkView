#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>

bool
renderer_init (Renderer *r, int width, int height)
{
    /* Hints antes do SDL_Init para ter efeito */
    SDL_SetHint (SDL_HINT_RENDER_VSYNC,         "0");
    SDL_SetHint (SDL_HINT_RENDER_SCALE_QUALITY, "2");

    /* Em sessão Wayland, força o backend wayland para evitar o crash
     * X_GLXCreateContext que ocorre quando o SDL usa XWayland + GLX.
     * Se o backend wayland falhar (compositor sem suporte), cai para x11. */
    const char *xdg = getenv ("XDG_SESSION_TYPE");
    if (xdg && strcmp (xdg, "wayland") == 0 && !getenv ("SDL_VIDEODRIVER"))
        setenv ("SDL_VIDEODRIVER", "wayland", 1);

    if (SDL_Init (SDL_INIT_VIDEO) < 0) {
        const char *vd = getenv ("SDL_VIDEODRIVER");
        if (vd && strcmp (vd, "wayland") == 0) {
            fprintf (stderr, "Wayland falhou (%s) — tentando x11\n", SDL_GetError ());
            setenv ("SDL_VIDEODRIVER", "x11", 1);
            if (SDL_Init (SDL_INIT_VIDEO) < 0) {
                fprintf (stderr, "SDL_Init(VIDEO): %s\n", SDL_GetError ());
                return false;
            }
        } else {
            fprintf (stderr, "SDL_Init(VIDEO): %s\n", SDL_GetError ());
            return false;
        }
    }

    r->window = SDL_CreateWindow (
        "Camlink 4k View",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!r->window) {
        fprintf (stderr, "SDL_CreateWindow: %s\n", SDL_GetError ());
        SDL_QuitSubSystem (SDL_INIT_VIDEO);
        return false;
    }

    r->renderer = SDL_CreateRenderer (r->window, -1, SDL_RENDERER_ACCELERATED);
    if (!r->renderer) {
        fprintf (stderr, "renderer acelerado indisponível (%s) — usando software\n",
                 SDL_GetError ());
        r->renderer = SDL_CreateRenderer (r->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!r->renderer) {
        fprintf (stderr, "SDL_CreateRenderer: %s\n", SDL_GetError ());
        SDL_DestroyWindow (r->window);
        SDL_QuitSubSystem (SDL_INIT_VIDEO);
        return false;
    }

    return renderer_resize (r, width, height);
}

bool
renderer_resize (Renderer *r, int width, int height)
{
    if (r->tex_frame)
        SDL_DestroyTexture (r->tex_frame);

    r->tex_frame = SDL_CreateTexture (r->renderer,
        SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING,
        width, height);
    if (!r->tex_frame) {
        fprintf (stderr, "SDL_CreateTexture: %s\n", SDL_GetError ());
        return false;
    }
    return true;
}

void
renderer_deinit (Renderer *r)
{
    if (r->tex_frame) SDL_DestroyTexture  (r->tex_frame);
    if (r->renderer)  SDL_DestroyRenderer (r->renderer);
    if (r->window) {
        SDL_DestroyWindow (r->window);
        SDL_QuitSubSystem (SDL_INIT_VIDEO);
    }
}

SDL_Rect
renderer_aspect_rect (int win_w, int win_h, int src_w, int src_h)
{
    const float src_r = (float)src_w / (float)src_h;
    const float win_r = (float)win_w / (float)win_h;
    SDL_Rect dst;
    if (win_r > src_r) {
        dst.h = win_h;
        dst.w = (int)((float)win_h * src_r);
        dst.x = (win_w - dst.w) / 2;
        dst.y = 0;
    } else {
        dst.w = win_w;
        dst.h = (int)((float)win_w / src_r);
        dst.x = 0;
        dst.y = (win_h - dst.h) / 2;
    }
    return dst;
}
