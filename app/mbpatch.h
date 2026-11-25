/* SPDX-License-Identifier: MIT
 * MB-1's patches: the parameters (their names, types, ranges and MIDI CCs), INIT and the factory sounds, how a
 * patch's 0..127 values become the engine's (mbvoice, mbfx), and how a value reads on screen. Out of app.c, the
 * same, so the editor's engine in the browser (app/editor) hears and shows a patch as the FM-1 does. Header-only:
 * included once, by app.c or by the editor's wasm. Needs fastmath.h (fm_*), mbvoice.h, mbfx.h, mb_tapers.h. */
#ifndef MBPATCH_H
#define MBPATCH_H
#include <stdint.h>

/* ------------------------------------------------------------- parameters --- */
enum { T_LVL, T_BIP, T_KBD, T_CUT, T_ATT, T_DR, T_LFO, T_PW, T_USR, T_VIB, T_GLIDE, T_DB,
       T_DTIME, T_DTONE, T_RDEC, T_RSIZE, T_RPRE, T_XDRV, T_XTONE, T_CRATE, T_CDEPTH, T_CPHASE, T_KTHR, T_KRAT,
       T_KATT, T_KREL, T_MODE, T_LWAVE, T_VTYPE, T_WDEST,
       T_SUBOCT, T_SUBSHP, T_PLAY, T_BEND, T_ONOFF, T_ARP, T_AMODE, T_AOCT, T_ASTEP, T_ASWING, T_HORD,
       T_LSYNC, T_TEMPO, T_SPAT, T_SLEN, T_DSYNC, T_DMODE, T_FXORD, T_XTYPE, T_KEYS };
static const uint8_t ENUM_N[] = {[T_MODE] = 4, [T_LWAVE] = 6, [T_VTYPE] = 3, [T_WDEST] = 3, [T_SUBOCT] = 2,
                                 [T_SUBSHP] = 2, [T_PLAY] = 2, [T_BEND] = 12, [T_ONOFF] = 2,
                                 [T_ARP] = 3, [T_AMODE] = 4, [T_AOCT] = 4, [T_ASTEP] = 6, [T_ASWING] = 6, [T_HORD] = 2,
                                 [T_LSYNC] = 2, [T_TEMPO] = 231, [T_SPAT] = 8, [T_SLEN] = 64,
                                 [T_DSYNC] = 2, [T_DMODE] = 2, [T_FXORD] = 2, [T_XTYPE] = 4, [T_KEYS] = 2};
static const char *const MODE_NAME[] = {"LP", "BP", "HP", "NOTCH"};
static const char *const LWAVE_NAME[] = {"SINE", "TRI", "SAW", "SQUARE", "S&H", "SMOOTH"};
static const char *const VTYPE_NAME[] = {"-SQR", "SINE", "+SQR"};
static const char *const WDEST_NAME[] = {"LFO AMT", "VIBRATO", "CUTOFF"};
static const char *const SUBOCT_NAME[] = {"-1 OCT", "-2 OCT"};
static const char *const SUBSHP_NAME[] = {"SQUARE", "SINE"};
static const char *const PLAY_NAME[] = {"LEGATO", "RETRIG"};
static const char *const ONOFF_NAME[] = {"OFF", "ON"};
static const char *const ARP_NAME[] = {"OFF", "ON", "HOLD"};
static const char *const AMODE_NAME[] = {"UP", "DOWN", "UP/DOWN", "RANDOM"};
static const char *const AOCT_NAME[] = {"1 OCT", "2 OCT", "3 OCT", "4 OCT"};
static const char *const ASTEP_NAME[] = {"1/4", "1/8", "1/16", "1/4T", "1/8T", "1/16T"};
static const char *const HORD_NAME[] = {"PITCH", "PLAYED"};
static const char *const KEYS_NAME[] = {"YES", "NO"};   /* (the SMK-37's keys: GLOBAL VEL SENS, VELOCITY SENSITIVE) */
static const char *const LSYNC_NAME[] = {"FREE", "ARP"};
static const char *const DSYNC_NAME[] = {"FREE", "CLOCK"};
static const char *const DMODE_NAME[] = {"STEREO", "PING-PONG"};
static const char *const FXORD_NAME[] = {"DLY>REV", "REV>DLY"};
static const char *const XTYPE_NAME[] = {"SOFT", "HARD", "ASYM", "FOLD"};

