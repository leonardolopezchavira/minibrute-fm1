/* SPDX-License-Identifier: MIT */
/* mbvoice.c — see mbvoice.h. Every constant below is traced to a measurement in the
 * "FM-1 Minibrute Firmware — DSP Measurement Spec" doc (MiniBrute V plugin renders). */
#include "mbvoice.h"
#include "mb_port.h"

#define PI 3.14159265358979f
#define OS 4                          /* filter oversampling */
#define DEC_TAPS 64                   /* decimation FIR length (at the oversampled rate) */

#include "mbcal.h"
#ifdef MB_MEASURED_METAL
#include "metal_tables.h"            /* measured from the plugin: private builds only, not in this repository */
#endif
/* ---- measured calibration (see mbcal.h; values fitted by engine/calibrate.py) ---- */
MBCal mb_cal = {
    .out_ref = 0.462925f,
    .g_square = 1.04002f,
    .g_tri = 1.35881f,
    .g_sub_sq = 1.11569f,
    .g_sub_sine = 2.36297f,
    .g_noise = 2.20169f,
    .mix_exp = 1.7f,
    .duty50 = 0.481f,
    .duty_slope = 0.0095f,
    .tri_skew = 0.0133333f,
    .tri_round = 29.356f,
    .us_mix = -0.7f,
    .us_exp = 1.0f,
    .us_beta = 0.5f,
    .us_rate_mult = 1.045f,
    .us_main = 0.4f,
    .us_phase0 = 0.85f,
    .metal_gain = 12.0f,
    .metal_lower_start = 0.74f,
    .kt_ref = 24.0f,
    .fenv_oct = 18.8f,
    .wheel_cut_oct = 9.5f,
    .lfo_filter_oct = 9.2f,
    .at_cut_oct = 9.2f,
    .res_q = { 0.231771f, 0.395441f, 0.564915f, 0.893563f, 1.30041f, 2.11513f, 3.34325f, 8.94427f, 75.2121f, 21.0f, 40.0f },
    .res_fr = { 1.00556f, 0.877778f, 0.877777f, 0.861111f, 0.9f, 0.89f, 0.933333f, 1.0f, 1.0f, 1.225f, 1.67777f },
    .f_ceiling = 7745.97f,
    .svf_sat = 25.0f,
    .in_drive = 0.632456f,
    .mode_gain = { 1.0f, 1.96436f, 1.0473f, 0.58569f },
    .brute_k = 2.27755f,
    .vib_st = 5.4f,
    .vib_sq_st = 3.0f,
    .at_vib_st = 5.3f,
    .lfo_pitch_st = 26.0f,
    .lfo_pitch_shape = { 0.55f, 0.79f, 0.92f, 1.0f, 1.0f, 1.0f },
    .lfo_amp_shape = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    .lfo_saw_rate_mult = 2.0f,
    .pw_lfo = 0.61f,
    .pw_env = 0.27f,
    .metal_lfo = 0.5f,
    .vintage_cents = { 0.0f, 7.0f, -5.2f, 10.4f },
    .noise_inject = 0.001f,
    .selfosc_k = -0.35f,
    .ring_lo = 110.0f,
    .ring_hi = 10000.0f,
    .sat_asym = 0.0f,
    .ring_fcorr = { 1.0f, 1.16772f, 1.16772f, 0.978313f, 0.959697f, 0.9649f, 0.967713f, 0.979264f, 0.973749f, 0.961427f, 0.949502f, 0.934695f, 0.951036f, 0.976493f, 1.03807f, 1.01737f, 1.01737f },
    .ring_gain = { 0.25f, 0.365642f, 0.365642f, 0.536543f, 0.539439f, 0.502302f, 0.464023f, 0.40971f, 0.366451f, 0.326502f, 0.290898f, 0.262719f, 0.230453f, 0.206533f, 0.18165f, 0.158306f, 0.158306f },
    .ring_h2 = 0.0405556f,
    .ring_h3 = 0.132778f,
    .ring_h5 = 0.0311111f,
    .sub_sine_fc = 184.274f,
    .shaper_res_start = 0.8f,
    .knob_corr = { 3.0f, 1.3f, 0.948159f, 1.22474f, 1.58201f },
    .wheel_cut_curve = 0.095f,
    .lim_level = 1.0f,
    .lim_start = 0.6f,
    .lim_makeup = 2.2f,
    .lim_makeup_exp = 5.0f,
    .bass_k = 0.25f,
    .bass_exp = 1.0f,
    .agc_k = 3.0f,
    .agc_level = 2.4f,
    .agc_start = 0.6f,
    .vca_drive = 1.0f,
    .fb_hp_hz = 1.0f,
    .in_hp_hz = 0.0f,
    .out_hp_hz = 1.0f,
};
#define C mb_cal
/* ---- helpers ----------------------------------------------------------------------- */
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float lerp_tab(const float *t, float x01) {
    float x = clampf(x01, 0.f, 1.f) * 10.f; int i = (int)x; if (i >= 10) return t[10];
    float f = x - i; return t[i] + (t[i + 1] - t[i]) * f;
}
static inline float fast_tanh(float x) {
    x = clampf(x, -3.f, 3.f); float x2 = x * x; return x * (27.f + x2) / (27.f + 9.f * x2);
}
static inline float poly_blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
static inline float frac(float x) { return x - mb_floorf(x); }
static inline float saw_down(float ph, float dt) { return (1.f - 2.f * ph) + poly_blep(ph, dt); }
static inline float tri_skewed(float ph, float skew) {      /* rises for (0.5+skew) of the cycle */
    float r = 0.5f + skew;
    return ph < r ? -1.f + 2.f * ph / r : 1.f - 2.f * (ph - r) / (1.f - r);
}
static inline uint32_t xorshift(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
static inline float noise(uint32_t *s) { return (float)(int32_t)xorshift(s) * (1.f / 2147483648.f); }

#define RING_N 17
static float ring_tab(const float *t, float knob) {      /* log-spaced 110 Hz .. 10 kHz */
    const float lo = 6.7814f, hi = 13.2877f;
    float x = (mb_log2f(mb_fmaxf(knob, 1.f)) - lo) / (hi - lo) * (RING_N - 1);
    if (x <= 0.f) return t[0];
    if (x >= RING_N - 1) return t[RING_N - 1];
    int i = (int)x; float f = x - i; return t[i] + (t[i + 1] - t[i]) * f;
}
static const float KC_LOG2[5] = { 6.6439f, 7.6439f, 9.9658f, 12.2877f, 14.2877f };
static float knob_corr(float knob) {
    float x = mb_log2f(mb_fmaxf(knob, 1.f)); int i;
    if (x <= KC_LOG2[0]) return C.knob_corr[0];
    for (i = 0; i < 4; i++) if (x < KC_LOG2[i + 1]) {
        float f = (x - KC_LOG2[i]) / (KC_LOG2[i + 1] - KC_LOG2[i]); return C.knob_corr[i] + (C.knob_corr[i + 1] - C.knob_corr[i]) * f; }
    return C.knob_corr[4];
}
/* compression of the filter's frequency near its 6.8 kHz ceiling (renders F1) */
static float filter_freq(float knob_eff, float res) {
    float f = knob_eff * lerp_tab(C.res_fr, res);
    float c = C.f_ceiling, x = f / c;
    float sat = f / mb_sqrtf(1.f + x * x);
    float s = res * res * res * res;               /* compression only matters when resonant */
    return f * (1.f - s) + sat * s;
}

/* Metalizer, the parametric model (this repository's): a one-sided sine folder on the triangle, the lower half
 * joining in above metal_lower_start. A private build may define MB_MEASURED_METAL for the measured cycles instead. */
static float metalize(float x, float m) {
    if (m <= 0.f) return x;
    float c = clampf(m * 8.f, 0.f, 1.f);
    if (x > 0.f) {
        float g = 1.f + C.metal_gain * m;
        return x + c * (mb_sinf(0.5f * PI * g * x) - x);
    } else if (m > C.metal_lower_start) {
        float m2 = (m - C.metal_lower_start) / (1.f - C.metal_lower_start), g = 1.f + C.metal_gain * m2;
        float c2 = clampf(m2 * 4.f, 0.f, 1.f);
        return x + c2 * (mb_sinf(0.5f * PI * g * x) - x);
    }
    return x;
}

#ifdef MB_MEASURED_METAL
/* Metalizer: measured single cycles, interpolated across the 9 settings, band-limited by pitch */
static float metal_wave(float ph, float m, float f0, float sr) {
    float hmax = 0.45f * sr / mb_fmaxf(f0, 1.f); int lv = 0;     /* highest harmonic we may keep */
    while (lv < MT_LEVELS - 1 && (240 >> lv) > hmax) lv++;
    float pos = ph * MT_N; int i0 = (int)pos; float fr = pos - i0; int i1 = (i0 + 1) & (MT_N - 1); i0 &= MT_N - 1;
    float ms = clampf(m, 0.f, 1.f) * (MT_STEPS - 1); int s0 = (int)ms; if (s0 > MT_STEPS - 2) s0 = MT_STEPS - 2; float sf = ms - s0;
    const float *a = MT_TAB[lv][s0], *b = MT_TAB[lv][s0 + 1];
    float va = a[i0] + (a[i1] - a[i0]) * fr, vb = b[i0] + (b[i1] - b[i0]) * fr;
    return va + (vb - va) * sf;
}

#else
static float metal_wave(float ph, float m, float f0, float sr) {
    /* the folds' harmonics alias at 44.1 kHz on high notes: the folding fades out from 2 kHz to 6 kHz */
    float fade = clampf((6000.f - f0) * (1.f / 4000.f), 0.f, 1.f), tri;
    (void)sr;
    tri = tri_skewed(ph, C.tri_skew);
    return metalize(tri, m * fade);
}
#endif

/* ---- API --------------------------------------------------------------------------- */
void mb_default_params(MBParams *p) {
    mb_zero(p, sizeof *p);
    p->saw = 1.f; p->pw_pct = 50.f; p->ultrasaw_rate_hz = 1.f;
    p->filter_mode = MB_FILT_LP; p->cutoff_hz = 20000.f;
    p->f_a = 0.001f; p->f_d = 0.5f; p->f_s = 0.f; p->f_r = 0.001f;
    p->a_a = 0.001f; p->a_d = 0.001f; p->a_s = 1.f; p->a_r = 0.001f;
    p->lfo_rate_hz = 2.f; p->vib_type = MB_VIB_SINE; p->vib_rate_hz = 5.f;
    p->wheel_dest = MB_WHEEL_VIBRATO; p->wheel_amt = 0.481f;
    p->master_db = -12.f;
}

/* small held-note stack for last-note priority */
#define STACK 8
static int g_stack[STACK]; static int g_depth = 0;

static float g_dec[DEC_TAPS]; static int g_dec_ready = 0;
static void dec_design(void) {          /* windowed-sinc low-pass, cutoff 0.1 x oversampled rate (about 19 kHz) */
    int i; float sum = 0.f;
    for (i = 0; i < DEC_TAPS; i++) {
        float n = i - (DEC_TAPS - 1) * 0.5f, x = 2.f * PI * 0.1f * n;
        float sinc = mb_fabsf(n) < 1e-6f ? 2.f * 0.1f : mb_sinf(x) / (PI * n);
        float w = 0.42f - 0.5f * mb_cosf(2.f * PI * i / (DEC_TAPS - 1)) + 0.08f * mb_cosf(4.f * PI * i / (DEC_TAPS - 1));
        g_dec[i] = sinc * w; sum += g_dec[i];
    }
    for (i = 0; i < DEC_TAPS; i++) g_dec[i] /= sum;
    g_dec_ready = 1;
}

void mb_init(MBVoice *v, float sr) {
    g_depth = 0;
    if (!g_dec_ready) dec_design();
    MBParams keep;
    mb_copy(&keep, &v->p, sizeof keep);
    mb_zero(v, sizeof *v);
    mb_copy(&v->p, &keep, sizeof keep); v->sr = sr; v->rng = 0x12345678u; v->note = -1;
    v->pitch_st = v->target_st = 60.f;
    v->us_lfo = C.us_phase0;
}


void mb_note_on(MBVoice *v, int note, int vel) {
    int i, j;
    for (i = 0; i < g_depth; i++) if (g_stack[i] == note) { for (j = i; j < g_depth - 1; j++) g_stack[j] = g_stack[j + 1]; g_depth--; break; }
    if (g_depth < STACK) g_stack[g_depth++] = note;
    int legato = v->gate;
    v->note = note; v->vel = vel; v->gate = 1;
    float cents = C.vintage_cents[v->note_count & 3] * v->p.vintage;
    v->note_count++;
    v->target_st = (float)note + cents / 100.f;
    if (!legato || v->p.glide_s_oct <= 0.f) v->pitch_st = v->target_st;
    if (v->p.glide_s_oct > 0.f && legato == 0 && v->aenv > 0.0001f) { /* glide from last pitch */ }
    if (!legato || v->p.retrig) {           /* the envelopes restart (from where they are: no click) */
        v->ast = 1; v->fst = 1;
        if (v->aenv <= 0.f) { v->ph = 0.f; }
        if (v->p.lfo_retrig) v->lfo_ph = 0.f;   /* else free-running */
    }
}

void mb_note_off(MBVoice *v, int note) {
    int i, j;
    for (i = 0; i < g_depth; i++) if (g_stack[i] == note) { for (j = i; j < g_depth - 1; j++) g_stack[j] = g_stack[j + 1]; g_depth--; break; }
    if (g_depth > 0) { int prev = g_stack[g_depth - 1]; if (prev != v->note) { v->note = prev; v->target_st = (float)prev; if (v->p.glide_s_oct <= 0.f) v->pitch_st = v->target_st; } return; }
    v->gate = 0; v->ast = 3; v->fst = 3;
}

void mb_set_wheel(MBVoice *v, float w) { v->wheel = clampf(w, 0.f, 1.f); }
void mb_set_aftertouch(MBVoice *v, float a) { v->aftertouch = clampf(a, 0.f, 1.f); }
void mb_set_bend(MBVoice *v, float st) { v->bend_st = st; }
void mb_all_off(MBVoice *v) { g_depth = 0; v->gate = 0; if (v->ast) v->ast = 3; if (v->fst) v->fst = 3; }

/* the envelopes' rates are worked out once a block (mb_render): a = attack step, d / r = decay / release factors */
static float env_step(float e, int *st, float a, float d, float s, float r) {
    switch (*st) {
    case 1: e += a; if (e >= 1.f) { e = 1.f; *st = 2; } break;
    case 2: e = s + (e - s) * d; break;
    case 3: e *= r; if (e < 1e-6f) { e = 0.f; *st = 0; } break;
    default: break;
    }
    return e;
}
static float env_att(float t, float sr) { return 1.f / (mb_fmaxf(t, 0.0005f) * sr); }
static float env_fac(float t, float sr) { return mb_expf(-4.605f / (mb_fmaxf(t, 0.0005f) * sr)); }

static float lfo_value(MBVoice *v, float dt_rate) {
    int w = v->p.lfo_wave;
    float rate = v->p.lfo_rate_hz * (w == MB_LFO_SAW ? C.lfo_saw_rate_mult : 1.f);
    float ph = v->lfo_ph + rate * dt_rate, val;
    if (ph >= 1.f) { ph -= mb_floorf(ph); v->lfo_sh_prev = v->lfo_sh; v->lfo_sh = noise(&v->rng); }
    v->lfo_ph = ph;
    switch (w) {
    case MB_LFO_SINE:   val = -mb_cosf(2.f * PI * ph); break;          /* starts at its minimum, like the plugin */
    case MB_LFO_TRI:    val = 1.f - 4.f * mb_fabsf(ph - 0.5f); break;
    case MB_LFO_SAW:    val = 1.f - 2.f * ph; break;               /* falling ramp */
    case MB_LFO_SQUARE: val = ph < 0.5f ? 1.f : -1.f; break;
    case MB_LFO_SH:     val = v->lfo_sh; break;
    default:            val = v->lfo_sh_prev + (v->lfo_sh - v->lfo_sh_prev) * ph; break;
    }
    return val;
}

void mb_render(MBVoice *v, float *out, int n) {
    const MBParams *p = &v->p;
    const float sr = v->sr, isr = 1.f / sr;
    const float g_saw = mb_powf(p->saw, C.mix_exp), g_sq = mb_powf(p->square, C.mix_exp) * C.g_square;
    const float g_tri = mb_powf(p->triangle, C.mix_exp) * C.g_tri, g_noise = mb_powf(p->noise, C.mix_exp) * C.g_noise;
    const float g_sub = mb_powf(p->sub, C.mix_exp) * (p->sub_sine ? C.g_sub_sine : C.g_sub_sq);
    const float master = C.out_ref * mb_exp2f(p->master_db * 0.16609640f);
    const float vel = (float)v->vel / 127.f;
    const float avel = 1.f - p->amp_vel + p->amp_vel * vel, fvel = 1.f - p->fenv_vel + p->fenv_vel * vel;
    const float us_mix = C.us_mix * mb_powf(p->ultrasaw, C.us_exp), us_beta = C.us_beta;
    const float glide_rate = p->glide_s_oct > 0.f ? 12.f / p->glide_s_oct * isr : 1e9f;
    const float res = clampf(p->resonance, 0.f, 1.f);
    float k_damp = res < 0.9f ? 1.f / lerp_tab(C.res_q, res)
                 : (1.f / C.res_q[9]) + (C.selfosc_k - 1.f / C.res_q[9]) * (res - 0.9f) * 10.f;
    const float brute_a = C.brute_k * p->brute;
    const float fb_r = 1.f - 2.f * PI * C.fb_hp_hz / (sr * OS);   /* the DC blockers' poles */
    const float in_r = 1.f - 2.f * PI * C.in_hp_hz / (sr * OS);
    const float out_r = 1.f - 2.f * PI * C.out_hp_hz / sr;
    const float bass_a = C.bass_k * mb_powf(clampf(p->resonance, 0.f, 1.f), C.bass_exp);
    const float fa = env_att(p->f_a, sr), fd = env_fac(p->f_d, sr), fr = env_fac(p->f_r, sr);
    const float aa = env_att(p->a_a, sr), ad = env_fac(p->a_d, sr), ar = env_fac(p->a_r, sr);
    int i, o;

    for (i = 0; i < n; i++) {
        /* --- modulation sources --- */
        v->fenv = env_step(v->fenv, &v->fst, fa, fd, p->f_s, fr);
        v->aenv = env_step(v->aenv, &v->ast, aa, ad, p->a_s, ar);
        float lfo_raw = lfo_value(v, isr);
        if (p->wheel_dest == MB_WHEEL_LFOAMT) lfo_raw *= v->wheel;
        float lfo = lfo_raw * C.lfo_pitch_shape[p->lfo_wave], lfo_a = lfo_raw * C.lfo_amp_shape[p->lfo_wave];
        v->vib_ph = frac(v->vib_ph + p->vib_rate_hz * isr);
        float vib_st = 0.f, ws = p->wheel_amt * v->wheel;
        if (p->wheel_dest == MB_WHEEL_VIBRATO) {
            if (p->vib_type == MB_VIB_SINE) vib_st = C.vib_st * ws * mb_sinf(2.f * PI * v->vib_ph);
            else vib_st = (p->vib_type == MB_VIB_POSSQ ? 1.f : -1.f) * C.vib_sq_st * ws * (v->vib_ph < 0.5f ? 1.f : 0.f);
        }
        vib_st += C.at_vib_st * p->at_vib * v->aftertouch * mb_sinf(2.f * PI * v->vib_ph);

        /* --- pitch --- */
        if (v->pitch_st < v->target_st) v->pitch_st = mb_fminf(v->pitch_st + glide_rate, v->target_st);
        else if (v->pitch_st > v->target_st) v->pitch_st = mb_fmaxf(v->pitch_st - glide_rate, v->target_st);
        float st = v->pitch_st + v->bend_st + vib_st + C.lfo_pitch_st * p->lfo_pitch * lfo;
        float f0 = 440.f * mb_exp2f((st - 69.f) * (1.f / 12.f));
        float dt = clampf(f0 * isr, 0.f, 0.45f);

        /* --- oscillators --- */
        v->ph += dt; if (v->ph >= 1.f) v->ph -= 1.f;
        float subdiv = p->sub_oct2 ? 0.25f : 0.5f;
        v->sub_ph += dt * subdiv; if (v->sub_ph >= 1.f) v->sub_ph -= 1.f;
        float s = 0.f;
        if (g_saw > 0.f) {
            float sw = saw_down(v->ph, dt);
            if (us_mix != 0.f) {
                v->us_lfo = frac(v->us_lfo + C.us_rate_mult * p->ultrasaw_rate_hz * isr);
                float cph = frac(v->ph + us_beta * mb_sinf(2.f * PI * v->us_lfo));
                sw = (1.f + C.us_main * p->ultrasaw) * sw + us_mix * saw_down(cph, dt);
            }
            s += g_saw * sw;
        }
        if (g_sq > 0.f) {
            float duty = C.duty50 + (p->pw_pct - 50.f) * C.duty_slope + C.pw_env * p->pw_env * v->fenv + C.pw_lfo * p->lfo_pwm_metal * lfo;
            duty = clampf(duty, 0.05f, 0.95f);
            float sq = (v->ph < duty ? 1.f : -1.f) + poly_blep(v->ph, dt) - poly_blep(frac(v->ph - duty + 1.f), dt);
            s += g_sq * sq;
        }
        if (g_tri > 0.f) {
            float m = clampf(p->metalizer + p->metal_env * v->fenv + C.metal_lfo * p->lfo_pwm_metal * lfo, 0.f, 1.f);
            s += g_tri * metal_wave(v->ph, m, f0, sr);
        }
        if (g_sub > 0.f) {
            float sub = p->sub_sine ? (v->sub_lp += (1.f - mb_expf(-2.f * PI * C.sub_sine_fc * isr)) * (tri_skewed(v->sub_ph, 0.f) - v->sub_lp))
                                    : (v->sub_ph < 0.5f ? 1.f : -1.f) + poly_blep(v->sub_ph, dt * subdiv) - poly_blep(frac(v->sub_ph + 0.5f), dt * subdiv);
            s += g_sub * sub;
        }
        if (g_noise > 0.f) s += g_noise * noise(&v->rng);

        /* --- filter cutoff in "knob octaves" --- */
        float oct = p->keytrack * ((float)v->note - C.kt_ref) / 12.f
                  + C.fenv_oct * p->fenv_amt * v->fenv * fvel
                  + C.lfo_filter_oct * p->lfo_filter * lfo
                  + C.at_cut_oct * p->at_cut * v->aftertouch;
        if (p->wheel_dest == MB_WHEEL_CUTOFF) oct += C.wheel_cut_oct * ws * (1.f - C.wheel_cut_curve * ws);
        float knob_eff = clampf(p->cutoff_hz * mb_exp2f(oct), 10.f, 40000.f);
        float ringw = clampf((res - 0.9f) * 10.f, 0.f, 1.f);
        float corr = ringw > 0.f ? 1.f + ringw * (ring_tab(C.ring_fcorr, knob_eff) - 1.f)
                                 : knob_corr(knob_eff);
        float fc = clampf(filter_freq(knob_eff, res) * corr, 10.f, 0.45f * sr * OS);
        float rgain = 1.f + ringw * (ring_tab(C.ring_gain, knob_eff) - 1.f);
        float g = mb_tanf(PI * fc / (sr * OS));
        float kd = k_damp;
        float agw = clampf((res - C.agc_start) / mb_fmaxf(0.9f - C.agc_start, 0.01f), 0.f, 1.f) * (1.f - ringw);
        if (agw > 0.f) kd += agw * C.agc_k * mb_fmaxf(v->agc_env - C.agc_level, 0.f);   /* loud resonance damps itself */
        if (kd < 0.05f && (knob_eff < C.ring_lo || knob_eff > C.ring_hi)) kd = 0.05f;   /* plugin stops ringing outside this range */
        float a1 = 1.f / (1.f + g * (g + kd)), a2 = g * a1, a3 = g * a2;

        float limw = clampf((res - C.lim_start) / mb_fmaxf(0.9f - C.lim_start, 0.01f), 0.f, 1.f) * (1.f - ringw);
        float lim_mk = limw > 0.f ? mb_powf(C.lim_makeup, mb_powf(limw, C.lim_makeup_exp)) : 1.f;
        float shw = clampf((res - C.shaper_res_start) / mb_fmaxf(1.f - C.shaper_res_start, 0.01f), 0.f, 1.f);
        /* --- VCA gain (before the filter: the Brute Factor takes the VCA's output back to the mixer) --- */
        float la = p->lfo_amp >= 0.f ? lfo_a : -lfo_a;
        float amp = v->aenv * avel * clampf(1.f + mb_fabsf(p->lfo_amp) * (la - 1.f), 0.f, 1.f);
        /* --- filter, oversampled TPT SVF with saturating band state + Brute feedback --- */
        float y = 0.f;
        for (o = 0; o < OS; o++) {
            float xin = v->x_prev + (s - v->x_prev) * (float)(o + 1) / OS;
            float vin = xin + brute_a * v->vca_fb - bass_a * (1.f - ringw) * v->brute_fb;
            if (in_r < 1.f) {                  /* the filter's input coupling: no DC into it (and none round the Brute loop) */
                float hy = in_r * (v->in_y1 + vin - v->in_x1);
                v->in_x1 = vin;
                vin = v->in_y1 = hy;
            }
            float v0 = fast_tanh(C.in_drive * vin) / C.in_drive + C.noise_inject * noise(&v->rng);
            float v3 = v0 - v->hp;                     /* hp field holds ic2eq */
            float v1 = a1 * v->bp + a2 * v3;
            float v2 = v->hp + a2 * v->bp + a3 * v3;
            { float u = (2.f * v1 - v->bp) / C.svf_sat; v->bp = C.svf_sat * fast_tanh(u + C.sat_asym * u * u); }   /* ic1eq, saturating (slightly asymmetric) */
            v->hp = 2.f * v2 - v->hp;                  /* ic2eq */
            float lp = v2, bp = v1, hp = v0 - kd * v1 - v2;
            { float a = mb_fabsf(bp); v->agc_env += (a > v->agc_env ? 0.002f : 0.0002f) * (a - v->agc_env); }
            v->brute_fb = lp;
            switch (p->filter_mode) {
            case MB_FILT_BP: y = bp; break;
            case MB_FILT_HP: y = hp; break;
            case MB_FILT_NOTCH: y = lp + hp; break;
            default: y = lp; break;
            }
            if (shw > 0.f) {                   /* resonant tone: the plugin's 2nd/3rd/5th harmonics, at the oversampled rate */
                v->ring_pk += 0.0002f * (y * y - v->ring_pk);              /* smooth mean-square follower (~25 ms) */
                float pk = mb_fmaxf(1.41421f * mb_sqrtf(v->ring_pk), 1e-6f), u = clampf(y / pk, -1.2f, 1.2f), u2 = u * u;
                float t2 = 2.f * u2 - 1.f, t3 = u * (4.f * u2 - 3.f), t5 = u * (16.f * u2 * u2 - 20.f * u2 + 5.f);
                y += shw * pk * (C.ring_h2 * t2 + C.ring_h3 * t3 + C.ring_h5 * t5);
            }
            if (limw > 0.f) y = (y + limw * (C.lim_level * fast_tanh(y / C.lim_level) - y)) * lim_mk;   /* resonance squashes like the plugin */
            y *= C.mode_gain[p->filter_mode & 3] * rgain;
            v->dec_hist[v->dec_pos] = y;
            {   /* the VCA (a transistor pair), then the feedback path's coupling capacitor: no DC goes round */
                float fx = fast_tanh(C.vca_drive * y * amp) / C.vca_drive;
                v->fb_y1 = fb_r * (v->fb_y1 + fx - v->fb_x1);
                v->fb_x1 = fx;
                v->vca_fb = v->fb_y1;
            }
            v->dec_pos = (v->dec_pos + 1) & (DEC_TAPS - 1);
        }
        { int t; y = 0.f; for (t = 0; t < DEC_TAPS; t++) y += g_dec[t] * v->dec_hist[(v->dec_pos + t) & (DEC_TAPS - 1)]; }
        v->x_prev = s;

        /* --- VCA, then the output's coupling: no DC out --- */
        {
            float yo = y * amp * master;
            v->out_y1 = out_r * (v->out_y1 + yo - v->out_x1);
            v->out_x1 = yo;
            out[i] = v->out_y1;
        }
    }
}
