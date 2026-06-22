#define _XOPEN_SOURCE 700
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>

/* pw_init/pw_deinit são globais — rastreamos com refcount.
 * NOTA: não é thread-safe; audio_create deve ser chamado do thread principal. */
static int pw_init_count = 0;

/*
 * Arquitetura PipeWire nativa (sem SDL):
 *
 *   [Camlink source] ──► [capture stream] ──► on_process ──► [playback stream]
 *                                                              │
 *                                                         visível pro Discord
 *                                                    como Stream/Output/Audio
 *
 * O playback stream é registrado com media.role=Game para que o Discord
 * identifique e capture junto com a janela.
 */
struct AudioState {
    struct pw_thread_loop *loop;
    struct pw_context     *context;
    struct pw_core        *core;
    struct pw_stream      *capture;        /* input: Camlink source  */
    struct pw_stream      *playback;       /* output: visível Discord */
    struct spa_hook        capture_hook;   /* deve sobreviver enquanto o stream existir */
    struct spa_hook        playback_hook;
    bool                   running;
};

/* ── Format params compartilhado ─────────────────────────────────────────── */

static const struct spa_pod *
build_audio_params (struct spa_pod_builder *b)
{
    return spa_format_audio_raw_build (b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT (
            .format   = SPA_AUDIO_FORMAT_F32,
            .rate     = AUDIO_RATE,
            .channels = AUDIO_CHANNELS));
}

/* ── Capture → Playback bridge ───────────────────────────────────────────── */

static void
on_capture_process (void *userdata)
{
    AudioState *a = userdata;

    struct pw_buffer *in = pw_stream_dequeue_buffer (a->capture);
    if (!in) return;

    struct spa_buffer *inbuf = in->buffer;
    uint32_t           size  = inbuf->datas[0].chunk->size;

    if (size > 0 && inbuf->datas[0].data) {
        /* Enfileira direto no playback stream */
        struct pw_buffer *out = pw_stream_dequeue_buffer (a->playback);
        if (out) {
            struct spa_buffer *outbuf = out->buffer;
            uint32_t           cap    = outbuf->datas[0].maxsize;
            uint32_t           copy   = size < cap ? size : cap;

            memcpy (outbuf->datas[0].data, inbuf->datas[0].data, copy);
            outbuf->datas[0].chunk->size   = copy;
            outbuf->datas[0].chunk->offset = 0;
            outbuf->datas[0].chunk->stride = sizeof (float) * AUDIO_CHANNELS;
            pw_stream_queue_buffer (a->playback, out);
        }
    }

    pw_stream_queue_buffer (a->capture, in);
}

static const struct pw_stream_events capture_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .process = on_capture_process,
};

static const struct pw_stream_events playback_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    /* playback só precisa de trigger — o bridge é feito no capture */
};

/* ── Public API ──────────────────────────────────────────────────────────── */

AudioState *
audio_create (const char *source)
{
    if (!source) return NULL;

    AudioState *a = calloc (1, sizeof *a);
    if (!a) return NULL;

    if (pw_init_count == 0)
        pw_init (NULL, NULL);
    pw_init_count++;

    a->loop = pw_thread_loop_new ("camlink-audio", NULL);
    if (!a->loop) {
        fprintf (stderr, "audio: pw_thread_loop_new failed\n");
        goto err_pw;
    }

    a->context = pw_context_new (
        pw_thread_loop_get_loop (a->loop), NULL, 0);
    if (!a->context) {
        fprintf (stderr, "audio: pw_context_new failed\n");
        goto err_loop;
    }

    a->core = pw_context_connect (a->context, NULL, 0);
    if (!a->core) {
        fprintf (stderr, "audio: pw_context_connect failed\n");
        goto err_context;
    }

    /* ── Capture stream (input da Camlink) ───────────────────────────── */
    struct pw_properties *cap_props = pw_properties_new (
        PW_KEY_MEDIA_TYPE,     "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE,     "Production",
        PW_KEY_APP_NAME,       "Camlink View",
        PW_KEY_NODE_NAME,      "camlink-capture",
        "target.object",        source,
        NULL);

    a->capture = pw_stream_new (a->core, "camlink-capture", cap_props);
    if (!a->capture) {
        fprintf (stderr, "audio: capture stream failed\n");
        goto err_core;
    }

    /* ── Playback stream (output visível pro Discord) ────────────────── */
    struct pw_properties *play_props = pw_properties_new (
        PW_KEY_MEDIA_TYPE,     "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE,     "Game",          /* Discord captura Game/Movie */
        PW_KEY_APP_NAME,       "Camlink View",
        PW_KEY_NODE_NAME,      "camlink-playback",
        PW_KEY_MEDIA_NAME,     "Camlink 4K Audio",
        NULL);

    a->playback = pw_stream_new (a->core, "camlink-playback", play_props);
    if (!a->playback) {
        fprintf (stderr, "audio: playback stream failed\n");
        goto err_capture;
    }

    /* Registra eventos — spa_hook deve persistir enquanto o stream existir */
    memset (&a->capture_hook,  0, sizeof a->capture_hook);
    memset (&a->playback_hook, 0, sizeof a->playback_hook);
    pw_stream_add_listener (a->capture,  &a->capture_hook,  &capture_events,  a);
    pw_stream_add_listener (a->playback, &a->playback_hook, &playback_events, a);

    /* Conecta os dois streams */
    uint8_t pod_buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT (pod_buf, sizeof pod_buf);
    const struct spa_pod *params[1] = { build_audio_params (&b) };

    pw_stream_connect (a->capture,
        PW_DIRECTION_INPUT, PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
        params, 1);

    /* Reinicia o builder para o segundo connect */
    b = (struct spa_pod_builder) SPA_POD_BUILDER_INIT (pod_buf, sizeof pod_buf);
    params[0] = build_audio_params (&b);

    pw_stream_connect (a->playback,
        PW_DIRECTION_OUTPUT, PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
        params, 1);

    pw_thread_loop_start (a->loop);
    a->running = true;

    printf ("audio : PipeWire native  capture→playback  source=%s\n", source);
    return a;

err_capture:
    pw_stream_destroy (a->capture);
err_core:
    pw_core_disconnect (a->core);
err_context:
    pw_context_destroy (a->context);
err_loop:
    pw_thread_loop_destroy (a->loop);
err_pw:
    if (--pw_init_count == 0)
        pw_deinit ();
    free (a);
    return NULL;
}

void
audio_destroy (AudioState *a)
{
    if (!a) return;
    pw_thread_loop_stop    (a->loop);
    pw_stream_destroy      (a->playback);
    pw_stream_destroy      (a->capture);
    pw_core_disconnect     (a->core);
    pw_context_destroy     (a->context);
    pw_thread_loop_destroy (a->loop);
    if (--pw_init_count == 0)
        pw_deinit ();
    free (a);
}
