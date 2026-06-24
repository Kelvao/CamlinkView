#define _XOPEN_SOURCE 700
#include "controls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

/* Mapeamento CtrlId → V4L2_CID_* e nome amigável */
static const struct {
    const char *name;       /* nome de exibição (pode conter acentos) */
    const char *key;        /* chave ASCII usada no arquivo de config  */
    int         v4l2_id;
} CTRL_META[CTRL_COUNT] = {
    [CTRL_BRIGHTNESS] = { "Brilho",    "brightness", V4L2_CID_BRIGHTNESS },
    [CTRL_CONTRAST]   = { "Contraste", "contrast",   V4L2_CID_CONTRAST   },
    [CTRL_SATURATION] = { "Saturação", "saturation", V4L2_CID_SATURATION },
};

/* ── Helpers V4L2 ────────────────────────────────────────────────────────── */

static bool
query_ctrl (int fd, int v4l2_id, Control *c)
{
    struct v4l2_queryctrl qc = { .id = (uint32_t)v4l2_id };
    if (ioctl (fd, VIDIOC_QUERYCTRL, &qc) < 0) return false;
    if (qc.flags & V4L2_CTRL_FLAG_DISABLED)     return false;

    c->min         = qc.minimum;
    c->max         = qc.maximum;
    c->step        = qc.step > 0 ? qc.step : 1;
    c->default_val = qc.default_value;
    return true;
}

static int
get_ctrl (int fd, int v4l2_id, int fallback)
{
    /* Tenta a ioctl simples primeiro */
    struct v4l2_control vc = { .id = (uint32_t)v4l2_id };
    if (ioctl (fd, VIDIOC_G_CTRL, &vc) == 0)
        return vc.value;

    /* Alguns dispositivos UVC só respondem via G_EXT_CTRLS */
    struct v4l2_ext_control  exc  = { .id = (uint32_t)v4l2_id };
    struct v4l2_ext_controls excs = {
        .ctrl_class = V4L2_CTRL_CLASS_USER,
        .count      = 1,
        .controls   = &exc,
    };
    if (ioctl (fd, VIDIOC_G_EXT_CTRLS, &excs) == 0)
        return exc.value;

    return fallback;
}

static bool
set_ctrl (int fd, int v4l2_id, int value)
{
    struct v4l2_control vc = {
        .id    = (uint32_t)v4l2_id,
        .value = value,
    };
    if (ioctl (fd, VIDIOC_S_CTRL, &vc) == 0)
        return true;

    struct v4l2_ext_control  exc = { .id = (uint32_t)v4l2_id, .value = value };
    struct v4l2_ext_controls excs = {
        .ctrl_class = V4L2_CTRL_CLASS_USER,
        .count      = 1,
        .controls   = &exc,
    };
    return ioctl (fd, VIDIOC_S_EXT_CTRLS, &excs) == 0;
}

static int
clamp_val (const Control *c, int v)
{
    if (v < c->min) return c->min;
    if (v > c->max) return c->max;
    /* Snap to step grid from min */
    int offset = (v - c->min) / c->step * c->step;
    return c->min + offset;
}

/* ── Config path ─────────────────────────────────────────────────────────── */

