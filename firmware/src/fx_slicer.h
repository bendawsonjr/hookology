/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Ported from SLOOP by isod89 (https://github.com/isod89/sloop-fm1, firmware/src/slicer.c at bac9c05), whose file
 * carries the notice above. */
/* (Hookology, beta51) SLICER: GATE and STUTTER, a track's INSERTs (fx.c fx_insert), ported from SLOOP's slicer.c: a
 * tempo-synced 16-step gate / stutter on the track's signal, before its LEVEL, PAN and sends.
 *   the slot's MIX   DEPTH: GATE how far a '.' step closes (100 % = silent); STUTTER the level of the repeat
 *   its RATE         the step: 1/8, 1/16, 1/32, 8T, 16T, 32T (FX_SL_DEN)
 *   its CHAR         the PATTERN, one of SLOOP's 16 ('x' live / open, '.' gated / repeated)
 * The kernel makes the full effect; the slot's MIX fade (fx_return's) gives DEPTH, so on, off and DEPTH all glide.
 * GATE: the gain moves at most 1 / FX_SL_RAMP a sample (2.9 ms open to closed): a closing ramp ends on the step
 *   boundary, an opening one starts on it.
 * STUTTER: an 'x' step plays live and is recorded (mono, 22.05 kHz, 16 bit, FX_SL_LEN samples: 186 ms, 8 KB from the
 *   arena while chosen); a '.' step plays that recording from its start, looped at the step length (halved until
 *   it fits), every pass windowed (FX_SL_RAMP at both ends and at the step end).
 * The step clock: the track's song clock (seq.c seq_clock, musical units, 0 at PLAY with the sequencer's step 0) while
 * it plays, so step 1 of the pattern is the track's step 1 and the track's + the global swing apply as the sequencer
 * has them (core.h swing_step_len); stopped, a free-running clock at the tempo, so the gate is heard while you play. */
#ifndef MIDI_BEAT_UNITS
#define MIDI_BEAT_UNITS 24576u                            /* (mix.c, the same) */
#endif
#define FX_SL_NPAT 16u
#define FX_SL_LEN 4096u
#define FX_SL_RAMP_LOG2 7
#define FX_SL_RAMP (1 << FX_SL_RAMP_LOG2)   /* 128 samples, 2.9 ms */
#define FX_SL_SLOPE (32768 / FX_SL_RAMP)
/* bit k = step k: 1 = 'x' (open / live), 0 = '.' (closed / repeat): SLOOP's 16 */
static const uint16_t FX_SL_PAT[FX_SL_NPAT] = {
    0x5555, 0xB6DB, 0x5249, 0xB777, 0x6D6D, 0x0F0F, 0x1111, 0x54A5,
    0x5333, 0xA929, 0xAAFF, 0xD501, 0xAAAA, 0xAB6B, 0xBB5D, 0x7597,
};
static const uint8_t FX_SL_DEN[6] = {2, 4, 8, 3, 6, 12};   /* RATE: a step = 1 / DEN beats */
static int fx_slicer_type(int32_t t) { return t == FX_GATE || t == FX_STUTTER; }
typedef struct {
    uint32_t free, free_rem;     /* the clock while stopped (units), its remainder (units x beat samples) */
    uint32_t rp, loop;           /* repeat: read position, loop length (44.1 kHz samples), 0 = none */
    uint32_t rec, half_n;        /* recorded (22.05 kHz samples); samples taken this step */
    int32_t gc, w, half;         /* gate closure, repeat level (Q15); the first sample of a recorded pair */
    uint8_t idx, bit, rec_on, primed;
} fx_slicer_t;
static uint32_t fx_slicer_bytes(int32_t t) { return sizeof(fx_slicer_t) + (t == FX_STUTTER ? FX_SL_LEN * 2u : 0u); }
static uint32_t fx_slicer_pattern(int32_t character) { return FX_SL_PAT[(uint32_t)clamp(character, 0, FX_SL_NPAT - 1)]; }
/* the step idx of a pattern starts: its bit latched, STUTTER records it or loops the last live one */
static void fx_slicer_enter(fx_slicer_t *s, uint32_t idx, uint32_t pat, uint32_t stut, uint32_t step_len)
{
    s->idx = (uint8_t)idx;
    s->bit = (uint8_t)((pat >> idx) & 1u);
    s->rp = s->loop = 0;
    s->rec_on = 0;
    if (!stut) s->rec = 0;
    else if (s->bit) { s->rec = 0; s->rec_on = 1; s->half_n = 0; }
    else if (s->rec >= 2u * FX_SL_RAMP) {
        uint32_t l = step_len;
        while (l > 2u * FX_SL_LEN) l >>= 1;
        s->loop = l < 2u * s->rec ? l : 2u * s->rec;
    }
}
/* m samples of one step (no boundary inside) from i; left0: samples to the step end at its first */
static __attribute__((noinline,section(".audio_text"))) void fx_slicer_seg(fx_slicer_t *s, int16_t *buf, uint32_t stut,
    uint32_t nbit, int32_t *l, int32_t *r, uint32_t m, uint32_t left0)
{
    int32_t tc = !stut && !s->bit ? 32767 : 0, tn = !stut && !nbit ? 32767 : 0, tw = stut && s->loop ? 32767 : 0;
    for (uint32_t j = 0; j < m; j++) {
        int32_t xl = l[j], xr = r[j], tg = tc;
        uint32_t left = left0 > j ? left0 - j : 1u;
        if (left <= (uint32_t)FX_SL_RAMP && tn > tg) tg = tn;   /* (close by the boundary) */
        s->gc += clamp(tg - s->gc, -FX_SL_SLOPE, FX_SL_SLOPE);
        s->w += clamp(tw - s->w, -FX_SL_SLOPE, FX_SL_SLOPE);
        if (s->gc) { xl -= mulq16(xl, (uint32_t)s->gc << 1); xr -= mulq16(xr, (uint32_t)s->gc << 1); }
        if (s->rec_on) {                                  /* 2:1, the pair's mean, mono */
            int32_t x = (l[j] >> 1) + (r[j] >> 1);
            if ((s->half_n++) & 1u) {
                buf[s->rec] = (int16_t)clamp((s->half + x) >> 3, -32768, 32767);
                if (++s->rec >= FX_SL_LEN) s->rec_on = 0;
            } else s->half = x;
        }
        if (s->loop) {                                    /* the repeat, windowed, read between samples */
            uint32_t rp = s->rp, e = s->loop - 1u - rp;
            int32_t y = buf[rp >> 1];
            if (rp & 1u) y = (y + buf[((rp >> 1) + 1u) & (FX_SL_LEN - 1u)]) >> 1;
            y <<= 2;
            if (rp < e) e = rp;
            if (left - 1u < e) e = left - 1u;
            if (e < (uint32_t)FX_SL_RAMP) y = (y * (int32_t)e) >> FX_SL_RAMP_LOG2;
            if (++s->rp >= s->loop) s->rp = 0;
            xl += mulq16(y - xl, (uint32_t)s->w << 1);
            xr += mulq16(y - xr, (uint32_t)s->w << 1);
        } else if (s->w) {
            xl -= mulq16(xl, (uint32_t)s->w << 1);
            xr -= mulq16(xr, (uint32_t)s->w << 1);
        }
        l[j] = xl; r[j] = xr;
    }
}
/* the step that a clock position (units) is in, and the units left in it */
static uint32_t fx_slicer_locate(const track_t *t, uint32_t base, uint32_t pos, uint32_t *left)
{
    uint32_t a = swing_step_len(t, base, 0), b = swing_step_len(t, base, 1), pair = a + b;
    uint32_t q = pos % (pair * (FX_SL_NPAT / 2u)), idx = q / pair * 2u, rem = q % pair;
    if (rem >= a) { idx++; rem -= a; }
    *left = (idx & 1u ? b : a) - rem;
    return idx;
}
/* n samples of fx_left / fx_right through the SLICER at full depth, then MIX's fade from the dry (DEPTH) */
static __attribute__((noinline)) void fx_slicer_run(fx_state_t *f, void *mem, const track_t *t, uint32_t n, int32_t target)
{
    fx_slicer_t *s = mem;
    int16_t *buf = (int16_t *)(s + 1);
    uint32_t stut = f->type == FX_STUTTER, pat = fx_slicer_pattern(f->character);
    uint32_t base = MIDI_BEAT_UNITS / FX_SL_DEN[(uint32_t)clamp(f->rate, 0, 5)];
    uint32_t bs = beat_samples(), whole = n * MIDI_BEAT_UNITS, units;
    if (!bs) bs = 1u;
    uint32_t us8 = MIDI_BEAT_UNITS * 256u / bs;                                     /* (units a sample, Q8) */
    uint32_t pos, left_u;
    if (song.playing && t->seq_pos < 0x7FFFFFFFu) {   /* (the clock is at this block's end: events_block ran) */
        units = whole / bs;
        pos = t->seq_clock >= units ? t->seq_clock - units : 0u;
    } else {
        pos = s->free;
        units = (whole + s->free_rem) / bs;
        s->free_rem = (whole + s->free_rem) % bs;
        s->free += units;
    }
    for (uint32_t i = 0; i < n; i++) { fx_dry_l[i] = fx_left[i]; fx_dry_r[i] = fx_right[i]; }
    uint32_t idx = fx_slicer_locate(t, base, pos, &left_u);
    if (s->primed && idx == ((s->idx + 15u) & 15u) && left_u * 256u <= 2u * us8) {
        idx = s->idx;                                     /* (the last block entered it a sample or two early: stay) */
        left_u = swing_step_len(t, base, idx);
    }
    if (!s->primed || idx != s->idx) {
        s->primed = 1;
        fx_slicer_enter(s, idx, pat, stut, (idx & 1u ? swing_step_len(t, base, 1) : swing_step_len(t, base, 0)) * 256u / (us8 ? us8 : 1u));
    }
    for (uint32_t i = 0; i < n;) {
        uint32_t left = us8 ? (left_u * 256u + us8 - 1u) / us8 : n;   /* samples to the boundary */
        uint32_t m = left < n - i ? left : n - i;
        if (!m) m = 1;
        fx_slicer_seg(s, buf, stut, (pat >> ((s->idx + 1u) & 15u)) & 1u, fx_left + i, fx_right + i, m, left);
        i += m;
        if (m >= left) {                                  /* the next step */
            uint32_t next = (s->idx + 1u) & 15u, len_u = swing_step_len(t, base, next);
            fx_slicer_enter(s, next, pat, stut, us8 ? len_u * 256u / us8 : len_u);
            left_u = len_u;
        } else left_u = left_u > m * us8 / 256u ? left_u - m * us8 / 256u : 0u;
    }
    int32_t mix = f->mix;                                 /* DEPTH: from the dry to the full effect */
    for (uint32_t i = 0; i < n; i++) {
        if (mix != target) mix += clamp(target - mix, -128, 128);
        fx_left[i] = fx_dry_l[i] + fx_scale(fx_left[i] - fx_dry_l[i], (uint32_t)mix * 2u);
        fx_right[i] = fx_dry_r[i] + fx_scale(fx_right[i] - fx_dry_r[i], (uint32_t)mix * 2u);
    }
    f->mix = mix;
}
