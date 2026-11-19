/* SPDX-License-Identifier: MIT
 * MB-1 for the M-VAVE FM-1 (an fm1-base app; started from fm1-base's template, MPL-2.0, and laid out after
 * sq80-fm1's app): one monophonic Minibrute-style voice (mbvoice/, measured against the MiniBrute V plugin).
 *
 * The panel: K1..K4 turn the four parameters of the page on screen. The page buttons pick a page, a second press
 * its second page: HOME the filter (FILTER, FILTER 2), SEL the oscillators (OSC, OSC 2), ENV the envelopes
 * (FILT ENV, AMP ENV), LFO the LFO and vibrato, EDIT the performance controls (glide, vintage, the wheel), GLO the
 * rest (sub octave and shape, Ultrasaw rate, level), FX the touch (aftertouch, PW and Metalizer envelope). SELECT
 * steps through every page. ALGO is the mod wheel (MIDI CC 1 too); the keys' pressure is aftertouch. PRESET picks
 * one of 32 patch slots (the first 8 hold factory sounds until saved over), SAVE stores the patch in the slot,
 * OCT- / OCT+ the octave. Every parameter has a MIDI CC (PARAM[].cc), so a controller or a DAW can play them.
 *
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. The engine is compiled apart at -O2 (fm1.toml o2). */
#include "fastmath.h"
#include "mbvoice.h"
#include "mb_tapers.h"

#define C_ACCENT RGB(240, 96, 40)       /* the highlight: Minibrute orange */

/* ------------------------------------------------------------- parameters --- */
enum { T_LVL, T_BIP, T_KBD, T_CUT, T_ATT, T_DR, T_LFO, T_PW, T_USR, T_VIB, T_GLIDE, T_DB, T_MODE, T_LWAVE, T_VTYPE, T_WDEST,
       T_SUBOCT, T_SUBSHP };
static const uint8_t ENUM_N[] = {[T_MODE] = 4, [T_LWAVE] = 6, [T_VTYPE] = 3, [T_WDEST] = 3, [T_SUBOCT] = 2,
                                 [T_SUBSHP] = 2};
static const char *const MODE_NAME[] = {"LP", "BP", "HP", "NOTCH"};
static const char *const LWAVE_NAME[] = {"SINE", "TRI", "SAW", "SQUARE", "S&H", "SMOOTH"};
static const char *const VTYPE_NAME[] = {"-SQR", "SINE", "+SQR"};
static const char *const WDEST_NAME[] = {"LFO AMT", "VIBRATO", "CUTOFF"};
static const char *const SUBOCT_NAME[] = {"-1 OCT", "-2 OCT"};
static const char *const SUBSHP_NAME[] = {"SQUARE", "SINE"};

enum { P_CUT, P_RES, P_BRUTE, P_FENV, P_MODE, P_KBD, P_FVEL, P_AVEL,
       P_SAW, P_SQR, P_TRI, P_SUB, P_NOISE, P_ULTRA, P_PW, P_METAL,
       P_FA, P_FD, P_FS, P_FR, P_AA, P_AD, P_AS, P_AR,
       P_LRATE, P_LWAVE, P_LPITCH, P_LFILT, P_LPWM, P_LAMP, P_VRATE, P_VTYPE,
       P_GLIDE, P_VINT, P_WDEST, P_WAMT, P_SUBOCT, P_SUBSHP, P_USRATE, P_LEVEL,
       P_ATVIB, P_ATCUT, P_PWENV, P_MTENV, NP };

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
};

static int is_enum(uint32_t t) { return t >= T_MODE; }
static uint32_t pmax(uint32_t i) { return is_enum(PARAM[i].type) ? ENUM_N[PARAM[i].type] - 1u : 127u; }

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
    {"TOUCH", B_FX, {P_ATVIB, P_ATCUT, P_PWENV, P_MTENV}},
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

#define BANK_MAGIC 0x4D423143u           /* "MB1C": 32 slots ("MB1B": 0.1's 16) */
typedef struct {
    uint32_t magic;
    patch_t slot[NSLOT];
} bank_t;
static bank_t bank;

#define SET_MAGIC 0x4D423153u            /* "MB1S" */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -2 .. +2 */
    uint8_t slot;                        /* 0 .. 15 */
    uint8_t page;
    uint8_t edited;                      /* the patch differs from its slot */
    patch_t cur;                         /* the patch playing, as it was left */
} settings_t;
static settings_t set;
static uint8_t set_dirty;

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
}

/* ----------------------------------------------------------------- engine --- */
static MBVoice voice;
static float buf[APP_BLOCK];
_Static_assert(sizeof(MBParams) % 4u == 0, "MBParams: copied a word at a time");
static MBParams par[2];                  /* the main loop writes the one not posted last */
static uint8_t par_ix;
static volatile uint32_t par_posted, par_done;

/* the main loop's events for the audio interrupt: one writer (the main loop), one reader (app_render) */
enum { Q_ON, Q_OFF, Q_PAR, Q_WHEEL, Q_AT, Q_ALLOFF };
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

