/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chord keys (SCL > CHORD: P_CHRD, P_VOIC): one key, MIDI note or arp input plays a chord. Included by seq.c
 * after kb_map; runs in the audio ISR with the rest of the note input.
 *   CHRD OFF; DIA3 / DIA7: the triad / seventh of the track's scale (ROOT, SCALE) built on the key, every tone
 *        in key (a key outside the scale takes the scale note below it, as QNT SNAP does; CHR builds on the
 *        major scale of ROOT); MAJ MIN DOM7 MAJ7 MIN7 SUS4 POW: that shape on the key, whatever the scale.
 *        HERA: the chord set P_CHSET gives the key (melodee_hera.h, HERA's 100 sets), its notes as the set lists them.
 *   VOIC CLOSE (root position); OPEN (the second tone an octave up: 1-5-3); INV1 / INV2 (the lowest tone, then
 *        the next, an octave up); +OCT (the root an octave down added; a seventh drops its fifth). At most
 *        CHORD_MAX notes, notes outside 0..127 left out.
 * The key is mapped first (kb_map: QNT SNAP / WHITE, TRN, the octave), the chord is built on the note it gives.
 * MONO / LEGATO / UNISON parts play the chord's root only; a kit (the DRUM engine, SAMPLE PERC, slices: an
 * engine that maps the keys itself) ignores CHRD. Every source keeps the notes it started (kb_chord for a
 * key, mchord for a MIDI note) and its release ends exactly those, so CHRD / VOIC changed while it is held
 * leave nothing hanging. A note several sources hold sounds once and ends with the last of them. */
/* Append modes: these values are stored in songs, presets and editor files. */
enum { CH_OFF, CH_DIA3, CH_DIA7, CH_MAJ, CH_MIN, CH_DOM7, CH_MAJ7, CH_MIN7, CH_SUS4, CH_POW, CH_AUTO, CH_MELODY, CH_HERA = CHRD_HERA };
enum { VC_CLOSE, VC_OPEN, VC_INV1, VC_INV2, VC_BASS, VC_LEAD };
/* EXTRA (P_CHEXT, Hookology): a colour on a DIA3 / DIA7 / fixed-shape chord (chord_extra) */
enum { EX_OFF, EX_6, EX_9, EX_69, EX_SUS2, EX_SUS4 };
#define CHORD_MAX 4u
_Static_assert(CH_HERA == CH_MELODY + 1, "CHRD: stored values");
#include "melodee_hera.h"                /* tools/gen_hera_chords.py */
#define MCHORD_N 24u                     /* MIDI notes held as chords at once (more: their root alone) */

static const int8_t CHORD_SHAPE[CH_POW - CH_MAJ + 1][CHORD_MAX] = {
    {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 5, 7, -1}, {0, 7, 12, -1},
};

static uint8_t kb_chord[27][CHORD_MAX], kb_chn[27];   /* the notes key k started (kb_chn 0: none) */
/* Pending tones are separate from started ownership: early release never emits
 * an unmatched note-off or lets an unplayed tone keep another source alive. */
/* rate: bits 0-2 the division (N_STRUM), (Hookology) bit 3 RANGE 2 OCT, bit 4 the stroke went up, bits 5-7 the chord's
 * own notes (with RANGE 2 OCT its strings are those and an octave above them: chord_tone_at); slop: bits 4-7 SLOP
 * 0..15, bits 0-3 the press's seed (chord_strum_due). In the struct's spare byte: no RAM to grow it */
typedef struct { uint32_t pos, beat; uint8_t total, rate, vel, slop; } chord_strum_t;
#define STRUM_DIV(s) ((s)->rate & 7u)
#define STRUM_OCT2 8u
#define STRUM_UP 16u
/* string i of a strum over notes (Hookology: RANGE 2 OCT, the chord's own notes and an octave above, in the stroke's
 * order) */
