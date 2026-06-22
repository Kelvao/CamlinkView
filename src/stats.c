#include "stats.h"
#include <stdint.h>

void
stats_update (Stats *s, double latency_ms)
{
    s->latency_ms = s->latency_ms * 0.8 + latency_ms * 0.2;
    s->fps_frame_acc++;

    const uint64_t now     = mono_ns ();
    const double   elapsed = (double)(now - s->last_fps_ts) / 1e9;
    if (elapsed >= 0.5) {
        s->fps           = (double)s->fps_frame_acc / elapsed;
        s->fps_frame_acc = 0;
        s->last_fps_ts   = now;
    }
}
