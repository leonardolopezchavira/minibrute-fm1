/* SPDX-License-Identifier: MIT
 * MB-1 for the M-VAVE FM-1 (an fm1-base app; started from fm1-base's template, MPL-2.0, and laid out after
 * sq80-fm1's app): one monophonic Minibrute-style voice (mbvoice/, measured against the MiniBrute V plugin).
 *
 * The panel: K1..K4 turn the four parameters of the page on screen. The page buttons pick a page, a second press
 * its second page: HOME the filter (FILTER, FILTER 2), SEL the oscillators (OSC, OSC 2), ENV the envelopes
 * (FILT ENV, AMP ENV), LFO the LFO and vibrato, EDIT the performance controls (glide, vintage, the wheel), GLO the
 * rest (MORE: sub octave and shape, Ultrasaw rate, level; VOICE: legato or retrig, bend range, LFO retrigger), FX the touch (aftertouch, PW and Metalizer envelope). SELECT
 * steps through every page. ALGO is the mod wheel (MIDI CC 1 too); the keys' pressure is aftertouch. PRESET picks
 * one of 32 patch slots (the first 8 hold factory sounds until saved over), SAVE stores the patch in the slot,
 * OCT- / OCT+ the octave. Every parameter has a MIDI CC (PARAM[].cc), so a controller or a DAW can play them.
 *
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. The engine is compiled apart at -O2 (fm1.toml o2). */
#include "fastmath.h"
#include "mbvoice.h"
#include "mbfx.h"
#include "mb_tapers.h"

#define C_ACCENT RGB(240, 96, 40)       /* the highlight: Minibrute orange */


/* ------------------------------------------------------------- parameters --- */
enum { T_LVL, T_BIP, T_KBD, T_CUT, T_ATT, T_DR, T_LFO, T_PW, T_USR, T_VIB, T_GLIDE, T_DB,
       T_DTIME, T_DTONE, T_RDEC, T_RSIZE, T_RPRE, T_MODE, T_LWAVE, T_VTYPE, T_WDEST,
       T_SUBOCT, T_SUBSHP, T_PLAY, T_BEND, T_ONOFF, T_ARP, T_AMODE, T_AOCT, T_ASTEP, T_ASWING, T_HORD,
       T_LSYNC, T_TEMPO, T_SPAT, T_SLEN, T_DSYNC, T_DMODE, T_FXORD };
static const uint8_t ENUM_N[] = {[T_MODE] = 4, [T_LWAVE] = 6, [T_VTYPE] = 3, [T_WDEST] = 3, [T_SUBOCT] = 2,
                                 [T_SUBSHP] = 2, [T_PLAY] = 2, [T_BEND] = 12, [T_ONOFF] = 2,
                                 [T_ARP] = 3, [T_AMODE] = 4, [T_AOCT] = 4, [T_ASTEP] = 6, [T_ASWING] = 6, [T_HORD] = 2,
                                 [T_LSYNC] = 2, [T_TEMPO] = 231, [T_SPAT] = 8, [T_SLEN] = 64,
                                 [T_DSYNC] = 2, [T_DMODE] = 2, [T_FXORD] = 2};
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
static const char *const LSYNC_NAME[] = {"FREE", "ARP"};
static const char *const DSYNC_NAME[] = {"FREE", "CLOCK"};
static const char *const DMODE_NAME[] = {"STEREO", "PING-PONG"};
static const char *const FXORD_NAME[] = {"DLY>REV", "REV>DLY"};

enum { P_CUT, P_RES, P_BRUTE, P_FENV, P_MODE, P_KBD, P_FVEL, P_AVEL,
       P_SAW, P_SQR, P_TRI, P_SUB, P_NOISE, P_ULTRA, P_PW, P_METAL,
       P_FA, P_FD, P_FS, P_FR, P_AA, P_AD, P_AS, P_AR,
       P_LRATE, P_LWAVE, P_LPITCH, P_LFILT, P_LPWM, P_LAMP, P_VRATE, P_VTYPE,
       P_GLIDE, P_VINT, P_WDEST, P_WAMT, P_SUBOCT, P_SUBSHP, P_USRATE, P_LEVEL,
       P_ATVIB, P_ATCUT, P_PWENV, P_MTENV, P_PLAY, P_BEND, P_LRTRG,
       P_ARP, P_AMODE, P_AOCT, P_ASTEP, P_ASWING,
       P_DTIME, P_DFB, P_DTONE, P_DMIX, P_DSYNC, P_DMODE, P_RDECAY, P_RSIZE, P_RDAMP, P_RMIX, P_RPRE, P_FXORD, NP };
#define NP_08 47u                        /* 0.8's patches: no arpeggiator, no effects (the loader takes them still) */
#define NP_09 52u                        /* 0.8.2 .. 0.9's: no effects */
#define NP_10 63u                        /* 0.10's: no FX ORDER */
#define P_NONE 0xFFu                     /* an empty place on a page */
/* the instrument's settings (not a patch's): the tempo, the Hold order, the LFO's clock, and the sequencer's
 * pattern, length, step and swing; numbered after the patch's parameters. (The arpeggiator's Off/On/Hold, Mode,
 * Octave, Step and Swing are the patch's, as in the plugin.) */
enum { G_TEMPO = NP, G_HORD, G_LSYNC, G_SPAT, G_SLEN, G_SSTEP, G_SSWING, G_END };
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
};

static const param_t GPARAM[NG] = {
    [G_SSTEP - NP] = {"STEP", T_ASTEP, 0},     [G_SSWING - NP] = {"SWING", T_ASWING, 0},
    [G_TEMPO - NP] = {"TEMPO", T_TEMPO, 0},
    [G_HORD - NP] = {"HOLD ORD", T_HORD, 0},   [G_LSYNC - NP] = {"LFO CLK", T_LSYNC, 0},
    [G_SPAT - NP] = {"PATTERN", T_SPAT, 0},    [G_SLEN - NP] = {"LENGTH", T_SLEN, 0},
};
static const param_t *pinfo(uint32_t i) { return i < NP ? &PARAM[i] : &GPARAM[(i - NP) % NG]; }
static int is_enum(uint32_t t) { return t >= T_MODE && t != T_TEMPO && t != T_SLEN; }
static uint32_t pmax(uint32_t i)
{
    uint32_t t = pinfo(i)->type;
    return t == T_TEMPO ? 230u : t == T_SLEN ? 63u : is_enum(t) ? ENUM_N[t] - 1u : 127u;
}

/* the pages: four parameters each, and the button that shows them (a second press: its second page) */
typedef struct {
    const char *title;
    uint8_t btn, p[4];
} page_t;
static const page_t PAGE[] = {
    {"FILTER", B_HOME, {P_CUT, P_RES, P_BRUTE, P_FENV}},     {"FILTER 2", B_HOME, {P_MODE, P_KBD, P_FVEL, P_AVEL}},
    {"OSC", B_SEL, {P_SAW, P_SQR, P_TRI, P_SUB}},            {"OSC 2", B_SEL, {P_NOISE, P_ULTRA, P_PW, P_METAL}},
    {"FILT ENV", B_ENV, {P_FA, P_FD, P_FS, P_FR}},           {"AMP ENV", B_ENV, {P_AA, P_AD, P_AS, P_AR}},
    {"LFO", B_LFO, {P_LRATE, P_LWAVE, P_LPITCH, P_LFILT}},   {"LFO 2", B_LFO, {P_LPWM, P_LAMP, P_VRATE, P_VTYPE}},
    {"PERFORM", B_EDIT, {P_GLIDE, P_VINT, P_WDEST, P_WAMT}}, {"MORE", B_GLO, {P_SUBOCT, P_SUBSHP, P_USRATE, P_LEVEL}},
    {"VOICE", B_GLO, {P_PLAY, P_BEND, P_LRTRG, P_NONE}},
    {"TOUCH", B_FX, {P_ATVIB, P_ATCUT, P_PWENV, P_MTENV}},
    {"DELAY", B_FX, {P_DTIME, P_DFB, P_DTONE, P_DMIX}},      {"REVERB", B_FX, {P_RDECAY, P_RSIZE, P_RDAMP, P_RMIX}},
    {"FX 2", B_FX, {P_DSYNC, P_DMODE, P_RPRE, P_FXORD}},
    {"ARP", B_ARP, {P_AMODE, P_AOCT, P_ASTEP, P_ASWING}},   {"ARP 2", B_ARP, {G_TEMPO, P_ARP, G_HORD, G_LSYNC}},
    {"SEQ", B_SEQ, {G_SPAT, G_SLEN, G_SSTEP, G_SSWING}},
};
#define NPAGE (sizeof PAGE / sizeof PAGE[0])