static uint32_t chord_tone_at(const uint8_t *notes, const chord_strum_t *s, uint32_t i)
{
    if (!(s->rate & STRUM_OCT2)) return notes[i];
    uint32_t n = (uint32_t)s->rate >> 5, hi = (s->rate & STRUM_UP) ? i < n : i >= n, x = notes[i % (n ? n : 1u)] + (hi ? 12u : 0u);
    return x < 128u ? x : notes[i % (n ? n : 1u)];
}

static chord_strum_t kb_strum[27];
static uint32_t kb_strum_pending, midi_strum_pending;
static uint8_t chord_alternate[NTRK];
typedef struct { uint8_t id, ch, src, n, note[CHORD_MAX]; chord_strum_t strum; } mchord_t;   /* id: track + 1, 0 = free */
static mchord_t mchord[MCHORD_N];
/* the last chord played per track (the CHORD page shows it): its root, notes, and the shape's tones above the
 * root (bit i = i semitones, 0..11) */
static struct { uint8_t root, n, note[CHORD_MAX]; uint16_t mask; uint8_t gen, hera, set; } chord_last[NTRK];
/* VOIC LEAD (Hookology): chord_last[k] is the chord to lead from; 0 after PLAY, STOP and panic (root position) */
static uint8_t chord_lead_ok[NTRK];
static void chord_lead_forget(void) { memset(chord_lead_ok, 0, sizeof chord_lead_ok); }

/* 1 = the track's keys are a kit: no chords */
static int chord_kit(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e->oneshot || (e->keys && e->keys(t, 0) >= 0);
}

/* HERA: the key (a note before TRN) -> its chord of 12 and the octaves it is above C4 */
static uint32_t hera_key(const track_t *t, uint32_t note, int32_t *octave)
{
    int32_t key = (int32_t)note - t->p[P_TRANS] + 120;
    *octave = key / 12 - 15;
    return (uint32_t)key % HERA_KEYS;
}
static uint32_t hera_set(const track_t *t) { return (uint32_t)clamp(t->p[P_CHSET], 0, (int32_t)HERA_SETS - 1); }
/* the chord of `note` -> its tones above *root, *root its lowest note, the number of tones */
static uint32_t hera_tones(const track_t *t, uint32_t note, int32_t *root, int32_t *iv)
{
    int32_t octave;
    uint32_t key = hera_key(t, note, &octave), n = 0;
    const uint8_t *c = &HERA_NOTES[(hera_set(t) * HERA_KEYS + key) * CHORD_MAX];
    *root = c[0] + 12 * octave + t->p[P_TRANS];
    while (n < CHORD_MAX && c[n]) {
        iv[n] = c[n] - c[0];
        n++;
    }
    return n;
}
/* a chord's name for the screen (b: HERA_NAME_MAX + 1 bytes). Without the resource region, none: the caller's */
static int hera_name(char *b, uint32_t set, uint32_t key)
{
    if (!RES_OK()) return 0;
    str_cpy(b, HERA_NAMES + HERA_NAME_OFF[set * HERA_KEYS + key], HERA_NAME_MAX + 1u);
    return 1;
}
static const char *hera_genre(uint32_t set) { return RES_OK() ? HERA_GENRES + HERA_GENRE_OFF[HERA_GENRE[set]] : ""; }

/* EXTRA applies: CHRD DIA3, DIA7 or a fixed shape, a 12-note scale, not a kit (CHORD 2 shows) */
static int chord_extra_on(const track_t *t)
{
    return t->p[P_CHRD] >= CH_DIA3 && t->p[P_CHRD] <= CH_POW && !chord_kit(t) && !micro_active(t);
}

/* EXTRA ex on the tones iv[0..n) (ascending, iv[0] = 0, iv[1] the third when it is 3 or 4) -> their number.
 * x: the 2nd, 4th, 6th and 9th to use (the scale's on DIA, 2 5 9 14 on a shape). At most CHORD_MAX: 6 or 9 on a
 * seventh drops the fifth (the 6th then an octave up, a 13th); 6/9 keeps the root and the third. A tone already in
 * the chord (a scale without it: the note below is one of the chord's) is not added; SUS without a third does nothing */
