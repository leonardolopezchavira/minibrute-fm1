/* SPDX-License-Identifier: MIT
 * MB-1 for the M-VAVE FM-1 (an fm1-base app; started from fm1-base's template, MPL-2.0, and laid out after
 * sq80-fm1's app): one monophonic Minibrute-style voice (mbvoice/, measured against the MiniBrute V plugin).
 *
 * The panel: K1..K4 turn the four parameters of the page on screen. Each page button is a section of the patch
 * editor's sheet, its pages that section's columns (PAGE[] below): HOME the filter, SEL the oscillators (a page a
 * waveform), ENV the envelopes and velocity, LFO the LFO and vibrato, EDIT playing (legato, glide, bend, vintage; the
 * wheel and aftertouch), GLO the instrument's own settings, FX the effects in their order. A press: its first page, then
 * the next; held half a second, back to its first. SELECT
 * steps through every page. ALGO is the mod wheel (MIDI CC 1 too); the keys' pressure is aftertouch. PRESET picks
 * one of 128 patch slots, four banks of 32, A01 .. D32 (the first 8 hold factory sounds until saved over), SAVE stores the patch in the slot,
 * OCT- / OCT+ the octave. Every parameter has a MIDI CC (PARAM[].cc), so a controller or a DAW can play them.
 *
 * Part of the unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in
 * scope. The engine is compiled apart at -O2 (fm1.toml o2). */
#include "fastmath.h"
#include "mbvoice.h"
#include "mbfx.h"
#include "mb_tapers.h"

#define C_ACCENT RGB(240, 96, 40)       /* the highlight: Minibrute orange */
#if defined(BOARD_SMK37) && BOARD_SMK37
#define APP_SMK 1                       /* the M-Vave SMK-37 (fm1-base BOARD=smk37): its keys' own velocity, the
                                         * wheels, the pedal and the outer keys (smk_input); the rest as on the FM-1 */
#else
#define APP_SMK 0
#endif


#include "mbpatch.h"

/* the pages: four parameters each, and the button that shows them (a second press: its second page) */
typedef struct {
    const char *title;
    uint8_t btn, p[4];
} page_t;
/* each button a section of the patch sheet (app/editor), each page one of its columns or groups; GLO the
 * instrument's own settings (not the patch's) */
static const page_t PAGE[] = {
    {"FILTER", B_HOME, {P_CUT, P_RES, P_BRUTE, P_LEVEL}},     {"FILTER 2", B_HOME, {P_MODE, P_FENV, P_KBD, P_NONE}},
    {"SAW", B_SEL, {P_SAW, P_ULTRA, P_USRATE, P_NONE}},       {"SQUARE", B_SEL, {P_SQR, P_PW, P_PWENV, P_NONE}},
    {"TRI", B_SEL, {P_TRI, P_METAL, P_MTENV, P_NONE}},        {"SUB", B_SEL, {P_SUB, P_SUBSHP, P_SUBOCT, P_NOISE}},
    {"FILT ENV", B_ENV, {P_FA, P_FD, P_FS, P_FR}},            {"AMP ENV", B_ENV, {P_AA, P_AD, P_AS, P_AR}},
    {"VELOCITY", B_ENV, {P_FVEL, P_AVEL, P_NONE, P_NONE}},
    {"LFO", B_LFO, {P_LRATE, P_LWAVE, P_LRTRG, P_NONE}},      {"LFO AMT", B_LFO, {P_LPITCH, P_LFILT, P_LPWM, P_LAMP}},
    {"VIBRATO", B_LFO, {P_VRATE, P_VTYPE, P_NONE, P_NONE}},
    {"PLAY", B_EDIT, {P_PLAY, P_GLIDE, P_BEND, P_VINT}},      {"CONTROLS", B_EDIT, {P_WDEST, P_WAMT, P_ATVIB, P_ATCUT}},
    {"GLOBAL", B_GLO, {G_TEMPO, G_HORD, G_LSYNC, P_NONE}},
    {"DIST", B_FX, {P_XTYPE, P_XDRV, P_XTONE, P_XMIX}},       {"CHORUS", B_FX, {P_CRATE, P_CDEPTH, P_CPHASE, P_CMIX}},
    {"DELAY", B_FX, {P_DTIME, P_DFB, P_DTONE, P_DMIX}},       {"DELAY 2", B_FX, {P_DSYNC, P_DMODE, P_NONE, P_NONE}},
    {"REVERB", B_FX, {P_RDECAY, P_RSIZE, P_RDAMP, P_RMIX}},   {"REVERB 2", B_FX, {P_RPRE, P_FXORD, P_NONE, P_NONE}},
    {"COMP", B_FX, {P_KTHR, P_KRAT, P_KATT, P_KREL}},
    {"ARP", B_ARP, {P_AMODE, P_AOCT, P_ASTEP, P_ASWING}},    {"ARP 2", B_ARP, {P_ARP, G_TEMPO, G_HORD, P_NONE}},
    {"SEQ", B_SEQ, {G_SPAT, G_SLEN, G_SSTEP, G_SSWING}},
#if APP_SMK
    {"PIANO ROLL", B_SEQ, {G_SPAT, G_SLEN, G_SSTEP, G_SSWING}},   /* (SEQ again: the pattern drawn; the pads its steps) */
#endif
};
#define NPAGE (sizeof PAGE / sizeof PAGE[0])
#define PG_ROLL (APP_SMK ? NPAGE - 1u : NPAGE)
#if APP_SMK
static int outer_held(void);
#define OUTER_HELD() outer_held()
#else
#define OUTER_HELD() 0
#endif   /* the SMK-37's piano roll (none on the FM-1) */

/* ---------------------------------------------------------------- patches --- */
#define NBANK 4u                         /* A..D */
#define BANK_N 32u                       /* slots a bank (a store object each: 4 + 32 x 84 B, under PLAT_STORE_MAX) */
#define NSLOT (NBANK * BANK_N)
#define BANK_MAGIC 0x4D423148u           /* "MB1H": 76 parameters, distortion, chorus, compressor ("MB1G": 0.10.4's 64,
                                         * "MB1F": 0.10's 63, "MB1E": 0.8.2 .. 0.9's 52,
                                         * "MB1D": 0.3 .. 0.8's 47, "MB1C": 0.2's 44, "MB1B": 0.1's 16 slots) */
typedef struct {
    uint32_t magic;
    patch_t slot[BANK_N];
} bank_t;
static bank_t banks[NBANK] PLAT_POOL;    /* (in the pool: main RAM is nearly full) */
static const uint32_t BANK_OBJ[NBANK] = {OBJ_BANK, OBJ_BANK_B, OBJ_BANK_C, OBJ_BANK_D};
#define SLOT(s) (banks[(s) / BANK_N].slot[(s) % BANK_N])
static uint8_t bank_dirty;               /* bit k: bank k changed since stored */
static int banks_save(void)              /* the banks that changed: 0 = stored */
{
    uint32_t k;
    for (k = 0; k < NBANK; k++)
        if (bank_dirty & (1u << k)) {
            if (plat_store_save(BANK_OBJ[k], &banks[k], sizeof banks[k]) != 0)
                return -1;
            bank_dirty &= (uint8_t)~(1u << k);
        }
    return 0;
}

#define SET_MAGIC 0x4D423156u            /* "MB1V": 0.9 ("MB1U": 0.8, "MB1T": 0.3 .. 0.7, "MB1S": 0.1 .. 0.2) */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -3 .. +3 */
    uint8_t slot;                        /* 0 .. 127: A01 .. D32 */
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

static volatile float arp_bpm = 120.0f;     /* the arpeggiator's tempo now (the knob, a tap or MIDI clock) */
static void params_from(const uint8_t *v, MBParams *p)
{
    mb_params_core(v, p);
    if (set.g[G_LSYNC - NP])                 /* the LFO on the arpeggiator's clock */
        p->lfo_rate_hz = arp_bpm / 60.0f / lsync_beats(v[P_LRATE]);
}

/* ----------------------------------------------------------------- engine --- */
static MBVoice voice;
/* the effects: a block behind the voice, on the second core while the first renders the next block (or after it,
 * on one core); the dry signal goes out at once, so only the effects are a block (2.9 ms) later */
static MBFx fx;
static int16_t fx_mem[MBFX_MEM] PLAT_POOL;
static float out_peak;
static float fx_in[APP_BLOCK], fx_wl[APP_BLOCK], fx_wr[APP_BLOCK], pre_l[APP_BLOCK], pre_r[APP_BLOCK];
static uint32_t fx_n;                        /* fx_in's samples (the last block's voice) */
static uint32_t fx_silent_n;                 /* samples the voice has been silent for */
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

/* the main loop's events for the audio interrupt: one writer (the main loop), one reader (app_render) */
enum { Q_ON, Q_OFF, Q_PAR, Q_WHEEL, Q_AT, Q_ALLOFF, Q_ARP, Q_SEQ, Q_BEND, Q_SUS };
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
static volatile int16_t bend_now;            /* the panel's bend (the SMK-37's wheel), -8192..8191 */
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

/* The keys transpose the pattern while it plays, as on the MicroBrute: a key sets the shift from the pattern's first
 * note (pressed, that note plays it as recorded) and it stays until the next key, or the next start. With REC armed
 * the keys record as before. */
static volatile int32_t seq_xpose;           /* semitones */
static uint32_t seq_root(void)               /* the pattern's first note (C4 when it has none) */
{
    const seqpat_t *pt = seq_pat();
    uint32_t i, L = seq_len(pt);
    for (i = 0; i < L; i++)
        if (pt->vel[i] && !(pt->flag[i] & SF_TIE))
            return pt->note[i];
    return 60u;
}

