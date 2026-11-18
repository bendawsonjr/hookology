/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* (Hookology beta69) EQ: a track's INSERT, three bands at the full rate, on each side: a LOW shelf (120 Hz), a MID bell at
 * FREQ (200 Hz .. 5 kHz, Q 0.9) and a HIGH shelf (6 kHz), each -12 .. +12 dB (the RBJ cookbook's biquads, transposed
 * direct form II in float). The slot's values: MIX LOW, RATE MID, CHAR FREQ (64 the middle: flat, 1 kHz), its TONE HIGH
 * (-63 .. 63, 0 flat). The state is the slot's fx_state_t (q[0..2] the coefficients, history[side][0..2]); nothing from
 * the arena. A new EQ fades in over a block and out again when it is changed or taken away, as the other inserts.
 * Included by fx.c */
#define FX_EQ_LOW_HZ 120.0f
#define FX_EQ_HIGH_HZ 6000.0f
#define FX_EQ_Q 0.9f
/* (fx_eq_db10, fx_eq_hz: params.c, which the display shares) */
/* one biquad, normalised: kind 0 the low shelf, 1 the bell, 2 the high shelf */
static void fx_eq_design(fx_coeff_t *c, uint32_t kind, float hz, int32_t db10)
{
    float w = 2.0f * FM_PI * hz / (float)FS, cs = fm_cosf(w), sn = fm_sinf(w);
    float a = fm_exp2f((float)db10 * (0.0830482f / 10.0f));             /* 10^(dB / 40) */
    float b0, b1, b2, a0, a1, a2;
    if (kind == 1u) {
        float al = sn / (2.0f * FX_EQ_Q);
        b0 = 1.0f + al * a; b1 = -2.0f * cs; b2 = 1.0f - al * a;
        a0 = 1.0f + al / a; a1 = -2.0f * cs; a2 = 1.0f - al / a;
    } else {
        float sa = 2.0f * fm_sqrtf(a) * (sn * 0.70710678f);           /* 2 sqrt(A) alpha, the slope S = 1 */
        float p = a + 1.0f, m = a - 1.0f;
        if (kind == 0u) {
            b0 = a * (p - m * cs + sa); b1 = 2.0f * a * (m - p * cs); b2 = a * (p - m * cs - sa);
            a0 = p + m * cs + sa; a1 = -2.0f * (m + p * cs); a2 = p + m * cs - sa;
        } else {
            b0 = a * (p + m * cs + sa); b1 = -2.0f * a * (m + p * cs); b2 = a * (p + m * cs - sa);
            a0 = p - m * cs + sa; a1 = 2.0f * (m - p * cs); a2 = p - m * cs - sa;
        }
    }
    c->b0 = b0 / a0; c->b1 = b1 / a0; c->b2 = b2 / a0; c->a1 = a1 / a0; c->a2 = a2 / a0;
}
/* fx_left / fx_right through the bands, faded from the dry by the slot's mix (f->mix -> target over the block);
 * p: the slot's TYPE MIX RATE CHAR, tone its TONE */
static __attribute__((noinline,section(".audio_text"))) void fx_eq_run(fx_state_t *restrict f, const int16_t *p, int32_t tone, uint32_t n,
                                                                    int32_t target)
{
    int32_t lo = fx_eq_db10(p[1], 0), mid = fx_eq_db10(p[2], 0), hi = fx_eq_db10(tone, 1);
    uint32_t fr = (uint32_t)clamp(p[3], 0, 127);
    uint32_t key = (uint32_t)(lo + 128) | (uint32_t)(mid + 128) << 8 | fr << 16 | (uint32_t)(hi + 128) << 24;
    if (key != f->pos || !f->age) {                    /* a knob turned (or a fresh state): the bands again */
        fx_eq_design(&f->q[0], 0u, FX_EQ_LOW_HZ, lo);
        fx_eq_design(&f->q[1], 1u, fx_eq_hz((int32_t)fr), mid);
        fx_eq_design(&f->q[2], 2u, FX_EQ_HIGH_HZ, hi);
        f->pos = key;
        f->age = 1;
    }
    int32_t m0 = f->mix, dm = n ? (target - m0) / (int32_t)n : 0;
    for (uint32_t ch = 0; ch < 2u; ch++) {
        int32_t *b = ch ? fx_right : fx_left;
        fx_history_t *h = f->history[ch];
        int32_t m = m0;
        for (uint32_t i = 0; i < n; i++) {
            float x = (float)b[i];
            for (uint32_t k = 0; k < 3u; k++) {        /* transposed direct form II */
                const fx_coeff_t *c = &f->q[k];
                float y = c->b0 * x + h[k].z1;
                h[k].z1 = c->b1 * x - c->a1 * y + h[k].z2;
                h[k].z2 = c->b2 * x - c->a2 * y;
                x = y;
            }
            int32_t wet = (int32_t)fx_bound(x, 884000.0f);
            m += dm;
            b[i] = b[i] + (int32_t)(((int64_t)(wet - b[i]) * (i + 1u == n ? target : m)) >> 15);
        }
    }
    f->mix = target;
}