static int chord_has_pc(const int32_t *iv, uint32_t n, uint32_t skip, int32_t v)
{
    for (uint32_t i = 0; i < n; i++)
        if (i != skip && (iv[i] - v) % 12 == 0) return 1;
    return 0;
}
static uint32_t chord_extra(int32_t *iv, uint32_t n, uint32_t ex, const int32_t *x)
{
    uint32_t i, j;
    int third = n >= 2u && (iv[1] == 3 || iv[1] == 4);
    switch (ex) {
    case EX_SUS2:
    case EX_SUS4:
        if (third && !chord_has_pc(iv, n, 1u, x[ex == EX_SUS2 ? 0 : 1])) iv[1] = x[ex == EX_SUS2 ? 0 : 1];
        break;
    case EX_6:
    case EX_9: {
        int32_t v = x[ex == EX_6 ? 2 : 3];
        if (chord_has_pc(iv, n, 99u, v)) break;
        if (n >= CHORD_MAX) {                           /* a seventh: the fifth goes */
            iv[2] = iv[3];
            n--;
            if (ex == EX_6) v += 12;
        }
        iv[n++] = v;
        break;
    }
    case EX_69:
        if (n > 2u) n = 2u;                             /* the root and the third (or the shape's second tone) */
        if (!chord_has_pc(iv, n, 99u, x[2])) iv[n++] = x[2];
        if (!chord_has_pc(iv, n, 99u, x[3])) iv[n++] = x[3];
        break;
    default:
        break;
    }
    for (i = 1; i < n; i++)                             /* ascending */
        for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
            int32_t y = iv[j];
            iv[j] = iv[j - 1u];
            iv[j - 1u] = y;
        }
    return n;
}

#include "chord_auto.c"
#include "chord_melody.c"

static const int32_t X_SHAPE[4] = {2, 5, 9, 14};     /* EXTRA's 2nd 4th 6th 9th on a shape */
/* the tones of the chord (semitones above *root, ascending, iv[0] = 0) -> their number; DIA may move *root
 * down onto the scale */