enum { P_CUT, P_RES, P_BRUTE, P_FENV, P_MODE, P_KBD, P_FVEL, P_AVEL,
       P_SAW, P_SQR, P_TRI, P_SUB, P_NOISE, P_ULTRA, P_PW, P_METAL,
       P_FA, P_FD, P_FS, P_FR, P_AA, P_AD, P_AS, P_AR,
       P_LRATE, P_LWAVE, P_LPITCH, P_LFILT, P_LPWM, P_LAMP, P_VRATE, P_VTYPE,
       P_GLIDE, P_VINT, P_WDEST, P_WAMT, P_SUBOCT, P_SUBSHP, P_USRATE, P_LEVEL,
       P_ATVIB, P_ATCUT, P_PWENV, P_MTENV, P_PLAY, P_BEND, P_LRTRG,
       P_ARP, P_AMODE, P_AOCT, P_ASTEP, P_ASWING,
       P_DTIME, P_DFB, P_DTONE, P_DMIX, P_DSYNC, P_DMODE, P_RDECAY, P_RSIZE, P_RDAMP, P_RMIX, P_RPRE, P_FXORD,
       P_XTYPE, P_XDRV, P_XTONE, P_XMIX, P_CRATE, P_CDEPTH, P_CPHASE, P_CMIX, P_KTHR, P_KRAT, P_KATT, P_KREL, NP };
#define NP_08 47u                        /* 0.8's patches: no arpeggiator, no effects (the loader takes them still) */
#define NP_09 52u                        /* 0.8.2 .. 0.9's: no effects */
#define NP_10 63u                        /* 0.10's: no FX ORDER */
#define NP_104 64u                       /* 0.10.4's: no distortion, chorus, compressor */
#define P_NONE 0xFFu                     /* an empty place on a page */
/* the instrument's settings (not a patch's): the tempo, the Hold order, the LFO's clock, and the sequencer's
 * pattern, length, step and swing; numbered after the patch's parameters. (The arpeggiator's Off/On/Hold, Mode,
 * Octave, Step and Swing are the patch's, as in the plugin.) */
enum { G_TEMPO = NP, G_HORD, G_LSYNC, G_SPAT, G_SLEN, G_SSTEP, G_SSWING, G_KEYS, G_END };
#define NG (G_END - NP)

