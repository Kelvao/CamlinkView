/*
 * menu.h — OSD estilo monitor: navegação por controles de imagem
 *
 * Layout:
 *
 *  ┌─────────────────────────────┐
 *  │  CAMLINK VIEW               │
 *  │  ─────────────────────────  │
 *  │  Brilho    [████████░░] 80  │  ← selecionado
 *  │  Contraste [██████░░░░] 60  │
 *  │  Saturação [███████░░░] 70  │
 *  │  ─────────────────────────  │
 *  │  [R] Reset   [S] Salvar     │
 *  └─────────────────────────────┘
 *
 * Teclas:
 *   ↑ / ↓   — navega entre itens
 *   ← / →   — decrementa / incrementa valor
 *   R        — reset para padrão
 *   S        — salva configuração
 *   M / Esc  — fecha o menu
 */
#pragma once

#include "common.h"
#include "controls.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct Menu Menu;

/* Cria o menu. Retorna NULL em falha (não-fatal). */
Menu *menu_create  (SDL_Renderer *renderer, Controls *controls);

/* Libera recursos. Seguro com NULL. */
void  menu_destroy (Menu *menu);

/* Retorna true se o menu está visível. */
bool  menu_is_open (const Menu *menu);

/* Abre ou fecha o menu. */
void  menu_toggle  (Menu *menu);

/*
 * Processa um evento SDL.
 * Retorna true se o evento foi consumido pelo menu (não repassar ao caller).
 */
bool  menu_handle_event (Menu *menu, const SDL_Event *ev);

/*
 * Renderiza o menu centralizado na janela.
 * Não faz nada se o menu estiver fechado.
 */
void  menu_draw (Menu *menu, SDL_Renderer *renderer, int win_w, int win_h);
