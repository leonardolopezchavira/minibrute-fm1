/* SPDX-License-Identifier: MIT
 * The knobs' response: each parameter's value at 33 even steps of its knob (0, 1/32 .. 1), as the MiniBrute V
 * plugin shows it, so that MB-1's knob positions mean what the plugin's do. Read off the plugin's parameter
 * display during the measurements (tools: the capture rig, not in this repository). */
#pragma once
#define TP_N 33
static const float TP_CUT[TP_N] = {   /* filter_cutoff, Hz */
    20.0f, 30.2f, 42.6f, 57.5f, 75.5f, 97.1f, 123.0f, 155.0f, 193.0f, 239.0f, 294.0f, 361.0f, 441.0f, 539.0f, 656.0f, 797.0f, 968.0f, 1173.0f, 1421.0f, 1721.0f, 2081.0f, 2517.0f, 3042.0f, 3675.0f, 4440.0f, 5361.0f, 6473.0f, 7814.0f, 9432.0f, 11383.0f, 13737.0f, 16576.0f, 20000.0f};
static const float TP_ATT[TP_N] = {   /* filter_envelope_attack, s */
    0.001f, 0.007f, 0.015f, 0.024f, 0.036f, 0.049f, 0.066f, 0.085f, 0.109f, 0.138f, 0.172f, 0.214f, 0.265f, 0.325f, 0.399f, 0.487f, 0.594f, 0.722f, 0.878f, 1.06f, 1.29f, 1.56f, 1.89f, 2.29f, 2.77f, 3.34f, 4.04f, 4.88f, 5.89f, 7.11f, 8.58f, 10.4f, 12.5f};
static const float TP_DR[TP_N] = {   /* filter_envelope_decay, s */
    0.001f, 0.014f, 0.029f, 0.048f, 0.07f, 0.098f, 0.13f, 0.17f, 0.217f, 0.275f, 0.344f, 0.427f, 0.528f, 0.65f, 0.796f, 0.973f, 1.19f, 1.44f, 1.75f, 2.13f, 2.58f, 3.13f, 3.78f, 4.57f, 5.53f, 6.68f, 8.08f, 9.75f, 11.8f, 14.2f, 17.2f, 20.7f, 25.0f};
static const float TP_LFO[TP_N] = {   /* lfo_rate_free_hertz, Hz */
    0.05f, 0.072f, 0.1f, 0.135f, 0.178f, 0.231f, 0.298f, 0.381f, 0.484f, 0.612f, 0.772f, 0.971f, 1.22f, 1.53f, 1.91f, 2.39f, 2.98f, 3.72f, 4.64f, 5.78f, 7.21f, 8.98f, 11.2f, 13.9f, 17.3f, 21.6f, 26.9f, 33.5f, 41.7f, 51.9f, 64.6f, 80.3f, 100.0f};
static const float TP_USR[TP_N] = {   /* ultrasaw_rate, Hz */
    0.005f, 0.047f, 0.097f, 0.155f, 0.223f, 0.304f, 0.399f, 0.51f, 0.642f, 0.796f, 0.978f, 1.19f, 1.44f, 1.74f, 2.09f, 2.5f, 2.98f, 3.54f, 4.21f, 4.99f, 5.92f, 7.0f, 8.28f, 9.78f, 11.5f, 13.6f, 16.1f, 19.0f, 22.3f, 26.3f, 31.0f, 36.5f, 43.0f};
static const float TP_VIB[TP_N] = {   /* vibrato_rate, Hz */
    1.0f, 1.06f, 1.12f, 1.2f, 1.29f, 1.39f, 1.52f, 1.66f, 1.83f, 2.02f, 2.25f, 2.52f, 2.84f, 3.2f, 3.63f, 4.13f, 4.72f, 5.4f, 6.2f, 7.14f, 8.23f, 9.51f, 11.0f, 12.8f, 14.8f, 17.2f, 20.0f, 23.3f, 27.1f, 31.5f, 36.8f, 42.9f, 50.0f};
static const float TP_GLIDE[TP_N] = {   /* pitch_glide_amount_s, s/oct */
    0.0f, 0.002f, 0.005f, 0.008f, 0.012f, 0.016f, 0.021f, 0.027f, 0.034f, 0.042f, 0.051f, 0.062f, 0.075f, 0.09f, 0.107f, 0.128f, 0.152f, 0.18f, 0.212f, 0.251f, 0.295f, 0.347f, 0.408f, 0.48f, 0.563f, 0.661f, 0.775f, 0.908f, 1.06f, 1.25f, 1.46f, 1.71f, 2.0f};
static const float TP_DB[TP_N] = {   /* master_volume_db, dB */
    -60.0f, -54.3f, -49.2f, -44.5f, -40.3f, -36.4f, -32.8f, -29.6f, -26.7f, -24.0f, -21.6f, -19.4f, -17.4f, -15.5f, -13.9f, -12.3f, -10.9f, -9.68f, -8.54f, -7.49f, -6.54f, -5.67f, -4.88f, -4.17f, -3.51f, -2.92f, -2.37f, -1.88f, -1.43f, -1.02f, -0.648f, -0.309f, 0.0f};