static void
config_path (char *buf, size_t len)
{
    const char *home = getenv ("HOME");
    if (!home) home = "/tmp";
    snprintf (buf, len, "%s/.config/camlink-view.conf", home);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void
controls_init (Controls *c, int v4l2_fd)
{
    memset (c, 0, sizeof *c);   /* garante campos sem lixo de stack */
    c->fd = v4l2_fd;
    for (int i = 0; i < CTRL_COUNT; i++) {
        Control *ctrl = &c->controls[i];
        ctrl->name    = CTRL_META[i].name;
        ctrl->key     = CTRL_META[i].key;
        ctrl->v4l2_id = CTRL_META[i].v4l2_id;

        /* fd=-1 significa que o device ainda não foi aberto; adiar queries. */
        if (v4l2_fd < 0) {
            ctrl->supported = false;
            continue;
        }

        ctrl->supported = query_ctrl (v4l2_fd, ctrl->v4l2_id, ctrl);
        if (ctrl->supported) {
            ctrl->value       = get_ctrl (v4l2_fd, ctrl->v4l2_id, ctrl->default_val);
            ctrl->initial_val = ctrl->value;
        }
    }
}

void
controls_inc (Controls *c, CtrlId id, int steps)
{
    Control *ctrl = &c->controls[id];
    if (!ctrl->supported) return;
    ctrl->value = clamp_val (ctrl, ctrl->value + ctrl->step * steps);
    set_ctrl (c->fd, ctrl->v4l2_id, ctrl->value);
}

void
controls_dec (Controls *c, CtrlId id, int steps)
{
    controls_inc (c, id, -steps);
}

void
controls_refresh (Controls *c)
{
    /* Relê cada controle direto do driver — útil após VIDIOC_STREAMON
     * quando FLAG_INACTIVE é levantado e os valores reais ficam disponíveis.
     * Também inicializa metadados se controls_init foi chamado com fd=-1. */
    for (int i = 0; i < CTRL_COUNT; i++) {
        Control *ctrl = &c->controls[i];

        /* Se ainda não foi consultado (fd era inválido na init), faz agora */
        if (!ctrl->supported) {
            ctrl->supported = query_ctrl (c->fd, ctrl->v4l2_id, ctrl);
            if (!ctrl->supported) continue;
        }

        ctrl->value       = get_ctrl (c->fd, ctrl->v4l2_id, ctrl->default_val);
        ctrl->initial_val = ctrl->value;
    }
}

void
controls_restore_initial (Controls *c)
{
    for (int i = 0; i < CTRL_COUNT; i++) {
        Control *ctrl = &c->controls[i];
        if (!ctrl->supported) continue;
        ctrl->value = ctrl->initial_val;
        set_ctrl (c->fd, ctrl->v4l2_id, ctrl->value);
    }
}

void
controls_reset_defaults (Controls *c)
{
    for (int i = 0; i < CTRL_COUNT; i++) {
        Control *ctrl = &c->controls[i];
        if (!ctrl->supported) continue;
        ctrl->value = ctrl->default_val;
        set_ctrl (c->fd, ctrl->v4l2_id, ctrl->value);
    }
}

#define CONFIG_VERSION 1

bool
controls_save (const Controls *c)
{
    char path[512];
    config_path (path, sizeof path);

    FILE *f = fopen (path, "w");
    if (!f) {
        fprintf (stderr, "controls: fopen(%s): %s\n", path, strerror (errno));
        return false;
    }
    fprintf (f, "# camlink-view settings\n");
    fprintf (f, "version=%d\n", CONFIG_VERSION);
    for (int i = 0; i < CTRL_COUNT; i++) {
        const Control *ctrl = &c->controls[i];
        if (ctrl->supported)
            fprintf (f, "%s=%d\n", ctrl->key, ctrl->value);
    }
    fclose (f);
    return true;
}

bool
controls_load (Controls *c)
{
    char path[512];
    config_path (path, sizeof path);

    FILE *f = fopen (path, "r");
    if (!f) return false;   /* arquivo não existe ainda — não é erro */

    /* Primeira passagem: valida versão do arquivo.
     * Arquivos sem versão foram gerados por versões antigas com bug — ignora. */
    bool version_ok = false;
    char line[128];
    while (fgets (line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int ver;
        if (sscanf (line, "version=%d", &ver) == 1) {
            version_ok = (ver == CONFIG_VERSION);
            break;
        }
    }

    if (!version_ok) {
        fclose (f);
        fprintf (stderr, "controls: config sem versão ou incompatível — ignorando.\n"
                         "          Delete ~/.config/camlink-view.conf para resetar.\n");
        return false;
    }

    /* Segunda passagem: aplica valores */
    rewind (f);
    while (fgets (line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp (line, "version=", 8) == 0) continue;

        char key[64]; int val;
        if (sscanf (line, "%63[^=]=%d", key, &val) != 2) continue;

        for (int i = 0; i < CTRL_COUNT; i++) {
            Control *ctrl = &c->controls[i];
            if (!ctrl->supported) continue;
            if (strcmp (key, ctrl->key) == 0) {
                ctrl->value = clamp_val (ctrl, val);
                set_ctrl (c->fd, ctrl->v4l2_id, ctrl->value);
                break;
            }
        }
    }
    fclose (f);
    return true;
}