/* ---------------------------------------------------------------- patches --- */
#define NSLOT 32u
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
};
/* the factory sounds: changes from INIT (parameter, value; 0xFF ends) */
static const struct {
    char name[9];
    uint8_t d[28];
} FACTORY[] = {
    {"INIT", {0xFF}},
    {"BASS", {P_SUB, 100, P_CUT, 35, P_RES, 40, P_BRUTE, 60, P_FENV, 100, P_FD, 17, P_AD, 27, P_AS, 100, P_AR, 2,
              P_KBD, 32, 0xFF}},
    {"ACID", {P_SAW, 0, P_SQR, 127, P_CUT, 30, P_RES, 100, P_FENV, 110, P_FD, 13, P_GLIDE, 27, P_WDEST, 2,
              P_AR, 1, 0xFF}},
    {"METALEAD", {P_SAW, 60, P_TRI, 127, P_METAL, 70, P_CUT, 85, P_RES, 50, P_VINT, 40, P_GLIDE, 15, P_KBD, 50,
                  0xFF}},
    {"ULTRAPAD", {P_ULTRA, 100, P_USRATE, 25, P_CUT, 62, P_RES, 30, P_FENV, 80, P_FA, 52, P_FD, 60, P_FS, 60,
                  P_AA, 59, P_AD, 53, P_AS, 110, P_AR, 53, 0xFF}},
    {"PERC", {P_SAW, 0, P_NOISE, 127, P_MODE, 1, P_CUT, 73, P_RES, 90, P_AD, 10, P_AS, 0, P_AR, 10, P_FENV, 90,
              P_FD, 10, 0xFF}},
    {"BRASS", {P_SQR, 60, P_CUT, 40, P_FENV, 110, P_FA, 16, P_FD, 33, P_FS, 60, P_AA, 5, P_AR, 10, 0xFF}},
    {"WOBBLE", {P_SUB, 90, P_CUT, 35, P_RES, 70, P_LRATE, 51, P_LFILT, 110, P_BRUTE, 50, P_WDEST, 0, 0xFF}},
};
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
    for (i = 0; i + 1u < sizeof FACTORY[0].d && FACTORY[slot].d[i] != 0xFFu; i += 2u)
        p->v[FACTORY[slot].d[i]] = FACTORY[slot].d[i + 1u];
}

#define BANK_MAGIC 0x4D423147u           /* "MB1G": 64 parameters, FX ORDER too ("MB1F": 0.10's 63, "MB1E": 0.8.2 .. 0.9's 52,
                                         * "MB1D": 0.3 .. 0.8's 47, "MB1C": 0.2's 44, "MB1B": 0.1's 16 slots) */
typedef struct {
    uint32_t magic;
    patch_t slot[NSLOT];
} bank_t;
static bank_t bank;

#define SET_MAGIC 0x4D423156u            /* "MB1V": 0.9 ("MB1U": 0.8, "MB1T": 0.3 .. 0.7, "MB1S": 0.1 .. 0.2) */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -2 .. +2 */
    uint8_t slot;                        /* 0 .. 15 */
    uint8_t page;
    uint8_t edited;                      /* the patch differs from its slot */
    patch_t cur;                         /* the patch playing, as it was left */
    uint8_t g[NG];                       /* the instrument's settings (G_*) */
} settings_t;
static settings_t set;
static uint8_t set_dirty;
/* ------------------------------------------------------------ the sequences --- */
/* 8 patterns of up to 64 steps (after the KeyStep's sequencer, as on the MiniBrute 2): each step a note and its
 * velocity, a rest (velocity 0), or a tie (the note before goes on through it) */
#define SEQ_MAGIC 0x4D425351u            /* "MBSQ" */
#define NPAT 8u
#define NSTEP 64u
enum { SF_TIE = 1 };
typedef struct {
    uint8_t len, rsv[3];                 /* 1..64 */
    uint8_t note[NSTEP], vel[NSTEP], flag[NSTEP];
} seqpat_t;
typedef struct {
    uint32_t magic;
    seqpat_t pat[NPAT];
} seqbank_t;
static seqbank_t sq;
static uint8_t sq_dirty;                 /* changed since stored */
static uint32_t sq_changed_ms;

static uint32_t pget(uint32_t i)
{
    if (i == G_SLEN)
        return sq.pat[set.g[G_SPAT - NP] % NPAT].len - 1u;
    return i < NP ? set.cur.v[i] : set.g[(i - NP) % NG];
}
static const uint8_t G_DEF[NG] = {[G_TEMPO - NP] = 90, [G_HORD - NP] = 0, [G_LSYNC - NP] = 0, [G_SPAT - NP] = 0,
                                  [G_SLEN - NP] = 0, [G_SSTEP - NP] = 2, [G_SSWING - NP] = 0};

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
static volatile float arp_bpm = 120.0f;     /* the arpeggiator's tempo now (the knob, a tap or MIDI clock) */
#define LEVEL_OFFSET_DB 9.0f             /* MB-1's output above the plugin's master level */

static void params_from(const uint8_t *v, MBParams *p)
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
    if (set.g[G_LSYNC - NP])                 /* the LFO on the arpeggiator's clock */
        p->lfo_rate_hz = arp_bpm / 60.0f / lsync_beats(v[P_LRATE]);
    p->retrig = v[P_PLAY];
    p->lfo_retrig = v[P_LRTRG];
}

/* ----------------------------------------------------------------- engine --- */
static MBVoice voice;
/* the effects: a block behind the voice, on the second core while the first renders the next block (or after it,
 * on one core); the dry signal goes out at once, so only the effects are a block (5.8 ms) later */
static MBFx fx;
static int16_t fx_mem[MBFX_MEM] PLAT_POOL;
static float fx_in[APP_BLOCK], fx_wl[APP_BLOCK], fx_wr[APP_BLOCK];
static uint32_t fx_n;                        /* fx_in's samples (the last block's voice) */
static MBFxParams fx_now;                    /* as the audio side has them */
static float fx_beats_now;
static volatile uint8_t fx_cores;            /* 2: the last block's effects ran on the second core */
static void fx_job(uint32_t n) { mbfx_process(&fx, fx_in, fx_wl, fx_wr, (int)n); }
static float buf[APP_BLOCK];
_Static_assert(sizeof(MBParams) % 4u == 0, "MBParams: copied a word at a time");
static MBParams par[2];                  /* the main loop writes the one not posted last */
static uint8_t par_ix;
static volatile uint32_t par_posted, par_done;
static MBFxParams fxpar[2];               /* the effects' (posted with par[]) */
static float fxbeats[2];                  /* the delay on the clock: its length in beats (0: free) */

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
    *beats = v[P_DSYNC] ? DSYNC_BEATS[dsync_ix(v[P_DTIME])] : 0.0f;
}

/* the main loop's events for the audio interrupt: one writer (the main loop), one reader (app_render) */
enum { Q_ON, Q_OFF, Q_PAR, Q_WHEEL, Q_AT, Q_ALLOFF, Q_ARP, Q_SEQ };
#define NEV 64u
static volatile uint32_t ev_q[NEV];
static volatile uint32_t ev_w, ev_r;
static void post(uint32_t kind, uint32_t a, uint32_t b)
{
    uint32_t w = ev_w;
    if (w - ev_r >= NEV)
        return;
    ev_q[w % NEV] = kind | a << 8 | b << 16;
    ev_w = w + 1u;
}

static volatile int32_t voice_note = -1;     /* the note sounding, for the screen */
static volatile uint8_t wheel_now, at_now;   /* 0..127, for the screen (from the panel or MIDI) */
static volatile uint8_t cc_req[NP];          /* a parameter MIDI changed: value | 0x80, for the main loop */
static volatile uint32_t midi_count;
static volatile uint8_t bend_range = 2;     /* semitones (the patch's BEND) */

/* --------------------------------------------------------- the arpeggiator --- */
/* After the Minibrute's: ON plays the held keys while they are held; HOLD keeps the last chord (or, HOLD ORD
 * PLAYED, up to 16 notes in the order played) until a key goes down with none held. MODE Up, Down, Up/Down (the
 * end notes repeat), Random over OCTAVE 1..4; STEP 1/4 .. 1/16T; SWING 50..75 % (every second note later); TEMPO
 * 30..260 BPM, or MIDI clock while it comes (TEMPO at its ends: half and double). A note sounds half its step.
 * All of it in the audio interrupt: the render is cut at each note's start and end. */
