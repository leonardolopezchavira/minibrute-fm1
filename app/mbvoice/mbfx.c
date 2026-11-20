/* SPDX-License-Identifier: MIT
 * MB-1's effects: a delay, then a plate reverb (mbfx.h). */
#include "mbfx.h"
#include "mb_port.h"

#define S16 8192.0f                       /* a line's full scale: +-4 (headroom for the tanks and the repeats) */
#define S16_INV (1.0f / 8192.0f)

static inline int16_t to16(float v)       /* (a NaN, or a value a misread made huge: silence, or the rail) */
{
    if (!(v > -3.999f))
        v = v < 0.0f ? -3.999f : 0.0f;
    else if (!(v < 3.999f))
        v = 3.999f;
    return (int16_t)(v * S16 + (v < 0.0f ? -0.5f : 0.5f));   /* (rounded: a tail fades, not cut) */
}
static inline float from16(int16_t s) { return (float)s * S16_INV; }

/* -------------------------------------------------------------------- lines --- */
static int16_t *line_at(MBFxLine *l, int16_t *mem, uint32_t len)
{
    l->buf = mem;
    l->len = len;
    l->w = 0;
    return mem + len;
}
static inline float line_tap(const MBFxLine *l, uint32_t d)   /* the sample d ago (1 .. len) */
{
    uint32_t i = l->w + l->len - d;
    if (i >= l->len)
        i -= l->len;
    return from16(l->buf[i]);
}
static inline float line_tapf(const MBFxLine *l, float d)     /* fractional, linear */
{
    uint32_t di = (uint32_t)d;
    float fr = d - (float)di, a = line_tap(l, di), b = line_tap(l, di + 1u);
    return a + fr * (b - a);
}
static inline void line_put(MBFxLine *l, float v)
{
    l->buf[l->w] = to16(v);
    if (++l->w >= l->len)
        l->w = 0;
}
/* an allpass over a line: the read d back */
static inline float allpass(MBFxLine *l, float x, float g, uint32_t d)
{
    float z = line_tap(l, d), v = x - g * z;
    line_put(l, v);
    return z + g * v;
}
static inline float allpass_f(MBFxLine *l, float x, float g, float d)
{
    float z = line_tapf(l, d), v = x - g * z;
    line_put(l, v);
    return z + g * v;
}

/* --------------------------------------------------------------- the reverb --- */
/* Dattorro's lengths (29761 Hz) times 1.4818 for 44.1 kHz */
static const uint32_t IN_AP[4] = {210, 159, 562, 410};
static const uint32_t AP1[2] = {996, 1345}, DL1[2] = {6598, 6249}, AP2[2] = {2667, 3936}, DL2[2] = {5512, 4687};
#define EXCURSION 24.0f                  /* the first tank allpass's swing, samples */

void mbfx_clear(MBFx *f)
{
    mb_zero(f->dl, (MBFX_DL_LEN + MBFX_PRE_LEN + MBFX_REV_LEN) * sizeof(int16_t));
    f->d_lpa = f->d_lpb = f->d_hpa = f->d_hpb = f->d_hpxa = f->d_hpxb = 0.0f;
    f->r_lp_in = f->r_damp_st[0] = f->r_damp_st[1] = f->r_tank_fb[0] = f->r_tank_fb[1] = 0.0f;
}

void mbfx_init(MBFx *f, float sr, int16_t *mem)
{
    int16_t *m = mem;
    uint32_t i;
    mb_zero(f, sizeof *f);
    f->sr = sr;
    f->dl = m;
    m += MBFX_DL_LEN;
    m = line_at(&f->pre, m, MBFX_PRE_LEN);
    for (i = 0; i < 4u; i++)
        m = line_at(&f->in_ap[i], m, IN_AP[i] + 1u);
    for (i = 0; i < 2u; i++) {
        m = line_at(&f->ap1[i], m, AP1[i] + (uint32_t)EXCURSION + 2u);
        m = line_at(&f->dl1[i], m, DL1[i] + 1u);
        m = line_at(&f->ap2[i], m, AP2[i] + 1u);
        m = line_at(&f->dl2[i], m, DL2[i] + 1u);
    }
    f->r_scale = 1.0f;
    f->r_lc = 1.0f;
    f->d_cur = 0.3f * sr;
    mbfx_clear(f);
}

void mbfx_set(MBFx *f, const MBFxParams *p) { f->p = *p; }

float mbfx_dry_gain(const MBFxParams *p) { return (1.0f - p->d_mix) * (1.0f - p->r_mix); }

