/* SPDX-License-Identifier: GPL-3.0-only */
/* KIT SAMPLE (drum_lanes.c's lanes): recorded acoustic drums, one sample per lane (tools/gen_drum_samples.py, CC0, the
 * cuts and levels after SLOOP's kit by isod89), IMA ADPCM read in place from flash. A resource-region sample is silent
 * until the region is installed. TUNE every drum; TONE below 64 a low-pass, above it a brighter top; DECY below 64 ends
 * the drum sooner, at 64 and above it plays out. Ghost and hard hits are velocities: the sample is the same.
 * ph[0]: the position (Q16), inc[0]: its step, ph[1]: samples decoded; k[0], k[1]: the decoder; m1: the sample before;
 * ng: TONE's boost; burst: DECY's envelope on */
typedef struct { const uint8_t *data; uint32_t n, rate, res; } smpk_t;
#include "melodee_samples.h"
#define SMPK_OUT 4096                            /* Q12: the kit level with the 808's */
#define SMPK_NONE 0xFFu
static const int16_t SMPK_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
static const int8_t SMPK_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
/* a drum's sample (DVT_*): the kit has eight; a conga is the tom, a clave the rim, a cymbal the open hat pitched down */
static const uint8_t SMPK_OF[DVT_COUNT] = {DV_KICK, DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_TOM,
                                           DV_RIM, DV_RIM, DV_BELL, DV_HATO};
#define SMPK_CYM_ST (-5)

static int smpk_hit(dlk_ch_t *c, uint32_t role, int32_t tune, const int16_t *q)
{
    const smpk_t *s = &SMPK[SMPK_OF[role]];
    int32_t tone = q[2];
    c->rec = s->res && !RES_OK() ? SMPK_NONE : SMPK_OF[role];
    if (role == DVT_CYM) tune += SMPK_CYM_ST * 16;
    c->inc[0] = (uint32_t)((float)s->rate * 65536.0f / (float)FS * dlk_exp2((float)tune / 192.0f));
    c->ph[0] = c->ph[1] = 0;
    c->k[0] = c->k[1] = 0;
    c->m1 = 0;
    c->ts = 0;
    c->tk = tone < 64 ? (tone + 4) * (tone + 4) * 7 / 2 : 6000;
    c->ng = tone > 64 ? (tone - 64) * 2 : 0;
    c->burst = q[3] < 64;
    if (c->burst) dlk_set(&c->e[0], 30u + (uint32_t)q[3] * 6u, 0);
    return 1;
}

/* the lane's drum, one block into y; 0 once it has played */
static __attribute__((noinline)) int smpk_run(dlk_ch_t *restrict c, int32_t *y, uint32_t n)
{
    if (c->rec == SMPK_NONE) return 0;
    const smpk_t *s = &SMPK[c->rec];
    const uint8_t *d = s->data;
    uint32_t pos = c->ph[0], at = c->ph[1], len = s->n, inc = c->inc[0], i;
    int32_t pred = c->k[0], idx = c->k[1], prev = c->m1, tk = c->tk, boost = c->ng, ts = c->ts, e0 = 1 << 24, de = 0, live = 1;
    if (c->burst) de = dre_next(&c->e[0], &e0);
    for (i = 0; i < n; i++) {
        uint32_t want = (pos >> 16) + 2u;
        while (at < want && at < len) {          /* a decoded sample: pred; the one before it: prev */
            uint32_t code = (uint32_t)(d[at >> 1] >> ((at & 1u) * 4u)) & 15u;
            int32_t step = SMPK_STEP[idx], vd = step >> 3;
            if (code & 4u) vd += step;
            if (code & 2u) vd += step >> 1;
            if (code & 1u) vd += step >> 2;
            prev = pred;
            pred = clamp(code & 8u ? pred - vd : pred + vd, -32768, 32767);
            idx = clamp(idx + SMPK_IDX[code & 7u], 0, 88);
            at++;
        }
        if (at < want) {
            live = 0;
            break;
        }
        {
            int32_t f = (int32_t)((pos & 0xFFFFu) >> 1), v = prev + (((pred - prev) * f) >> 15);
            ts += ((v - ts) * tk) >> 14;
            v = tk >= 16384 ? v : boost ? v + (((v - ts) * boost) >> 6) : ts;
            if (c->burst) v = (v * DRE_AT(e0, de, i)) >> 15;
            y[i] += (v * SMPK_OUT) >> 12;
        }
        pos += inc;
    }
    c->ph[0] = pos;
    c->ph[1] = at;
    c->k[0] = pred;
    c->k[1] = idx;
    c->m1 = prev;
    c->ts = ts;
    return live && (!c->burst || c->e[0].e >= DRE_LOW);
}