#define ARP_MAX 16u
static struct {
    uint8_t pool[ARP_MAX], vel[ARP_MAX], n;  /* the notes it plays */
    uint8_t held;                            /* keys down now (HOLD: a key with none down starts a new chord) */
    uint8_t state;                           /* P_ARP as the audio side last saw it */
    uint32_t due;                            /* samples to the next event */
    uint8_t gate;                            /* 1: a note sounds (its end is the next event) */
    int32_t sounding;                        /* that note */
    uint32_t pos, rng;
    uint32_t clk_ticks, clk_last, clk_beat_t, now;   /* MIDI clock: ticks, when the last came, the beat's start */
    float clk_bpm;
    uint32_t beat_acc;
    int32_t last;                            /* the last note it played (the host's checks) */
} arp = {.sounding = -1, .rng = 0x9E3779B9u};
static volatile uint32_t arp_beat;           /* bumped every beat (the ARP light) */

static float arp_tempo_now(void)             /* BPM: MIDI clock while it comes, else TEMPO */
{
    uint32_t t = set.g[G_TEMPO - NP];
    if (arp.clk_bpm > 0.0f && arp.now - arp.clk_last < 22050u)
        return arp.clk_bpm * (t == 0u ? 0.5f : t == 230u ? 2.0f : 1.0f);
    return 30.0f + (float)t;
}

static uint32_t step_len(uint32_t k, uint32_t step, uint32_t swing)   /* step k's length in samples (swing: the
                                                                     * second of a pair later) */
{
    static const float BEATS[6] = {1.0f, 0.5f, 0.25f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f};
    float len = 44100.0f * 60.0f / arp_tempo_now() * BEATS[step % 6u];
    float sw = 0.5f + 0.05f * (float)(swing % 6u);
    len *= 2.0f * ((k & 1u) ? 1.0f - sw : sw);
    return len < 64.0f ? 64u : (uint32_t)len;
}

static void arp_sound(int32_t note, uint32_t vel)
{
    if (arp.sounding >= 0)
        mb_note_off(&voice, (int)arp.sounding);
    arp.sounding = note;
    if (note >= 0) {
        arp.last = note;
        mb_note_on(&voice, (int)note, (int)(vel ? vel : 1u));
        voice_note = (int32_t)voice.note;
    } else {
        voice_note = voice.gate ? (int32_t)voice.note : -1;
    }
}

static void arp_stop(void)
{
    arp_sound(-1, 0);
    arp.n = 0;
    arp.held = 0;
    arp.gate = 0;
    arp.due = 0;
    arp.pos = 0;
}

static void arp_key(uint32_t note, uint32_t vel)   /* a key or a MIDI note into the arpeggiator (vel 0: up) */
{
    uint32_t i, j;
    if (vel) {
        if (arp.state == 2u && arp.held == 0u) {   /* HOLD: a key with none held starts a new chord */
            arp.n = 0;
            arp.pos = 0;
        }
        arp.held++;
        if (arp.n >= ARP_MAX)
            return;
        if (arp.state == 2u && set.g[G_HORD - NP]) {   /* HOLD, PLAYED: a melody, a note as often as it is played */
            i = arp.n;
        } else {                             /* a chord: each note once */
            for (i = 0; i < arp.n && arp.pool[i] != note; i++)
                ;
            if (i < arp.n)
                return;
        }
        if (!set.g[G_HORD - NP])             /* by pitch */
            for (i = 0; i < arp.n && arp.pool[i] < note; i++)
                ;
        for (j = arp.n; j > i; j--) {
            arp.pool[j] = arp.pool[j - 1u];
            arp.vel[j] = arp.vel[j - 1u];
        }
        arp.pool[i] = (uint8_t)note;
        arp.vel[i] = (uint8_t)vel;
        if (arp.n++ == 0u && !arp.gate) {    /* the first note: it starts now */
            arp.due = 0;
            arp.pos = 0;
        }
        return;
    }
    if (arp.held)
        arp.held--;
    if (arp.state == 2u)
        return;
    for (i = 0; i < arp.n && arp.pool[i] != note; i++)
        ;
    if (i == arp.n)
        return;
    for (; i + 1u < arp.n; i++) {
        arp.pool[i] = arp.pool[i + 1u];
        arp.vel[i] = arp.vel[i + 1u];
    }
    if (--arp.n == 0u)
        arp_stop();
}

static void arp_event(void)                  /* the next note's start, or the sounding note's end */
{
    uint32_t len = step_len(arp.pos, set.cur.v[P_ASTEP], set.cur.v[P_ASWING]);
    if (arp.gate) {                          /* the note's end; the rest of the step */
        arp_sound(-1, 0);
        arp.gate = 0;
        arp.due = len - len / 2u;
        arp.pos++;
        return;
    }
    if (arp.n == 0u) {
        arp.due = 0;
        return;
    }
    {
        uint32_t oct = set.cur.v[P_AOCT] % 4u + 1u, L = arp.n * oct, i = arp.pos % (L * 2u), k;
        switch (set.cur.v[P_AMODE] % 4u) {
        case 1: k = L - 1u - arp.pos % L; break;                       /* down */
        case 2: k = i < L ? i : 2u * L - 1u - i; break;              /* up and down: the ends twice */
        case 3: arp.rng = arp.rng * 1664525u + 1013904223u; k = (arp.rng >> 8) % L; break;
        default: k = arp.pos % L; break;                               /* up */
        }
        {
            int32_t note = (int32_t)arp.pool[k % arp.n] + 12 * (int32_t)(k / arp.n);
            arp_sound(note > 127 ? 127 : note, arp.vel[k % arp.n]);
        }
    }
    arp.gate = 1;
    arp.due = len / 2u;
}

/* ------------------------------------------------------------ the sequencer --- */
/* Plays the pattern (sq, G_SPAT) on the arpeggiator's clock (TEMPO, a tap, MIDI clock), at its own STEP and SWING.
 * A step: a note (it sounds half the step, or on into a tie), a rest, or a tie (the note before goes on). While it
 * plays the arpeggiator rests and the keys play over it; REC armed, they are recorded, each to the nearest step
 * (a key held over steps: ties). In the audio interrupt, as the arpeggiator. */
static struct {
    uint8_t run, phase;                      /* phase 1: the step's note ends next */
    uint32_t due, len, el;                   /* samples to the next event; the step's length; samples into it */
    uint32_t pos, cur;                       /* the next step; the step playing */
    int32_t sounding, last;                  /* the note it sounds; the last it played (the host's checks) */
    int32_t rec_note;                        /* REC: the key held, recorded at rec_at */
    uint32_t rec_at;
} seq = {.sounding = -1, .last = -1, .rec_note = -1};
static volatile uint8_t seq_rec;             /* REC armed (the main loop's) */
static volatile uint8_t seq_running;         /* seq.run, for the main loop */
static volatile uint8_t seq_wrote;           /* the audio side recorded a step (the main loop: stored later) */
static volatile uint32_t seq_cur_now;        /* the step playing, for the screen */

static seqpat_t *seq_pat(void) { return &sq.pat[set.g[G_SPAT - NP] % NPAT]; }
static uint32_t seq_len(const seqpat_t *pt) { return pt->len < 1u ? 1u : pt->len > NSTEP ? NSTEP : pt->len; }

static void seq_sound(int32_t note, uint32_t vel)
{
    if (seq.sounding >= 0)
        mb_note_off(&voice, (int)seq.sounding);
    seq.sounding = note;
    if (note >= 0) {
        seq.last = note;
        mb_note_on(&voice, (int)note, (int)(vel ? vel : 1u));
    }
    voice_note = voice.gate ? (int32_t)voice.note : -1;
}

static void seq_transport(uint32_t how)      /* 0 stop, 1 start (from the first step), 2 continue */
{
    if (how == 0u) {
        seq_sound(-1, 0);
        seq.run = 0;
        seq.rec_note = -1;
    } else {
        arp_stop();                          /* (the arpeggiator rests while the sequencer plays) */
        seq_sound(-1, 0);
        if (how == 1u)
            seq.pos = 0;
        seq.run = 1;
        seq.phase = 0;
        seq.due = 0;
    }
    seq_running = seq.run;
}