static uint32_t chord_tones(const track_t *t, uint32_t mode, int32_t *root, int32_t *iv)
{
    uint32_t n = 0, i;
    if (micro_active(t)) {
        if (mode < CH_MAJ) {
            n = mode == CH_DIA7 ? 4u : 3u;
            for (i = 0; i < n; i++) iv[i] = 2 * (int32_t)i;
        } else {
            for (i = 0; i < CHORD_MAX && CHORD_SHAPE[mode - CH_MAJ][i] >= 0; i++) {
                int32_t target = micro_pitch(t, *root) + CHORD_SHAPE[mode - CH_MAJ][i] * 10000;
                int32_t lo = 0, hi = 127;
                while (lo < hi) {
                    int32_t mid = (lo + hi) / 2;
                    if (micro_pitch(t, mid) < target) lo = mid + 1; else hi = mid;
                }
                if (lo && target - micro_pitch(t, lo - 1) < micro_pitch(t, lo) - target) lo--;
                iv[n++] = lo - *root;
            }
        }
        return n;
    }
    if (mode >= CH_MAJ) {
        for (i = 0; i < CHORD_MAX && CHORD_SHAPE[mode - CH_MAJ][i] >= 0; i++)
            iv[n++] = CHORD_SHAPE[mode - CH_MAJ][i];
        return chord_extra_on(t) ? chord_extra(iv, n, (uint32_t)t->p[P_CHEXT], X_SHAPE) : n;
    }
    {
        uint32_t mask = scale_mask(t), deg[12], c = 0, at = 0, guard = 12;
        int32_t r = t->p[P_ROOT];
        if (mask == 0xFFFu)
            mask = SCALE_MASK[1];                       /* CHR: no key of its own, the major scale of ROOT */
        {   /* BORROW (Hookology, P_CHBOR 0 = ON): a key outside the scale plays a borrowed chord. b2: the Neapolitan
             * (major / maj7); #4: a passing diminished (dim / dim7); else the parallel key's chord on it (a scale with
             * a major 3rd borrows from the natural minor, one without from the major); none of these: as before */
            static const int8_t BOR[2][4] = {{0, 4, 7, 11}, {0, 3, 6, 9}};
            uint32_t rel = (uint32_t)((*root - r + 120) % 12);
            if (!((mask >> rel) & 1u) && t->p[P_CHBOR] == 0) {
                if (rel == 1u || rel == 6u) {
                    n = mode == CH_DIA7 ? 4u : 3u;
                    for (i = 0; i < n; i++) iv[i] = BOR[rel == 6u][i];
                    return chord_extra_on(t) && t->p[P_CHEXT] ? chord_extra(iv, n, (uint32_t)t->p[P_CHEXT], X_SHAPE) : n;
                }
                uint32_t par = (mask >> 4) & 1u ? SCALE_MASK[2] : SCALE_MASK[1];
                if ((par >> rel) & 1u)
                    mask = par;                         /* (EXTRA below then takes the parallel scale's notes too) */
            }
        }
        while (guard-- && !((mask >> (uint32_t)((*root - r + 120) % 12)) & 1u))
            (*root)--;                                  /* outside the scale: the scale note below */
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (i == (uint32_t)((*root - r + 120) % 12))
                    at = c;
                deg[c++] = i;
            }
        for (i = 0; i < (mode == CH_DIA7 ? 4u : 3u); i++) {   /* every other note of the scale */
            uint32_t k = at + 2u * i;
            iv[n++] = (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
        }
        if (chord_extra_on(t) && t->p[P_CHEXT]) {      /* EXTRA: the 2nd 4th 6th 9th, each the scale's note at or below */
            static const int8_t X_STD[4] = {2, 5, 9, 14};
            int32_t x[4];
            for (i = 0; i < 4u; i++) {
                int32_t v = X_STD[i], g = 12;
                while (g-- && !((mask >> (uint32_t)((*root - r + v + 1200) % 12)) & 1u))
                    v--;
                x[i] = v;
            }
            n = chord_extra(iv, n, (uint32_t)t->p[P_CHEXT], x);
        }
        return n;
    }
}

/* VOIC LEAD: of the chord's inversions, an octave down, in place or up, the one closest to the track's last chord
 * (after fm1-chord's voice leading: the distance of the sorted notes, the shorter list padded with its top note,
 * +6 when the lowest note moves more than a fifth; ties: root position, no octave). None to lead from: as it is */
static void chord_lead(const track_t *t, int32_t r, int32_t *iv, uint32_t n, int32_t period)
{
    uint32_t k = trk_index(t), np = chord_lead_ok[k] ? chord_last[k].n : 0u, inv, i, j;
    const uint8_t *prev = chord_last[k].note;
    int32_t o, best = 0x7FFFFFFF, c[CHORD_MAX + 1u], keep[CHORD_MAX + 1u];
    if (!np || !n) return;
    for (i = 0; i < n; i++) keep[i] = iv[i];
    for (o = -period; o <= period; o += period)
        for (inv = 0; inv < n; inv++) {
            int32_t cost = 0, d;
            for (i = 0; i < n; i++)                     /* the lowest inv tones an octave up */
                c[i] = iv[i] + (i < inv ? period : 0) + o;
            for (i = 1; i < n; i++)
                for (j = i; j > 0 && c[j - 1u] > c[j]; j--) {
                    d = c[j]; c[j] = c[j - 1u]; c[j - 1u] = d;
                }
            if (c[0] < -period || c[0] >= period) continue;   /* the lowest note within an octave of the key: no drift */
            for (i = 0; i < (n > np ? n : np); i++) {
                d = r + c[i < n ? i : n - 1u] - (int32_t)prev[i < np ? i : np - 1u];
                cost += d < 0 ? -d : d;
            }
            d = r + c[0] - (int32_t)prev[0];
            if (d > 7 || d < -7) cost += 6;
            cost = cost * 4 + (int32_t)inv + (o ? 2 : 0);
            if (cost < best) {
                best = cost;
                for (i = 0; i < n; i++) keep[i] = c[i];
            }
        }
    for (i = 0; i < n; i++) iv[i] = keep[i];
}

