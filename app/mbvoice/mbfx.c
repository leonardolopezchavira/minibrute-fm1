/* SPDX-License-Identifier: MIT
 * MB-1's effects: a delay, then a plate reverb (mbfx.h). */
#include "mbfx.h"
#include "mb_port.h"

/* fitted to the MiniBrute V's FX (plan7 renders): the reverb's decay time against DECAY (s, at SIZE 0.5, DAMP 0.5),
 * and its scaling by SIZE and DAMP; its input's bandwidth, level, stereo spread; the delay's right channel in
 * STEREO (a little later than the left), ping-pong's spill to the other side */
#ifndef RV_INLP
#define RV_INLP 0.18f                     /* the input low-pass (one pole, per sample) */
#endif
#ifndef RV_GAIN
#define RV_GAIN 0.35f
#endif
#ifndef RV_XC
#define RV_XC 0.32f                       /* each side takes this much of the other (the plugin's L/R correlation 0.57) */
#endif
#ifndef RV_TONE0
#define RV_TONE0 0.25f                    /* the tanks' damping low-pass: RV_TONE0 + RV_TONE1 x DAMP */
#endif
#ifndef RV_TONE1
#define RV_TONE1 0.35f
#endif
#ifndef DL_RSTRETCH
#define DL_RSTRETCH 1.04f
#endif
#ifndef DL_LEAK
#define DL_LEAK 0.4f
#endif
static const float RT_DECAY[5] = {0.69f, 1.58f, 3.14f, 8.2f, 13.7f};   /* DECAY 0, .25, .5, .75, 1 (the plugin's 0.75, 1.49,
                                                                          * 2.73, 5.82, 8.61 s, after our tanks' own losses) */
static const float RV_LVL[5] = {2.45f, 1.35f, 1.0f, 0.75f, 1.12f};     /* level at DECAY 0, .25, .5, .75, 1 (x) */
static const float RT_SIZE[3] = {0.12f, 1.0f, 1.84f};                   /* SIZE 0, .5, 1 (x) */
static const float RT_DAMP[3] = {1.03f, 1.0f, 0.09f};                   /* DAMP 0, .5, 1 (x) */
static float tab_log(const float *t, int n, float x)   /* geometric interpolation, x 0..1 over n points */
{
    float f = (x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x) * (float)(n - 1);
    int i = (int)f;
    if (i >= n - 1)
        return t[n - 1];
    return t[i] * mb_exp2f((f - (float)i) * mb_log2f(t[i + 1] / t[i]));
}

#define S16 8192.0f                       /* a line's full scale: +-4 (headroom for the tanks and the repeats) */
#define S16_INV (1.0f / 8192.0f)