static void seq_event(void)                  /* a step's start, or its note's end */
{
    seqpat_t *pt = seq_pat();
    uint32_t L = seq_len(pt), c, nx;
    int hold;
    if (seq.phase) {                         /* the note's end; the rest of the step */
        seq_sound(-1, 0);
        seq.phase = 0;
        seq.due = seq.len - seq.len / 2u;
        return;
    }
    if (seq.pos >= L)
        seq.pos = 0;
    c = seq.cur = seq.pos;
    seq_cur_now = c;
    seq.len = step_len(c, set.g[G_SSTEP - NP], set.g[G_SSWING - NP]);
    seq.el = 0;
    if (seq_rec && seq.rec_note >= 0 && c != seq.rec_at) {   /* REC, a key still held: a tie */
        pt->note[c] = (uint8_t)seq.rec_note;
        pt->flag[c] = SF_TIE;
        seq_wrote = 1;
    }
    if ((pt->flag[c] & SF_TIE) && seq.sounding >= 0)
        ;                                    /* a tie: the note goes on */
    else if (pt->vel[c] && !(pt->flag[c] & SF_TIE))
        seq_sound((int32_t)pt->note[c], pt->vel[c]);
    else
        seq_sound(-1, 0);                    /* a rest (or a tie after one) */
    nx = (c + 1u) % L;
    hold = (pt->flag[nx] & SF_TIE) || (seq_rec && seq.rec_note >= 0);
    seq.pos = nx;
    if (seq.sounding >= 0 && !hold) {
        seq.phase = 1;
        seq.due = seq.len / 2u;
    } else {
        seq.due = seq.len;
    }
}

static void seq_record(uint32_t note, uint32_t vel)   /* REC while it plays: a key to the nearest step */
{
    seqpat_t *pt = seq_pat();
    uint32_t L = seq_len(pt), at;
    if (!vel) {
        if ((int32_t)note == seq.rec_note)
            seq.rec_note = -1;
        return;
    }
    at = seq.el < seq.len / 2u ? seq.cur : (seq.cur + 1u) % L;
    pt->note[at] = (uint8_t)note;
    pt->vel[at] = (uint8_t)vel;
    pt->flag[at] = 0;
    seq.rec_note = (int32_t)note;
    seq.rec_at = at;
    seq_wrote = 1;
}

static void note_in(uint32_t note, uint32_t vel)   /* audio interrupt: a key or a MIDI note (vel 0: up) */
{
    if (seq.run && seq_rec)
        seq_record(note, vel);
    if (arp.state != 0u && !seq.run) {
        arp_key(note, vel);
        return;
    }
    if (vel) {
        mb_note_on(&voice, (int)note, (int)vel);
        voice_note = (int32_t)voice.note;
    } else {
        mb_note_off(&voice, (int)note);
        voice_note = voice.gate ? (int32_t)voice.note : -1;
    }
}

static void events(void)                     /* audio interrupt */
{
    while (ev_r != ev_w) {
        uint32_t e = ev_q[ev_r % NEV], a = (e >> 8) & 0xFFu, b = e >> 16;
        switch (e & 0xFFu) {
        case Q_ON:
            note_in(a, b ? b : 1u);
            break;
        case Q_OFF:
            note_in(a, 0);
            break;
        case Q_SEQ:
            seq_transport(a);
            break;
        case Q_ARP:                          /* the arpeggiator switched (a = the new state) */
            arp_stop();
            seq.sounding = -1;
            mb_all_off(&voice);
            voice_note = -1;
            arp.state = (uint8_t)a;
            break;
        case Q_PAR:
            {
                const uint32_t *f = (const uint32_t *)(const void *)&par[a & 1u];
                uint32_t *t = (uint32_t *)(void *)&voice.p, j;
                for (j = 0; j < sizeof voice.p / 4u; j++)
                    t[j] = f[j];
                fx_now = fxpar[a & 1u];
                fx_beats_now = fxbeats[a & 1u];
            }
            par_done = b;
            break;
        case Q_WHEEL:
            mb_set_wheel(&voice, (float)a * (1.0f / 127.0f));
            break;
        case Q_AT:
            mb_set_aftertouch(&voice, (float)a * (1.0f / 127.0f));
            break;
        case Q_ALLOFF:
            arp_stop();
            seq.sounding = -1;
            mb_all_off(&voice);
            voice_note = -1;
            break;
        }
        ev_r = ev_r + 1u;
    }
}

static int param_of_cc(uint32_t cc)
{
    uint32_t i;
    for (i = 0; i < NP; i++)
        if (PARAM[i].cc == cc)
            return (int)i;
    return -1;
}

static void midi_in(void)                    /* audio interrupt */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        int p;
        if (st == 0xF8u) {                   /* MIDI clock: 24 a beat; the tempo from each beat's length */
            if (arp.clk_ticks % 24u == 0u) {
                uint32_t d = arp.now - arp.clk_beat_t;
                if (arp.clk_ticks && d > 2000u && d < 200000u)
                    arp.clk_bpm = arp.clk_bpm > 0.0f && arp.now - arp.clk_last < 22050u
                                      ? arp.clk_bpm + 0.5f * (2646000.0f / (float)d - arp.clk_bpm)
                                      : 2646000.0f / (float)d;
                arp.clk_beat_t = arp.now;
            }
            arp.clk_ticks++;
            arp.clk_last = arp.now;
            continue;
        }
        if (st == 0xFAu) {                   /* start: the pattern from its first note */
            arp.clk_ticks = 0;
            arp.pos = 0;
            if (arp.n && !arp.gate)
                arp.due = 0;
            if (arp.state == 0u)             /* (the arpeggiator off: the sequencer starts) */
                seq_transport(1);
            continue;
        }
        if (st == 0xFBu) {                   /* continue */
            arp.clk_ticks = 0;
            if (arp.state == 0u)
                seq_transport(2);
            continue;
        }
        if (st == 0xFCu) {                   /* stop */
            seq_transport(0);
            continue;
        }
        if (st >= 0xF8u)
            continue;
        midi_count++;
        switch (st & 0xF0u) {
        case 0x90u:
        case 0x80u:
            note_in(d1, (st & 0xF0u) == 0x90u ? d2 : 0u);   /* (a note on at velocity 0 is an off) */
            break;
        case 0xB0u:
            if (d1 == 1u) {
                mb_set_wheel(&voice, (float)d2 * (1.0f / 127.0f));
                wheel_now = (uint8_t)d2;
            } else if (d1 == 120u || d1 == 123u) {
                arp_stop();
                mb_all_off(&voice);
                voice_note = -1;
            } else if ((p = param_of_cc(d1)) >= 0) {
                cc_req[p] = (uint8_t)(0x80u | d2);
            }
            break;
        case 0xA0u:                          /* poly pressure: taken as the channel's */
            mb_set_aftertouch(&voice, (float)d2 * (1.0f / 127.0f));
            at_now = (uint8_t)d2;
            break;
        case 0xD0u:
            mb_set_aftertouch(&voice, (float)d1 * (1.0f / 127.0f));
            at_now = (uint8_t)d1;
            break;
        case 0xE0u:                          /* +-2 semitones */
            mb_set_bend(&voice, (float)((int32_t)(d1 | d2 << 7) - 8192) * (float)bend_range * (1.0f / 8192.0f));
            break;
        default:
            break;
        }
    }
}

/* the scope: the output, the last 512 samples (the main loop reads them) */
#define SCOPE_N 512u
static int16_t scope_cap[SCOPE_N];
static volatile uint32_t scope_w;