/* the chord on note `root` as track t plays it now -> out (ascending), the number of notes (1..CHORD_MAX);
 * *rp its root, *maskp its tones above the root (bit i: i semitones; 0 = no chord). The root alone when CHRD
 * is OFF, on a kit, or for MONO / LEGATO / UNISON (their chord's root) */
static uint32_t chord_make(const track_t *t, uint32_t root, uint8_t *out, int32_t *rp, uint16_t *maskp)
{
    int32_t iv[CHORD_MAX + 1u], r = (int32_t)root, x;
    uint32_t mode = (uint32_t)t->p[P_CHRD], n, i, j, m = 0;
    int32_t period = (int32_t)scale_note_period(t);
    uint16_t mask = 0;
    *rp = r;
    *maskp = 0;
    int automatic = mode == CH_AUTO || mode == CH_MELODY;
    if (automatic && micro_scale(t)) *maskp = 1;
    if (!mode || mode > CH_HERA || chord_kit(t) || (automatic && micro_scale(t)) || (mode == CH_HERA && micro_active(t))) {
        out[0] = (uint8_t)root;
        return 1;
    }
    if (mode == CH_AUTO) return chord_auto_make(t, root, out, rp, maskp);
    if (mode == CH_MELODY) return chord_melody_make(t, root, out, rp, maskp);
    n = mode == CH_HERA ? hera_tones(t, root, &r, iv) : chord_tones(t, mode, &r, iv);
    for (i = 0; i < n; i++)
        mask |= (uint16_t)(1u << (micro_active(t) ?
            ((micro_pitch(t, r + iv[i]) - micro_pitch(t, r) + 5000) / 10000 % 12) : iv[i] % 12));
    *rp = r;
    *maskp = mask;
    if (trk_vmode(t) != V_POLY) {                       /* one voice: the root */
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    switch (t->p[P_VOIC]) {
    case VC_OPEN:                                       /* 1-5-3(-7): the second tone an octave up */
        if (n >= 3u)
            iv[1] += period;
        break;
    case VC_INV2:
    case VC_INV1:
        for (j = 0; j < (t->p[P_VOIC] == VC_INV2 ? 2u : 1u); j++) {   /* the lowest tone an octave up */
            x = iv[0] + period;
            for (i = 1; i < n; i++)
                iv[i - 1u] = iv[i];
            iv[n - 1u] = x;
        }
        break;
    case VC_LEAD:
        chord_lead(t, r, iv, n, period);
        break;
    case VC_BASS:                                       /* the root an octave down; a seventh drops its fifth */
        if (n == CHORD_MAX && chord_extra_on(t) && t->p[P_CHEXT] && !(iv[2] >= 6 && iv[2] <= 8)) {
            iv[0] -= period;                            /* EXTRA's four, no fifth: the root goes down, no tone lost */
            break;
        }
        if (n == CHORD_MAX) {
            iv[2] = iv[3];
            n--;
        }
        for (i = n; i > 0; i--)
            iv[i] = iv[i - 1u];
        iv[0] = -period;
        n++;
        break;
    default:
        break;
    }
    for (i = 1; i < n; i++)                             /* ascending */
        for (j = i; j > 0 && iv[j - 1u] > iv[j]; j--) {
            x = iv[j];
            iv[j] = iv[j - 1u];
            iv[j - 1u] = x;
        }
    for (i = 0; i < n && m < CHORD_MAX; i++) {          /* inside 0..127, each note once */
        x = r + iv[i];
        if (x < 0 || x > 127 || (m && out[m - 1u] == (uint8_t)x))
            continue;
        if (micro_active(t) && (micro_pitch(t, x) < 0 || micro_pitch(t, x) > 1270000)) continue;
        out[m++] = (uint8_t)x;
    }
    if (!m) {
        out[0] = (uint8_t)clamp(r, 0, 127);
        return 1;
    }
    return m;
}

/* chord_make for a note played now: a chord is kept in chord_last (the CHORD page shows it) */
static uint32_t chord_build(track_t *t, uint32_t root, uint8_t *out)
{
    int32_t r;
    uint16_t mask;
    uint32_t m = chord_make(t, root, out, &r, &mask), i, k = trk_index(t);
    if (t->p[P_CHRD] == CH_AUTO && !micro_scale(t) && !chord_kit(t) && trk_vmode(t) == V_POLY)
        chord_auto_remember(t, r, mask, out, m);
    if (m > 1u || t->p[P_CHRD] == CH_MELODY) {
        chord_last[k].root = (uint8_t)clamp(r, 0, 127);
        chord_last[k].mask = mask;
        chord_last[k].n = (uint8_t)m;
        for (i = 0; i < CHORD_MAX; i++)
            chord_last[k].note[i] = i < m ? out[i] : 0u;
        chord_last[k].gen++;
        chord_lead_ok[k] = 1;
        chord_last[k].hera = 0;
        if (t->p[P_CHRD] == CH_HERA) {
            int32_t octave;
            chord_last[k].hera = (uint8_t)(hera_key(t, root, &octave) + 1u);
            chord_last[k].set = (uint8_t)hera_set(t);
        }
    }
    return m;
}

/* a chord's name from its root and tones (bit i: i semitones above the root): "C", "Cm7", "F#m7b5", "Gsus4",
 * "A5" -> b (12 bytes) */
static void chord_name(char *b, uint32_t root, uint32_t mask)
{
    uint32_t mi = (mask >> 3) & 1u, ma = (mask >> 4) & 1u, b5 = (mask >> 6) & 1u, p5 = (mask >> 7) & 1u;
    uint32_t s5 = (mask >> 8) & 1u, d7 = (mask >> 9) & 1u, m7 = (mask >> 10) & 1u, M7 = (mask >> 11) & 1u;
    uint32_t n9 = (mask >> 2) & 1u, b9 = (mask >> 1) & 1u;   /* with a third: the 9th, the b9 (EXTRA, Hookology) */
    const char *q, *x = "";
    str_cpy(b, chord_note(root), 12);
    if (mi && !ma && b5 && !p5)
        q = d7 ? "dim7" : m7 ? "m7b5" : "dim";
    else if (ma && !mi && s5 && !p5)
        q = m7 ? "+7" : M7 ? "+M7" : "aug";
    else if (ma && !mi)
        q = m7 ? (d7 ? "13" : n9 ? "9" : "7") : M7 ? (d7 ? "maj13" : n9 ? "maj9" : "maj7") :
            d7 ? (n9 ? "6/9" : "6") : n9 ? "add9" : "";
    else if (mi)
        q = m7 ? (d7 ? "m13" : n9 ? "m9" : "m7") : M7 ? (n9 ? "mM9" : "mM7") : d7 ? (n9 ? "m6/9" : "m6") :
            n9 ? "madd9" : "m";
    else if ((mask >> 5) & 1u) {                        /* no third: suspended */
        q = m7 ? "7" : "";
        x = "sus4";
    } else if ((mask >> 2) & 1u) {
        q = m7 ? "7" : "";
        x = "sus2";
    } else
        q = "5";
    if ((ma || mi) && b9 && !n9)
        x = m7 || M7 ? "b9" : "addb9";
    str_cpy(b + str_len(b), q, 12 - str_len(b));
    str_cpy(b + str_len(b), x, 12 - str_len(b));
}

/* the MIDI note (ch, src) of track id - 1 that plays a chord, 0 = none */
static mchord_t *mchord_of(uint32_t ch, uint32_t src, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id && mchord[i].ch == ch && mchord[i].src == src)
            return &mchord[i];
    return 0;
}