static inline int16_t to16(float v)       /* (a NaN, or a value a misread made huge: silence, or the rail) */
{
    if (!(v > -3.999f))
        v = v < 0.0f ? -3.999f : 0.0f;
    else if (!(v < 3.999f))
        v = 3.999f;
    return (int16_t)(v * S16);            /* (toward zero: no tail is kept alive by rounding) */
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
    f->d_lpa = f->d_lpb = f->d_lpa2 = f->d_lpb2 = f->d_hpa = f->d_hpb = f->d_hpxa = f->d_hpxb = 0.0f;
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

float mbfx_dry_gain(const MBFxParams *p)
{
    float d = p->d_mix > 0.001f ? mb_cosf(1.5707963f * p->d_mix) : 1.0f, r = p->r_mix > 0.001f ? mb_cosf(1.5707963f * p->r_mix) : 1.0f;
    return d * r;
}

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
    /* MIX: an equal-power crossfade, as the plugin's (half way: both at -3 dB) */
    float dm = d_on ? mb_sinf(1.5707963f * p->d_mix) : 0.0f, dd = d_on ? mb_cosf(1.5707963f * p->d_mix) : 1.0f;
    float rm = r_on ? mb_sinf(1.5707963f * p->r_mix) : 0.0f, rd = r_on ? mb_cosf(1.5707963f * p->r_mix) : 1.0f;
    /* the delay */
    uint32_t pp = p->d_pp != 0u, half = MBFX_DL_LEN / 2u, mask = pp ? half - 1u : MBFX_DL_LEN - 1u;
    float maxd = (float)(pp ? half : MBFX_DL_LEN) - 4.0f;
    float target = p->d_time_s * f->sr, fb = p->d_fb > 0.995f ? 0.995f : p->d_fb < 0.0f ? 0.0f : p->d_fb;
    float lpf = (p->d_lp_hz < 20.0f ? 20.0f : p->d_lp_hz) * 1.554f;   /* (two poles: -3 dB at TONE) */
    float lpc = lpf > 0.45f * f->sr ? 1.0f : 1.0f - mb_expf(-6.2831853f * lpf / f->sr);
    const float hpc = 6.2831853f * 30.0f / f->sr;
    /* the reverb */
    float k = size_scale(p->r_size), dec;
    float damp_raw = p->r_damp < 0.0f ? 0.0f : p->r_damp > 1.0f ? 1.0f : p->r_damp, damp = RV_TONE0 + RV_TONE1 * damp_raw;
    uint32_t pre = (uint32_t)(p->r_pre_s * f->sr), j;
    const float lr_c = 0.99999998f, lr_s = 6.2831853f * 1.0f / f->sr;   /* (1 Hz a step: cos, sin) */
    uint32_t a1[2], d1[2], a2[2], d2[2];
    float r_alive = 0.0f, r_gain = RV_GAIN;
    int rev_first = p->order == 1u;
    if (pre < 1u)
        pre = 1u;
    if (pre > MBFX_PRE_LEN - 1u)
        pre = MBFX_PRE_LEN - 1u;
    if (target < 1.0f)
        target = 1.0f;
    if (target > maxd)
        target = maxd;
    /* No memory is cleared here (200 KB at once would overrun a block): the delay, switched on (or its mode), reads
     * silence until it has written as far back as it reads (d_fill); the reverb, switched off, runs on unheard with
     * no input until its tail has died away (r_run), so that switched on again it starts from silence. */
    if (d_on && (!f->d_on || pp != f->d_pp_was)) {
        f->d_fill = 0;
        f->d_lpa = f->d_lpb = f->d_lpa2 = f->d_lpb2 = f->d_hpa = f->d_hpb = f->d_hpxa = f->d_hpxb = 0.0f;
        f->d_cur = target;
    }
    if (r_on && !f->r_run)
        f->r_scale = k;
    f->d_on = (uint32_t)d_on;
    f->r_on = (uint32_t)r_on;
    f->d_pp_was = pp;
    f->alive = 0.0f;
    if (!d_on && !r_on && !f->r_run)
        return;
    f->r_scale += 0.1f * (k - f->r_scale);   /* (a size turned: eased, block by block) */
    k = f->r_scale;
    {   /* the tank's LFO (1 Hz, a sine and a cosine by rotation), its length kept at 1 each block */
        float g = 1.5f - 0.5f * (f->r_lc * f->r_lc + f->r_ls * f->r_ls);
        f->r_lc *= g;
        f->r_ls *= g;
    }
    {   /* the decay: the time the plugin takes at these settings, from the loop's length (dec twice a half loop) */
        float rt = tab_log(RT_DECAY, 5, p->r_decay * (1.0f / 0.925f)) * tab_log(RT_SIZE, 3, (p->r_size - 0.1f) * (1.0f / 1.4f)) *
                   tab_log(RT_DAMP, 3, damp_raw);
        float lh = 0.5f * (float)(AP1[0] + DL1[0] + AP2[0] + DL2[0] + AP1[1] + DL1[1] + AP2[1] + DL2[1]) * k / f->sr;
        float gh = mb_exp2f(-3.0f * 3.3219281f * lh / (rt > 0.05f ? rt : 0.05f));   /* 10^(-3 lh / rt) */
        dec = mb_sqrtf(gh);
        {
            float x = (p->r_decay * (1.0f / 0.925f)) * 4.0f;
            int ix = x < 0.0f ? 0 : x >= 4.0f ? 3 : (int)x;
            float fr = x - (float)ix;
            if (fr < 0.0f) fr = 0.0f;
            if (fr > 1.0f) fr = 1.0f;
            r_gain = RV_GAIN * (RV_LVL[ix] + (RV_LVL[ix + 1] - RV_LVL[ix]) * fr);
        }
        if (dec > 0.995f)
            dec = 0.995f;
    }
    for (j = 0; j < 2u; j++) {
        a1[j] = sc(AP1[j], k);
        d1[j] = sc(DL1[j], k);
        a2[j] = sc(AP2[j], k);
        d2[j] = sc(DL2[j], k);
    }
    for (i = 0; i < n; i++) {
        /* ORDER DLY>REV: the delay, then the reverb on its output; REV>DLY: the reverb first, the delay on its output.
         * What goes out is the wet part only (the dry, dd x rd x the input, the caller's) */
        float x = in[i], yl = 0.0f, yr = 0.0f, rl = 0.0f, rr = 0.0f, ax, xd = x, xr = x;
        if (rev_first) {
            if (r_on || f->r_run) {
                float v, lo, ro, m0, m1;
                line_put(&f->pre, r_on ? xr : 0.0f);
                v = line_tap(&f->pre, pre);
                f->r_lp_in += RV_INLP * (v - f->r_lp_in);   /* the input's bandwidth */
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
                rl = r_gain * (lo + RV_XC * ro);
                rr = r_gain * (ro + RV_XC * lo);
                ax = mb_fabsf(v) + mb_fabsf(f->r_tank_fb[0]) + mb_fabsf(f->r_tank_fb[1]);
                if (ax > r_alive)
                    r_alive = ax;
            }
            xd = rd * x + rm * 0.5f * (rl + rr);
            if (d_on) {
                float d = f->d_cur += 0.0005f * (target - f->d_cur);   /* (a time turned: glides, as tape) */
                uint32_t di = (uint32_t)d, w = f->dw;
                float fr = d - (float)di;
                int fresh = di + 1u < f->d_fill;   /* (not yet written that far back: silence) */
                if (!pp) {                        /* the right a little later than the left (the plugin's STEREO) */
                    uint32_t r0 = (w - di) & mask, r1 = (w - di - 1u) & mask;
                    float a = fresh ? from16(f->dl[r0]) + fr * (from16(f->dl[r1]) - from16(f->dl[r0])) : 0.0f;
                    float dr = d * DL_RSTRETCH;
                    uint32_t dri = (uint32_t)dr, q0 = (w - dri) & mask, q1 = (w - dri - 1u) & mask;
                    float frr = dr - (float)dri;
                    float b = dri + 1u < f->d_fill ? from16(f->dl[q0]) + frr * (from16(f->dl[q1]) - from16(f->dl[q0])) : 0.0f;
                    float h;
                    f->d_lpa += lpc * (a - f->d_lpa);       /* (two poles: 12 dB an octave, as the plugin's) */
                    f->d_lpa2 += lpc * (f->d_lpa - f->d_lpa2);
                    f->d_lpb += lpc * (b - f->d_lpb);
                    f->d_lpb2 += lpc * (f->d_lpb - f->d_lpb2);
                    f->d_hpa += hpc * (f->d_lpa2 - f->d_hpa);
                    h = f->d_lpa2 - f->d_hpa;
                    f->dl[w & mask] = to16(xd + fb * h);
                    yl = f->d_lpa2;
                    yr = f->d_lpb2;
                } else {                          /* two lines, each feeding the other: L, R, L, ... */
                    int16_t *A = f->dl, *B = f->dl + half;
                    uint32_t r0 = (w - di) & mask, r1 = (w - di - 1u) & mask;
                    float a = fresh ? from16(A[r0]) + fr * (from16(A[r1]) - from16(A[r0])) : 0.0f;
                    float b = fresh ? from16(B[r0]) + fr * (from16(B[r1]) - from16(B[r0])) : 0.0f;
                    float ha, hb;
                    f->d_lpa += lpc * (a - f->d_lpa);
                    f->d_lpa2 += lpc * (f->d_lpa - f->d_lpa2);
                    f->d_lpb += lpc * (b - f->d_lpb);
                    f->d_lpb2 += lpc * (f->d_lpb - f->d_lpb2);
                    f->d_hpa += hpc * (f->d_lpa2 - f->d_hpa);
                    f->d_hpb += hpc * (f->d_lpb2 - f->d_hpb);
                    ha = f->d_lpa2 - f->d_hpa;
                    hb = f->d_lpb2 - f->d_hpb;
                    A[w & mask] = to16(xd + fb * hb);   /* (FEEDBACK once a round: each side's repeats fall by it) */
                    B[w & mask] = to16(ha);
                    yl = f->d_lpa2 + DL_LEAK * f->d_lpb2;
                    yr = f->d_lpb2 + DL_LEAK * f->d_lpa2;
                }
                f->dw = (w + 1u) & mask;
                if (f->d_fill <= mask)
                    f->d_fill++;
                ax = mb_fabsf(yl) + mb_fabsf(yr);
                if (ax > f->alive)
                    f->alive = ax;
            }
            outL[i] += dd * rm * rl + dm * yl;
            outR[i] += dd * rm * rr + dm * yr;
        } else {
            if (d_on) {
                float d = f->d_cur += 0.0005f * (target - f->d_cur);   /* (a time turned: glides, as tape) */
                uint32_t di = (uint32_t)d, w = f->dw;
                float fr = d - (float)di;
                int fresh = di + 1u < f->d_fill;   /* (not yet written that far back: silence) */
                if (!pp) {                        /* the right a little later than the left (the plugin's STEREO) */
                    uint32_t r0 = (w - di) & mask, r1 = (w - di - 1u) & mask;
                    float a = fresh ? from16(f->dl[r0]) + fr * (from16(f->dl[r1]) - from16(f->dl[r0])) : 0.0f;
                    float dr = d * DL_RSTRETCH;
                    uint32_t dri = (uint32_t)dr, q0 = (w - dri) & mask, q1 = (w - dri - 1u) & mask;
                    float frr = dr - (float)dri;
                    float b = dri + 1u < f->d_fill ? from16(f->dl[q0]) + frr * (from16(f->dl[q1]) - from16(f->dl[q0])) : 0.0f;
                    float h;
                    f->d_lpa += lpc * (a - f->d_lpa);       /* (two poles: 12 dB an octave, as the plugin's) */
                    f->d_lpa2 += lpc * (f->d_lpa - f->d_lpa2);
                    f->d_lpb += lpc * (b - f->d_lpb);
                    f->d_lpb2 += lpc * (f->d_lpb - f->d_lpb2);
                    f->d_hpa += hpc * (f->d_lpa2 - f->d_hpa);
                    h = f->d_lpa2 - f->d_hpa;
                    f->dl[w & mask] = to16(xd + fb * h);
                    yl = f->d_lpa2;
                    yr = f->d_lpb2;
                } else {                          /* two lines, each feeding the other: L, R, L, ... */
                    int16_t *A = f->dl, *B = f->dl + half;
                    uint32_t r0 = (w - di) & mask, r1 = (w - di - 1u) & mask;
                    float a = fresh ? from16(A[r0]) + fr * (from16(A[r1]) - from16(A[r0])) : 0.0f;
                    float b = fresh ? from16(B[r0]) + fr * (from16(B[r1]) - from16(B[r0])) : 0.0f;
                    float ha, hb;
                    f->d_lpa += lpc * (a - f->d_lpa);
                    f->d_lpa2 += lpc * (f->d_lpa - f->d_lpa2);
                    f->d_lpb += lpc * (b - f->d_lpb);
                    f->d_lpb2 += lpc * (f->d_lpb - f->d_lpb2);
                    f->d_hpa += hpc * (f->d_lpa2 - f->d_hpa);
                    f->d_hpb += hpc * (f->d_lpb2 - f->d_hpb);
                    ha = f->d_lpa2 - f->d_hpa;
                    hb = f->d_lpb2 - f->d_hpb;
                    A[w & mask] = to16(xd + fb * hb);   /* (FEEDBACK once a round: each side's repeats fall by it) */
                    B[w & mask] = to16(ha);
                    yl = f->d_lpa2 + DL_LEAK * f->d_lpb2;
                    yr = f->d_lpb2 + DL_LEAK * f->d_lpa2;
                }
                f->dw = (w + 1u) & mask;
                if (f->d_fill <= mask)
                    f->d_fill++;
                ax = mb_fabsf(yl) + mb_fabsf(yr);
                if (ax > f->alive)
                    f->alive = ax;
            }
            xr = dd * x + dm * 0.5f * (yl + yr);
            if (r_on || f->r_run) {
                float v, lo, ro, m0, m1;
                line_put(&f->pre, r_on ? xr : 0.0f);
                v = line_tap(&f->pre, pre);
                f->r_lp_in += RV_INLP * (v - f->r_lp_in);   /* the input's bandwidth */
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
                rl = r_gain * (lo + RV_XC * ro);
                rr = r_gain * (ro + RV_XC * lo);
                ax = mb_fabsf(v) + mb_fabsf(f->r_tank_fb[0]) + mb_fabsf(f->r_tank_fb[1]);
                if (ax > r_alive)
                    r_alive = ax;
            }
            outL[i] += rd * dm * yl + rm * rl;
            outR[i] += rd * dm * yr + rm * rr;
        }
    }
    if (r_on)
        f->r_run = 1;
    if (r_alive > f->alive)
        f->alive = r_alive;
    if (!r_on && f->r_run && r_alive < 1e-5f)  /* (off, and its tail gone: it stops) */
        f->r_run = 0;
}