static inline float ceiling(float y)          /* a soft ceiling above -2 dB (loud resonant BP/HP), not a hard clip */
{
    float a = fm_fabsf(y);
    if (a > 0.8f)
        y = (y > 0.0f ? 1.0f : -1.0f) * (0.8f + 0.2f * fm_tanhf((a - 0.8f) * 5.0f));
    return y;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, w = scope_w;
    float g = (float)master_q12 * (1.0f / 4096.0f) * 8388000.0f;
    if (n > APP_BLOCK)
        n = APP_BLOCK;
    uint32_t fn = fx_n;
    int fx_run = 0, on2 = 0;
    float dry;
    events();
    midi_in();
    {   /* the last block's effects: handed to the second core (or done after the voice) */
        MBFxParams q = fx_now;
        if (fx_beats_now > 0.0f)
            q.d_time_s = fx_beats_now * 60.0f / arp_tempo_now();
        dry = mbfx_dry_gain(&q);
        int quiet = fx.alive < 1e-5f;        /* (the effects silent, and the voice too: nothing to do) */
        for (i = 0; quiet && i < fn; i++)
            quiet = fm_fabsf(fx_in[i]) < 1e-6f;
        if (quiet)
            fx.d_on = 0;                     /* (idle: the delay starts afresh when it is next heard) */
        if (fn && !quiet && (q.d_mix > 0.001f || q.r_mix > 0.001f || fx.d_on || fx.r_run)) {
            mbfx_set(&fx, &q);
            for (i = 0; i < fn; i++)
                fx_wl[i] = fx_wr[i] = 0.0f;
            fx_run = 1;
            on2 = plat_cpu2_run(fx_job, fn);
        }
    }
    {   /* the voice, cut where the arpeggiator's notes start and end */
        uint32_t off = 0, guard = 0;
        while (off < n && guard++ < 64u) {
            uint32_t m = n - off;
            int ar = arp.state != 0u && arp.n && !seq.run;
            if (seq.run) {
                if (seq.due == 0u)
                    seq_event();
                if (seq.due < m)
                    m = seq.due;
            }
            if (ar) {
                if (arp.due == 0u)
                    arp_event();
                if (arp.due < m)
                    m = arp.due;
            }
            if (m) {
                mb_render(&voice, buf + off, (int)m);
                off += m;
                if (seq.run) {
                    seq.due -= m;
                    seq.el += m;
                }
                if (ar)
                    arp.due -= m;
            }
        }
        if (off < n)
            mb_render(&voice, buf + off, (int)(n - off));
        if (arp.gate && arp.n == 0u)         /* (released mid-note: the note ends) */
            arp_stop();
        arp.now += n;
        {   /* the beat, for the ARP light (and the tempo the LFO follows) */
            float bpm = arp_tempo_now();
            uint32_t beat = (uint32_t)(2646000.0f / bpm);
            arp_bpm = bpm;
            arp.beat_acc += n;
            if (arp.beat_acc >= beat) {
                arp.beat_acc -= beat;
                arp_beat++;
            }
        }
    }
    if (fx_run) {
        if (!on2 || plat_cpu2_wait() < 0)
            fx_job(fn);
        fx_cores = on2 ? 2u : 1u;
    } else {
        fn = 0;
    }
    for (i = 0; i < n; i++) {
        float y = buf[i], yl = dry * y, yr = yl;
        if (i < fn) {
            yl += fx_wl[i];
            yr += fx_wr[i];
        }
        yl = ceiling(yl);
        yr = ceiling(yr);
        lr[2u * i] = (int32_t)(yl * g);
        lr[2u * i + 1u] = (int32_t)(yr * g);
        scope_cap[(w + i) % SCOPE_N] = (int16_t)(0.5f * (yl + yr) * 32767.0f);
        fx_in[i] = y;                        /* (the next block's effects take this one) */
    }
    fx_n = n;
    scope_w = w + n;
    if (!voice.gate && voice.ast == 0)
        voice_note = -1;
}

void app_panic(void)
{
    mb_all_off(&voice);
    voice.aenv = 0.0f;
    voice.ast = 0;
}

/* --------------------------------------------------------------- the patch --- */
static void params_post(void)                /* set.cur to the engine */
{
    uint32_t gen = par_posted + 1u;
    par_ix ^= 1u;
    params_from(set.cur.v, &par[par_ix]);
    fx_params_from(set.cur.v, &fxpar[par_ix], &fxbeats[par_ix]);
    bend_range = (uint8_t)(set.cur.v[P_BEND] + 1u);
    par_posted = gen;
    post(Q_PAR, par_ix, gen & 0xFFFFu);
}
static uint8_t params_stale;                 /* changed since the last post (posted once a frame, when the last is in) */

static void slot_load(uint32_t s)
{
    set.slot = (uint8_t)(s % NSLOT);
    set.cur = bank.slot[set.slot];
    set.edited = 0;
    set_dirty = 1;
    params_stale = 1;
}

static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.page = 0;
    set.slot = 0;
    set.cur = bank.slot[0];
    set.edited = 0;
    for (uint32_t i = 0; i < NG; i++)
        set.g[i] = G_DEF[i];
}

static uint8_t arp_posted;                   /* the arpeggiator's state the audio side was told */

static int bank_migrate(void)                /* an older bank (47 or 52 parameters a patch): the rest as INIT */
{
    static const struct { uint32_t magic, np; } OLD[] = {{0x4D423144u, NP_08}, {0x4D423145u, NP_09}, {0x4D423146u, NP_10}};
    uint32_t k, i, j, size = (uint32_t)plat_store_size(OBJ_BANK);
    for (k = 0; k < sizeof OLD / sizeof OLD[0]; k++) {
        uint32_t so = 8u + OLD[k].np;
        if (size != 4u + NSLOT * so || size > sizeof bank ||
            plat_store_load(OBJ_BANK, &bank, sizeof bank) != (int)size || bank.magic != OLD[k].magic)
            continue;
        for (i = NSLOT; i-- > 0u;) {         /* (from the last: each new patch is longer than its old one) */
            uint8_t o[8u + NP];
            const uint8_t *src = (const uint8_t *)bank.slot + i * so;
            for (j = 0; j < so; j++)
                o[j] = src[j];
            for (j = 0; j < 8u; j++)
                bank.slot[i].name[j] = (char)o[j];
            for (j = 0; j < NP; j++)
                bank.slot[i].v[j] = j < OLD[k].np ? o[8u + j] : INIT_V[j];
        }
        bank.magic = BANK_MAGIC;
        return 1;
    }
    return 0;
}

void app_init(void)
{
    uint32_t i, j;
    if ((plat_store_load(OBJ_BANK, &bank, sizeof bank) != (int)sizeof bank || bank.magic != BANK_MAGIC) &&
        !bank_migrate()) {
        bank.magic = BANK_MAGIC;
        for (i = 0; i < NSLOT; i++)
            patch_factory(&bank.slot[i], i);
    }
    for (i = 0; i < NSLOT; i++)
        for (j = 0; j < NP; j++)
            if (bank.slot[i].v[j] > pmax(j))
                bank.slot[i].v[j] = (uint8_t)pmax(j);
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC ||
        set.octave < -2 || set.octave > 2 || set.slot >= NSLOT || set.page >= NPAGE)
        settings_defaults();
    for (j = 0; j < NP; j++)
        if (set.cur.v[j] > pmax(j))
            set.cur.v[j] = (uint8_t)pmax(j);
    for (j = 0; j < NG; j++)
        if (set.g[j] > pmax(NP + j))
            set.g[j] = G_DEF[j];
    arp.state = arp_posted = set.cur.v[P_ARP];   /* (as the patch has it) */
    if (plat_store_load(OBJ_SEQ, &sq, sizeof sq) != (int)sizeof sq || sq.magic != SEQ_MAGIC) {
        sq.magic = SEQ_MAGIC;                /* empty patterns of 16 steps */
        for (i = 0; i < NPAT; i++) {
            sq.pat[i].len = 16;
            for (j = 0; j < NSTEP; j++) {
                sq.pat[i].note[j] = 60;
                sq.pat[i].vel[j] = 0;
                sq.pat[i].flag[j] = 0;
            }
        }
    }
    for (i = 0; i < NPAT; i++)
        sq.pat[i].len = (uint8_t)seq_len(&sq.pat[i]);
    params_from(set.cur.v, &voice.p);
    mb_init(&voice, 44100.0f);
    mbfx_init(&fx, 44100.0f, fx_mem);
    (void)plat_cpu2_set(1);                  /* the second core for the effects (here only: at power-on) */
    params_post();
    events();                                /* (no audio yet: the patch in place before the first render) */
}

/* ------------------------------------------------------------ the loader --- */
/* patches over USB SysEx: F0 7D 4D 42 CMD ... F7 (7D: non-commercial; "MB")
 *   01 SLOT NAME[8] V[NP]   a patch into a slot (SLOT 0..31; NAME ASCII; V: each parameter 0..127, PARAM's order:
 *                           64; 0.8's 47, 0.9's 52 and 0.10's 63 taken, the rest as INIT);
 *                           kept in RAM until 02. The slot playing: played at once
 *   02                      the slots stored in the flash
 *   03                      the factory slots back (stored)
 *   04 SLOT                 a slot played (as PRESET)
 * A frame's answer: F0 7D 4D 42 7F CMD STATUS F7 (STATUS 0 done, 1 refused) */
