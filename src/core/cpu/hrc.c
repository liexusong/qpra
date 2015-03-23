/*
 * core/cpu/hrc.c -- CPU high resolution counter.
 *
 *
 */

#include <string.h>

#include "core/cpu/hrc.h"

int hrc_hz[] = {
    -1,
    60, 120, 240, 480, 960,
    -1, -1
};

int hrc_cycles[] = {
    -1,
    65536, 32768, 16384, 8192, 4096,
    -1, -1
};

int hrc_us[] = {
    -1,
    16667, 8334, 4167, 2084, 1042,
    -1, -1
};

static inline void core_cpu_hrc__diff(struct core_hrc *hrc)
{
    int old_hz = hrc->elapsed_hz;
    hrc->elapsed_us = (hrc->cur.tv_sec - hrc->start.tv_sec) * 1000 +
        (hrc->cur.tv_nsec - hrc->start.tv_nsec) / 1000;
    hrc->elapsed_hz = ((long)CPU_FREQ_HZ * (long)hrc->elapsed_us) / 1000;
    hrc->v -= hrc->elapsed_hz - old_hz;
}

static inline void core_cpu_hrc__trigger_int(struct core_cpu *cpu)
{
    cpu->interrupt = INT_TIMER_IRQ;
}

void core_cpu_hrc_init(struct core_cpu *cpu)
{
    memset(cpu->hrc, 0, sizeof(struct core_hrc));
}

void core_cpu_hrc_step(struct core_cpu *cpu)
{
    struct core_hrc *hrc = cpu->hrc;

    if(hrc->type == HRC_DISABLED ||
       hrc->type == HRC_DISABLED6 || hrc->type == HRC_DISABLED7)
        return;

    /* Update the elapsed time; if we have reached one counter cycle, trigger
     * an interrupt. */
    clock_gettime(CLOCK_MONOTONIC, &hrc->cur);
    core_cpu_hrc__diff(hrc);
    if(hrc->elapsed_hz >= hrc_hz[hrc->type])
        core_cpu_hrc__trigger_int(cpu);
}

void core_cpu_hrc_settype(struct core_hrc *hrc, int type)
{
    switch(type) {
        case HRC_DISABLED:
        case HRC_60HZ:
        case HRC_120HZ:
        case HRC_240HZ:
        case HRC_480HZ:
        case HRC_960HZ:
        case HRC_DISABLED6:
        case HRC_DISABLED7:
            hrc->type = type;
            break;
        default:
            hrc->type = HRC_DISABLED;
            break;
    }
}