/* the size: the tanks' lines read shorter (0.1 .. 1.5 -> 0.35 .. 1 of their length) */
static float size_scale(float s)
{
    float k = 0.35f + 0.65f * (s - 0.1f) * (1.0f / 1.4f);
    return k < 0.35f ? 0.35f : k > 1.0f ? 1.0f : k;
}
static inline uint32_t sc(uint32_t n, float k)
{
    uint32_t r = (uint32_t)((float)n * k);
    return r < 1u ? 1u : r;
}

void mbfx_process(MBFx *f, const float *in, float *outL, float *outR, int n)
{
    const MBFxParams *p = &f->p;
    int d_on = p->d_mix > 0.001f, r_on = p->r_mix > 0.001f, i;
    float dm = d_on ? p->d_mix : 0.0f, rm = r_on ? p->r_mix : 0.0f;
    /* the delay */
    uint32_t pp = p->d_pp != 0u, half = MBFX_DL_LEN / 2u, mask = pp ? half - 1u : MBFX_DL_LEN - 1u;
    float maxd = (float)(pp ? half : MBFX_DL_LEN) - 4.0f;
    float target = p->d_time_s * f->sr, fb = p->d_fb > 0.995f ? 0.995f : p->d_fb < 0.0f ? 0.0f : p->d_fb;
    float lpc = 1.0f - mb_expf(-6.2831853f * (p->d_lp_hz < 20.0f ? 20.0f : p->d_lp_hz) / f->sr);
    const float hpc = 6.2831853f * 30.0f / 44100.0f;
    /* the reverb */
    float k = size_scale(p->r_size), dec = p->r_decay < 0.0f ? 0.0f : p->r_decay > 0.95f ? 0.95f : p->r_decay;
    float damp = 0.9f * (p->r_damp < 0.0f ? 0.0f : p->r_damp > 1.0f ? 1.0f : p->r_damp);
    uint32_t pre = (uint32_t)(p->r_pre_s * f->sr), j;
    const float lr_c = 0.99999998f, lr_s = 6.2831853f * 1.0f / 44100.0f;   /* (1 Hz a step: cos, sin) */
    uint32_t a1[2], d1[2], a2[2], d2[2];
    if (pre < 1u)
        pre = 1u;
    if (pre > MBFX_PRE_LEN - 1u)
        pre = MBFX_PRE_LEN - 1u;
    if (target < 1.0f)
        target = 1.0f;
    if (target > maxd)
        target = maxd;
    if (d_on && (!f->d_on || pp != f->d_pp_was)) {   /* switched on (or its mode): from silence */
        mb_zero(f->dl, MBFX_DL_LEN * sizeof(int16_t));
        f->d_lpa = f->d_lpb = f->d_hpa = f->d_hpb = f->d_hpxa = f->d_hpxb = 0.0f;
        f->d_cur = target;
    }
    if (r_on && !f->r_on) {
        mb_zero(f->pre.buf, (MBFX_PRE_LEN + MBFX_REV_LEN) * sizeof(int16_t));
        f->r_lp_in = f->r_damp_st[0] = f->r_damp_st[1] = f->r_tank_fb[0] = f->r_tank_fb[1] = 0.0f;
        f->r_scale = k;
    }
    f->d_on = (uint32_t)d_on;
    f->r_on = (uint32_t)r_on;
    f->d_pp_was = pp;
    if (!d_on && !r_on)
        return;
    f->r_scale += 0.1f * (k - f->r_scale);   /* (a size turned: eased, block by block) */
    k = f->r_scale;
    {   /* the tank's LFO (1 Hz, a sine and a cosine by rotation), its length kept at 1 each block */
        float g = 1.5f - 0.5f * (f->r_lc * f->r_lc + f->r_ls * f->r_ls);
        f->r_lc *= g;
        f->r_ls *= g;
    }
    for (j = 0; j < 2u; j++) {
        a1[j] = sc(AP1[j], k);
        d1[j] = sc(DL1[j], k);
        a2[j] = sc(AP2[j], k);
        d2[j] = sc(DL2[j], k);
    }
    for (i = 0; i < n; i++) {
        float x = in[i], yl = 0.0f, yr = 0.0f, dl_, dr_;
        if (d_on) {
            float d = f->d_cur += 0.0005f * (target - f->d_cur);   /* (a time turned: glides, as tape) */
            uint32_t di = (uint32_t)d, w = f->dw;
            float fr = d - (float)di;
            if (!pp) {
                uint32_t r0 = (w - di) & mask, r1 = (w - di - 1u) & mask;
                float a = from16(f->dl[r0]) + fr * (from16(f->dl[r1]) - from16(f->dl[r0]));
                float h;
                f->d_lpa += lpc * (a - f->d_lpa);
                f->d_hpa += hpc * (f->d_lpa - f->d_hpa);
                h = f->d_lpa - f->d_hpa;
                f->dl[w & mask] = to16(x + fb * h);
                yl = yr = f->d_lpa;
            } else {                          /* two lines, each feeding the other: L, R, L, ... */
                int16_t *A = f->dl, *B = f->dl + half;
                uint32_t r0 = (w - di) & mask, r1 = (w - di - 1u) & mask;
                float a = from16(A[r0]) + fr * (from16(A[r1]) - from16(A[r0]));
                float b = from16(B[r0]) + fr * (from16(B[r1]) - from16(B[r0]));
                float ha, hb;
                f->d_lpa += lpc * (a - f->d_lpa);
                f->d_lpb += lpc * (b - f->d_lpb);
                f->d_hpa += hpc * (f->d_lpa - f->d_hpa);
                f->d_hpb += hpc * (f->d_lpb - f->d_hpb);
                ha = f->d_lpa - f->d_hpa;
                hb = f->d_lpb - f->d_hpb;
                A[w & mask] = to16(x + fb * hb);
                B[w & mask] = to16(fb * ha);
                yl = f->d_lpa;
                yr = f->d_lpb;
            }
            f->dw = (w + 1u) & mask;
        }
        dl_ = (1.0f - dm) * x + dm * yl;
        dr_ = (1.0f - dm) * x + dm * yr;
        outL[i] += (1.0f - rm) * dm * yl;
        outR[i] += (1.0f - rm) * dm * yr;
        if (r_on) {
            float v, lo, ro, m0, m1;
            line_put(&f->pre, 0.5f * (dl_ + dr_));
            v = line_tap(&f->pre, pre);
            f->r_lp_in += 0.6f * (v - f->r_lp_in);   /* the input's bandwidth (about 9 kHz) */
            v = allpass(&f->in_ap[0], f->r_lp_in, 0.75f, IN_AP[0]);
            v = allpass(&f->in_ap[1], v, 0.75f, IN_AP[1]);
            v = allpass(&f->in_ap[2], v, 0.625f, IN_AP[2]);
            v = allpass(&f->in_ap[3], v, 0.625f, IN_AP[3]);
            {
                float c = f->r_lc * lr_c - f->r_ls * lr_s;
                f->r_ls = f->r_ls * lr_c + f->r_lc * lr_s;
                f->r_lc = c;
            }
            m0 = EXCURSION * 0.5f * (1.0f + f->r_ls);
            m1 = EXCURSION * 0.5f * (1.0f + f->r_lc);
            for (j = 0; j < 2u; j++) {           /* the tanks: each fed the other's end */
                float t = v + dec * f->r_tank_fb[j ^ 1u];
                t = allpass_f(&f->ap1[j], t, -0.7f, (float)a1[j] + (j ? m1 : m0));
                line_put(&f->dl1[j], t);
                t = line_tap(&f->dl1[j], d1[j]);
                f->r_damp_st[j] += (1.0f - damp) * (t - f->r_damp_st[j]);
                t = allpass(&f->ap2[j], f->r_damp_st[j] * dec, 0.5f, a2[j]);
                line_put(&f->dl2[j], t);
                f->r_tank_fb[j] = line_tap(&f->dl2[j], d2[j]);
            }
            /* the outputs: Dattorro's taps (scaled to 44.1 kHz and the size) */
            lo = line_tap(&f->dl1[1], sc(394, k)) + line_tap(&f->dl1[1], sc(4407, k)) -
                 line_tap(&f->ap2[1], sc(2835, k)) + line_tap(&f->dl2[1], sc(2958, k)) -
                 line_tap(&f->dl1[0], sc(2949, k)) - line_tap(&f->ap2[0], sc(277, k)) -
                 line_tap(&f->dl2[0], sc(1580, k));
            ro = line_tap(&f->dl1[0], sc(523, k)) + line_tap(&f->dl1[0], sc(5375, k)) -
                 line_tap(&f->ap2[0], sc(1820, k)) + line_tap(&f->dl2[0], sc(3961, k)) -
                 line_tap(&f->dl1[1], sc(3128, k)) - line_tap(&f->ap2[1], sc(496, k)) -
                 line_tap(&f->dl2[1], sc(179, k));
            outL[i] += rm * 0.6f * lo;
            outR[i] += rm * 0.6f * ro;
        }
    }
}
