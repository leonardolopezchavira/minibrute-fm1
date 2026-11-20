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
    float out_hp_hz;
    /* the Ultrasaw as the hardware makes it: two copies of the saw, each with a pulse added where the saw crosses a
     * slow triangle LFO (one fixed, one the RATE knob), summed with the saw */
    float us_fixed_hz, us_depth, us_pulse, us_shape, us_gain, us_phase1;
    /* the Metalizer as the hardware makes it: the triangle, scaled by the knob (gain mt_g0 + mt_g1 * m^mt_pw) and
     * offset (mt_d), through four folding stages (metal_stage.h), mt_a0 of it added to the triangle */
    float mt_g0, mt_g1, mt_pw, mt_a0, mt_d, mt_norm;
    /* the filter's modes as the Steiner-Parker network gives them (the signal into its LP, BP or HP input): each mode
     * a mix of the low-, band- and high-pass responses (LP, BP, HP, NOTCH x L, B, H) */
    float mode_mix[12];
    float mode_damp[4], mode_fr[4];   /* per mode: the resonance's damping and its frequency, relative to LP's */
    float mt_phase, mt_d1, mt_k, mt_a1;  /* Metalizer: the folded path's lag behind the clean triangle (cycles); the
                                          * offset's and the amount's slope with the knob; the input stage's rounding */
} MBCal;
extern MBCal mb_cal;
#endif
