#include "menu.h"
#define MENU_FONT_IMPLEMENTATION
#include "menu_font.h" /* MFONT, mfont_glyph_idx — fonte 5×7 própria do menu */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Layout ──────────────────────────────────────────────────────────────── */

#define MENU_SCALE       2              /* pixels por pixel de fonte          */
#define MENU_CHAR_W      (MFONT_W * MENU_SCALE + MFONT_GAP * MENU_SCALE)
#define MENU_CHAR_H      (MFONT_H * MENU_SCALE)
#define MENU_LINE_GAP    8              /* espaço extra entre linhas (px)     */
#define MENU_LINE_H      (MENU_CHAR_H + MENU_LINE_GAP)
#define MENU_PAD         18             /* padding interno                    */
#define MENU_LABEL_COLS  11             /* colunas reservadas para o label    */
#define BAR_W            130            /* largura da barra de progresso (px) */
#define BAR_H            (MENU_CHAR_H)
#define VAL_COLS         4              /* colunas para o valor numérico      */

/* largura total do menu */
#define MENU_INNER_W  (MENU_LABEL_COLS * MENU_CHAR_W + BAR_W + VAL_COLS * MENU_CHAR_W + MENU_PAD)
#define MENU_W        (MENU_INNER_W + MENU_PAD * 2)

/* linhas: título + separador + CTRL_COUNT itens + separador + rodapé */
#define MENU_ROWS     (CTRL_COUNT + 4)
#define MENU_H        (MENU_ROWS * MENU_LINE_H + MENU_PAD * 2)

/* ── Paleta ──────────────────────────────────────────────────────────────── */

static const SDL_Color C_BG       = {  15,  15,  20, 220};
static const SDL_Color C_BORDER   = {  80,  80, 120, 255};
static const SDL_Color C_TITLE    = { 200, 200, 255, 255};
static const SDL_Color C_LABEL    = { 180, 180, 180, 255};
static const SDL_Color C_SEL_BG   = {  40,  60, 100, 255};
static const SDL_Color C_SEL_TEXT = { 255, 255, 255, 255};
static const SDL_Color C_BAR_BG   = {  50,  50,  60, 255};
static const SDL_Color C_BAR_FG   = {  80, 160, 255, 255};
static const SDL_Color C_HINT     = { 120, 120, 140, 255};
static const SDL_Color C_SAVED    = {  80, 220, 120, 255};

/* ── State ───────────────────────────────────────────────────────────────── */

struct Menu {
    SDL_Texture *tex;
    Controls    *controls;
    int          selected;   /* índice do item selecionado */
    bool         open;
    bool         dirty;      /* valores mudaram desde o último save */
    bool         just_saved; /* feedback visual de "salvo"          */
    uint32_t     saved_ts;   /* timestamp do feedback               */
    bool         render_dirty; /* texture precisa ser rerenderizada */
};

/* ── Font helpers ────────────────────────────────────────────────────────── */

static void
menu_draw_string (SDL_Renderer *r, const char *s, int x, int y, SDL_Color fg)
{
    SDL_SetRenderDrawColor (r, fg.r, fg.g, fg.b, fg.a);
    for (int ci = 0; s[ci]; ci++) {
        const uint8_t *glyph = MFONT[mfont_glyph_idx (s[ci])];
        for (int col = 0; col < MFONT_W; col++) {
            for (int row = 0; row < MFONT_H; row++) {
                if (!((glyph[col] >> row) & 1)) continue;
                SDL_Rect px = {
                    x + ci * MENU_CHAR_W + col * MENU_SCALE,
                    y + row * MENU_SCALE,
                    MENU_SCALE, MENU_SCALE
                };
                SDL_RenderFillRect (r, &px);
            }
        }
    }
}

/* ── Render helpers ──────────────────────────────────────────────────────── */

static void
draw_hline (SDL_Renderer *r, int x, int y, int w, SDL_Color c)
{
    SDL_SetRenderDrawColor (r, c.r, c.g, c.b, c.a);
    SDL_Rect line = { x, y, w, 1 };
    SDL_RenderFillRect (r, &line);
}

