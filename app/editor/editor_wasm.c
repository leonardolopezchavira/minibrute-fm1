/* SPDX-License-Identifier: MIT
 * MB-1's engine in the browser, for the editor (app/editor/index.html): the voice (mbvoice), the effects (mbfx) and
 * the patch's mapping (mbpatch.h), the very files the firmware builds, so a patch sounds here as on the FM-1. The
 * page runs it in an AudioWorklet: ed_render fills a block, the page copies it out. Not here: the arpeggiator and
 * the sequencer (the FM-1's), and the second core's block (the delay and reverb hear the same block here).
 *
 * Built by tools/build_editor.sh: clang --target=wasm32, no libc (fastmath.h in place of libm). */
#include <stddef.h>
#include <stdint.h>
#include "fastmath.h"
#include "mbvoice.h"
#include "mbfx.h"
#include "mb_tapers.h"
#include "mbpatch.h"

#define EXPORT __attribute__((visibility("default")))

/* (the compiler may call these for a struct's copy or zeroing) */
void *memset(void *d, int c, size_t n)
{
    unsigned char *p = (unsigned char *)d;
    while (n--)
        *p++ = (unsigned char)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--)
        *p++ = *q++;
    return d;
}

#define BLOCK 128
static MBVoice voice;
static MBFx fx;
static int16_t fx_mem[MBFX_MEM];
static MBParams par;
static MBFxParams fxp;
static float fx_beats;
static uint8_t v[NP];
static float tempo = 120.0f;
static int lsync;
static float buf[BLOCK], pl[BLOCK], pr[BLOCK], wl[BLOCK], wr[BLOCK], mono[BLOCK];
static float out[2 * BLOCK];
static char text[32];
static float sr = 48000.0f;

static float ceiling(float y)                /* app.c's: a soft ceiling above -2 dB */
{
    float a = fm_fabsf(y);
    if (a > 0.8f)
        y = (y > 0.0f ? 1.0f : -1.0f) * (0.8f + 0.2f * fm_tanhf((a - 0.8f) * 5.0f));
    return y;
}

static void apply(void)
{
    mb_params_core(v, &par);
    if (lsync)
        par.lfo_rate_hz = tempo / 60.0f / lsync_beats(v[P_LRATE]);
    voice.p = par;
    fx_params_from(v, &fxp, &fx_beats);
    if (fx_beats > 0.0f)
        fxp.d_time_s = fx_beats * 60.0f / tempo;
    mbfx_set(&fx, &fxp);
}

EXPORT void ed_init(float rate)
{
    uint32_t i;
    sr = rate;
    mb_init(&voice, rate);
    mbfx_init(&fx, rate, fx_mem);
    for (i = 0; i < NP; i++)
        v[i] = INIT_V[i];
    apply();
}

/* the parameters, for the page: how many, their names, types, ranges, CCs, INIT, and a value as the screen reads it */
EXPORT int ed_np(void) { return NP; }
EXPORT const char *ed_name(int i) { return PARAM[i].name; }
EXPORT int ed_type(int i) { return PARAM[i].type; }
EXPORT int ed_max(int i) { return (int)pmax((uint32_t)i); }
EXPORT int ed_cc(int i) { return PARAM[i].cc; }
EXPORT int ed_init_v(int i) { return INIT_V[i]; }
EXPORT const char *ed_text(int i, int x, int dsync)
{
    mb_value_text((uint32_t)i, (uint32_t)x, lsync, dsync, text);
    return text;
}
EXPORT int ed_nfactory(void) { return (int)NFACTORY; }
EXPORT const char *ed_factory(int slot, uint8_t *dst)   /* its values into dst (NP bytes); its name back */
{
    static patch_t p;
    uint32_t i;
    patch_factory(&p, (uint32_t)slot);
    for (i = 0; i < NP; i++)
        dst[i] = p.v[i];
    for (i = 0; i < 8u; i++)
        text[i] = p.name[i];
    text[8] = 0;
    return text;
}
EXPORT uint8_t *ed_values(void) { return v; }   /* the page writes the patch here, then ed_apply */
EXPORT void ed_apply(void) { apply(); }
EXPORT void ed_set(int i, int x)
{
    if (i < 0 || i >= (int)NP)
        return;
    v[i] = (uint8_t)(x < 0 ? 0 : x > (int)pmax((uint32_t)i) ? (int)pmax((uint32_t)i) : x);
    apply();
}
EXPORT void ed_tempo(float bpm, int lfo_on_clock)
{
    tempo = bpm;
    lsync = lfo_on_clock;
    apply();
}

/* playing */
EXPORT void ed_note_on(int note, int vel) { mb_note_on(&voice, note, vel); }
EXPORT void ed_note_off(int note) { mb_note_off(&voice, note); }
EXPORT void ed_all_off(void) { mb_all_off(&voice); }
EXPORT void ed_wheel(float w) { mb_set_wheel(&voice, w); }
EXPORT void ed_aftertouch(float a) { mb_set_aftertouch(&voice, a); }
EXPORT void ed_bend(float st) { mb_set_bend(&voice, st); }
EXPORT void ed_panic(void)
{
    mb_all_off(&voice);
    mb_init(&voice, sr);
    mbfx_clear(&fx);
    apply();
}

/* a block (up to 128 samples), as app_render: the voice; the distortion and chorus; the delay's and reverb's wet
 * added; the compressor; the ceiling. Out: L R interleaved */
EXPORT int ed_block(void) { return BLOCK; }
EXPORT float *ed_render(int n)
{
    int i;
    float dry;
    if (n > BLOCK)
        n = BLOCK;
    mb_render(&voice, buf, n);
    mbfx_pre(&fx, &fxp, buf, pl, pr, n);
    for (i = 0; i < n; i++) {
        mono[i] = 0.5f * (pl[i] + pr[i]);
        wl[i] = wr[i] = 0.0f;
    }
    mbfx_process(&fx, mono, wl, wr, n);
    dry = mbfx_dry_gain(&fxp);
    for (i = 0; i < n; i++) {
        pl[i] = dry * pl[i] + wl[i];
        pr[i] = dry * pr[i] + wr[i];
    }
    mbfx_comp(&fx, &fxp, pl, pr, n);
    for (i = 0; i < n; i++) {
        out[2 * i] = ceiling(pl[i]);
        out[2 * i + 1] = ceiling(pr[i]);
    }
    return out;
}