static uint8_t bank_loaded;                  /* slots came in over SysEx since the last store */
static void name_text(const patch_t *p, char *b);
static void sysex_answer(uint32_t cmd, uint32_t st)
{
    uint8_t a[8] = {0xF0, 0x7D, 0x4D, 0x42, 0x7F, (uint8_t)cmd, (uint8_t)st, 0xF7};
    (void)plat_sysex_send(a, sizeof a);
}
static int bank_store(void)
{
    int r = plat_store_save(OBJ_BANK, &bank, sizeof bank);
    if (r == 0)
        r = plat_store_save(OBJ_SETTINGS, &set, sizeof set);   /* (the slot playing, too) */
    if (r == 0) {
        bank_loaded = 0;
        set_dirty = 0;
    }
    return r;
}
int app_sysex(const uint8_t *p, uint32_t n)
{
    uint32_t cmd, i;
    char t[16];
    if (n < 4u || p[0] != 0x7Du || p[1] != 0x4Du || p[2] != 0x42u)
        return 0;
    cmd = p[3];
    if (cmd == 1u && (n == 13u + NP || n == 13u + NP_08 || n == 13u + NP_09 || n == 13u + NP_10) && p[4] < NSLOT) {
        patch_t *d = &bank.slot[p[4]];
        uint32_t nv = n - 13u;               /* (an older loader's 47 or 52: the rest as INIT) */
        for (i = 0; i < 8u; i++)
            d->name[i] = p[5u + i] >= 32u && p[5u + i] < 127u ? (char)p[5u + i] : ' ';
        for (i = 0; i < NP; i++)
            d->v[i] = i >= nv ? INIT_V[i] : (uint8_t)(p[13u + i] > pmax(i) ? pmax(i) : p[13u + i]);
        bank_loaded = 1;
        if (p[4] == set.slot)
            slot_load(set.slot);
        sysex_answer(cmd, 0);
    } else if (cmd == 2u && n == 4u) {
        i = (uint32_t)bank_store();
        app_say(i ? "STORE" : "PATCHES", i ? "FAILED" : "STORED");
        sysex_answer(cmd, i ? 1u : 0u);
    } else if (cmd == 3u && n == 4u) {
        for (i = 0; i < NSLOT; i++)
            patch_factory(&bank.slot[i], i);
        i = (uint32_t)bank_store();
        slot_load(set.slot);
        app_say("FACTORY", i ? "FAILED" : "PATCHES");
        sysex_answer(cmd, i ? 1u : 0u);
    } else if (cmd == 4u && n == 5u && p[4] < NSLOT) {
        slot_load(p[4]);
        sysex_answer(cmd, 0);
    } else {
        sysex_answer(cmd, 1);
        return 1;
    }
    if (cmd == 1u || cmd == 4u) {
        name_text(&set.cur, t);
        app_say("LOADED", t);
    }
    return 1;
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint8_t vel[NKEYS];
    uint8_t wheel, at;                       /* the panel's: ALGO, the keys' pressure */
    uint32_t turned_at[4];                   /* when K1..K4 last moved: the cell lit a moment */
    uint32_t last_scope;
    uint8_t redraw;
} ui;

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

static void value_text(uint32_t i, uint32_t x, char *b)
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
        if (set.g[G_LSYNC - NP])
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
        if (set.cur.v[P_DSYNC])
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
    }
}

static void name_text(const patch_t *p, char *b)   /* the slot and the name: "03 ACID*" */
{
    uint32_t i, n = 0;
    b[n++] = (char)('0' + (set.slot + 1u) / 10u);
    b[n++] = (char)('0' + (set.slot + 1u) % 10u);
    b[n++] = ' ';
    for (i = 0; i < 8u && p->name[i] != ' '; i++)
        b[n++] = p->name[i];
    if (set.edited)
        b[n++] = '*';
    b[n] = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void speak(void)                      /* a screen reader that starts listening hears the patch and the page */
{
    char t[96], n[16];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, APP_NAME ". ");
        name_text(&set.cur, n);
        text_cat(t, n);
        text_cat(t, ". ");
        text_cat(t, PAGE[set.page].title);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

static void param_set(uint32_t i, int32_t v)
{
    v = clamp(v, 0, (int32_t)pmax(i));
    if ((uint32_t)v == pget(i))
        return;
    if (i == G_SLEN) {                       /* the pattern's length (kept with the sequences) */
        sq.pat[set.g[G_SPAT - NP] % NPAT].len = (uint8_t)(v + 1);
        sq_dirty = 1;
        sq_changed_ms = plat_ms();
        ui.redraw = 1;
        return;
    }
    if (i >= NP) {                           /* the instrument's settings */
        set.g[i - NP] = (uint8_t)v;
        set_dirty = 1;
        if (i == G_LSYNC || (i == G_TEMPO && set.g[G_LSYNC - NP]))
            params_stale = 1;
        ui.redraw = 1;
        return;
    }
    set.cur.v[i] = (uint8_t)v;
    set.edited = 1;
    set_dirty = 1;
    params_stale = 1;
    ui.redraw = 1;
}

static void page_show(uint32_t pg)
{
    set.page = (uint8_t)(pg % NPAGE);
    set_dirty = 1;
    app_say(PAGE[set.page].title, "");
}

static void page_button(uint32_t b)          /* its page; pressed again, its other page */
{
    uint32_t i, first = NPAGE;
    for (i = 0; i < NPAGE; i++)
        if (PAGE[i].btn == b) {
            if (first == NPAGE)
                first = i;
            if (i == set.page) {            /* on one of its pages: the next of its own */
                uint32_t j = i + 1u;
                page_show(j < NPAGE && PAGE[j].btn == b ? j : first);
                return;
            }
        }
    if (first < NPAGE)
        page_show(first);
}

static void save(void)
{
    char t[16];
    bank.slot[set.slot] = set.cur;
    set.edited = 0;
    if (plat_store_save(OBJ_BANK, &bank, sizeof bank) == 0 && plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0 &&
        (!sq_dirty || plat_store_save(OBJ_SEQ, &sq, sizeof sq) == 0)) {
        set_dirty = 0;
        sq_dirty = 0;
        name_text(&set.cur, t);
        app_say("SAVED", t);
    } else {
        app_say("SAVE", "FAILED");
    }
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -2, 2);
    char t[4];
    if (o == set.octave)
        return;
    post(Q_ALLOFF, 0, 0);                    /* a key held across the change would not find its note off */
    set.octave = (int8_t)o;
    set_dirty = 1;
    t[0] = o < 0 ? '-' : '+';
    t[1] = (char)('0' + (o < 0 ? -o : o));
    t[2] = 0;
    app_say("OCTAVE", t);
}

static void tap_tempo(void)                 /* SEQ held, PLAY tapped: the tempo from the last taps */
{
    static uint32_t last, avg;
    uint32_t now = plat_ms(), d = now - last;
    char t[12];
    last = now;
    if (d < 230u || d > 2000u) {             /* (30..260 BPM; a long gap starts again) */
        avg = 0;
        app_say("TAP", "");
        return;
    }
    avg = avg ? (avg * 2u + d) / 3u : d;
    param_set(G_TEMPO, clamp((int32_t)(60000u / avg) - 30, 0, 230));
    value_text(G_TEMPO, set.g[G_TEMPO - NP], t);
    app_say("TEMPO", t);
}

static uint32_t arp_down_ms;
static uint8_t arp_long;

/* the sequencer from the panel: PLAY starts and stops it, REC arms recording (stopped: step entry, a key a step,
 * FX a rest, FX with a key held a tie; playing: in time). A pattern recorded step by step is as long as its steps. */
static uint32_t seq_edit;                    /* step entry: the next step */
static uint8_t seq_entered;                  /* steps entered since REC */
static int32_t seq_key_note = -1;            /* step entry: the last key's note (FX with it held: a tie) */
static void seq_say_step(const char *what, uint32_t i)
{
    char t[12];
    int_text((int32_t)i + 1, t);
    app_say(what, t);
}

static void seq_step_write(uint32_t note, uint32_t vel, uint32_t flag)   /* step entry: one step, the next */
{
    seqpat_t *pt = seq_pat();
    uint32_t e = seq_edit % NSTEP;
    pt->note[e] = (uint8_t)note;
    pt->vel[e] = (uint8_t)vel;
    pt->flag[e] = (uint8_t)flag;
    if (seq_entered < NSTEP)
        seq_entered++;
    pt->len = seq_entered;
    seq_edit = (e + 1u) % NSTEP;
    sq_dirty = 1;
    sq_changed_ms = plat_ms();
    seq_say_step(flag & SF_TIE ? "TIE" : vel ? "STEP" : "REST", e);
    ui.redraw = 1;
}

static void seq_store(void)
{
    if (plat_store_save(OBJ_SEQ, &sq, sizeof sq) == 0)
        sq_dirty = 0;
}

static void rec_button(void)
{
    seq_rec = (uint8_t)!seq_rec;
    if (seq_rec) {
        if (!seq_running) {                  /* step entry from the first step */
            seq_edit = 0;
            seq_entered = 0;
            seq_key_note = -1;
            app_say("STEP REC", "STEP 1");
        } else {
            app_say("REC", "ON");
        }
        if (PAGE[set.page].btn != B_SEQ)
            page_button(B_SEQ);
    } else {
        app_say("REC", "OFF");
        if (sq_dirty && !seq_running)
            seq_store();
    }
}