static void
draw_bar (SDL_Renderer *r, int x, int y, int w, int h,
          int val, int min, int max)
{
    /* Background */
    SDL_SetRenderDrawColor (r, C_BAR_BG.r, C_BAR_BG.g, C_BAR_BG.b, C_BAR_BG.a);
    SDL_Rect bg = { x, y, w, h };
    SDL_RenderFillRect (r, &bg);

    /* Fill proporcional */
    int fill = (max > min)
        ? (int)((float)(val - min) / (float)(max - min) * (float)w)
        : 0;
    if (fill > 0) {
        SDL_SetRenderDrawColor (r, C_BAR_FG.r, C_BAR_FG.g, C_BAR_FG.b, C_BAR_FG.a);
        SDL_Rect fg = { x, y, fill, h };
        SDL_RenderFillRect (r, &fg);
    }

    /* Borda */
    SDL_SetRenderDrawColor (r, C_BORDER.r, C_BORDER.g, C_BORDER.b, 180);
    SDL_RenderDrawRect (r, &bg);
}

/* ── Render do menu ──────────────────────────────────────────────────────── */

static void
render_menu (Menu *menu, SDL_Renderer *renderer)
{
    SDL_SetRenderTarget (renderer, menu->tex);
    SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);

    /* Fundo */
    SDL_SetRenderDrawColor (renderer, C_BG.r, C_BG.g, C_BG.b, C_BG.a);
    SDL_RenderClear (renderer);

    /* Borda */
    SDL_SetRenderDrawColor (renderer, C_BORDER.r, C_BORDER.g, C_BORDER.b, C_BORDER.a);
    SDL_Rect border = { 0, 0, MENU_W, MENU_H };
    SDL_RenderDrawRect (renderer, &border);

    int y = MENU_PAD;
    const int x0 = MENU_PAD;

    /* Título */
    menu_draw_string (renderer, "camlink view", x0, y, C_TITLE);
    y += MENU_LINE_H;

    /* Separador */
    draw_hline (renderer, x0, y, MENU_INNER_W, C_BORDER);
    y += MENU_LINE_H;

    /* Itens de controle */
    for (int i = 0; i < CTRL_COUNT; i++) {
        const Control *ctrl = &menu->controls->controls[i];
        if (!ctrl->supported) continue;

        bool sel = (i == menu->selected);

        /* Fundo de seleção */
        if (sel) {
            SDL_SetRenderDrawColor (renderer,
                C_SEL_BG.r, C_SEL_BG.g, C_SEL_BG.b, C_SEL_BG.a);
            SDL_Rect selbg = { x0 - 4, y - 2, MENU_INNER_W + 8, MENU_LINE_H };
            SDL_RenderFillRect (renderer, &selbg);
        }

        SDL_Color label_col = sel ? C_SEL_TEXT : C_LABEL;

        /* Seta de seleção */
        if (sel)
            menu_draw_string (renderer, ".", x0, y, C_SEL_TEXT);

        /* Label */
        menu_draw_string (renderer, ctrl->name,
            x0 + MENU_CHAR_W * 2, y, label_col);

        /* Barra de progresso */
        int bar_x = x0 + MENU_LABEL_COLS * MENU_CHAR_W;
        int bar_y = y + (MENU_LINE_H - BAR_H) / 2;
        draw_bar (renderer, bar_x, bar_y, BAR_W, BAR_H,
                  ctrl->value, ctrl->min, ctrl->max);

        /* Valor numérico */
        char val_str[16];
        snprintf (val_str, sizeof val_str, "%d", ctrl->value);
        menu_draw_string (renderer, val_str,
            bar_x + BAR_W + 8, y, label_col);

        y += MENU_LINE_H;
    }

    /* Separador */
    draw_hline (renderer, x0, y, MENU_INNER_W, C_BORDER);
    y += MENU_LINE_H;

    /* Rodapé — hints */
    uint32_t now = SDL_GetTicks ();
    bool show_saved = menu->just_saved && (now - menu->saved_ts < 1500);

    if (show_saved) {
        menu_draw_string (renderer, "salvo.", x0, y, C_SAVED);
    } else {
        menu_draw_string (renderer, "s.salva", x0, y,
            menu->dirty ? C_HINT : C_LABEL);
        const char *hint_r = "r.reset";
        int rx = x0 + MENU_INNER_W - (int)strlen (hint_r) * MENU_CHAR_W;
        menu_draw_string (renderer, hint_r, rx, y, C_HINT);
    }

    SDL_SetRenderTarget (renderer, NULL);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

