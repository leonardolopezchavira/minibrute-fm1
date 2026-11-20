/* SPDX-License-Identifier: MIT */
/* mbvoice — Minibrute-style monophonic voice for the M-VAVE FM-1 firmware project.
 * Portable C99, no allocation, float only. Parameters are in physical units
 * matching the MiniBrute V plugin's displayed values (see the DSP Measurement Spec). */
#ifndef MBVOICE_H
#define MBVOICE_H
#include <stdint.h>

enum { MB_FILT_LP = 0, MB_FILT_BP, MB_FILT_HP, MB_FILT_NOTCH };
enum { MB_LFO_SINE = 0, MB_LFO_TRI, MB_LFO_SAW, MB_LFO_SQUARE, MB_LFO_SH, MB_LFO_SMOOTH };
enum { MB_VIB_NEGSQ = 0, MB_VIB_SINE, MB_VIB_POSSQ };
enum { MB_WHEEL_LFOAMT = 0, MB_WHEEL_VIBRATO, MB_WHEEL_CUTOFF };

typedef struct {
    /* mixer (0..1 knob) */
    float saw, square, triangle, sub, noise;
    int   sub_sine;          /* 0 square, 1 sine */
    int   sub_oct2;          /* 0 = -1 oct, 1 = -2 oct */
    /* oscillator shaping */
    float pw_pct;            /* 50..90 */
    float pw_env;            /* -1..1 */
    float ultrasaw, ultrasaw_rate_hz;
    float metalizer, metal_env;   /* 0..1, -1..1 */
    /* filter */
    int   filter_mode;
    float cutoff_hz;         /* knob label, 20..20000 */
    float resonance;         /* 0..1 */
    float brute;             /* 0..1 */
    float fenv_amt;          /* -1..1 */
    float keytrack;          /* 0..2 (100% = 1) */
    float fenv_vel;          /* 0..1 */
    /* envelopes, seconds */
    float f_a, f_d, f_s, f_r;
    float a_a, a_d, a_s, a_r;
    float amp_vel;           /* 0..1 */
    /* LFO */
    int   lfo_wave;
    float lfo_rate_hz;
    float lfo_pwm_metal, lfo_pitch, lfo_filter, lfo_amp;   /* -1..1 */
    /* vibrato / wheel / aftertouch */
    int   vib_type;
    float vib_rate_hz;
    int   wheel_dest;
    float wheel_amt;         /* AMT knob 0..1 */
    float at_vib, at_cut;    /* 0..1 */
    /* misc */
    float glide_s_oct;
    int   retrig;            /* 0 mono legato: a note played over a held one keeps the envelopes going; 1 mono retrig: every note restarts them */
    int   lfo_retrig;        /* 1: the LFO restarts at every note that starts the envelopes */
    float vintage;           /* 0..1 */
    float master_db;
} MBParams;

typedef struct {
    float sr;
    MBParams p;
    /* note state */
    int   note, gate, held, vel;
    float pitch_st, target_st;   /* current/target pitch in semitones (MIDI) */
    unsigned note_count;
    float wheel, aftertouch;     /* 0..1 */
    float bend_st;               /* pitch bend, semitones */
    /* oscillators */
    float ph, sub_ph, us_ph1, us_ph2, us_lfo;
    uint32_t rng;
    /* envelopes */
    float fenv, aenv; int fst, ast;   /* stage: 0 idle 1 att 2 dec/sus 3 rel */
    /* lfos */
    float lfo_ph, lfo_sh, lfo_sh_prev, vib_ph;
    /* filter (2x oversampled Chamberlin SVF) */
    float lp, bp, hp, brute_fb, tri_lp, sub_lp, ring_pk;
    float dec_hist[64]; int dec_pos; float agc_env;
    float vca_fb, fb_x1, fb_y1, in_x1, in_y1, out_x1, out_y1;  /* (and its DC blocker's state) */                /* Brute Factor: the VCA's output, back into the mixer (oversampled) */
    float x_prev;
} MBVoice;

void mb_default_params(MBParams *p);
void mb_init(MBVoice *v, float sample_rate);
void mb_note_on(MBVoice *v, int note, int velocity);
void mb_note_off(MBVoice *v, int note);
void mb_set_wheel(MBVoice *v, float w);        /* 0..1 */
void mb_set_aftertouch(MBVoice *v, float a);   /* 0..1 */
void mb_set_bend(MBVoice *v, float st);        /* semitones */
void mb_all_off(MBVoice *v);                   /* release every note (the envelopes run on) */
void mb_render(MBVoice *v, float *out, int n);
#endif