typedef struct {
    const char *name;
    uint8_t type, cc;
} param_t;
/* the CCs: 20..31, then 70..101 (none of the reserved or the common ones) */
static const param_t PARAM[NP] = {
    [P_CUT] = {"CUTOFF", T_CUT, 74},   [P_RES] = {"RESO", T_LVL, 71},     [P_BRUTE] = {"BRUTE", T_LVL, 20},
    [P_FENV] = {"ENV AMT", T_BIP, 21}, [P_MODE] = {"MODE", T_MODE, 22},   [P_KBD] = {"KBD TRK", T_KBD, 23},
    [P_FVEL] = {"ENV VEL", T_LVL, 24}, [P_AVEL] = {"AMP VEL", T_LVL, 25},
    [P_SAW] = {"SAW", T_LVL, 26},      [P_SQR] = {"SQUARE", T_LVL, 27},   [P_TRI] = {"TRI", T_LVL, 28},
    [P_SUB] = {"SUB", T_LVL, 29},      [P_NOISE] = {"NOISE", T_LVL, 30},  [P_ULTRA] = {"ULTRASAW", T_LVL, 31},
    [P_PW] = {"PW", T_PW, 75},         [P_METAL] = {"METAL", T_LVL, 76},
    [P_FA] = {"ATTACK", T_ATT, 77},   [P_FD] = {"DECAY", T_DR, 78},    [P_FS] = {"SUSTAIN", T_LVL, 79},
    [P_FR] = {"RELEASE", T_DR, 80},  [P_AA] = {"ATTACK", T_ATT, 73},   [P_AD] = {"DECAY", T_DR, 81},
    [P_AS] = {"SUSTAIN", T_LVL, 82},   [P_AR] = {"RELEASE", T_DR, 72},
    [P_LRATE] = {"RATE", T_LFO, 83},   [P_LWAVE] = {"WAVE", T_LWAVE, 84}, [P_LPITCH] = {"PITCH", T_BIP, 85},
    [P_LFILT] = {"FILTER", T_BIP, 86}, [P_LPWM] = {"PWM/MTL", T_BIP, 87}, [P_LAMP] = {"AMP", T_BIP, 88},
    [P_VRATE] = {"VIB RATE", T_VIB, 89}, [P_VTYPE] = {"VIB TYPE", T_VTYPE, 90},
    [P_GLIDE] = {"GLIDE", T_GLIDE, 5}, [P_VINT] = {"VINTAGE", T_LVL, 91}, [P_WDEST] = {"WHEEL TO", T_WDEST, 92},
    [P_WAMT] = {"WHL AMT", T_LVL, 93}, [P_SUBOCT] = {"SUB OCT", T_SUBOCT, 94},
    [P_SUBSHP] = {"SUB WAVE", T_SUBSHP, 95}, [P_USRATE] = {"US RATE", T_USR, 96}, [P_LEVEL] = {"LEVEL", T_DB, 7},
    [P_ATVIB] = {"AT VIB", T_LVL, 97}, [P_ATCUT] = {"AT CUT", T_LVL, 98}, [P_PWENV] = {"PW ENV", T_BIP, 99},
    [P_MTENV] = {"MTL ENV", T_BIP, 100},
    [P_PLAY] = {"PLAY", T_PLAY, 102},   [P_BEND] = {"BEND", T_BEND, 103},  [P_LRTRG] = {"LFO RTRG", T_ONOFF, 104},
    [P_ARP] = {"ARP", T_ARP, 105},      [P_AMODE] = {"MODE", T_AMODE, 106}, [P_AOCT] = {"OCTAVE", T_AOCT, 107},
    [P_ASTEP] = {"STEP", T_ASTEP, 108}, [P_ASWING] = {"SWING", T_ASWING, 109},
    [P_DTIME] = {"TIME", T_DTIME, 110},   [P_DFB] = {"FEEDBACK", T_LVL, 111}, [P_DTONE] = {"TONE", T_DTONE, 112},
    [P_DMIX] = {"MIX", T_LVL, 113},       [P_DSYNC] = {"DLY SYNC", T_DSYNC, 114}, [P_DMODE] = {"DLY MODE", T_DMODE, 115},
    [P_RDECAY] = {"DECAY", T_RDEC, 116},  [P_RSIZE] = {"SIZE", T_RSIZE, 117}, [P_RDAMP] = {"DAMP", T_LVL, 118},
    [P_RMIX] = {"MIX", T_LVL, 119},       [P_RPRE] = {"PRE-DLY", T_RPRE, 14},  [P_FXORD] = {"FX ORDER", T_FXORD, 15},
    [P_XTYPE] = {"TYPE", T_XTYPE, 16},    [P_XDRV] = {"DRIVE", T_XDRV, 17},   [P_XTONE] = {"TONE", T_XTONE, 18},
    [P_XMIX] = {"MIX", T_LVL, 19},        [P_CRATE] = {"RATE", T_CRATE, 52},  [P_CDEPTH] = {"DEPTH", T_CDEPTH, 53},
    [P_CPHASE] = {"STEREO", T_CPHASE, 54}, [P_CMIX] = {"MIX", T_LVL, 55},     [P_KTHR] = {"THRESH", T_KTHR, 56},
    [P_KRAT] = {"RATIO", T_KRAT, 57},     [P_KATT] = {"ATTACK", T_KATT, 58},  [P_KREL] = {"RELEASE", T_KREL, 59},
};

static const param_t GPARAM[NG] = {
    [G_SSTEP - NP] = {"STEP", T_ASTEP, 0},     [G_SSWING - NP] = {"SWING", T_ASWING, 0},
    [G_TEMPO - NP] = {"TEMPO", T_TEMPO, 0},
    [G_HORD - NP] = {"HOLD ORD", T_HORD, 0},   [G_LSYNC - NP] = {"LFO CLK", T_LSYNC, 0},
    [G_SPAT - NP] = {"PATTERN", T_SPAT, 0},    [G_SLEN - NP] = {"LENGTH", T_SLEN, 0},
    [G_KEYS - NP] = {"VEL SENS", T_KEYS, 0},
};
static const param_t *pinfo(uint32_t i) { return i < NP ? &PARAM[i] : &GPARAM[(i - NP) % NG]; }
static int is_enum(uint32_t t) { return t >= T_MODE && t != T_TEMPO && t != T_SLEN; }
static uint32_t pmax(uint32_t i)
{
    uint32_t t = pinfo(i)->type;
    return t == T_TEMPO ? 230u : t == T_SLEN ? 63u : is_enum(t) ? ENUM_N[t] - 1u : 127u;
}

