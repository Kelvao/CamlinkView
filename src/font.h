/*
 * font.h — Fonte bitmap 3×5 para o overlay (FPS/latência)
 *
 * Cobre apenas os caracteres usados pelo overlay: dígitos 0–9,
 * ponto '.', e as letras 'f','p','m','s' (sufixos "fps" e "ms").
 * Não use para texto alfabético genérico — use menu_font.h.
 *
 * Inclua em APENAS UMA unidade de compilação com FONT_IMPLEMENTATION
 * definido (overlay.c).
 */
#pragma once

#include "common.h"

/* Dimensões do glifo — definidas aqui, usadas por overlay.h e overlay.c */
#define FONT_W  3
#define FONT_H  5

#ifdef FONT_IMPLEMENTATION

/*
 * Índices fixos:
 *   0–9 = dígitos '0'–'9'
 *   10  = '.'
 *   11  = 'm'
 *   12  = 's'
 *   13  = 'f'
 *   14  = 'p'
 *   15  = ' ' (espaço / fallback)
 *
 * Cada glifo = FONT_W (3) colunas; cada coluna = FONT_H (5) bits, bit0=topo.
 */
const uint8_t FONT3X5[][FONT_W] = {
    /* 0 */ {0x1F, 0x11, 0x1F},
    /* 1 */ {0x12, 0x1F, 0x10},
    /* 2 */ {0x1D, 0x15, 0x17},
    /* 3 */ {0x11, 0x15, 0x1F},
    /* 4 */ {0x07, 0x04, 0x1F},
    /* 5 */ {0x17, 0x15, 0x1D},
    /* 6 */ {0x1F, 0x15, 0x1D},
    /* 7 */ {0x01, 0x01, 0x1F},
    /* 8 */ {0x1F, 0x15, 0x1F},
    /* 9 */ {0x17, 0x15, 0x1F},
    /* . */ {0x10, 0x00, 0x00},
    /* m */ {0x1F, 0x02, 0x1F},
    /* s */ {0x17, 0x15, 0x1D},
    /* f */ {0x1F, 0x05, 0x01},
    /* p */ {0x1F, 0x05, 0x07},
    /*   */ {0x00, 0x00, 0x00},
};

int
glyph_idx (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    switch (c) {
    case '.': return 10;
    case 'm': return 11;
    case 's': return 12;
    case 'f': return 13;
    case 'p': return 14;
    default:  return 15;
    }
}

#else

extern const uint8_t FONT3X5[][FONT_W];
int glyph_idx (char c);

#endif /* FONT_IMPLEMENTATION */
