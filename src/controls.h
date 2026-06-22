/*
 * controls.h — Controles de imagem V4L2 (brilho, contraste, saturação)
 *
 * Lê valores atuais e padrões via VIDIOC_QUERYCTRL / VIDIOC_G_CTRL,
 * escreve via VIDIOC_S_CTRL, persiste em ~/.config/camlink-view.conf
 *
 * Nota: depende de common.h para bool (via stdbool.h). Sempre inclua
 * common.h antes deste header, ou inclua-o via common.h.
 */
#pragma once

#include "common.h"

/* Controles suportados */
typedef enum {
    CTRL_BRIGHTNESS = 0,
    CTRL_CONTRAST,
    CTRL_SATURATION,
    CTRL_COUNT
} CtrlId;

typedef struct {
    const char *name;       /* nome amigável para exibição (pode ter acentos) */
    const char *key;        /* chave ASCII usada no arquivo de config          */
    int         v4l2_id;    /* V4L2_CID_*                */
    int         value;      /* valor atual               */
    int         default_val; /* valor padrão reportado pelo kernel (qc.default_value) */
    int         initial_val; /* valor lido do device na inicialização — usado no reset */
    int         min;
    int         max;
    int         step;
    bool        supported;  /* false se o device não tem */
} Control;

typedef struct {
    Control controls[CTRL_COUNT];
    int     fd;             /* fd V4L2 — não owna, só referencia */
} Controls;

/* Inicializa, lê metadados e valores atuais do device */
void controls_init    (Controls *c, int v4l2_fd);

/* Incrementa/decrementa um controle por `step` passos */
void controls_inc     (Controls *c, CtrlId id, int steps);
void controls_dec     (Controls *c, CtrlId id, int steps);

/*
 * Restaura os controles ao estado inicial (valores lidos na inicialização).
 * Para restaurar os padrões de fábrica do kernel, use controls_reset_defaults.
 */
void controls_restore_initial  (Controls *c);
void controls_reset_defaults   (Controls *c);

/* Persiste valores atuais em ~/.config/camlink-view.conf */
bool controls_save    (const Controls *c);

/* Relê os valores atuais do device — usa driver como fonte de verdade */
void controls_refresh (Controls *c);

/* Persiste valores atuais em ~/.config/camlink-view.conf */
bool controls_save    (const Controls *c);

/* Carrega valores persistidos e os aplica ao device */
bool controls_load    (Controls *c);