typedef struct {
    char name[8];                        /* upper case, space padded, no terminator needed */
    uint8_t v[NP];
} patch_t;

/* INIT: a plain sawtooth through an open filter, organ envelope */
static const uint8_t INIT_V[NP] = {
    [P_CUT] = 96, [P_RES] = 0, [P_BRUTE] = 0, [P_FENV] = 64, [P_MODE] = 0, [P_KBD] = 0, [P_FVEL] = 0, [P_AVEL] = 0,
    [P_SAW] = 127, [P_PW] = 0, [P_FA] = 0, [P_FD] = 27, [P_FS] = 0, [P_FR] = 5, [P_AA] = 0, [P_AD] = 17,
    [P_AS] = 127, [P_AR] = 5, [P_LRATE] = 51, [P_LWAVE] = 0, [P_LPITCH] = 64, [P_LFILT] = 64, [P_LPWM] = 64,
    [P_LAMP] = 64, [P_VRATE] = 57, [P_VTYPE] = 1, [P_GLIDE] = 0, [P_VINT] = 0, [P_WDEST] = 1, [P_WAMT] = 61,
    [P_SUBOCT] = 0, [P_SUBSHP] = 0, [P_USRATE] = 31, [P_LEVEL] = 60, [P_PWENV] = 64, [P_MTENV] = 64,
    [P_PLAY] = 0, [P_BEND] = 1, [P_LRTRG] = 0, [P_ARP] = 0, [P_AMODE] = 0, [P_AOCT] = 0, [P_ASTEP] = 1,
    [P_ASWING] = 0, [P_DTIME] = 77, [P_DFB] = 51, [P_DTONE] = 95, [P_DMIX] = 0, [P_DSYNC] = 0, [P_DMODE] = 0,
    [P_RDECAY] = 64, [P_RSIZE] = 64, [P_RDAMP] = 64, [P_RMIX] = 0, [P_RPRE] = 0, [P_FXORD] = 0,
    [P_XTYPE] = 0, [P_XDRV] = 64, [P_XTONE] = 127, [P_XMIX] = 0, [P_CRATE] = 23, [P_CDEPTH] = 127, [P_CPHASE] = 127,
    [P_CMIX] = 0, [P_KTHR] = 51, [P_KRAT] = 0, [P_KATT] = 64, [P_KREL] = 64,
};
/* the factory sounds: bank A's first slots (app/factory, tools/make_factory.py) */
#include "factory_bank.h"
#define NFACTORY (sizeof FACTORY / sizeof FACTORY[0])

static void patch_name(patch_t *p, const char *s)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        p->name[i] = *s ? *s++ : ' ';
}

static void patch_factory(patch_t *p, uint32_t slot)
{
    uint32_t i;
    for (i = 0; i < NP; i++)
        p->v[i] = INIT_V[i];
    if (slot >= NFACTORY) {
        patch_name(p, "INIT");
        return;
    }
    patch_name(p, FACTORY[slot].name);
    for (i = 0; i < NP; i++)
        p->v[i] = FACTORY[slot].v[i];
}