static void events(void)                     /* audio interrupt */
{
    while (ev_r != ev_w) {
        uint32_t e = ev_q[ev_r % NEV], a = (e >> 8) & 0xFFu, b = e >> 16;
        switch (e & 0xFFu) {
        case Q_ON:
            mb_note_on(&voice, (int)a, (int)(b ? b : 1u));
            voice_note = (int32_t)voice.note;
            break;
        case Q_OFF:
            mb_note_off(&voice, (int)a);
            voice_note = voice.gate ? (int32_t)voice.note : -1;
            break;
        case Q_PAR:
            {
                const uint32_t *f = (const uint32_t *)(const void *)&par[a & 1u];
                uint32_t *t = (uint32_t *)(void *)&voice.p, j;
                for (j = 0; j < sizeof voice.p / 4u; j++)
                    t[j] = f[j];
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
        if (st >= 0xF8u)
            continue;
        midi_count++;
        switch (st & 0xF0u) {
        case 0x90u:
        case 0x80u:
            if ((st & 0xF0u) == 0x90u && d2) {
                mb_note_on(&voice, (int)d1, (int)d2);
                voice_note = (int32_t)voice.note;
                break;
            }
            mb_note_off(&voice, (int)d1);         /* (a note on at velocity 0 too) */
            voice_note = voice.gate ? (int32_t)voice.note : -1;
            break;
        case 0xB0u:
            if (d1 == 1u) {
                mb_set_wheel(&voice, (float)d2 * (1.0f / 127.0f));
                wheel_now = (uint8_t)d2;
            } else if (d1 == 120u || d1 == 123u) {
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
            mb_set_bend(&voice, (float)((int32_t)(d1 | d2 << 7) - 8192) * (2.0f / 8192.0f));
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

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i, w = scope_w;
    float g = (float)master_q12 * (1.0f / 4096.0f) * 8388000.0f;
    if (n > APP_BLOCK)
        n = APP_BLOCK;
    events();
    midi_in();
    mb_render(&voice, buf, (int)n);
    for (i = 0; i < n; i++) {
        float y = fm_clampf(buf[i], -1.0f, 1.0f);
        int32_t s = (int32_t)(y * g);
        lr[2u * i] = s;
        lr[2u * i + 1u] = s;
        scope_cap[(w + i) % SCOPE_N] = (int16_t)(y * 32767.0f);
    }
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
}

void app_init(void)
{
    uint32_t i, j;
    if (plat_store_load(OBJ_BANK, &bank, sizeof bank) != (int)sizeof bank || bank.magic != BANK_MAGIC) {
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
    params_from(set.cur.v, &voice.p);
    mb_init(&voice, 44100.0f);
    params_post();
    events();                                /* (no audio yet: the patch in place before the first render) */
}

/* ------------------------------------------------------------ the loader --- */
/* patches over USB SysEx: F0 7D 4D 42 CMD ... F7 (7D: non-commercial; "MB")
 *   01 SLOT NAME[8] V[NP]   a patch into a slot (SLOT 0..31; NAME ASCII; V: each parameter 0..127, PARAM's order);
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
    if (cmd == 1u && n == 5u + 8u + NP && p[4] < NSLOT) {
        patch_t *d = &bank.slot[p[4]];
        for (i = 0; i < 8u; i++)
            d->name[i] = p[5u + i] >= 32u && p[5u + i] < 127u ? (char)p[5u + i] : ' ';
        for (i = 0; i < NP; i++)
            d->v[i] = (uint8_t)(p[13u + i] > pmax(i) ? pmax(i) : p[13u + i]);
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
    switch (PARAM[i].type) {
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
        hz_text(lfo_hz(x), b);
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
    if ((uint32_t)v == set.cur.v[i])
        return;
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
    if (plat_store_save(OBJ_BANK, &bank, sizeof bank) == 0 && plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
        set_dirty = 0;
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
        else
            page_button(k);
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
            int32_t step = is_enum(PARAM[p].type) ? (e > 0 ? 1 : -1) : (e > 1 || e < -1 ? 3 * e : e);
            param_set(p, (int32_t)set.cur.v[p] + step);
            ui.turned_at[q] = plat_ms();
            if (ann_on()) {
                value_text(p, set.cur.v[p], t);
                app_say(PARAM[p].name, t);
            }
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
        uint32_t lit = btn | (set_dirty || set.edited ? 1u << B_SAVE : 0u) | 1u << PAGE[set.page].btn;
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
        cv_rect(132 + (int32_t)i * 9, 4, 7, 4, i == set.page ? C_ACCENT : C_LINE);
    for (i = 0; i < 4u; i++) {
        uint32_t p = pg->p[i], v = set.cur.v[p];
        int32_t x = 4 + (int32_t)(i % 2u) * 118, y = 18 + (int32_t)(i / 2u) * 58;
        int lit = ui.turned_at[i] && now - ui.turned_at[i] < 400u;
        char k[4] = "K1";
        k[1] = (char)('1' + i);
        cv_rect(x, y, 114, 54, lit ? C_LINE : C_BLACK);
        cv_text(x + 4, y + 3, &FONT_XS, k, C_DIM);
        cv_text(x + 24, y + 3, &FONT_XS, PARAM[p].name, C_GRAY);
        value_text(p, v, t);
        cv_text(x + 4, y + 18, &FONT_B, t, lit ? C_WHITE : C_HI);
        bar(x + 4, y + 44, 106, v, pmax(p), PARAM[p].type == T_BIP, C_ACCENT);
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
    else if (!strcmp(what, "posted"))       /* the engine has the patch as edited */
        *got = !params_stale && par_done == (par_posted & 0xFFFFu);
    else if (what[0] == 'p' && what[1] >= '0' && what[1] <= '9')
        *got = set.cur.v[atoi(what + 1) % NP];
    else
        return 0;
    return 1;
}
#endif
