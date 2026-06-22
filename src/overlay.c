#define FONT_IMPLEMENTATION
#include "font.h"
#include "overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
draw_string (SDL_Renderer *r, const char *s, int x, int y, SDL_Color fg)
{
    SDL_SetRenderDrawColor (r, fg.r, fg.g, fg.b, fg.a);
    for (int ci = 0; s[ci]; ci++) {
        const uint8_t *glyph = FONT3X5[glyph_idx (s[ci])];
        for (int col = 0; col < FONT_W; col++) {
            for (int row = 0; row < FONT_H; row++) {
                if (!((glyph[col] >> row) & 1)) continue;
                const SDL_Rect px = {
                    .x = x + (ci * CHAR_W) + (col * FONT_SCALE),
                    .y = y + (row * FONT_SCALE),
                    .w = FONT_SCALE,
                    .h = FONT_SCALE,
                };
                SDL_RenderFillRect (r, &px);
            }
        }
    }
}

/* ── Overlay ─────────────────────────────────────────────────────────────── */

struct Overlay {
    SDL_Texture *tex;
};

Overlay *
overlay_create (SDL_Renderer *renderer)
{
    Overlay *ov = calloc (1, sizeof *ov);
    if (!ov) return NULL;

    ov->tex = SDL_CreateTexture (renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        OVERLAY_TEX_W, OVERLAY_TEX_H);
    if (!ov->tex) { free (ov); return NULL; }

    SDL_SetTextureBlendMode (ov->tex, SDL_BLENDMODE_BLEND);
    return ov;
}

void
overlay_destroy (Overlay *ov)
{
    if (!ov) return;
    SDL_DestroyTexture (ov->tex);
    free (ov);
}

void
overlay_draw (Overlay      *ov,
              SDL_Renderer *renderer,
              SDL_Rect      frame_rect,
              const Stats  *stats)
{
    if (!ov) return;

    char fps_str[32], lat_str[32];
    snprintf (fps_str, sizeof fps_str, "%.1f fps", stats->fps);
    snprintf (lat_str, sizeof lat_str, "%.2f ms",  stats->latency_ms);

    const int line_h  = FONT_H * FONT_SCALE + LINE_PADDING;
    const int len_fps = (int)strlen (fps_str);
    const int len_lat = (int)strlen (lat_str);
    const int max_len = len_fps > len_lat ? len_fps : len_lat;

    /* bg dimensões calculadas dinamicamente — nunca excedem OVERLAY_TEX_* */
    const SDL_Rect bg = {
        .x = 0, .y = 0,
        .w = (max_len * CHAR_W) + (OVERLAY_PAD_X * 2),
        .h = (line_h * 2) + LINE_PADDING,
    };

    /* Garante que bg cabe na texture */
    if (bg.w > OVERLAY_TEX_W || bg.h > OVERLAY_TEX_H) {
        fprintf (stderr, "overlay: bg (%dx%d) excede texture (%dx%d)\n",
                 bg.w, bg.h, OVERLAY_TEX_W, OVERLAY_TEX_H);
        return;
    }

    /* ── Renderiza na texture ─────────────────────────────────────────── */
    SDL_SetRenderTarget (renderer, ov->tex);
    SDL_SetRenderDrawColor (renderer, 0, 0, 0, 0);
    SDL_RenderClear (renderer);
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor (renderer, 0, 0, 0, 120);
    SDL_RenderFillRect (renderer, &bg);

    const SDL_Color white = {255, 255, 255, 255};
    SDL_Color lat_col;
    if      (stats->latency_ms < 33.0) lat_col = (SDL_Color){80,  255, 120, 255};
    else if (stats->latency_ms < 50.0) lat_col = (SDL_Color){255, 220, 60,  255};
    else                               lat_col = (SDL_Color){255, 80,  80,  255};

    draw_string (renderer, fps_str, OVERLAY_PAD_X, OVERLAY_PAD_Y,           white);
    draw_string (renderer, lat_str, OVERLAY_PAD_X, OVERLAY_PAD_Y + line_h,  lat_col);

    SDL_SetRenderTarget (renderer, NULL);

    /* ── Blita só a região usada ──────────────────────────────────────── */
    const SDL_Rect dst = {
        .x = frame_rect.x + OVERLAY_PAD_X,
        .y = frame_rect.y + OVERLAY_PAD_Y,
        .w = bg.w,
        .h = bg.h,
    };
    SDL_RenderCopy (renderer, ov->tex, &bg, &dst);
}