static void seq_sound(int32_t note, uint32_t vel)
{
    if (note >= 0) {                         /* (the transposition) */
        note += seq_xpose;
        note = note < 0 ? 0 : note > 127 ? 127 : note;
    }
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
        if (how == 1u) {
            seq.pos = 0;
            seq_xpose = 0;                   /* (a start: as recorded) */
        }
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

/* the sustain pedal (CC 64, and the SMK-37's jack): while it is down a key let go keeps sounding, as if held; let go,
 * the notes no longer held are released (the voice's note stack keeps last-note priority) */
static uint8_t sus_on;
static uint32_t sus_held[4];                 /* notes let go while the pedal was down: bit per MIDI note */
static void note_play(uint32_t note, uint32_t vel);
static void note_in(uint32_t note, uint32_t vel)   /* audio interrupt: a key or a MIDI note (vel 0: up) */
{
    note &= 127u;
    if (vel)
        sus_held[note >> 5] &= ~(1u << (note & 31u));
    else if (sus_on) {
        sus_held[note >> 5] |= 1u << (note & 31u);
        return;
    }
    note_play(note, vel);
}

static void sustain(uint32_t on)             /* audio interrupt */
{
    uint32_t n;
    sus_on = (uint8_t)(on != 0u);
    if (on)
        return;
    for (n = 0; n < 128u; n++)
        if (sus_held[n >> 5] & (1u << (n & 31u))) {
            sus_held[n >> 5] &= ~(1u << (n & 31u));
            note_play(n, 0);
        }
}

static void note_play(uint32_t note, uint32_t vel)
{
    if (seq.run && !seq_rec) {               /* the sequencer playing: a key transposes it (and plays nothing) */
        if (vel)
            seq_xpose = (int32_t)note - (int32_t)seq_root();
        return;
    }
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
        case Q_BEND:                         /* b: the 14-bit bend */
            bend_now = (int16_t)((int32_t)b - 8192);
            mb_set_bend(&voice, (float)((int32_t)b - 8192) * (float)bend_range * (1.0f / 8192.0f));
            break;
        case Q_SUS:
            sustain(a);
            break;
        case Q_ALLOFF:
            sus_on = 0;
            sus_held[0] = sus_held[1] = sus_held[2] = sus_held[3] = 0;
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
            } else if (d1 == 64u) {
                sustain(d2 >= 64u);
            } else if (d1 == 120u || d1 == 123u) {
                sus_on = 0;
                sus_held[0] = sus_held[1] = sus_held[2] = sus_held[3] = 0;
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
    MBFxParams q = fx_now;
    events();
    midi_in();
    {   /* the last block's effects: handed to the second core (or done after the voice) */
        if (fx_beats_now > 0.0f)
            q.d_time_s = fx_beats_now * 60.0f / arp_tempo_now();
        dry = mbfx_dry_gain(&q);
        /* idle: the voice silent for longer than the delay's time (its first repeat still to come), and the effects
         * silent too: nothing to do */
        int silent = 1, quiet;
        for (i = 0; silent && i < fn; i++)
            silent = fm_fabsf(fx_in[i]) < 1e-6f;
        fx_silent_n = silent ? fx_silent_n + fn : 0u;
        quiet = silent && fx.alive < 1e-5f && (float)fx_silent_n > (q.d_mix > 0.001f ? q.d_time_s * 44100.0f : 0.0f) + 8820.0f;
        if (quiet)
            fx.d_on = 0;                     /* (the delay starts afresh when it is next heard) */
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
    /* the distortion and chorus here, on this core (no block late); the delay's and reverb's wet (a block late, as a
     * send) added; the compressor last, on all of it */
    mbfx_pre(&fx, &q, buf, pre_l, pre_r, (int)n);
    for (i = 0; i < n; i++) {
        float yl = dry * pre_l[i], yr = dry * pre_r[i];
        if (i < fn) {
            yl += fx_wl[i];
            yr += fx_wr[i];
        }
        fx_in[i] = 0.5f * (pre_l[i] + pre_r[i]);   /* (the next block's delay and reverb take this one) */
        pre_l[i] = yl;
        pre_r[i] = yr;
    }
    mbfx_comp(&fx, &q, pre_l, pre_r, (int)n);
    out_peak = 0.0f;
    for (i = 0; i < n; i++) {
        float yl = ceiling(pre_l[i]), yr = ceiling(pre_r[i]);
        if (fm_fabsf(yl) > out_peak)
            out_peak = fm_fabsf(yl);
        lr[2u * i] = (int32_t)(yl * g);
        lr[2u * i + 1u] = (int32_t)(yr * g);
        scope_cap[(w + i) % SCOPE_N] = (int16_t)(0.5f * (yl + yr) * 32767.0f);
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
    set.cur = SLOT(set.slot);
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
    set.cur = SLOT(0);
    set.edited = 0;
    for (uint32_t i = 0; i < NG; i++)
        set.g[i] = G_DEF[i];
}

static uint8_t arp_posted;                   /* the arpeggiator's state the audio side was told */

static int bank_migrate(void)                /* an older bank A (47 .. 64 parameters a patch): the rest as INIT */
{
    static const struct { uint32_t magic, np; } OLD[] = {{0x4D423144u, NP_08}, {0x4D423145u, NP_09}, {0x4D423146u, NP_10}, {0x4D423147u, NP_104}};
    uint32_t k, i, j, size = (uint32_t)plat_store_size(OBJ_BANK);
    for (k = 0; k < sizeof OLD / sizeof OLD[0]; k++) {
        uint32_t so = 8u + OLD[k].np;
        if (size != 4u + BANK_N * so || size > sizeof banks[0] ||
            plat_store_load(OBJ_BANK, &banks[0], sizeof banks[0]) != (int)size || banks[0].magic != OLD[k].magic)
            continue;
        for (i = BANK_N; i-- > 0u;) {         /* (from the last: each new patch is longer than its old one) */
            uint8_t o[8u + NP];
            const uint8_t *src = (const uint8_t *)banks[0].slot + i * so;
            for (j = 0; j < so; j++)
                o[j] = src[j];
            for (j = 0; j < 8u; j++)
                banks[0].slot[i].name[j] = (char)o[j];
            for (j = 0; j < NP; j++)
                banks[0].slot[i].v[j] = j < OLD[k].np ? o[8u + j] : INIT_V[j];
        }
        banks[0].magic = BANK_MAGIC;
        return 1;
    }
    return 0;
}

static void store_load(void)                 /* the banks, the settings, the patterns from the flash */
{
    uint32_t i, j;
    for (i = 0; i < NBANK; i++) {            /* (bank A may be an older one; B .. D new in 0.12: INIT) */
        bank_t *b = &banks[i];
        if ((plat_store_load(BANK_OBJ[i], b, sizeof *b) != (int)sizeof *b || b->magic != BANK_MAGIC) &&
            !(i == 0u && bank_migrate())) {
            b->magic = BANK_MAGIC;
            for (j = 0; j < BANK_N; j++)
                patch_factory(&b->slot[j], i * BANK_N + j);
        }
    }
    for (i = 0; i < NSLOT; i++)
        for (j = 0; j < NP; j++)
            if (SLOT(i).v[j] > pmax(j))
                SLOT(i).v[j] = (uint8_t)pmax(j);
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC ||
        set.octave < -3 || set.octave > 3 || set.slot >= NSLOT || set.page >= NPAGE)
        settings_defaults();
    for (j = 0; j < NP; j++)
        if (set.cur.v[j] > pmax(j))
            set.cur.v[j] = (uint8_t)pmax(j);
    for (j = 0; j < NG; j++)
        if (set.g[j] > pmax(NP + j))
            set.g[j] = G_DEF[j];
    set.slot = 0;                            /* power-on: A01, as stored (an unsaved edit is not kept) */
    set.cur = SLOT(0);
    set.edited = 0;
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
}

void app_init(void)
{
    store_load();
    arp.state = arp_posted = set.cur.v[P_ARP];   /* (as the patch has it) */
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
 *                           76; 0.8's 47, 0.9's 52, 0.10's 63 and 0.10.4's 64 taken, the rest as INIT);
 *                           kept in RAM until 02. The slot playing: played at once
 *   02                      the slots stored in the flash
 *   03                      bank A's factory slots back (stored; banks B .. D stay as they are)
 *   04 SLOT                 a slot played (as PRESET)
 *   05 SLOT                 a slot's patch sent back: F0 7D 4D 42 05 SLOT NAME[8] V[NP] F7 (01's layout)
 *   09 PAT                  a sequencer pattern (0..7) sent back: F0 7D 4D 42 09 PAT LEN NOTE[64] VEL[64] TIE[64] F7
 *   0A PAT LEN NOTE[64] VEL[64] TIE[64]   a pattern in (stored); VEL 0: a rest, TIE 1: a tie (0.20)
 *   06                      what is there: F0 7D 4D 42 06 NP NBANK SLOT EDITED F7 (banks of 32; the slot playing,
 *                           1: edited)
 *   08                      the sound playing sent back, as it is now (edits not saved included): F0 7D 4D 42 08 SLOT
 *                           NAME[8] V[NP] F7 (SLOT: the one it came from)
 *   07 NAME[8] V[NP]        a patch played, into no slot (as edited: SAVE keeps it); the editor's
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
    int r = banks_save();
    if (r == 0)
        r = plat_store_save(OBJ_SETTINGS, &set, sizeof set);   /* (the slot playing, too) */
    if (r == 0) {
        bank_loaded = 0;
        set_dirty = 0;
    }
    return r;
}
static void seq_store(void);
int app_sysex(const uint8_t *p, uint32_t n)
{
    uint32_t cmd, i;
    char t[16];
    if (n < 4u || p[0] != 0x7Du || p[1] != 0x4Du || p[2] != 0x42u)
        return 0;
    cmd = p[3];
    if (cmd == 1u && (n == 13u + NP || n == 13u + NP_08 || n == 13u + NP_09 || n == 13u + NP_10 || n == 13u + NP_104) && p[4] < NSLOT) {
        patch_t *d = &SLOT(p[4]);
        bank_dirty |= (uint8_t)(1u << (p[4] / BANK_N));
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
        for (i = 0; i < BANK_N; i++)         /* (bank A, where the factory sounds are: B .. D stay as they are) */
            patch_factory(&SLOT(i), i);
        bank_dirty |= 1u;
        i = (uint32_t)bank_store();
        slot_load(set.slot);
        app_say("FACTORY", i ? "FAILED" : "PATCHES");
        sysex_answer(cmd, i ? 1u : 0u);
    } else if (cmd == 4u && n == 5u && p[4] < NSLOT) {
        slot_load(p[4]);
        sysex_answer(cmd, 0);
    } else if ((cmd == 5u && n == 5u && p[4] < NSLOT) || (cmd == 8u && n == 4u)) {
        const patch_t *d = cmd == 8u ? &set.cur : &SLOT(p[4]);
        uint8_t f[15u + NP] = {0xF0, 0x7D, 0x4D, 0x42, (uint8_t)cmd};
        f[5] = cmd == 8u ? set.slot : p[4];
        for (i = 0; i < 8u; i++)
            f[6u + i] = (uint8_t)d->name[i] & 0x7Fu;
        for (i = 0; i < NP; i++)
            f[14u + i] = d->v[i] & 0x7Fu;
        f[14u + NP] = 0xF7;
        (void)plat_sysex_send(f, sizeof f);
        return 1;
    } else if (cmd == 6u && n == 4u) {
        uint8_t f[10] = {0xF0, 0x7D, 0x4D, 0x42, 6, (uint8_t)NP, (uint8_t)NBANK, set.slot, set.edited ? 1u : 0u, 0xF7};
        (void)plat_sysex_send(f, sizeof f);
        return 1;
    } else if (cmd == 7u && n == 12u + NP) {
        for (i = 0; i < 8u; i++)
            set.cur.name[i] = p[4u + i] >= 32u && p[4u + i] < 127u ? (char)p[4u + i] : ' ';
        for (i = 0; i < NP; i++)
            set.cur.v[i] = (uint8_t)(p[12u + i] > pmax(i) ? pmax(i) : p[12u + i]);
        set.edited = 1;
        set_dirty = 1;
        params_stale = 1;
        sysex_answer(cmd, 0);
        return 1;
    } else if (cmd == 9u && n == 5u && p[4] < NPAT) {   /* a pattern sent back */
        const seqpat_t *pt = &sq.pat[p[4]];
        uint8_t f[8u + 3u * NSTEP] = {0xF0, 0x7D, 0x4D, 0x42, 9, p[4], (uint8_t)seq_len(pt)};
        for (i = 0; i < NSTEP; i++) {
            f[7u + i] = pt->note[i] & 0x7Fu;
            f[7u + NSTEP + i] = pt->vel[i] & 0x7Fu;
            f[7u + 2u * NSTEP + i] = pt->flag[i] & SF_TIE;
        }
        f[7u + 3u * NSTEP] = 0xF7;
        (void)plat_sysex_send(f, sizeof f);
        return 1;
    } else if (cmd == 10u && n == 6u + 3u * NSTEP && p[4] < NPAT && p[5] >= 1u && p[5] <= NSTEP) {   /* a pattern in */
        seqpat_t *pt = &sq.pat[p[4]];
        pt->len = p[5];
        for (i = 0; i < NSTEP; i++) {
            pt->note[i] = p[6u + i];
            pt->vel[i] = p[6u + NSTEP + i];
            pt->flag[i] = p[6u + 2u * NSTEP + i] & SF_TIE;
        }
        sq_dirty = 1;
        sq_changed_ms = plat_ms();
        seq_store();
        {
            char o[4];
            t[0] = 0;
            text_cat(t, "PATTERN ");
            int_text((int32_t)p[4] + 1, o);
            text_cat(t, o);
            app_say(t, "LOADED");
        }
        sysex_answer(cmd, sq_dirty ? 1u : 0u);   /* (1: not stored, a backup was streaming: stored later) */
        return 1;
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

static void value_text(uint32_t i, uint32_t x, char *b) { mb_value_text(i, x, set.g[G_LSYNC - NP], set.cur.v[P_DSYNC], b); }

static void name_text(const patch_t *p, char *b)   /* the slot and the name: "03 ACID*" */
{
    uint32_t i, n = 0;
    b[n++] = (char)('A' + set.slot / BANK_N);      /* A01 .. D32 */
    b[n++] = (char)('0' + (set.slot % BANK_N + 1u) / 10u);
    b[n++] = (char)('0' + (set.slot % BANK_N + 1u) % 10u);
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

/* RANDOM (HOME three times in a row): a new patch, every parameter picked at random within what sounds: at least
 * one oscillator up, the amp envelope heard, the filter open enough, the effects and the rarer sources now and
 * then. LEVEL, BEND and the arpeggiator stay as they were; the patch is new (edited, in no slot until SAVE) */
static uint32_t rnd_s = 0x2545F491u;
static uint32_t rnd(uint32_t n)              /* 0 .. n-1 */
{
    rnd_s ^= rnd_s << 13;
    rnd_s ^= rnd_s >> 17;
    rnd_s ^= rnd_s << 5;
    return n ? (rnd_s >> 8) % n : 0u;
}
static uint8_t rr(uint32_t lo, uint32_t hi) { return (uint8_t)(lo + rnd(hi - lo + 1u)); }
static uint8_t rchance(uint32_t pct, uint32_t lo, uint32_t hi, uint32_t other)   /* pct % of the time lo..hi */
{
    return rnd(100) < pct ? rr(lo, hi) : (uint8_t)other;
}
static uint8_t rbip(uint32_t pct, uint32_t span)   /* 64, or pct % of the time 64 +- up to span */
{
    return rnd(100) < pct ? (uint8_t)(64 + (int32_t)rr(0, 2u * span) - (int32_t)span) : 64u;
}
static void patch_random(void)
{
    uint8_t *v = set.cur.v, keep_lvl = v[P_LEVEL], keep_bend = v[P_BEND], keep_arp = v[P_ARP];
    uint32_t i;
    rnd_s ^= plat_ms() * 2654435761u + 1u;
    for (i = 0; i < NP; i++)                 /* the choices first: any of their positions */
        if (is_enum(PARAM[i].type))
            v[i] = (uint8_t)rnd(pmax(i) + 1u);
    v[P_MODE] = rnd(100) < 65u ? 0u : (uint8_t)rnd(4);
    v[P_CUT] = v[P_MODE] == 2u ? rr(30, 80) : v[P_MODE] == 1u ? rr(55, 100) : rr(45, 118);   /* (HP and BP: where they pass) */
    v[P_RES] = rr(0, 105);
    v[P_BRUTE] = rchance(55, 0, 100, 0);
    v[P_FENV] = rr(50, 127);
    v[P_KBD] = rr(0, 70);
    v[P_FVEL] = rchance(30, 0, 90, 0);
    v[P_AVEL] = rchance(40, 0, 90, 0);
    v[P_SAW] = rchance(60, 30, 127, 0);
    v[P_SQR] = rchance(50, 30, 127, 0);
    v[P_TRI] = rchance(40, 30, 127, 0);
    if (v[P_SAW] < 80u && v[P_SQR] < 80u && v[P_TRI] < 80u)   /* one of them up */
        v[P_SAW + rnd(3)] = rr(90, 127);
    v[P_SUB] = rchance(50, 20, 127, 0);
    v[P_NOISE] = rchance(15, 10, 80, 0);
    v[P_ULTRA] = rchance(30, 20, 127, 0);
    v[P_USRATE] = rr(0, 90);
    v[P_PW] = rr(0, 110);
    v[P_PWENV] = rbip(30, 50);
    v[P_METAL] = rchance(35, 10, 127, 0);
    v[P_MTENV] = rbip(30, 50);
    v[P_FA] = rchance(30, 10, 70, 0);
    v[P_FD] = rr(10, 85);
    v[P_FS] = rr(0, 127);
    v[P_FR] = rr(5, 60);
    v[P_AA] = rchance(25, 5, 60, 0);
    v[P_AD] = rr(10, 85);
    v[P_AS] = rr(50, 127);
    v[P_AR] = rr(5, 60);
    v[P_LRATE] = rr(20, 100);
    v[P_LPITCH] = rbip(15, 12);
    v[P_LFILT] = rbip(45, 50);
    v[P_LPWM] = rbip(35, 50);
    v[P_LAMP] = rbip(15, 25);
    v[P_VRATE] = rr(30, 90);
    v[P_GLIDE] = rchance(25, 5, 45, 0);
    v[P_VINT] = rchance(40, 0, 50, 0);
    v[P_WAMT] = rr(20, 100);
    v[P_ATVIB] = rchance(30, 0, 60, 0);
    v[P_ATCUT] = rchance(30, 0, 60, 0);
    v[P_DTIME] = rr(20, 110);
    v[P_DFB] = rr(20, 95);
    v[P_DTONE] = rr(40, 127);
    v[P_DMIX] = rchance(45, 10, 60, 0);
    v[P_RDECAY] = rr(20, 110);
    v[P_RSIZE] = rr(20, 120);
    v[P_RDAMP] = rr(20, 110);
    v[P_RMIX] = rchance(50, 10, 60, 0);
    v[P_RPRE] = rr(0, 60);
    v[P_XDRV] = rr(10, 100);
    v[P_XTONE] = rr(60, 127);
    v[P_XMIX] = rchance(30, 15, 110, 0);
    v[P_CRATE] = rr(0, 90);
    v[P_CDEPTH] = rr(30, 127);
    v[P_CPHASE] = rr(60, 127);
    v[P_CMIX] = rchance(30, 20, 100, 0);
    v[P_KTHR] = rr(30, 90);
    v[P_KRAT] = rchance(25, 20, 80, 0);
    v[P_KATT] = rr(10, 90);
    v[P_KREL] = rr(10, 90);
    v[P_LEVEL] = keep_lvl;
    v[P_BEND] = keep_bend;
    v[P_ARP] = keep_arp;
    for (i = 0; i < NP; i++)
        if (v[i] > pmax(i))
            v[i] = (uint8_t)pmax(i);
    {   /* RND001, RND002 ... (as sq80-fm1 names its random programs) */
        static uint32_t rnd_n;
        char nm[9] = "RND000";
        rnd_n = rnd_n % 999u + 1u;
        nm[3] = (char)('0' + rnd_n / 100u);
        nm[4] = (char)('0' + rnd_n / 10u % 10u);
        nm[5] = (char)('0' + rnd_n % 10u);
        patch_name(&set.cur, nm);
    }
    set.edited = 1;
    set_dirty = 1;
    params_stale = 1;
    page_show(0);
    app_say("RANDOM", "A NEW PATCH");
}
static uint32_t home_taps, home_last_ms;

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

/* SAVE: a first press picks where (PRESET turns through the 128 slots, the patch's own first), SAVE again stores
 * it there (and plays from there), any other button leaves it */
static struct {
    uint8_t on, to, pos;                     /* (pos: the name's letter K1 is on) */
    char was[8];                             /* the name before (a cancel puts it back) */
} sv;
static const char NAME_CH[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-#&+.!";   /* what K2 steps through */
#define NNAME_CH (sizeof NAME_CH - 1u)
static void name_step(int32_t e)             /* the letter under the cursor, e steps on */
{
    char *c = &set.cur.name[sv.pos & 7u];
    uint32_t i;
    for (i = 0; i < NNAME_CH && NAME_CH[i] != *c; i++)
        ;
    if (i == NNAME_CH)
        i = 0;
    *c = NAME_CH[(uint32_t)((int32_t)i + (int32_t)NNAME_CH * 8 + e) % NNAME_CH];
}
static void slot_text(uint32_t s, char *b)   /* "B03 ACID" */
{
    const patch_t *p = &SLOT(s);
    uint32_t i, n = 0;
    b[n++] = (char)('A' + s / BANK_N);
    b[n++] = (char)('0' + (s % BANK_N + 1u) / 10u);
    b[n++] = (char)('0' + (s % BANK_N + 1u) % 10u);
    b[n++] = ' ';
    for (i = 0; i < 8u && p->name[i] != ' '; i++)
        b[n++] = p->name[i];
    b[n] = 0;
}
static void save(void)
{
    char t[16];
    set.slot = (uint8_t)(sv.to % NSLOT);
    SLOT(set.slot) = set.cur;
    bank_dirty |= (uint8_t)(1u << (set.slot / BANK_N));
    set.edited = 0;
    if (banks_save() == 0 && plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0 &&
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
    int32_t o = clamp(set.octave + d, -3, 3);   /* (-3..+3: an 88-key piano's A0..C8, and more) */
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
static uint32_t rec_down_ms;                 /* REC: pressed (it acts on release), held a second: the pattern cleared */
static uint8_t rec_long;
static uint32_t pg_btn = 0xFFu, pg_down_ms;   /* a page button held: back to its first page after 500 ms */

/* the sequencer from the panel: PLAY starts and stops it, REC arms recording (stopped: step entry, a key a step,
 * FX a rest, FX with a key held a tie; playing: in time). A pattern recorded step by step is as long as its steps. */
static uint32_t seq_edit;                    /* step entry: the next step */
static uint8_t seq_entered;                  /* steps entered since REC */
static uint8_t seq_picked;                   /* the SMK-37's pads picked the step: the pattern keeps its length */
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
    if (seq_picked) {                        /* (a step picked on the pads: the pattern at least that long) */
        if (pt->len < e + 1u)
            pt->len = (uint8_t)(e + 1u);
    } else {
        pt->len = seq_entered;
    }
    seq_edit = (e + 1u) % NSTEP;
    sq_dirty = 1;
    sq_changed_ms = plat_ms();
    seq_say_step(flag & SF_TIE ? "TIE" : vel ? "STEP" : "REST", e);
    ui.redraw = 1;
}

static uint8_t save_held;                      /* a backup streams: nothing saved meanwhile */
static void seq_store(void)
{
    if (save_held)                             /* (later: it stays dirty) */
        return;
    if (plat_store_save(OBJ_SEQ, &sq, sizeof sq) == 0)
        sq_dirty = 0;
}

/* Backup and restore over USB (fm1-base device/backup.c; the site's backup/ page): the settings, the four banks and
 * the patterns (app.h APP_BACKUP_OBJS), all or nothing. A restore waits for the sequencer to stop. */
int app_backup_playing(void) { return seq_running; }
void app_backup_flush(void)                  /* a backup starts: what is not saved yet, saved */
{
    if (sq_dirty)
        seq_store();
    if (bank_dirty)
        (void)banks_save();
    if (set_dirty && plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0)
        set_dirty = 0;
}
void app_backup_hold(int on) { save_held = (uint8_t)(on != 0); }
void app_backup_restored(void)               /* everything from the flash again, as at power-on (A01) */
{
    store_load();
    arp_posted = set.cur.v[P_ARP];
    post(Q_ARP, arp_posted, 0);              /* (the arpeggiator as the patch has it; every note off) */
    set_dirty = 0;
    bank_dirty = 0;
    sq_dirty = 0;
    params_stale = 1;
    ui.redraw = 1;
    app_say("RESTORED", "FROM USB");
}

static void rec_button(void)
{
    seq_rec = (uint8_t)!seq_rec;
    if (seq_rec) {
        if (!seq_running) {                  /* step entry from the first step */
            seq_edit = 0;
            seq_entered = 0;
            seq_picked = 0;
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

#if APP_SMK
/* ------------------------------------------------------------------ SMK-37 --- */
/* The SMK-37's own (fm1-base BOARD=smk37; after SQ-80's): the pitch wheel bends (the patch's BEND range), the mod
 * wheel is the mod wheel (as ALGO), the pedal sustains, and the keys outside the FM-1's 27 (C3..E3, G#5..C6) play too,
 * each with its velocity, and go out over USB and Bluetooth as the others do. The buttons, the knobs (K1 = MASTER,
 * K2 SELECT, K3 ALGO, K4 PRESET, K5..K8 the page's four) and the pads (the white keys) are the platform's, as on the
 * FM-1 */
static struct {
    int8_t pedal;                            /* the pedal's last state (-1: none) */
    uint8_t outer[SMK_NKEYBED];              /* an outer key's note + 1 while it sounds (0: none) */
} smk = {-1, {0}};

/* The SMK-37's own panel (phase 2): at power-on the eight knobs are the MiniBrute's character and shaping, the four
 * faders its oscillator mixer. KNOB BANK switches the knobs to the pages (the FM-1's way: K1 volume, K2 SELECT, K3
 * ALGO, K4 PRESET, K5..K8 the page's four) and back; FADER BANK steps the faders A..D; < and > the patch (on the
 * panel, where PRESET is a knob). A fader takes a value over once it reaches it (no jump on a patch change): until then
 * the header says where to. */
#define PK_DECREL 0xFEu                      /* knob 7: decay and release, the amp's and the filter's, together */
static const uint8_t PK[8] = {P_ULTRA, P_METAL, P_BRUTE, P_CUT, P_RES, P_FENV, PK_DECREL, P_LRATE};
static const char *const PK_NAME[8] = {"ULTRASAW", "METAL", "BRUTE", "CUTOFF", "RESO", "ENV AMT", "DEC/REL", "LFO RATE"};
static const uint8_t PF[4][4] = {{P_SAW, P_SQR, P_TRI, P_SUB}, {P_FA, P_FD, P_FS, P_FR}, {P_AA, P_AD, P_AS, P_AR},
                                 {P_NOISE, P_XMIX, P_DMIX, P_RMIX}};
static const char *const PF_BANK[4] = {"MIXER", "FILT ENV", "AMP ENV", "NOISE FX"};
static const char *const PF_SHORT[4][4] = {{"SAW", "SQUARE", "TRI", "SUB"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"},
                                           {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}, {"NOISE", "DIST", "DELAY", "REVERB"}};
static struct {
    uint8_t on;                              /* 1: the knobs are the panel's (0: the pages') */
    uint8_t bank;                            /* the faders' bank, 0..3 (A..D) */
    int16_t last[4];                         /* each fader's last reading (-1: none yet) */
    uint8_t caught[4];                       /* 1: the fader holds its value */
    int16_t wrote[4];                        /* the value it last set (another change lets it go) */
    uint32_t turned_at[8];                   /* a knob's last turn (the screen lights it) */
    uint32_t moved_at[4];                    /* a fader's */
} pf = {1, 0, {-1, -1, -1, -1}, {0}, {0}, {0}, {0}};

static void param_cc(uint32_t p)             /* a parameter's value out as its CC */
{
    if (p < NP)
        plat_midi_send(0x0Bu | 0xB0u << 8 | (uint32_t)PARAM[p].cc << 16 |
                           (is_enum(PARAM[p].type) ? set.cur.v[p] * 127u / pmax(p) : set.cur.v[p]) << 24,
                       TO_USB | TO_BLE);
}

static void pf_knob(uint32_t i, int32_t e)   /* panel knob i (0..7) turned e detents */
{
    uint32_t p = PK[i];
    int32_t step = e > 1 || e < -1 ? 3 * e : e;   /* (a fast turn moves further) */
    char t[16];
    pf.turned_at[i] = plat_ms();
    ui.redraw = 1;
    if (p == PK_DECREL) {                    /* each moves by the same steps: the patch's shape kept */
        static const uint8_t DR[4] = {P_AD, P_AR, P_FD, P_FR};
        uint32_t j;
        for (j = 0; j < 4u; j++) {
            param_set(DR[j], (int32_t)pget(DR[j]) + step);
            param_cc(DR[j]);
        }
        value_text(P_AD, pget(P_AD), t);
        app_say("DEC/REL", t);
        return;
    }
    if (is_enum(pinfo(p)->type))
        step = e > 0 ? 1 : -1;
    param_set(p, (int32_t)pget(p) + step);
    param_cc(p);
    value_text(p, pget(p), t);
    app_say(PK_NAME[i], t);
}

static void pf_set(int on)
{
    pf.on = (uint8_t)(on != 0);
    plat_smk_k1_raw(pf.on);
    app_say("KNOBS", pf.on ? "PANEL" : "PAGES");
}

static void pf_fader_bank(uint32_t b)
{
    uint32_t i;
    char t[12] = "FADERS ";
    pf.bank = (uint8_t)(b & 3u);
    for (i = 0; i < 4u; i++)
        pf.caught[i] = 0;
    t[7] = (char)('A' + pf.bank);
    t[8] = 0;
    app_say(t, PF_BANK[pf.bank]);
}

static void pf_faders(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int f = plat_fader((int)i);
        uint32_t p = PF[pf.bank][i], max = pmax(p);
        int32_t v, cur = (int32_t)pget(p), was = pf.last[i];
        char t[16], n[16];
        if (pf.caught[i] && cur != pf.wrote[i])   /* the patch changed under it: let go */
            pf.caught[i] = 0;
        if (f < 0 || f == was)
            continue;
        pf.last[i] = (int16_t)f;
        if (was < 0)                         /* (the first reading: where it is, nothing moved) */
            continue;
        v = (int32_t)((uint32_t)f * max + 63u) / 127;
        if (!pf.caught[i]) {                 /* caught when it reaches the value, or passes it */
            int32_t w = (int32_t)(((uint32_t)was * max + 63u) / 127u);
            if ((v - cur) * (w - cur) <= 0 || v == cur)
                pf.caught[i] = 1;
        }
        pf.moved_at[i] = plat_ms();
        ui.redraw = 1;
        if (!pf.caught[i]) {
            value_text(p, (uint32_t)cur, n);
            t[0] = 0;
            text_cat(t, v < cur ? "UP TO " : "DOWN TO ");
            text_cat(t, n);
            app_say(PF_SHORT[pf.bank][i], t);
            continue;
        }
        param_set(p, v);
        param_cc(p);
        pf.wrote[i] = (int16_t)pget(p);
        value_text(p, pget(p), t);
        app_say(PF_SHORT[pf.bank][i], t);
    }
}

/* The sequencer on the pads: while it plays, and on the piano roll, the 16 pads are 16 steps (the top row 1..8, the
 * bottom 9..16 of the 16 shown): the step playing bright orange, a note white, a tie a dim orange, a rest dark, past
 * the pattern's end off; on the piano roll the step picked is lit too. On the piano roll the pads pick the step (REC
 * stopped: the next key, FX or FX with a key is written there), < and > show the 16 before or after (1-16 .. 49-64).
 * Elsewhere the pads are the white keys, as before. */
static uint32_t roll_win;                    /* the piano roll's 16 steps: roll_win * 16 .. + 15 (stopped) */
static uint32_t roll_pick = 0xFFu;           /* the step the pads picked (0xFF: none) */
static void roll_say(void);
static uint32_t roll_window(void)
{
    if (seq_running)
        return seq_cur_now / 16u;
    if (seq_rec)                             /* (step entry: where the next step goes) */
        return (seq_edit % NSTEP) / 16u;
    return roll_win & 3u;                    /* (< >: any of the four, past the pattern's end too) */
}

static void pf_pads(void)
{
    static uint32_t said = 0xFFFFFFFFu;
    int roll = set.page == PG_ROLL, show = roll || seq_running;
    uint32_t hits = plat_pad_hits(), win, p, hash = 0;
    plat_pads_raw(roll);                     /* (the piano roll: the pads play no notes) */
    if (!show) {
        if (said != 0u) {
            plat_pad_rgb(-1, 0, 0, 0);       /* (the pads back to the white keys' lights) */
            said = 0;
        }
        return;
    }
    win = roll_window();
    if (roll && hits) {                      /* a pad: its step picked */
        for (p = 0; p < 16u && !(hits & (1u << p)); p++)
            ;
        roll_pick = win * 16u + p;
        if (seq_rec && !seq_running) {
            seq_edit = roll_pick;
            seq_picked = 1;
        }
        roll_say();
        ui.redraw = 1;
    }
    {
        const seqpat_t *pt = seq_pat();
        uint32_t L = seq_len(pt), cur = seq_running ? seq_cur_now : 0xFFu,
                 pick = !roll ? 0xFFu : seq_rec && !seq_running ? seq_edit % NSTEP : roll_pick;
        uint8_t c[16][3];
        for (p = 0; p < 16u; p++) {
            uint32_t st = win * 16u + p;
            uint8_t r = 0, g = 0, b = 0;
            if (st >= L)
                ;                            /* past the end: off */
            else if (st == cur)
                r = 255, g = 80, b = 10;
            else if (st == pick)
                r = 160, g = 160, b = 160;
            else if (pt->flag[st] & SF_TIE)
                r = 60, g = 20, b = 4;
            else if (pt->vel[st])
                r = 40, g = 40, b = 40;
            else
                r = 4, g = 4, b = 4;
            c[p][0] = r, c[p][1] = g, c[p][2] = b;
            hash = hash * 31u + (uint32_t)r * 7u + g * 3u + b + 1u;
        }
        if (hash != said) {
            for (p = 0; p < 16u; p++)
                plat_pad_rgb((int)p, c[p][0], c[p][1], c[p][2]);
            said = hash;
        }
    }
}

/* On the piano roll the knobs K5..K8 edit the step picked (a pad, or K7): K5 its note (a semitone a detent; a rest
 * becomes a note), K6 its velocity, K7 which step, K8 what it is (NOTE, TIE, REST); FX makes it a rest (deleted). */
static void note_name(int32_t n, char *b)    /* "C#4" */
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char o[6];
    b[0] = 0;
    text_cat(b, N[(n % 12 + 12) % 12]);
    int_text(n / 12 - 1, o);
    text_cat(b, o);
}

static void roll_say(void)                   /* the step picked, at the top */
{
    const seqpat_t *pt = seq_pat();
    char t[16], n[8];
    uint32_t st = roll_pick % NSTEP;
    int_text((int32_t)st + 1, t);
    if (pt->flag[st] & SF_TIE)
        text_cat(t, " TIE");
    else if (!pt->vel[st])
        text_cat(t, " REST");
    else {
        note_name(pt->note[st], n);
        text_cat(t, " ");
        text_cat(t, n);
    }
    app_say("STEP", t);
}

static void roll_changed(seqpat_t *pt, uint32_t st)
{
    if (pt->len < st + 1u)                   /* (a step past the end: the pattern made that long) */
        pt->len = (uint8_t)(st + 1u);
    sq_dirty = 1;
    sq_changed_ms = plat_ms();
    ui.redraw = 1;
    roll_say();
}

static void roll_knob(uint32_t q, int32_t e)   /* K5..K8 on the piano roll (q 0..3) */
{
    seqpat_t *pt = seq_pat();
    uint32_t st;
    if (roll_pick >= NSTEP)                  /* (nothing picked yet: the window's first step) */
        roll_pick = roll_window() * 16u;
    st = roll_pick;
    if (q == 2u) {                           /* K7: the step picked, left or right */
        uint32_t L = seq_len(pt), n = L < NSTEP ? L + 1u : NSTEP;   /* (one past the end: to add a step) */
        roll_pick = (st + n * 4u + (uint32_t)(e > 0 ? 1 : n - 1u)) % n;
        roll_win = roll_pick / 16u;
        if (seq_rec && !seq_running) {
            seq_edit = roll_pick;
            seq_picked = 1;
        }
        ui.redraw = 1;
        roll_say();
        return;
    }
    if (q == 0u) {                           /* K5: the note */
        if (!pt->vel[st] || (pt->flag[st] & SF_TIE)) {   /* a rest or a tie becomes a note */
            pt->vel[st] = pt->vel[st] ? pt->vel[st] : 100u;
            pt->flag[st] = 0;
        } else {
            pt->note[st] = (uint8_t)clamp((int32_t)pt->note[st] + (e > 1 || e < -1 ? 12 * (e > 0 ? 1 : -1) : e), 0, 127);
        }
        post(Q_ON, pt->note[st], pt->vel[st]);   /* (heard a moment) */
        post(Q_OFF, pt->note[st], 0);
    } else if (q == 1u) {                    /* K6: the velocity */
        if (pt->vel[st] && !(pt->flag[st] & SF_TIE))
            pt->vel[st] = (uint8_t)clamp((int32_t)pt->vel[st] + (e > 1 || e < -1 ? 3 * e : e), 1, 127);
        {
            char t[8];
            int_text(pt->vel[st], t);
            app_say("VELOCITY", t);
        }
        sq_dirty = 1;
        sq_changed_ms = plat_ms();
        ui.redraw = 1;
        return;
    } else {                                 /* K8: NOTE -> TIE -> REST */
        uint32_t ty = (pt->flag[st] & SF_TIE) ? 1u : pt->vel[st] ? 0u : 2u;
        ty = (ty + 3u + (e > 0 ? 1u : 2u)) % 3u;
        pt->flag[st] = ty == 1u ? SF_TIE : 0u;
        pt->vel[st] = ty == 2u ? 0u : pt->vel[st] ? pt->vel[st] : 100u;
    }
    roll_changed(pt, st);
}

static void roll_delete(void)                /* FX on the piano roll: the step picked a rest */
{
    seqpat_t *pt = seq_pat();
    if (roll_pick >= NSTEP)
        return;
    pt->vel[roll_pick] = 0;
    pt->flag[roll_pick] = 0;
    roll_changed(pt, roll_pick);
}

/* the volume (MASTER: K1 on the pages), shown in the foot and, as it changes, at the top; 50 % at power-on */
static uint32_t vol_pct(void) { return (plat_master() * 100u + 2048u) / 4096u; }

static int outer_held(void)                  /* an outer key held (step entry: FX with it, a tie) */
{
    uint32_t k;
    for (k = 0; k < SMK_NKEYBED; k++)
        if (smk.outer[k])
            return 1;
    return 0;
}

static void smk_note(uint32_t note, uint32_t vel)
{
    post(vel ? Q_ON : Q_OFF, note, vel);
    plat_midi_send(vel ? 0x09u | 0x90u << 8 | note << 16 | vel << 24 : 0x08u | 0x80u << 8 | note << 16, TO_USB | TO_BLE);
}

static void slot_load(uint32_t s);
static void smk_input(void)
{
    int32_t k, bend, mod;
    int ped;
    uint32_t hits = plat_smk_button_hits();
    if (hits & (1u << SB_KNOB_BANK))
        pf_set(!pf.on);
    if (hits & (1u << SB_FADER_BANK))
        pf_fader_bank(pf.bank + 1u);
    if ((hits & ((1u << SB_LEFT) | (1u << SB_RIGHT))) && set.page == PG_ROLL) {
        /* the piano roll: < > its 16 steps, 1-16, 17-32, 33-48, 49-64 (a pattern's room, past its end too: a step
         * picked there makes it longer) */
        char t[12], o[4];
        roll_win = (roll_window() + 4u + ((hits & (1u << SB_RIGHT)) ? 1u : 3u)) % 4u;
        if (seq_rec && !seq_running) {       /* (step entry: the next step goes there) */
            seq_edit = roll_win * 16u;
            seq_picked = 1;
        }
        t[0] = 0;
        int_text((int32_t)roll_win * 16 + 1, o);
        text_cat(t, o);
        text_cat(t, "-");
        int_text((int32_t)roll_win * 16 + 16, o);
        text_cat(t, o);
        app_say("STEPS", t);
        ui.redraw = 1;
    } else if (hits & ((1u << SB_LEFT) | (1u << SB_RIGHT))) {   /* < >: the patch before, the next */
        char t[16];
        slot_load((set.slot + NSLOT + ((hits & (1u << SB_RIGHT)) ? 1u : NSLOT - 1u)) % NSLOT);
        name_text(&set.cur, t);
        app_say("PATCH", t);
    }
    pf_faders();
    pf_pads();
    {
        static uint32_t vol_was = 0xFFu;
        uint32_t v = vol_pct();
        if (v != vol_was) {
            if (vol_was != 0xFFu) {
                char t[8];
                int_text((int32_t)v, t);
                text_cat(t, "%");
                app_say("VOLUME", t);
            }
            vol_was = v;
            ui.redraw = 1;
        }
    }
    plat_smk_leds((pf.on ? 1u << SB_KNOB_BANK : 0u) | (pf.bank ? 1u << SB_FADER_BANK : 0u));
    if (plat_wheels(&bend, &mod)) {
        uint32_t b14 = (uint32_t)(bend + 8192);
        post(Q_BEND, 0, b14);
        plat_midi_send(0x0Eu | 0xE0u << 8 | (b14 & 127u) << 16 | (b14 >> 7) << 24, TO_USB | TO_BLE);
        if ((uint32_t)mod != ui.wheel) {     /* the mod wheel: as ALGO */
            ui.wheel = (uint8_t)mod;
            wheel_now = (uint8_t)mod;
            post(Q_WHEEL, (uint32_t)mod, 0);
            plat_midi_send(0x0Bu | 0xB0u << 8 | 1u << 16 | (uint32_t)mod << 24, TO_USB | TO_BLE);
            ui.redraw = 1;
        }
    }
    if ((ped = plat_pedal()) != smk.pedal) {
        if (ped >= 0 || smk.pedal > 0) {     /* (unplugged while held: let go) */
            uint32_t v = ped > 0 ? 127u : 0u;
            post(Q_SUS, v ? 1u : 0u, 0);
            plat_midi_send(0x0Bu | 0xB0u << 8 | 64u << 16 | v << 24, TO_USB | TO_BLE);
        }
        smk.pedal = (int8_t)ped;
    }
    while ((k = plat_outer_key()) >= 0) {    /* C3..E3, G#5..C6 */
        uint32_t key = (uint32_t)k >> 8, v = (uint32_t)k & 127u;
        if (key >= SMK_NKEYBED)
            continue;
        if (v) {
            int32_t note = 48 + (int32_t)key + 12 * set.octave;
            if (note < 0 || note > 127)
                continue;
            if (smk.outer[key])
                smk_note(smk.outer[key] - 1u, 0);
            smk.outer[key] = (uint8_t)(note + 1);
            if (seq_rec && !seq_running) {   /* step entry: the outer keys too (as the middle ones) */
                seq_step_write((uint32_t)note, v ? v : 1u, 0);
                seq_key_note = note;
            }
            smk_note((uint32_t)note, v);
        } else if (smk.outer[key]) {
            smk_note(smk.outer[key] - 1u, 0);
            smk.outer[key] = 0;
        }
    }
}
#endif

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, at = 0;
    int32_t e;
    touch_poll();
#if APP_SMK
    smk_input();
#endif
    keys = touch_keys();
    for (k = 0; k < NKEYS; k++) {            /* key 0 is F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = 53 + (int32_t)k + 12 * set.octave;
        if ((keys & bit) && !(ui.keys & bit)) {
#if APP_SMK
            v = (uint32_t)plat_key_vel((int)k);   /* (the SMK-37's keys give it themselves) */
            if (v > 127u)
                v = 127u;
#else
            v = touch_vel(k);
#endif
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
        else if (sv.on && k != B_SAVE) {    /* any other button: no save */
            sv.on = 0;
            for (uint32_t c = 0; c < 8u; c++)
                set.cur.name[c] = sv.was[c];
            app_say("SAVE", "CANCELLED");
            ui.redraw = 1;
        } else if (k == B_SAVE) {
            if (!sv.on) {
                sv.on = 1;
                sv.to = set.slot;
                sv.pos = 0;
                for (uint32_t c = 0; c < 8u; c++)
                    sv.was[c] = set.cur.name[c];
                app_say("SAVE", "NAME AND SLOT");
            } else {
                sv.on = 0;
                save();
            }
            ui.redraw = 1;
        }
        else if (k == B_ARP) {               /* (on release: a long press is its pages) */
            arp_down_ms = plat_ms();
            arp_long = 0;
        } else if (k == B_PLAY)
            play_button(btn);
        else if (k == B_REC) {               /* (on release: a long press clears the pattern) */
            rec_down_ms = plat_ms();
            rec_long = 0;
        }
#if APP_SMK
        else if (k == B_FX && set.page == PG_ROLL && !(seq_rec && !seq_running))   /* the piano roll: deleted */
            roll_delete();
#endif
        else if (k == B_FX && seq_rec && !seq_running)   /* step entry: a rest, or with a key held a tie */
            seq_step_write((keys || OUTER_HELD()) && seq_key_note >= 0 ? (uint32_t)seq_key_note : 60u, 0,
                           (keys || OUTER_HELD()) && seq_key_note >= 0 ? SF_TIE : 0u);
        else {
            page_button(k);
            pg_btn = k;
            pg_down_ms = plat_ms();
            if (k == B_HOME) {               /* HOME three times in a row, each within 400 ms: RANDOM */
                home_taps = plat_ms() - home_last_ms < 400u ? home_taps + 1u : 1u;
                home_last_ms = plat_ms();
                if (home_taps >= 3u) {
                    home_taps = 0;
                    patch_random();
                }
            } else {
                home_taps = 0;
            }
        }
    }
    if (pg_btn < NB && (btn & 1u << pg_btn) && plat_ms() - pg_down_ms >= 500u) {   /* held: its first page */
        for (k = 0; k < NPAGE && PAGE[k].btn != pg_btn; k++)
            ;
        if (k < NPAGE && k != set.page)
            page_show(k);
        pg_btn = 0xFFu;
    } else if (pg_btn < NB && !(btn & 1u << pg_btn)) {
        pg_btn = 0xFFu;
    }
    {
        static int32_t xp_said;
        int32_t x = seq_xpose;
        if (x != xp_said && seq_running) {   /* the keys transposed the pattern */
            char t[8];
            t[0] = x < 0 ? '-' : '+';
            int_text(x < 0 ? -x : x, t + 1);
            app_say("TRANSPOSE", t);
            xp_said = x;
        } else if (!seq_running) {
            xp_said = 0;
        }
    }
    if (seq_wrote) {                         /* recorded in time (the audio side) */
        seq_wrote = 0;
        sq_dirty = 1;
        sq_changed_ms = plat_ms();
    }
    if (sq_dirty && !seq_rec && !seq_running && plat_ms() - sq_changed_ms > 2000u)   /* (LENGTH and the rest) */
        seq_store();
    if ((btn & 1u << B_REC) && !rec_long && plat_ms() - rec_down_ms >= 1000u) {   /* REC held: the pattern cleared */
        seqpat_t *pt = seq_pat();
        uint32_t j;
        char t[12] = "PATTERN ", o[4];
        rec_long = 1;
        for (j = 0; j < NSTEP; j++) {
            pt->note[j] = 60;
            pt->vel[j] = 0;
            pt->flag[j] = 0;
        }
        pt->len = 16;
        seq_edit = 0;                        /* (step entry: from the first step again) */
        seq_entered = 0;
        seq_picked = 0;
        seq_key_note = -1;
        sq_dirty = 1;
        sq_changed_ms = plat_ms();
        int_text((int32_t)set.g[G_SPAT - NP] % (int32_t)NPAT + 1, o);
        text_cat(t, o);
        app_say(t, "CLEARED");
        ui.redraw = 1;
    }
    if (!(btn & 1u << B_REC) && (ui.btn & 1u << B_REC) && !rec_long)   /* REC pressed: armed, or not */
        rec_button();
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
#if APP_SMK
    if (pf.on && !sv.on && set.page != PG_ROLL) {   /* the panel: K1 (raw), K2..K4 (SELECT, ALGO, PRESET), K5..K8 */
        if ((e = plat_smk_k1()) != 0)
            pf_knob(0, e);
        for (k = 0; k < NE; k++)
            if ((e = plat_enc((int)k)) != 0)
                pf_knob(k + 1u, e);
    } else {
        (void)plat_smk_k1();
    }
#endif
    for (k = 0; k < NE; k++) {
        char t[16];
#if APP_SMK
        if (pf.on && !sv.on && set.page != PG_ROLL)
            break;
#endif
        if ((e = plat_enc((int)k)) == 0)
            continue;
#if APP_SMK
        if (set.page == PG_ROLL && !sv.on && k >= EN_K1) {   /* the piano roll: K5..K8 edit the step picked */
            roll_knob(k - EN_K1, e);
            continue;
        }
#endif
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
        } else if (k == EN_PRESET && sv.on) {   /* where to save */
            sv.to = (uint8_t)(((int32_t)sv.to + (int32_t)NSLOT * 4 + e) % (int32_t)NSLOT);
            ui.redraw = 1;
        } else if (k == EN_PRESET) {
            slot_load((set.slot + NSLOT + (uint32_t)(e > 0 ? 1 : NSLOT - 1)) % NSLOT);
            name_text(&set.cur, t);
            app_say("PATCH", t);
        } else if (sv.on) {                  /* saving: K1 the letter's place, K2 the letter */
            if (k == EN_K1)
                sv.pos = (uint8_t)(((int32_t)sv.pos + 64 + e) & 7);
            else if (k == EN_K2)
                name_step(e);
            ui.redraw = 1;
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
        {   /* the unlit buttons and keys all glow (plat_leds lets only the buttons glow: the seven lowest keys then
             * caught some of it, the others none; as sq80-fm1 does), PLAY green */
            uint32_t on = lit, play = 1u << B_PLAY, dimb = ((1u << NB) - 1u) & ~on;
            on = (on & ~play) | (on & play ? LED_PLAY_GREEN : 0u);
            dimb = (dimb & ~play) | (dimb & play ? LED_PLAY_GREEN : 0u);
            plat_lights(on, keys, dimb, ((1u << NKEYS) - 1u) & ~keys, 2);
        }
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

/* a parameter drawn as the editor draws it (app/editor/index.html): a knob, a fader (the mixer's levels, the
 * envelopes) or a slide switch (the two- and three-way choices); the knobs' choices click round as rotary switches */
enum { K_KNOB, K_FADER, K_SWITCH };
static uint32_t ctl_kind(uint32_t p)
{
    switch (p) {
    case P_SUB: case P_SAW: case P_SQR: case P_TRI: case P_NOISE:
    case P_FA: case P_FD: case P_FS: case P_FR: case P_AA: case P_AD: case P_AS: case P_AR:
        return K_FADER;
    case P_SUBSHP: case P_SUBOCT: case P_WDEST: case P_PLAY: case P_VTYPE: case P_LRTRG: case P_ARP:
    case P_DSYNC: case P_DMODE: case P_FXORD: case G_HORD: case G_LSYNC:
        return K_SWITCH;
    default:
        return K_KNOB;
    }
}
static void disk(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t w = (int32_t)(fm_sqrtf((float)(r * r - dy * dy)) + 0.5f);
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static void ray(int32_t cx, int32_t cy, float deg, float r0, float r1, uint16_t c)   /* 0 deg: up, clockwise */
{
    float a = deg * 0.017453293f, sx = fm_sinf(a), sy = -fm_cosf(a);
    cv_line(cx + (int32_t)(sx * r0 + (sx < 0 ? -0.5f : 0.5f)), cy + (int32_t)(sy * r0 + (sy < 0 ? -0.5f : 0.5f)),
            cx + (int32_t)(sx * r1 + (sx < 0 ? -0.5f : 0.5f)), cy + (int32_t)(sy * r1 + (sy < 0 ? -0.5f : 0.5f)), c);
}
static void ctl_draw(uint32_t p, uint32_t v, int32_t cx, int32_t cy, int lit)
{
    uint32_t max = pmax(p), kind = ctl_kind(p), k;
    uint16_t hand = lit ? C_ACCENT : C_WHITE;   /* (white, as on the dark sheet; orange while turned) */
    if (kind == K_FADER) {                   /* a slot, a cap at the level, ticks beside */
        int32_t top = cy - 19, h = 38, at = top + h - (int32_t)(v * (uint32_t)h / (max ? max : 1u));
        for (k = 0; k <= 4u; k++)
            cv_rect(cx + 8, top + (int32_t)k * h / 4, 4, 1, C_GRAY);
        cv_rect(cx - 2, top, 4, h + 1, C_LINE);
        cv_rect(cx - 7, at - 2, 14, 5, hand);
    } else if (kind == K_SWITCH) {           /* a rounded slot, the dot at its position (the first at the top) */
        uint32_t n = max + 1u;
        int32_t gap = 11, h = gap * (int32_t)(n - 1u) + 12, top = cy - h / 2;
        cv_rect(cx - 5, top + 2, 10, h - 4, C_LINE);
        cv_rect(cx - 4, top + 1, 8, h - 2, C_LINE);
        for (k = 0; k < n; k++)
            cv_rect(cx + 8, top + 5 + (int32_t)k * gap, 3, 1, k == v ? C_GRAY : C_DIM);
        disk(cx, top + 6 + (int32_t)(v < n ? v : 0u) * gap, 4, hand);
    } else {                                 /* a knob: its ticks (a choice's positions), the cap, the pointer */
        int en = is_enum(pinfo(p)->type), bip = pinfo(p)->type == T_BIP;
        uint32_t n = en ? max + 1u : 11u;
        float step = en ? (270.0f / (float)(n - 1u) < 45.0f ? 270.0f / (float)(n - 1u) : 45.0f) : 27.0f;
        float a0 = -step * (float)(n - 1u) * 0.5f;
        for (k = 0; k < n; k++)
            ray(cx, cy, a0 + step * (float)k, 13.0f, (bip && k == 5u) || k == 0u || k + 1u == n ? 18.0f : 16.0f,
                en && k == v ? C_WHITE : C_GRAY);
        disk(cx, cy, 11, C_LINE);
        {   /* the pointer: three strokes wide */
            float a = en ? a0 + step * (float)(v < n ? v : 0u) : -135.0f + 270.0f * (float)v / (float)(max ? max : 1u);
            ray(cx, cy, a, 0.0f, 10.0f, hand);
            ray(cx + 1, cy, a, 0.0f, 10.0f, hand);
            ray(cx, cy + 1, a, 0.0f, 10.0f, hand);
        }
    }
}

static uint32_t cpu_peak;

/* a part of the screen out, then the keys and knobs read again: a redraw (a few ms) no longer holds a note back */
static void input(void);
static void blit_poll(int32_t x, int32_t y)
{
    cv_blit(x, y);
    input();
}

static void draw(void)
{
    int short_foot = 0;
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
    blit_poll(0, 0);
    /* the page: its title, and its four parameters (K1..K4), lit a moment after a turn */
    cv_begin(240, 136, C_BLACK);
    if (sv.on) {                             /* saving: the name (K1, K2), where (PRESET), and how */
        uint32_t c;
        cv_text(8, 0, &FONT_XS, "SAVE", C_ACCENT);
        cv_text(60, 0, &FONT_XS, "NAME  K1: MOVE  K2: LETTER", C_GRAY);
        for (c = 0; c < 8u; c++) {           /* the name, a letter a cell, the one K1 is on underlined */
            char ch[2] = {set.cur.name[c], 0};
            int32_t x = 8 + (int32_t)c * 28;
            if (ch[0] != ' ')
                cv_text(x + (24 - text_w(&FONT_M, ch)) / 2, 16, &FONT_M, ch, C_WHITE);
            cv_rect(x, 44, 24, c == sv.pos ? 3 : 1, c == sv.pos ? C_ACCENT : C_LINE);
        }
        cv_text(8, 56, &FONT_XS, sv.to == set.slot ? "INTO ITS OWN SLOT" : "INTO SLOT (REPLACES)", C_GRAY);
        slot_text(sv.to, t);
        cv_text(8, 72, &FONT_M, t, C_ACCENT);
        cv_text(8, 108, &FONT_XS, "PRESET: SLOT  SAVE: STORE", C_GRAY);
        cv_text(8, 122, &FONT_XS, "OTHER BUTTON: CANCEL", C_DIM);
        blit_poll(0, 36);
        goto foot;
    }
#if APP_SMK
    if (set.page == PG_ROLL) {               /* the piano roll: 16 steps of the pattern, the pitch upwards */
        /* the rows as a keyboard's: a white key's row grey, a black key's dark, a line where two white keys meet
         * (E-F, B-C), the keys drawn at the left and each C named; the step playing marked above, the step picked
         * framed. It takes the foot's room too (no meters here): drawn twice, the top 136 rows and the 68 below */
      uint32_t pass;
      for (pass = 0; pass < 2u; pass++) {
        const seqpat_t *pt = seq_pat();
        uint32_t L = seq_len(pt), win = roll_window(), lo = 127, hi = 0, st, nrow, rh;
        uint32_t cur = seq_running ? seq_cur_now : 0xFFu, pick = seq_rec && !seq_running ? seq_edit % NSTEP : roll_pick;
        static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
        int32_t gy = 18, gh = 166, gx = 34, cw = 12, pitch;
        char o[8];
        if (pass) {                          /* (the part below: the same drawing, 136 rows up) */
            cv_begin(240, 68, C_BLACK);
            cv_oy = -136;
            cv_y0 = 0;
            cv_y1 = 68;
        }
        if (pick < NSTEP) {                  /* the step picked: its number, note, velocity */
            int32_t x;
            t[0] = 0;
            text_cat(t, "STEP ");
            int_text((int32_t)pick + 1, o);
            text_cat(t, o);
            x = cv_text(4, 0, &FONT_XS, t, C_ACCENT);
            if (pick >= L || (!pt->vel[pick] && !(pt->flag[pick] & SF_TIE)))
                cv_text(x + 6, 0, &FONT_XS, "REST", C_GRAY);
            else if (pt->flag[pick] & SF_TIE)
                cv_text(x + 6, 0, &FONT_XS, "TIE", C_WHITE);
            else {
                note_name(pt->note[pick], t);
                x = cv_text(x + 6, 0, &FONT_XS, t, C_WHITE);
                t[0] = 0;
                text_cat(t, "VEL ");
                int_text(pt->vel[pick], o);
                text_cat(t, o);
                cv_text(x + 6, 0, &FONT_XS, t, C_GRAY);
            }
        } else {
            cv_text(4, 0, &FONT_XS, "PIANO ROLL", C_ACCENT);
        }
        t[0] = 0;
        text_cat(t, "PAT ");
        int_text((int32_t)set.g[G_SPAT - NP] % (int32_t)NPAT + 1, o);
        text_cat(t, o);
        text_cat(t, "  ");
        int_text((int32_t)win * 16 + 1, o);
        text_cat(t, o);
        text_cat(t, "-");
        int_text((int32_t)win * 16 + 16, o);
        text_cat(t, o);
        text_cat(t, "  LEN ");
        int_text((int32_t)L, o);
        text_cat(t, o);
        cv_text(236 - text_w(&FONT_XS, t), 0, &FONT_XS, t, C_GRAY);
        for (st = 0; st < L; st++)           /* the range: the pattern's notes, an octave at least, C to C */
            if (pt->vel[st] && !(pt->flag[st] & SF_TIE)) {
                lo = pt->note[st] < lo ? pt->note[st] : lo;
                hi = pt->note[st] > hi ? pt->note[st] : hi;
            }
        if (lo > hi)
            lo = 60, hi = 60;
        lo -= lo % 12u;                      /* (from a C up to the C above the highest) */
        hi = hi - hi % 12u + 12u;
        if (hi > 127u)
            hi = 127u;
        nrow = hi - lo + 1u;
        rh = (uint32_t)gh / nrow;
        if (rh < 2u)
            rh = 2u;
        gy += (gh - (int32_t)(rh * nrow)) / 2;
        for (pitch = (int32_t)hi; pitch >= (int32_t)lo; pitch--) {   /* the rows, and the keys at the left */
            int32_t y = gy + (int32_t)((hi - (uint32_t)pitch) * rh), blk = BLACK[pitch % 12];
            cv_rect(gx, y, cw * 16, (int32_t)rh, blk ? C_BLACK : C_LINE);
            cv_rect(gx - 12, y, 11, (int32_t)rh, C_GRAY);   /* (a white key the whole height) */
            if (blk)
                cv_rect(gx - 12, y, 7, (int32_t)rh, C_BLACK);
            if (pitch % 12 == 0 || pitch % 12 == 5)   /* (where two white keys meet: a line under F and C) */
                cv_rect(gx - 12, y + (int32_t)rh - 1, 11 + cw * 16, 1, C_BLACK);
            if (pitch % 12 == 0) {           /* the C's named */
                note_name(pitch, o);
                cv_text(1, y + (int32_t)rh / 2 - 5, &FONT_XS, o, C_GRAY);
            }
        }
        for (i = 0; i <= 16u; i += 4u)       /* the beats */
            cv_rect(gx + (int32_t)i * cw - 1, gy, 1, (int32_t)(rh * nrow), C_BLACK);
        pitch = -1;                          /* (a tie at the window's start: the note before it) */
        for (st = 0; st < win * 16u && st < L; st++)
            if (pt->vel[st] && !(pt->flag[st] & SF_TIE))
                pitch = pt->note[st];
            else if (!(pt->flag[st] & SF_TIE))
                pitch = -1;
        for (i = 0; i < 16u; i++) {
            int32_t x = gx + (int32_t)i * cw;
            st = win * 16u + i;
            if (st >= L) {                   /* past the pattern's end: dark */
                cv_rect(x, gy, cw, (int32_t)(rh * nrow), C_BLACK);
                continue;
            }
            if (st == cur)                   /* the step playing: a mark above */
                cv_rect(x + 1, gy - 4, cw - 2, 3, C_ACCENT);
            if (st == pick) {                /* the step picked: a frame */
                cv_rect(x, gy - 4, cw, 1, C_WHITE);
                cv_rect(x, gy + (int32_t)(rh * nrow) + 2, cw, 2, C_WHITE);
                cv_rect(x, gy - 4, 1, (int32_t)(rh * nrow) + 8, C_WHITE);
                cv_rect(x + cw - 1, gy - 4, 1, (int32_t)(rh * nrow) + 8, C_WHITE);
            }
            if (pt->flag[st] & SF_TIE) {     /* a tie: the note before, carried on */
                if (pitch >= 0 && (uint32_t)pitch >= lo && (uint32_t)pitch <= hi)
                    cv_rect(x - 1, gy + (int32_t)((hi - (uint32_t)pitch) * rh), cw, (int32_t)rh, st == cur ? C_WHITE : C_HI);
            } else if (pt->vel[st]) {
                pitch = pt->note[st];
                cv_rect(x + 1, gy + (int32_t)((hi - (uint32_t)pitch) * rh), cw - 2, (int32_t)rh,
                        st == cur ? C_WHITE : C_HI);
            } else {
                pitch = -1;
            }
        }
        cv_text(4, 192, &FONT_XS, "K5 NOTE  K6 VEL  K7 STEP  K8 TYPE  FX DEL", C_DIM);
        blit_poll(0, pass ? 172u : 36u);
      }
        return;
    }
    if (pf.on && PAGE[set.page].btn != B_SEQ) {   /* the SMK-37's panel: the eight knobs, the four faders (the foot
                                              * without its scope, for room) */
        short_foot = 1;
        cv_begin(240, 124, C_BLACK);
        cv_text(8, 0, &FONT_XS, "PANEL", C_ACCENT);
        t[0] = 0;
        text_cat(t, "FADERS ");
        {
            char b2[2] = {(char)('A' + pf.bank), 0};
            text_cat(t, b2);
        }
        text_cat(t, "  ");
        text_cat(t, PF_BANK[pf.bank]);
        cv_text(232 - text_w(&FONT_XS, t), 0, &FONT_XS, t, C_GRAY);
        for (i = 0; i < 8u; i++) {           /* two rows of four, as on the keyboard */
            uint32_t p = PK[i], v;
            int32_t cx = 30 + (int32_t)(i % 4u) * 60, cy = 32 + (int32_t)(i / 4u) * 52;
            int lit = pf.turned_at[i] && now - pf.turned_at[i] < 400u;
            if (p == PK_DECREL)              /* (drawn as a knob at the amp's decay) */
                p = P_RES, v = pget(P_AD) * 127u / pmax(P_AD);
            else
                v = pget(p);
            ctl_draw(p, v, cx, cy, lit);
            cv_text(cx - text_w(&FONT_XS, PK_NAME[i]) / 2, cy + 20, &FONT_XS, PK_NAME[i], lit ? C_WHITE : C_GRAY);
        }
        blit_poll(0, 36);
        cv_begin(240, 42, C_BLACK);
        for (i = 0; i < 4u; i++) {           /* the faders: a slot, the cap at the value (grey until caught) */
            uint32_t p = PF[pf.bank][i], v = pget(p), max = pmax(p);
            int32_t x = 6 + (int32_t)i * 60, top = 3, h = 36, at = top + h - (int32_t)(v * (uint32_t)h / (max ? max : 1u));
            int lit = pf.moved_at[i] && now - pf.moved_at[i] < 400u;
            uint16_t c = lit ? C_ACCENT : pf.caught[i] ? C_WHITE : C_GRAY;
            cv_rect(x + 4, top, 3, h + 1, C_LINE);
            if (pf.last[i] >= 0 && !pf.caught[i])   /* where the fader is, until it catches up */
                cv_rect(x + 9, top + h - pf.last[i] * h / 127, 3, 2, C_DIM);
            cv_rect(x, at - 1, 11, 3, c);
            cv_text(x + 15, top, &FONT_XS, PF_SHORT[pf.bank][i], C_GRAY);
            value_text(p, v, t);
            cv_text(x + 15, top + 14, &FONT_XS, t, lit ? C_WHITE : C_HI);
        }
        blit_poll(0, 160);
        goto foot;
    }
#endif
    cv_text(8, 0, &FONT_XS, pg->title, C_ACCENT);
    {   /* where the page is among its button's: a dot each, right-aligned */
        uint32_t n = 0, at = 0;
        for (i = 0; i < NPAGE; i++)
            if (PAGE[i].btn == pg->btn) {
                if (i == set.page)
                    at = n;
                n++;
            }
        for (i = 0; i < n; i++)
            cv_rect(232 - (int32_t)(n - i) * 10, 3, 6, 6, i == at ? C_ACCENT : C_LINE);
    }
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
        k[1] = (char)((APP_SMK ? '5' : '1') + i);   /* (the SMK-37's KNOB 1..4 are its K5..K8) */
        cv_rect(x, y, 114, 54, lit ? C_LINE : C_BLACK);
        if (lit)                             /* (the control's own ground, so its slot shows) */
            cv_rect(x + 2, y + 13, 38, 40, C_BLACK);
        cv_text(x + 3, y + 1, &FONT_XS, k, C_DIM);
        ctl_draw(p, v, x + 21, y + 33, lit);
        cv_text(x + 44, y + 3, &FONT_XS, pinfo(p)->name, C_GRAY);
        value_text(p, v, t);
        {   /* the value: as large as fits beside the control */
            const felucca_font_t *f = text_w(&FONT_B, t) <= 68 ? &FONT_B : text_w(&FONT_S, t) <= 68 ? &FONT_S : &FONT_XS;
            cv_text(x + 44, y + 24, f, t, lit ? C_WHITE : C_HI);
        }
    }
    blit_poll(0, 36);
foot:
    /* the foot: the wheel, the pressure, the octave, and the scope */
    cv_begin(240, short_foot ? 38 : 64, C_BLACK);
    cv_text(4, 2, &FONT_XS, "WHEEL", C_GRAY);
    bar(48, 6, 60, wheel_now, 127u, 0, C_HI);
#if APP_SMK
    cv_text(116, 2, &FONT_XS, "VOL", C_GRAY);   /* (the SMK-37's keys have no pressure: the volume) */
    bar(140, 6, 36, vol_pct(), 100u, 0, C_HI);
#else
    cv_text(118, 2, &FONT_XS, "AT", C_GRAY);
    bar(136, 6, 40, at_now, 127u, 0, C_HI);
#endif
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
    if (!short_foot) {
        uint32_t w = scope_w, j;
        int32_t prev = 0;
        for (j = 0; j < 232u; j++) {
            int32_t s = scope_cap[(w - 464u + 2u * j) % SCOPE_N], y = 47 - s * 15 / 32768;
            if (j)
                cv_line(3 + (int32_t)j, prev, 4 + (int32_t)j, y, C_DIM);
            prev = y;
        }
    }
    cv_blit(0, short_foot ? 202 : 172);
}

#include "splash_img.h"
void app_splash(void)                        /* the start-up screen (app/splash, tools/make_splash.py), band by band */
{
    uint32_t k = 0, left = 0, c = 0, y;
    for (y = 0; y < 240u; y += 16u) {
        uint32_t x, yy;
        cv_begin(240, 16, 0);
        for (yy = 0; yy < 16u; yy++)
            for (x = 0; x < 240u; x++) {
                if (!left && k < SPLASH_N) {
                    c = SPLASH_PAL[SPLASH_RLE[k] >> 4];
                    left = (SPLASH_RLE[k] & 15u) + 1u;
                    k++;
                }
                if (left)
                    left--;
                cv_pset((int32_t)x, (int32_t)yy, (uint16_t)c);
            }
        cv_blit(0, y);
    }
}

void app_ui_init(void)
{
    /* the site's dark look (tools/site_theme.py, the patch editor's dark sheet): black, the panel's grey, white
     * labels and ticks, orange values (its display's) and highlights */
    pal[0] = RGB(59, 59, 59);                /* C_LINE: the panel's grey #3b3b3b (slots, knob caps, rules) */
    pal[1] = RGB(110, 110, 110);             /* C_DIM: what is off, K1..K4 */
    pal[2] = RGB(236, 236, 236);             /* C_GRAY: the labels, the ticks: the sheet's white ink */
    pal[3] = RGB(166, 166, 166);             /* C_AMB: secondary text #a6a6a6 */
    pal[4] = RGB(240, 96, 40);               /* C_HI: the values, as the sheet's display has them: #f06028 */
    touch_on(1);
#if APP_SMK
    plat_smk_k1_raw(pf.on);                  /* (the panel at power-on: K1 a knob of it) */
    plat_smk_volume(50);                     /* (the volume at power-on: half) */
#endif
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
#if APP_SMK
    else if (!strcmp(what, "panel"))
        *got = pf.on;
    else if (!strcmp(what, "fbank"))
        *got = pf.bank;
    else if (!strncmp(what, "caught", 6))
        *got = pf.caught[(what[6] - '0') & 3];
    else if (!strcmp(what, "roll"))
        *got = set.page == PG_ROLL;
    else if (!strcmp(what, "rollpick"))
        *got = (int)roll_pick;
    else if (!strcmp(what, "seqedit"))
        *got = (int)seq_edit;
    else if (!strcmp(what, "vol"))
        *got = (int)vol_pct();
#endif
    else if (!strcmp(what, "xpose"))
        *got = seq_xpose;
    else if (!strcmp(what, "sustain"))
        *got = sus_on ? 127 : 0;
    else if (!strcmp(what, "bend"))
        *got = bend_now / 41;
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
    else if (!strcmp(what, "outpeak"))     /* the last block out, the louder side, x1000 */
        *got = (int)(out_peak * 1000.0f);
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
