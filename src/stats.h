/*
 * stats.h — FPS e latência com EMA
 */
#pragma once

#include "common.h"

typedef struct {
    double   fps;
    double   latency_ms;
    uint64_t last_fps_ts;
    uint32_t fps_frame_acc;
} Stats;

/* Atualiza FPS (janela 0.5s) e latência via EMA 0.8/0.2 */
void stats_update (Stats *s, double latency_ms);
