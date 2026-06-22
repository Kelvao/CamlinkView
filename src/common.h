/*
 * common.h — Tipos e constantes compartilhados entre módulos
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Build-time config ───────────────────────────────────────────────────── */

#define DEFAULT_DEVICE   "/dev/video0"
#define DEFAULT_WIDTH    1920
#define DEFAULT_HEIGHT   1080
#define DEFAULT_FPS      60

/* Returns monotonic time in nanoseconds */
uint64_t mono_ns (void);
