/* SPDX-License-Identifier: MIT */
/* Calibration table: every number that ties mbvoice to the MiniBrute V measurements.
 * Kept in one struct so the test rig can fit it; on the FM-1 it can become const. */
#ifndef MBCAL_H
#define MBCAL_H
typedef struct {
    float out_ref;                 /* saw amplitude at mixer 1.0, master 0 dB */
    float g_square, g_tri, g_sub_sq, g_sub_sine, g_noise;
    float mix_exp;                 /* level knob taper */
    float duty50, duty_slope;      /* duty at 50% setting, per % */
    float tri_skew, tri_round;     /* triangle asymmetry; rounding LP at tri_round x f0 */
    float us_mix, us_exp, us_beta, us_rate_mult, us_main, us_phase0;
    float metal_gain, metal_lower_start;
    float kt_ref, fenv_oct, wheel_cut_oct, lfo_filter_oct, at_cut_oct;
    float res_q[11], res_fr[11];
    float f_ceiling;               /* frequency compression knee (Hz) */
    float svf_sat, in_drive;
    float mode_gain[4];
    float brute_k;
    float vib_st, vib_sq_st, at_vib_st, lfo_pitch_st;
    float lfo_pitch_shape[6], lfo_amp_shape[6];
    float lfo_saw_rate_mult, pw_lfo, pw_env, metal_lfo;
    float vintage_cents[4];
    float noise_inject;
    float selfosc_k, ring_lo, ring_hi, sat_asym;
    float ring_fcorr[17], ring_gain[17]; /* 17 points, log-spaced 110 Hz .. 10 kHz knob */
    float ring_h2, ring_h3, ring_h5;
    float sub_sine_fc;
    float shaper_res_start;
    float knob_corr[5];
    float wheel_cut_curve;
    float lim_level, lim_start, lim_makeup, lim_makeup_exp;
    float bass_k, bass_exp;
    float agc_k, agc_level, agc_start;
    float vca_drive;
    float fb_hp_hz;
    float in_hp_hz;
    float out_hp_hz;               /* the output's coupling capacitors: the DC the saturations make goes, Hz */                /* the filter's input coupling (a capacitor in the hardware), Hz */                /* Brute Factor: the feedback path's AC coupling (a capacitor in the hardware), Hz */               /* Brute Factor: the VCA's saturation on the signal fed back to the mixer */  /* resonance loop self-limiting: damping rises with the band level */             /* passband (bass) loss with resonance: negative low-pass feedback */         /* soft limiter on the resonant output below self-oscillation */              /* wheel->cutoff octaves = oct*x*(1 - curve*x), x = AMT*wheel */                 /* filter frequency correction at knob 100, 200, 1k, 5k, 20k Hz (non-ringing) */             /* resonance where the ringing-harmonic shaper begins */                  /* fixed low-pass on the sub's sine shape (Hz) */     /* harmonic shaper used while ringing */   /* at knob 150,300,500,800,1200,2000,3000,5000,8000 Hz */
} MBCal;
extern MBCal mb_cal;
#endif