Menu *
menu_create (SDL_Renderer *renderer, Controls *controls)
{
    Menu *m = calloc (1, sizeof *m);
    if (!m) return NULL;

    m->controls = controls;
    m->selected = 0;
    m->open     = false;
    m->dirty    = false;

    /* Pula para o primeiro controle suportado */
    for (int i = 0; i < CTRL_COUNT; i++) {
        if (controls->controls[i].supported) { m->selected = i; break; }
    }

    m->tex = SDL_CreateTexture (renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        MENU_W, MENU_H);
    if (!m->tex) { free (m); return NULL; }
    SDL_SetTextureBlendMode (m->tex, SDL_BLENDMODE_BLEND);
    return m;
}

void
menu_destroy (Menu *m)
{
    if (!m) return;
    SDL_DestroyTexture (m->tex);
    free (m);
}

bool
menu_is_open (const Menu *m)
{
    return m && m->open;
}

void
menu_toggle (Menu *m)
{
    if (m) { m->open = !m->open; m->render_dirty = true; }
}

bool
menu_handle_event (Menu *m, const SDL_Event *ev)
{
    if (!m || !m->open) return false;
    if (ev->type != SDL_KEYDOWN) return false;

    switch (ev->key.keysym.sym) {

    /* Navegação vertical */
    case SDLK_UP: {
        int orig = m->selected;
        do {
            m->selected = (m->selected - 1 + CTRL_COUNT) % CTRL_COUNT;
        } while (!m->controls->controls[m->selected].supported
                 && m->selected != orig);
        m->render_dirty = true;
        return true;
    }

    case SDLK_DOWN: {
        int orig = m->selected;
        do {
            m->selected = (m->selected + 1) % CTRL_COUNT;
        } while (!m->controls->controls[m->selected].supported
                 && m->selected != orig);
        m->render_dirty = true;
        return true;
    }

    /* Ajuste de valor */
    case SDLK_RIGHT:
        controls_inc (m->controls, (CtrlId)m->selected, 1);
        m->dirty = true;
        m->render_dirty = true;
        return true;

    case SDLK_LEFT:
        controls_dec (m->controls, (CtrlId)m->selected, 1);
        m->dirty = true;
        m->render_dirty = true;
        return true;

    /* Passo grande com shift */
    case SDLK_PAGEUP:
        controls_inc (m->controls, (CtrlId)m->selected, 10);
        m->dirty = true;
        m->render_dirty = true;
        return true;

    case SDLK_PAGEDOWN:
        controls_dec (m->controls, (CtrlId)m->selected, 10);
        m->dirty = true;
        m->render_dirty = true;
        return true;

    /* Ações */
    case SDLK_r:
        controls_restore_initial (m->controls);
        m->dirty = true;
        m->render_dirty = true;
        return true;

    case SDLK_s:
        if (controls_save (m->controls)) {
            m->dirty      = false;
            m->just_saved = true;
            m->saved_ts   = SDL_GetTicks ();
            m->render_dirty = true;
        }
        return true;

    /* Fecha o menu mas propaga para o caller processar também */
    case SDLK_m:
        m->open = false;
        return true;

    /* Esc fecha o menu apenas — não propaga (não sai do app) */
    case SDLK_ESCAPE:
        m->open = false;
        return true;

    /* Teclas globais — fecha o menu e deixa o caller processar */
    case SDLK_f:
    case SDLK_o:
    case SDLK_q:
        m->open = false;
        return false;   /* não consome — caller recebe e age */

    default:
        return true;   /* consome todos os outros eventos enquanto aberto */
    }
}

void
menu_draw (Menu *m, SDL_Renderer *renderer, int win_w, int win_h)
{
    if (!m || !m->open) return;

    /* Limpa feedback de "salvo" após 1.5s — marca dirty para redesenhar */
    if (m->just_saved && SDL_GetTicks () - m->saved_ts >= 1500) {
        m->just_saved  = false;
        m->render_dirty = true;
    }

    /* Só rerenderiza a texture quando algo mudou */
    if (m->render_dirty) {
        render_menu (m, renderer);
        m->render_dirty = false;
    }

    /* Centraliza na janela */
    const SDL_Rect dst = {
        .x = (win_w - MENU_W) / 2,
        .y = (win_h - MENU_H) / 2,
        .w = MENU_W,
        .h = MENU_H,
    };
    SDL_RenderCopy (renderer, m->tex, NULL, &dst);
}
