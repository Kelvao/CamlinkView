/*
 * renderer.h — Janela SDL2, texturas, aspect ratio
 */
#pragma once

#include "common.h"
#include <SDL2/SDL.h>

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *tex_frame;
} Renderer;

/*
 * Inicializa SDL2 (vídeo apenas), cria janela, renderer e textura NV12.
 * width/height: dimensões do frame de entrada.
 */
bool renderer_init   (Renderer *r, int width, int height);

/*
 * Recria a textura NV12 com novas dimensões.
 * Útil quando video_open() ajusta width/height.
 */
bool renderer_resize (Renderer *r, int width, int height);

/* Destrói todas as texturas, renderer e janela. Chama SDL_Quit(VIDEO). */
void renderer_deinit (Renderer *r);

/*
 * Calcula o SDL_Rect de destino mantendo o aspect ratio da fonte,
 * com letterbox/pillarbox quando necessário.
 */
SDL_Rect renderer_aspect_rect (int win_w, int win_h, int src_w, int src_h);