/* a MIDI-held chord of track id - 1 sounds note */
static int mchord_held(uint32_t id, uint32_t note)
{
    uint32_t i, j;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == id)
            for (j = 0; j < mchord[i].n; j++)
                if (chord_tone_at(mchord[i].note, &mchord[i].strum, j) == note)
                    return 1;
    return 0;
}

static void mchord_forget(uint32_t track)               /* panic: the track's MIDI chords are gone */
{
    chord_auto_forget(track);
    chord_lead_ok[track] = 0;
    uint32_t i;
    for (i = 0; i < MCHORD_N; i++)
        if (mchord[i].id == track + 1u)
            mchord[i].id = 0;
}

/* A division describes the complete first-to-last sweep, not each string gap.
 * Freeze its rate and pitch order at the press; tempo may still change. ARP owns
 * timing while enabled, so it receives the complete chord immediately. */
static uint32_t chord_strum_begin(track_t *t, uint8_t *notes, uint32_t n, chord_strum_t *s, uint32_t vel)
{
    memset(s, 0, sizeof *s);
    uint32_t rate = (uint32_t)t->p[P_STRUM];
    if (!rate || rate >= NELEM(N_STRUM) || n < 2u || t->p[P_AMODE]) return n;
    uint32_t up = t->p[P_STROKE] == 1;
    if (t->p[P_STROKE] == 2) up = chord_alternate[trk_index(t)]++ & 1u;
    static uint8_t seed;                                 /* (SLOP: each press its own unevenness) */
    s->slop = (uint8_t)(((uint32_t)clamp(t->p[P_SLOP], 0, 127) * 15u / 127u) << 4 | (seed++ * 7u & 15u));
    if (up) for (uint32_t i = 0; i < n / 2u; i++) {
        uint8_t x = notes[i]; notes[i] = notes[n - 1u - i]; notes[n - 1u - i] = x;
    }
    if (!up && t->p[P_CHRD] == CH_MELODY && !micro_scale(t)) {
        /* The melody sounds on the press; DOWN/UP/ALT order its accompaniment.
         * Freeze that order here so later mode changes cannot delay the tune. */
        uint8_t melody = notes[n - 1u];
        for (uint32_t i = n - 1u; i; i--) notes[i] = notes[i - 1u];
        notes[0] = melody;
    }
    s->total = (uint8_t)n; s->rate = (uint8_t)rate; s->vel = (uint8_t)vel;
    if (t->p[P_SRANGE] == 1 && t->p[P_CHRD] != CH_MELODY && n <= CHORD_MAX) {   /* (Hookology) RANGE 2 OCT */
        s->total = (uint8_t)(2u * n);
        s->rate = (uint8_t)(rate | STRUM_OCT2 | (up ? STRUM_UP : 0u) | n << 5);
    }
    s->beat = seq_beat_samples();
    return 1u;
}

