/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* SECTIONS (Hookology, after SLOOP's sections by isod89; no SLOOP code): section A..H is pattern 1..8 on every
 * track in use. A section played while the transport runs waits for track 1's loop end, then every track starts its
 * section's pattern from step 0 together (as SONG's rows do, chain_tick); stopped, it is the loop at once. The UI
 * (ui_song_sections.c) copies a loop into a section and turns played sections into the song (the jam log, seq.c).
 * Included by song_chain.c, after the chain's state */
#define SEC_N 8u                                        /* sections A..H: patterns 1..8 (a MIDI-only track has 8) */
#define SEC_NONE 0xFFu
static struct {
    volatile uint8_t next;                              /* queued for track 1's loop end (SEC_NONE: nothing) */
    uint8_t rec;                                        /* SONG REC: the jam is being logged for the song */
    uint32_t carry;                                     /* samples past track 1's loop end, this block (seq_tick) */
} secs = {SEC_NONE, 0, 0};

static void pattern_switch(track_t *t, uint32_t bank);

/* power-on, a project loaded: nothing queued, no SONG REC (its jam belongs to the project before) */
static void sec_reset(void) { secs.next = SEC_NONE; secs.rec = 0; secs.carry = 0; }
/* the section every track in use plays (SEC_NONE: they differ, or past H) */
static uint32_t sec_playing(void)
{
    uint32_t k, n = trk_n(), b = trk[0].pattern;
    if (b >= SEC_N) return SEC_NONE;
    for (k = 1; k < n; k++)
        if (trk[k].pattern != b) return SEC_NONE;
    return b;
}
/* ISR, before the tracks advance: track 1 crossing its loop end with a section queued starts every track in use on
 * it from step 0, the samples past the end carried (seq_tick's start reads secs.carry) */
static void sec_tick(uint32_t n)
{
    track_t *t = &trk[0];
    uint32_t b = secs.next, k, nt, length;
    secs.carry = 0;
    if (b >= SEC_N || chain.running || chain.armed || !song.playing) return;
    if (t->seq_pos >= 0x7FFFFFFFu || t->seq_idx + 1u != (uint32_t)t->p[P_SLEN]) return;
    length = step_samples(t, seq_div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
    if (t->seq_pos + n < length) return;
    secs.carry = t->seq_pos + n - length;
    secs.next = SEC_NONE;
    for (k = 0, nt = trk_n(); k < nt; k++) {
        track_t *u = &trk[k];
        if (b != u->pattern) pattern_switch(u, b);
        else seq_release(u);
        u->pattern_next = 0xFFu;
        u->seq_idx = (uint16_t)(u->p[P_SLEN] - 1);
        u->seq_pos = 0x7FFFFFFFu;
        u->rh_n = u->rskip_n = 0;
    }
}
/* the bank pattern_poll prepares for track k: the queued section's */
static uint32_t sec_bank(uint32_t k, uint32_t bank)
{
    return secs.next < SEC_N && k < trk_n() ? secs.next : bank;
}