static void play_button(uint32_t btn)
{
    if (btn & 1u << B_SEQ) {                 /* SEQ held: a tap of the tempo */
        tap_tempo();
        return;
    }
    if (seq_running) {
        post(Q_SEQ, 0, 0);
        seq_running = 0;
        app_say("SEQ", "STOP");
    } else {
        post(Q_SEQ, 1, 0);
        seq_running = 1;
        seq_say_step("PATTERN", set.g[G_SPAT - NP]);
    }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, at = 0;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
            v = touch_vel(k);
            ui.vel[k] = (uint8_t)v;
            if (seq_rec && !seq_running) {   /* step entry: the key's note */
                seq_step_write((uint32_t)note, v ? v : 1u, 0);
                seq_key_note = note;
            }
            post(Q_ON, (uint32_t)note, v);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB | TO_BLE);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            post(Q_OFF, (uint32_t)note, 0);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB | TO_BLE);
        }
        if ((keys & bit) && touch_pressure(k) > at)
            at = touch_pressure(k);
    }
    if (at != ui.at) {                       /* the keys' pressure: aftertouch */
        ui.at = (uint8_t)at;
        at_now = (uint8_t)at;
        post(Q_AT, at, 0);
        plat_midi_send(0x0Du | 0xD0u << 8 | at << 16, TO_USB | TO_BLE);
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        if (k == B_OCTDN)
            octave_step(-1);
        else if (k == B_OCTUP)
            octave_step(1);
        else if (k == B_SAVE)
            save();
        else if (k == B_ARP) {               /* (on release: a long press is its pages) */
            arp_down_ms = plat_ms();
            arp_long = 0;
        } else if (k == B_PLAY)
            play_button(btn);
        else if (k == B_REC)
            rec_button();
        else if (k == B_FX && seq_rec && !seq_running)   /* step entry: a rest, or with a key held a tie */
            seq_step_write(keys && seq_key_note >= 0 ? (uint32_t)seq_key_note : 60u, 0,
                           keys && seq_key_note >= 0 ? SF_TIE : 0u);
        else
            page_button(k);
    }
    if (seq_wrote) {                         /* recorded in time (the audio side) */
        seq_wrote = 0;
        sq_dirty = 1;
        sq_changed_ms = plat_ms();
    }
    if (sq_dirty && !seq_rec && !seq_running && plat_ms() - sq_changed_ms > 2000u)   /* (LENGTH and the rest) */
        seq_store();
    if ((btn & 1u << B_ARP) && !arp_long && plat_ms() - arp_down_ms >= 500u) {   /* ARP held: ARP / ARP 2 */
        arp_long = 1;
        page_button(B_ARP);
    }
    if (!(btn & 1u << B_ARP) && (ui.btn & 1u << B_ARP) && !arp_long) {   /* ARP pressed: OFF -> ON -> HOLD -> OFF */
        char t[8];
        uint32_t v = (set.cur.v[P_ARP] + 1u) % 3u;
        param_set(P_ARP, (int32_t)v);
        plat_midi_send(0x0Bu | 0xB0u << 8 | (uint32_t)PARAM[P_ARP].cc << 16 | (v * 127u / 2u) << 24, TO_USB | TO_BLE);
        if (PAGE[set.page].btn != B_ARP)
            page_button(B_ARP);
        t[0] = 0;
        text_cat(t, ARP_NAME[v]);
        app_say("ARP", t);
    }
    if (set.cur.v[P_ARP] != arp_posted) {    /* the arpeggiator switched (ARP, a patch, a CC) */
        arp_posted = set.cur.v[P_ARP];
        post(Q_ARP, arp_posted, 0);
    }
    for (k = 0; k < NE; k++) {
        char t[16];
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.redraw = 1;
        if (k == EN_SELECT) {
            page_show((set.page + NPAGE + (uint32_t)(e > 0 ? 1 : NPAGE - 1)) % NPAGE);
        } else if (k == EN_ALGO) {           /* the mod wheel, 4 a detent */
            int32_t w = clamp((int32_t)ui.wheel + 4 * e, 0, 127);
            ui.wheel = (uint8_t)w;
            wheel_now = (uint8_t)w;
            post(Q_WHEEL, (uint32_t)w, 0);
            plat_midi_send(0x0Bu | 0xB0u << 8 | 1u << 16 | (uint32_t)w << 24, TO_USB | TO_BLE);
            int_text(w, t);
            app_say("MOD WHEEL", t);
        } else if (k == EN_PRESET) {
            slot_load((set.slot + NSLOT + (uint32_t)(e > 0 ? 1 : NSLOT - 1)) % NSLOT);
            name_text(&set.cur, t);
            app_say("PATCH", t);
        } else {                             /* K1..K4: the page's parameters; a fast turn moves further */
            uint32_t q = k - EN_K1, p = PAGE[set.page].p[q];
            int32_t step;
            if (p == P_NONE)
                continue;
            step = is_enum(pinfo(p)->type) ? (e > 0 ? 1 : -1) : (e > 1 || e < -1 ? 3 * e : e);
            param_set(p, (int32_t)pget(p) + step);
            ui.turned_at[q] = plat_ms();
            if (ann_on()) {
                value_text(p, pget(p), t);
                app_say(pinfo(p)->name, t);
            }
            if (p < NP)                      /* (the instrument's settings have no CC) */
                plat_midi_send(0x0Bu | 0xB0u << 8 | (uint32_t)PARAM[p].cc << 16 |
                                   (is_enum(PARAM[p].type) ? set.cur.v[p] * 127u / pmax(p) : set.cur.v[p]) << 24,
                               TO_USB | TO_BLE);
        }
    }
    for (k = 0; k < NP; k++) {               /* MIDI CCs (from the audio interrupt) */
        uint32_t r = cc_req[k];
        if (r & 0x80u) {
            cc_req[k] = 0;
            r &= 0x7Fu;
            param_set(k, (int32_t)(is_enum(PARAM[k].type) ? (r * (pmax(k) + 1u)) / 128u : r));
        }
    }
    {   /* the LFO on the arpeggiator's clock follows its tempo (MIDI clock too) */
        static float bpm_posted;
        float b = arp_bpm;
        if (set.g[G_LSYNC - NP] && (b > bpm_posted * 1.005f || b < bpm_posted * 0.995f)) {
            bpm_posted = b;
            params_stale = 1;
        }
    }
    if (params_stale && par_done == (par_posted & 0xFFFFu)) {   /* the last one taken: the next */
        params_stale = 0;
        params_post();
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    plat_glow(1);
    {
        static uint32_t beat_seen, beat_ms;
        uint32_t lit = btn | (set_dirty || set.edited ? 1u << B_SAVE : 0u) | 1u << PAGE[set.page].btn;
        if (arp_beat != beat_seen) {
            beat_seen = arp_beat;
            beat_ms = plat_ms();
        }
        int dark = plat_ms() - beat_ms < 70u;
        if (set.cur.v[P_ARP])               /* the arpeggiator on: ARP lit, dark a moment on each beat */
            lit = dark ? lit & ~(1u << B_ARP) : lit | 1u << B_ARP;
        if (seq_running)                     /* the sequencer: PLAY the same */
            lit = dark ? lit & ~(1u << B_PLAY) : lit | 1u << B_PLAY;
        if (seq_rec)
            lit |= 1u << B_REC;
        if (sq_dirty)
            lit |= 1u << B_SAVE;
        plat_leds(lit, keys);
    }
}

void app_input_only(void) { input(); }

static void bar(int32_t x, int32_t y, int32_t w, uint32_t v, uint32_t max, int bipolar, uint16_t c)
{
    int32_t f = (int32_t)(v * (uint32_t)w / (max ? max : 1u));
    cv_rect(x, y, w, 4, C_LINE);
    if (bipolar) {
        int32_t m = w / 2;
        if (f >= m)
            cv_rect(x + m, y, f - m + 1, 4, c);
        else
            cv_rect(x + f, y, m - f, 4, c);
    } else if (f) {
        cv_rect(x, y, f, 4, c);
    }
}

static uint32_t cpu_peak;