/* ---------------------------------------------------------------- mapping --- */
static float lvl(uint32_t x) { return (float)x * (1.0f / 127.0f); }
static float bip(uint32_t x) { return fm_clampf(((float)x - 64.0f) * (1.0f / 63.0f), -1.0f, 1.0f); }
/* the knobs as the plugin's (mb_tapers.h): x 0..127 is the plugin's 0..1 */
static float taper(const float *t, uint32_t x)
{
    float f = lvl(x) * (float)(TP_N - 1);
    uint32_t i = (uint32_t)f;
    if (i >= TP_N - 1u)
        return t[TP_N - 1];
    return t[i] + (t[i + 1u] - t[i]) * (f - (float)i);
}
static float cut_hz(uint32_t x) { return taper(TP_CUT, x); }        /* 20 Hz .. 20 kHz */
static float att_s(uint32_t x) { return taper(TP_ATT, x); }         /* 1 ms .. 12.5 s */
static float dr_s(uint32_t x) { return taper(TP_DR, x); }           /* 1 ms .. 25 s */
static float lfo_hz(uint32_t x) { return taper(TP_LFO, x); }        /* 0.05 .. 100 Hz */
static float usr_hz(uint32_t x) { return taper(TP_USR, x); }        /* 0.005 .. 43 Hz */
static float vib_hz(uint32_t x) { return taper(TP_VIB, x); }        /* 1 .. 50 Hz */
static float glide_s(uint32_t x) { return taper(TP_GLIDE, x); }     /* 0 .. 2 s an octave */
static float level_db(uint32_t x) { return taper(TP_DB, x); }       /* -60 .. 0 dB, the plugin's master */
static float dtime_s(uint32_t x) { return taper(TP_DTIME, x) * 0.001f; }   /* 2 ms .. 2 s (MB-1: up to 1.48 s) */
static float dlp_hz(uint32_t x) { return taper(TP_DLP, x); }        /* 250 Hz .. 20 kHz */
static float rpre_s(uint32_t x) { return taper(TP_RPRE, x); }       /* 0 .. 0.2 s */
static float rdecay(uint32_t x) { return lvl(x) * 0.925f; }         /* the plugin's DECAY, 0 .. 0.925 */
static float rsize(uint32_t x) { return 0.1f + 1.4f * lvl(x); }     /* the plugin's SIZE, 0.1 .. 1.5 */
/* the delay on the clock (DLY SYNC: CLOCK): TIME picks a note value; its length in beats */
#define NDSYNC 14u
static const char *const DSYNC_DIV[NDSYNC] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
                                             "1/4", "1/2T", "1/4D", "1/2", "1/2D", "1/1"};
static const float DSYNC_BEATS[NDSYNC] = {0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f,
                                         1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 3.0f, 4.0f};
static uint32_t dsync_ix(uint32_t x) { return x * NDSYNC / 128u; }
/* the chorus: RATE as the plugin's Juno-6 chorus (measured: plan8b), DEPTH its sweep, STEREO the right's LFO later */
static float crate_hz(uint32_t x)
{
    static const float R[7][2] = {{0.0f, 0.33f}, {0.1f, 0.33f}, {0.18f, 0.38f}, {0.3f, 0.7f}, {0.5f, 1.83f}, {0.75f, 5.37f}, {1.0f, 15.0f}};
    float r = lvl(x);
    uint32_t i;
    for (i = 0; i + 2u < 7u && r > R[i + 1u][0]; i++)
        ;
    return R[i][1] * fm_exp2f((r - R[i][0]) / (R[i + 1u][0] - R[i][0]) * fm_log2f(R[i + 1u][1] / R[i][1]));
}
static float tab5(const float *t, uint32_t x)   /* 5 points over the knob: 0, 1/4, 1/2, 3/4, 1 */
{
    float f = lvl(x) * 4.0f;
    uint32_t i = f >= 4.0f ? 3u : (uint32_t)f;
    return t[i] + (t[i + 1u] - t[i]) * (f - (float)i);
}
static float cdepth_ms(uint32_t x) { static const float T[5] = {0.f, 0.58f, 1.81f, 3.43f, 10.1f}; return tab5(T, x); }
static float cphase(uint32_t x) { static const float T[5] = {0.f, 0.15f, 0.24f, 0.5f, 1.0f}; return tab5(T, x); }   /* (x half
                                                                     * a cycle: the plugin's L/R correlation) */
static float kthr_db(uint32_t x) { return -60.0f + 80.0f * lvl(x); }
static float kratio(uint32_t x) { return taper(TP_KRAT, x); }
static float katt_ms(uint32_t x) { return taper(TP_KATT, x); }
static float krel_ms(uint32_t x) { return taper(TP_KREL, x); }
/* the LFO on the arpeggiator's clock (LFO CLK: ARP): RATE picks a note value, as the plugin's synced rates
 * (8 bars dotted .. a 64th triplet); its length in beats */
static const char *const LSYNC_DIV[30] = {"8.D", "8", "4.D", "8.T", "4", "2.D", "4.T", "2", "1.D", "2.T", "1",
    "1/2.D", "1.T", "1/2", "1/4.D", "1/2.T", "1/4", "1/8.D", "1/4.T", "1/8", "1/16.D", "1/8.T", "1/16", "1/32.D",
    "1/16.T", "1/32", "1/64.D", "1/32.T", "1/64", "1/64.T"};
