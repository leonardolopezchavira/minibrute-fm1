/* SPDX-License-Identifier: MIT
 * MB-1's effects: a delay (mono or ping-pong, a low-pass in its feedback) and then a plate reverb (after Dattorro's
 * "Effect Design, Part 1", 1997: four input diffusers, two cross-fed tanks), in series as the MiniBrute V plugin's
 * FX rack in its usual order. Mono in, stereo out. The delay lines are 16-bit (the memory: 1.5 s of delay, the
 * reverb, its pre-delay in about 220 KB), the state is float.
 *
 * mbfx_process gives the wet part only: the delay's and the reverb's, without the dry signal, which the caller
 * adds itself (mbfx_dry_gain): so that the dry signal can go out at once while the effects run a block behind (on
 * the second core). */
#ifndef MBFX_H
#define MBFX_H
#include <stdint.h>

#define MBFX_DL_LEN 65536u                /* delay memory (samples): 1.48 s mono, 0.74 s each way ping-pong */
#define MBFX_PRE_LEN 8832u                /* reverb pre-delay: 0.2 s */
#define MBFX_REV_LEN 33536u               /* the reverb's lines together (at 44.1 kHz) */
#define MBFX_MEM (MBFX_DL_LEN + MBFX_PRE_LEN + MBFX_REV_LEN)   /* int16 samples the caller gives */

typedef struct {
    float d_time_s;                       /* delay time (the caller works out a synced one) */
    float d_fb;                           /* feedback 0..1 */
    float d_lp_hz;                        /* the low-pass in the feedback (and on the repeats) */
    float d_mix;                          /* 0..1 (0: the delay is off) */
    uint32_t d_pp;                        /* 1: ping-pong */
    float r_decay;                        /* the tank's decay, 0..0.925 (the plugin's DECAY) */
    float r_size;                         /* 0.1..1.5 (the plugin's SIZE) */
    float r_damp;                         /* 0..1 */
    float r_mix;                          /* 0..1 (0: the reverb is off) */
    float r_pre_s;                        /* pre-delay, 0..0.2 s */
} MBFxParams;

typedef struct { int16_t *buf; uint32_t len, w; } MBFxLine;

typedef struct {
    float sr;
    MBFxParams p;
    int16_t *dl;                          /* the delay memory */
    uint32_t dw;                          /* its write position */
    float d_cur;                          /* the delay time now, in samples (eased toward the set one) */
    float d_lpa, d_lpb, d_hpa, d_hpb, d_hpxa, d_hpxb;   /* the feedback filters' states (line A, B) */
    float d_lpc;
    uint32_t d_pp_was, d_on, r_on;
    MBFxLine pre, in_ap[4], ap1[2], dl1[2], ap2[2], dl2[2];
    float r_lp_in, r_damp_st[2], r_tank_fb[2];
    float r_lc, r_ls;                    /* the tank LFO: cos, sin */
    float r_scale;                        /* the size now (the tank's lines read shorter) */
} MBFx;

void mbfx_init(MBFx *f, float sr, int16_t *mem);   /* mem: MBFX_MEM int16 samples */
void mbfx_set(MBFx *f, const MBFxParams *p);
/* in: n mono samples; outL/outR: the wet part (added to, not overwritten) */
void mbfx_process(MBFx *f, const float *in, float *outL, float *outR, int n);
float mbfx_dry_gain(const MBFxParams *p);   /* the dry signal's share: (1 - delay mix) (1 - reverb mix) */
void mbfx_clear(MBFx *f);                    /* silence (the lines zeroed) */

#endif
