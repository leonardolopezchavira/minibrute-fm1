/* SPDX-License-Identifier: MIT
 * MB-1: a monophonic Minibrute-style synth for the M-VAVE FM-1 (the mbvoice engine on fm1-base). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "MB-1"
#define APP_SPLASH_RGB RGB(240, 96, 40)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_SYSEX                        /* patches over USB SysEx (app.c: the loader) */
int app_sysex(const uint8_t *p, uint32_t n);

/* the store: the settings (with the patch being played), the panel calibration and Bluetooth's (the platform's),
 * and the 16 patch slots (in the data area, its first sector) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_BANK, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, 0x97000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

/* the host simulator's checks: note, slot, page, dirty, wheel, at, octave, pN (parameter N's value 0..127) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
