/*
 * overlay.h — HUD com FPS e latência, fonte bitmap 3×5 embutida
 */
#pragma once

#include "common.h"
#include "stats.h"
#include <SDL2/SDL.h>

/* ── Configuração da fonte ───────────────────────────────────────────────── */

#define FONT_SCALE  3
#define FONT_GAP    1
#define CHAR_W      ((FONT_W * FONT_SCALE) + (FONT_GAP * FONT_SCALE))

/* Dimensões máximas da texture do overlay */
#define LINE_PADDING   6    /* espaço vertical entre linhas do overlay */
#define OVERLAY_PAD_X  8    /* padding horizontal interno              */
#define OVERLAY_PAD_Y  6    /* padding vertical interno                */
#define OVERLAY_TEX_W  220
#define OVERLAY_TEX_H   60

typedef struct Overlay Overlay;

/* Cria overlay. Retorna NULL em falha (não-fatal). */
Overlay *overlay_create  (SDL_Renderer *renderer);

/* Libera recursos. Seguro com NULL. */
void     overlay_destroy (Overlay *ov);

/*
 * Renderiza stats na texture e blita no canto superior esquerdo
 * de frame_rect com fundo semi-transparente.
 */
void overlay_draw (Overlay      *ov,
                   SDL_Renderer *renderer,
                   SDL_Rect      frame_rect,
                   const Stats  *stats);
