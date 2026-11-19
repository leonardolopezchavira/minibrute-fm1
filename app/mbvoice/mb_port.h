/* SPDX-License-Identifier: MIT
 * What the mbvoice engine needs from the C library, in one place. Desktop builds (the measurement rig) use <math.h>;
 * firmware builds (fm1-base: no libm, a float-only FPU) define MB_FASTMATH and use fm1-base's fastmath.h. */
#ifndef MB_PORT_H
#define MB_PORT_H
#include <stddef.h>

#ifdef MB_FASTMATH
#include "fastmath.h"
#define mb_sinf fm_sinf
#define mb_cosf fm_cosf
#define mb_tanf fm_tanf
#define mb_expf fm_expf
#define mb_exp2f fm_exp2f
#define mb_log2f fm_log2f
#define mb_sqrtf fm_sqrtf
#define mb_floorf fm_floorf
#define mb_fabsf fm_fabsf
#define mb_fmaxf fm_maxf
#define mb_fminf fm_minf
#define mb_powf_ fm_powf
#else
#include <math.h>
#define mb_sinf sinf
#define mb_cosf cosf
#define mb_tanf tanf
#define mb_expf expf
#define mb_exp2f exp2f
#define mb_log2f log2f
#define mb_sqrtf sqrtf
#define mb_floorf floorf
#define mb_fabsf fabsf
#define mb_fmaxf fmaxf
#define mb_fminf fminf
#define mb_powf_ powf
#endif

static inline float mb_powf(float x, float y) { return x > 0.0f ? mb_powf_(x, y) : 0.0f; }   /* (x <= 0: 0) */

static inline void mb_zero(void *p, size_t n)
{
    unsigned char *d = (unsigned char *)p;
    while (n--)
        *d++ = 0;
}
static inline void mb_copy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
}
#endif