/* Keep elapsed beat fraction through tempo changes and INT/MIDI clock switches. */
static uint32_t chord_strum_due(chord_strum_t *s, uint32_t started, uint32_t elapsed)
{
    static const uint8_t DEN[] = {1, 2, 4, 8, 16, 32, 6, 12};
    uint32_t beat = seq_beat_samples();
    if (s->beat != beat) {
        s->pos = (uint32_t)(((uint64_t)s->pos * beat + s->beat / 2u) / s->beat);
        s->beat = beat;
    }
    s->pos += elapsed;
    uint32_t span = beat / DEN[STRUM_DIV(s)], target = span * started / (s->total - 1u);
    if (started && (s->slop >> 4)) {                     /* (Hookology) SLOP: up to the gap early or late, at random */
        uint32_t r = span / (s->total - 1u) * (s->slop >> 4) / 15u, h = (started * 2654435761u) ^ ((s->slop & 15u) * 40503u);
        h ^= h >> 13; h *= 2246822519u; h ^= h >> 16;
        int32_t j = (int32_t)(h % (2u * r + 1u)) - (int32_t)r;
        target = (int32_t)target + j > 0 ? (uint32_t)((int32_t)target + j) : 0u;
    }
    return s->pos < target ? target - s->pos : 0u;
}