static void draw(void)
{
    char t[24];
    uint32_t i, now = plat_ms();
    int32_t note = voice_note;
    const page_t *pg = &PAGE[set.page];
    /* the header: the patch, the note; or a message for a moment */
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_ACCENT);
    } else {
        name_text(&set.cur, t);
        cv_text(8, 6, &FONT_B, t, C_WHITE);
        if (note >= 0) {
            static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            char o[8];
            t[0] = 0;
            text_cat(t, N[note % 12]);
            int_text(note / 12 - 1, o);
            text_cat(t, o);
            cv_text(232 - text_w(&FONT_B, t), 6, &FONT_B, t, C_ACCENT);
        }
    }
    cv_blit(0, 0);
    /* the page: its title, and its four parameters (K1..K4), lit a moment after a turn */
    cv_begin(240, 136, C_BLACK);
    cv_text(8, 0, &FONT_XS, pg->title, C_ACCENT);
    for (i = 0; i < NPAGE; i++)              /* where the page is among them */
        cv_rect(112 + (int32_t)i * 7, 4, 5, 4, i == set.page ? C_ACCENT : C_LINE);
    if (seq_running || seq_rec) {            /* the sequencer: the step playing (or the next to enter) */
        const seqpat_t *pt = seq_pat();
        char o[8];
        t[0] = 0;
        text_cat(t, seq_rec ? "REC " : "SEQ ");
        int_text((int32_t)(seq_running ? seq_cur_now : seq_edit) + 1, o);
        text_cat(t, o);
        text_cat(t, "/");
        int_text((int32_t)seq_len(pt), o);
        text_cat(t, o);
        cv_text(58, 0, &FONT_XS, t, seq_rec ? C_ACCENT : C_HI);
    }
    for (i = 0; i < 4u; i++) {
        uint32_t p = pg->p[i], v;
        int32_t x = 4 + (int32_t)(i % 2u) * 118, y = 18 + (int32_t)(i / 2u) * 58;
        int lit = ui.turned_at[i] && now - ui.turned_at[i] < 400u;
        char k[4] = "K1";
        if (p == P_NONE)
            continue;
        v = pget(p);
        k[1] = (char)('1' + i);
        cv_rect(x, y, 114, 54, lit ? C_LINE : C_BLACK);
        cv_text(x + 4, y + 3, &FONT_XS, k, C_DIM);
        cv_text(x + 24, y + 3, &FONT_XS, pinfo(p)->name, C_GRAY);
        value_text(p, v, t);
        cv_text(x + 4, y + 18, &FONT_B, t, lit ? C_WHITE : C_HI);
        bar(x + 4, y + 44, 106, v, pmax(p), pinfo(p)->type == T_BIP, C_ACCENT);
    }
    cv_blit(0, 36);
    /* the foot: the wheel, the pressure, the octave, and the scope */
    cv_begin(240, 64, C_BLACK);
    cv_text(4, 2, &FONT_XS, "WHEEL", C_GRAY);
    bar(48, 6, 60, wheel_now, 127u, 0, C_HI);
    cv_text(118, 2, &FONT_XS, "AT", C_GRAY);
    bar(136, 6, 40, at_now, 127u, 0, C_HI);
    cv_text(186, 2, &FONT_XS, "OCT", C_GRAY);
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
    cv_text(216, 2, &FONT_XS, t, C_HI);
    {                                        /* the audio load: now, the most since boot, and the blocks missed */
        uint32_t c = plat_cpu_pct();
        if (c > cpu_peak)
            cpu_peak = c;
        cv_text(4, 16, &FONT_XS, "CPU", C_GRAY);
        int_text((int32_t)c, t);
        text_cat(t, "%");
        cv_text(34, 16, &FONT_XS, t, C_HI);
        cv_text(80, 16, &FONT_XS, "PEAK", C_GRAY);
        int_text((int32_t)cpu_peak, t);
        text_cat(t, "%");
        cv_text(114, 16, &FONT_XS, t, C_HI);
        cv_text(160, 16, &FONT_XS, "XRUN", C_GRAY);
        int_text((int32_t)plat_xruns(), t);
        cv_text(196, 16, &FONT_XS, t, plat_xruns() ? C_ACCENT : C_HI);
        if (fx_cores)                        /* where the effects run: 2C the second core, 1C the first */
            cv_text(222, 16, &FONT_XS, fx_cores == 2u ? "2C" : "1C", C_DIM);
    }
    {
        uint32_t w = scope_w, j;
        int32_t prev = 0;
        for (j = 0; j < 232u; j++) {
            int32_t s = scope_cap[(w - 464u + 2u * j) % SCOPE_N], y = 47 - s * 15 / 32768;
            if (j)
                cv_line(3 + (int32_t)j, prev, 4 + (int32_t)j, y, C_DIM);
            prev = y;
        }
    }
    cv_blit(0, 172);
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t lit_until, last_midi;
    uint32_t i, now = plat_ms();
    input();
    speak();
    for (i = 0; i < 4u; i++)
        if (ui.turned_at[i] && now - ui.turned_at[i] < 450u && now > lit_until)
            lit_until = ui.turned_at[i] + 400u;
    if (lit_until && now >= lit_until) {
        lit_until = 0;
        ui.redraw = 1;
    }
    {
        static uint32_t step_seen;
        if (seq_running && seq_cur_now != step_seen) {
            step_seen = seq_cur_now;
            ui.redraw = 1;
        }
    }
    if (midi_count != last_midi) {
        last_midi = midi_count;
        ui.redraw = 1;
    }
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (now - ui.last_scope >= (voice_note >= 0 ? 50u : 500u)) {   /* the scope moves while a note sounds; the load */
        ui.last_scope = now;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "note"))
        *got = voice_note;
    else if (!strcmp(what, "slot"))
        *got = set.slot;
    else if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "edited"))
        *got = set.edited;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "wheel"))
        *got = wheel_now;
    else if (!strcmp(what, "at"))
        *got = at_now;
    else if (!strcmp(what, "arp"))
        *got = set.cur.v[P_ARP];
    else if (!strcmp(what, "arpstate"))     /* as the audio side has it */
        *got = arp.state;
    else if (!strcmp(what, "fxcores"))
        *got = fx_cores;
    else if (!strcmp(what, "fxpeak")) {    /* the last block's effects, the louder side, x1000 */
        float m = 0.0f;
        for (uint32_t i = 0; i < APP_BLOCK; i++) {
            float a = fm_fabsf(fx_wl[i]) > fm_fabsf(fx_wr[i]) ? fm_fabsf(fx_wl[i]) : fm_fabsf(fx_wr[i]);
            if (a > m)
                m = a;
        }
        *got = (int)(m * 1000.0f);
    }
    else if (!strcmp(what, "seqrun"))
        *got = seq.run;
    else if (!strcmp(what, "seqrec"))
        *got = seq_rec;
    else if (!strcmp(what, "seqlast"))      /* the last note the sequencer played */
        *got = seq.last;
    else if (!strcmp(what, "seqlen"))
        *got = (int)seq_len(seq_pat());
    else if (!strcmp(what, "seqdirty"))
        *got = sq_dirty;
    else if (!strcmp(what, "seqnotes") || !strcmp(what, "seqties")) {   /* the pattern's notes, its ties */
        const seqpat_t *pt = seq_pat();
        uint32_t i, c = 0;
        for (i = 0; i < seq_len(pt); i++)
            c += what[3] == 'n' ? (pt->vel[i] && !(pt->flag[i] & SF_TIE)) : (pt->flag[i] & SF_TIE) != 0;
        *got = (int)c;
    }
    else if (what[0] == 's' && (what[1] == 'n' || what[1] == 'v' || what[1] == 't') && what[2] >= '0' && what[2] <= '9') {
        const seqpat_t *pt = seq_pat();      /* sn5 / sv5 / st5: step 5's note, velocity, tie (from 1) */
        uint32_t i = ((uint32_t)atoi(what + 2) + NSTEP - 1u) % NSTEP;
        *got = what[1] == 'n' ? pt->note[i] : what[1] == 'v' ? pt->vel[i] : (pt->flag[i] & SF_TIE) != 0;
    }
    else if (!strcmp(what, "arplast"))      /* the last note the arpeggiator played */
        *got = arp.last;
    else if (!strcmp(what, "arpn"))         /* the notes in the arpeggiator */
        *got = arp.n;
    else if (!strcmp(what, "tempo"))
        *got = (int)(arp_bpm + 0.5f);
    else if (!strcmp(what, "posted"))       /* the engine has the patch as edited */
        *got = !params_stale && par_done == (par_posted & 0xFFFFu);
    else if (what[0] == 'p' && what[1] >= '0' && what[1] <= '9')
        *got = set.cur.v[atoi(what + 1) % NP];
    else
        return 0;
    return 1;
}
#endif