static float lsync_beats(uint32_t x)        /* RATE 0..127 -> the cycle's length in beats */
{
    static const float W[30] = {12.0f, 8.0f, 6.0f, 16.0f / 3.0f, 4.0f, 3.0f, 8.0f / 3.0f, 2.0f, 1.5f, 4.0f / 3.0f,
        1.0f, 0.75f, 2.0f / 3.0f, 0.5f, 0.375f, 1.0f / 3.0f, 0.25f, 0.1875f, 1.0f / 6.0f, 0.125f, 0.09375f,
        1.0f / 12.0f, 0.0625f, 0.046875f, 1.0f / 24.0f, 0.03125f, 0.0234375f, 1.0f / 48.0f, 0.015625f, 1.0f / 96.0f};
    return W[x * 29u / 127u] * 4.0f;        /* (whole notes -> beats) */
}
#define LEVEL_OFFSET_DB 9.0f             /* MB-1's output above the plugin's master level */

static void mb_params_core(const uint8_t *v, MBParams *p)
{
    mb_default_params(p);
    p->saw = lvl(v[P_SAW]);
    p->square = lvl(v[P_SQR]);
    p->triangle = lvl(v[P_TRI]);
    p->sub = lvl(v[P_SUB]);
    p->noise = lvl(v[P_NOISE]);
    p->sub_sine = v[P_SUBSHP];
    p->sub_oct2 = v[P_SUBOCT];
    p->pw_pct = 50.0f + 40.0f * lvl(v[P_PW]);
    p->pw_env = bip(v[P_PWENV]);
    p->ultrasaw = lvl(v[P_ULTRA]);
    p->ultrasaw_rate_hz = usr_hz(v[P_USRATE]);
    p->metalizer = lvl(v[P_METAL]);
    p->metal_env = bip(v[P_MTENV]);
    p->filter_mode = v[P_MODE];
    p->cutoff_hz = cut_hz(v[P_CUT]);
    p->resonance = lvl(v[P_RES]);
    p->brute = lvl(v[P_BRUTE]);
    p->fenv_amt = bip(v[P_FENV]);
    p->keytrack = 2.0f * lvl(v[P_KBD]);   /* 0 .. 200 % */
    p->fenv_vel = lvl(v[P_FVEL]);
    p->f_a = att_s(v[P_FA]);
    p->f_d = dr_s(v[P_FD]);
    p->f_s = lvl(v[P_FS]);
    p->f_r = dr_s(v[P_FR]);
    p->a_a = att_s(v[P_AA]);
    p->a_d = dr_s(v[P_AD]);
    p->a_s = lvl(v[P_AS]);
    p->a_r = dr_s(v[P_AR]);
    p->amp_vel = lvl(v[P_AVEL]);
    p->lfo_wave = v[P_LWAVE];
    p->lfo_rate_hz = lfo_hz(v[P_LRATE]);
    p->lfo_pwm_metal = bip(v[P_LPWM]);
    p->lfo_pitch = bip(v[P_LPITCH]);
    p->lfo_filter = bip(v[P_LFILT]);
    p->lfo_amp = bip(v[P_LAMP]);
    p->vib_type = v[P_VTYPE];
    p->vib_rate_hz = vib_hz(v[P_VRATE]);
    p->wheel_dest = v[P_WDEST];
    p->wheel_amt = lvl(v[P_WAMT]);
    p->at_vib = lvl(v[P_ATVIB]);
    p->at_cut = lvl(v[P_ATCUT]);
    p->glide_s_oct = glide_s(v[P_GLIDE]);
    p->vintage = lvl(v[P_VINT]);
    p->master_db = level_db(v[P_LEVEL]) + LEVEL_OFFSET_DB;
    p->retrig = v[P_PLAY];
    p->lfo_retrig = v[P_LRTRG];
}

static void fx_params_from(const uint8_t *v, MBFxParams *f, float *beats)
{
    f->d_time_s = dtime_s(v[P_DTIME]);
    f->d_fb = lvl(v[P_DFB]);
    f->d_lp_hz = dlp_hz(v[P_DTONE]);
    f->d_mix = lvl(v[P_DMIX]);
    f->d_pp = v[P_DMODE];
    f->r_decay = rdecay(v[P_RDECAY]);
    f->r_size = rsize(v[P_RSIZE]);
    f->r_damp = lvl(v[P_RDAMP]);
    f->r_mix = lvl(v[P_RMIX]);
    f->r_pre_s = rpre_s(v[P_RPRE]);
    f->order = v[P_FXORD];
    f->x_type = v[P_XTYPE];
    f->x_drive_db = 48.0f * lvl(v[P_XDRV]);
    f->x_tone = lvl(v[P_XTONE]);
    f->x_mix = lvl(v[P_XMIX]);
    f->c_rate_hz = crate_hz(v[P_CRATE]);
    f->c_depth_ms = cdepth_ms(v[P_CDEPTH]);
    f->c_phase = cphase(v[P_CPHASE]);
    f->c_mix = lvl(v[P_CMIX]);
    f->k_thr_db = kthr_db(v[P_KTHR]);
    f->k_ratio = kratio(v[P_KRAT]);
    f->k_att_ms = katt_ms(v[P_KATT]);
    f->k_rel_ms = krel_ms(v[P_KREL]);
    *beats = v[P_DSYNC] ? DSYNC_BEATS[dsync_ix(v[P_DTIME])] : 0.0f;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void dec1_text(float f, char *b)      /* one decimal: 2.5 */
{
    int32_t t = (int32_t)(f * 10.0f + 0.5f);
    char d[4] = ".0";
    int_text(t / 10, b);
    d[1] = (char)('0' + t % 10);
    text_cat(b, d);
}

static void hz_text(float f, char *b)
{
    b[0] = 0;
    if (f >= 1000.0f) {
        dec1_text(f * 0.001f, b);
        text_cat(b, " KHZ");
    } else if (f >= 10.0f) {
        int_text((int32_t)(f + 0.5f), b);
        text_cat(b, " HZ");
    } else {
        dec1_text(f, b);
        text_cat(b, " HZ");
    }
}

static void time_text(float s, char *b)
{
    b[0] = 0;
    if (s < 1.0f) {
        int_text((int32_t)(s * 1000.0f + 0.5f), b);
        text_cat(b, " MS");
    } else {
        dec1_text(s, b);
        text_cat(b, " S");
    }
}

static void mb_value_text(uint32_t i, uint32_t x, int lsync, int dsync, char *b)
{
    b[0] = 0;
    switch (pinfo(i)->type) {
    case T_LVL:
        int_text((int32_t)(x * 100u + 63u) / 127, b);
        text_cat(b, "%");
        break;
    case T_BIP: {
        int32_t v = (int32_t)(bip(x) * 100.0f + (x >= 64u ? 0.5f : -0.5f));
        if (v > 0)
            text_cat(b, "+");
        int_text(v, b + (v > 0));
        text_cat(b, "%");
        break;
    }
    case T_CUT:
        hz_text(cut_hz(x), b);
        break;
    case T_KBD:
        int_text((int32_t)(x * 200u + 63u) / 127, b);
        text_cat(b, "%");
        break;
    case T_ATT:
        time_text(att_s(x), b);
        break;
    case T_DR:
        time_text(dr_s(x), b);
        break;
    case T_LFO:
        if (lsync)
            text_cat(b, LSYNC_DIV[x * 29u / 127u]);
        else
            hz_text(lfo_hz(x), b);
        break;
    case T_ARP:
        text_cat(b, ARP_NAME[x % 3u]);
        break;
    case T_AMODE:
        text_cat(b, AMODE_NAME[x & 3u]);
        break;
    case T_AOCT:
        text_cat(b, AOCT_NAME[x & 3u]);
        break;
    case T_ASTEP:
        text_cat(b, ASTEP_NAME[x % 6u]);
        break;
    case T_ASWING:
        int_text(50 + 5 * (int32_t)(x % 6u), b);
        text_cat(b, "%");
        break;
    case T_HORD:
        text_cat(b, HORD_NAME[x & 1u]);
        break;
    case T_KEYS:
        text_cat(b, KEYS_NAME[x & 1u]);
        break;
    case T_LSYNC:
        text_cat(b, LSYNC_NAME[x & 1u]);
        break;
    case T_TEMPO:
        int_text(30 + (int32_t)x, b);
        text_cat(b, " BPM");
        break;
    case T_SPAT:
        int_text((int32_t)x + 1, b);
        break;
    case T_SLEN:
        int_text((int32_t)x + 1, b);
        text_cat(b, " STEPS");
        break;
    case T_USR:
        hz_text(usr_hz(x), b);
        break;
    case T_VIB:
        hz_text(vib_hz(x), b);
        break;
    case T_PW:
        int_text((int32_t)(50.0f + 40.0f * lvl(x) + 0.5f), b);
        text_cat(b, "%");
        break;
    case T_GLIDE:
        if (!x)
            text_cat(b, "OFF");
        else
            time_text(glide_s(x), b);
        break;
    case T_DB: {
        float d = level_db(x);
        if (d < -0.05f) {
            text_cat(b, "-");
            d = -d;
        }
        dec1_text(d, b + (b[0] != 0));
        text_cat(b, " DB");
        break;
    }
    case T_MODE:
        text_cat(b, MODE_NAME[x & 3u]);
        break;
    case T_LWAVE:
        text_cat(b, LWAVE_NAME[x < 6u ? x : 0u]);
        break;
    case T_VTYPE:
        text_cat(b, VTYPE_NAME[x < 3u ? x : 1u]);
        break;
    case T_WDEST:
        text_cat(b, WDEST_NAME[x < 3u ? x : 1u]);
        break;
    case T_SUBOCT:
        text_cat(b, SUBOCT_NAME[x & 1u]);
        break;
    case T_SUBSHP:
        text_cat(b, SUBSHP_NAME[x & 1u]);
        break;
    case T_PLAY:
        text_cat(b, PLAY_NAME[x & 1u]);
        break;
    case T_BEND:
        int_text((int32_t)x + 1, b);
        text_cat(b, " ST");
        break;
    case T_ONOFF:
        text_cat(b, ONOFF_NAME[x & 1u]);
        break;
    case T_DTIME:                            /* (the patch on screen: free or on the clock) */
        if (dsync)
            text_cat(b, DSYNC_DIV[dsync_ix(x)]);
        else
            time_text(dtime_s(x), b);
        break;
    case T_DTONE:
        hz_text(dlp_hz(x), b);
        break;
    case T_RDEC:
        int_text((int32_t)(rdecay(x) * 100.0f + 0.5f), b);
        text_cat(b, "%");
        break;
    case T_RSIZE:
        dec1_text(rsize(x), b);
        break;
    case T_RPRE:
        int_text((int32_t)(rpre_s(x) * 1000.0f + 0.5f), b);
        text_cat(b, " MS");
        break;
    case T_DSYNC:
        text_cat(b, DSYNC_NAME[x & 1u]);
        break;
    case T_DMODE:
        text_cat(b, DMODE_NAME[x & 1u]);
        break;
    case T_FXORD:
        text_cat(b, FXORD_NAME[x & 1u]);
        break;
    case T_XTYPE:
        text_cat(b, XTYPE_NAME[x & 3u]);
        break;
    case T_XDRV:
        dec1_text(48.0f * lvl(x), b);
        text_cat(b, " DB");
        break;
    case T_XTONE:
        hz_text(1000.0f * fm_exp2f(4.3219281f * lvl(x)), b);
        break;
    case T_CRATE:
        hz_text(crate_hz(x), b);
        break;
    case T_CDEPTH:
        dec1_text(cdepth_ms(x), b);
        text_cat(b, " MS");
        break;
    case T_CPHASE:
        int_text((int32_t)(cphase(x) * 180.0f + 0.5f), b);
        text_cat(b, " DEG");
        break;
    case T_KTHR: {
        float d = kthr_db(x);
        if (d < -0.05f) {
            text_cat(b, "-");
            d = -d;
        }
        dec1_text(d, b + (b[0] != 0));
        text_cat(b, " DB");
        break;
    }
    case T_KRAT:
        if (x == 0u)
            text_cat(b, "OFF");
        else {
            dec1_text(kratio(x), b);
            text_cat(b, ":1");
        }
        break;
    case T_KATT:
        time_text(katt_ms(x) * 0.001f, b);
        break;
    case T_KREL:
        time_text(krel_ms(x) * 0.001f, b);
        break;
    }
}

#endif
