/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chord keys (firmware/src/chord.c; SCL > CHORD: CHRD, VOIC) against the real keyboard, MIDI IN, arp, recording
 * and voice code: the diatonic triads and sevenths of several scales and roots as music theory has them, the
 * fixed shapes, the voicings, at most 4 notes, the names, MONO plays the root, a key's or a MIDI note's release
 * ends exactly the notes it started (CHRD / VOIC changed while held too), shared notes, live recording writes the
 * chord into one step, the ARP gets the chord as held notes, QNT WHITE maps the key first, a kit (the DRUM engine,
 * SAMPLE PERC) ignores CHRD, MIDI OUT of the keys, and OFF plays exactly as before.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int gate_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void reset(void)
{
    ui_power_on();
    settings_sharps = 1;                                 /* (Hookology: the names below are spelled with sharps; flats: names_flat) */
    set_engine_of(&trk[0], HOST_SYNTH);             /* (NPOLY voices: the chords, not a voice cap, are tested) */
    memset(kb_chn, 0, sizeof kb_chn); memset(mchord, 0, sizeof mchord); memset(chord_last, 0, sizeof chord_last);
    fm1_in.notes = kb_prev = 0; mo_w = mo_r = 0; usb.config = 1;
    trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SCALE] = 1; trk[0].p[P_ROOT] = 0;   /* C major, POLY */
    events_block(CTL);
}
/* the chord track 0 plays on note r: "n1 n2 n3 [n4]" compared with want (0-terminated) */
static int chord_is(uint32_t r, const uint8_t *want)
{
    uint8_t out[CHORD_MAX];
    uint32_t n = chord_build(&trk[0], r, out), i;
    for (i = 0; i < n; i++) if (want[i] != out[i]) return 0;
    return i == CHORD_MAX || !want[i];
}
static int name_is(uint32_t r, const char *want)
{
    uint8_t out[CHORD_MAX];
    int32_t root;
    uint16_t mask;
    char b[12];
    chord_make(&trk[0], r, out, &root, &mask);
    chord_name(b, (uint32_t)root, mask);
    if (!str_eq(b, want)) printf("  name of %u: %s, want %s\n", r, b, want);
    return str_eq(b, want);
}
static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_enqueue(st >> 4 | st << 8 | d1 << 16 | d2 << 24, 1);
    events_block(CTL);
}
#define K_C4 7u                                   /* keys from F3 (0): C4 7, D4 9, E4 11, G4 14 */
#define K_D4 9u
#define K_E4 11u

static int theory(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    static const char *const MAJ3[7] = {"C", "Dm", "Em", "F", "G", "Am", "Bdim"};
    static const char *const MAJ7[7] = {"Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5"};
    static const uint8_t CMAJ[7] = {60, 62, 64, 65, 67, 69, 71};
    uint32_t i;
    reset();
    t->p[P_CHRD] = CH_DIA3;
    bad += check("C major DIA3 on D: D F A", chord_is(62, (const uint8_t[]){62, 65, 69, 0}));
    bad += check("C major DIA3 on B: B D F (diminished)", chord_is(71, (const uint8_t[]){71, 74, 77, 0}));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("C major DIA7 on G: G B D F", chord_is(67, (const uint8_t[]){67, 71, 74, 77}));
    bad += check("C major DIA7 on C: C E G B", chord_is(60, (const uint8_t[]){60, 64, 67, 71}));
    for (i = 0, ok = 1; i < 7u; i++) {
        t->p[P_CHRD] = CH_DIA3; ok &= name_is(CMAJ[i], MAJ3[i]);
        t->p[P_CHRD] = CH_DIA7; ok &= name_is(CMAJ[i], MAJ7[i]);
    }
    bad += check("C major: C Dm Em F G Am Bdim, Cmaj7 Dm7 Em7 Fmaj7 G7 Am7 Bm7b5", ok);
    t->p[P_SCALE] = 2; t->p[P_ROOT] = 9; t->p[P_CHRD] = CH_DIA3;   /* A minor */
    bad += check("A minor DIA3 on B: B D F", chord_is(71, (const uint8_t[]){71, 74, 77, 0}));
    bad += check("A minor DIA3 on A: A C E (Am), on C: C E G, on E: E G B (Em)",
                 chord_is(69, (const uint8_t[]){69, 72, 76, 0}) && chord_is(60, (const uint8_t[]){60, 64, 67, 0}) &&
                 chord_is(64, (const uint8_t[]){64, 67, 71, 0}) && name_is(69, "Am") && name_is(64, "Em"));
    t->p[P_SCALE] = 7;                                             /* A harmonic minor */
    bad += check("A harmonic minor DIA3 on E: E G# B (E), on C: C E G# (Caug)",
                 chord_is(64, (const uint8_t[]){64, 68, 71, 0}) && name_is(64, "E") &&
                 chord_is(60, (const uint8_t[]){60, 64, 68, 0}) && name_is(60, "Caug"));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("A harmonic minor DIA7 on E: E G# B D (E7), on G#: G# B D F (G#dim7), on A: AmM7",
                 chord_is(64, (const uint8_t[]){64, 68, 71, 74}) && name_is(64, "E7") && name_is(68, "G#dim7") &&
                 name_is(69, "AmM7"));
    t->p[P_SCALE] = 1; t->p[P_ROOT] = 3;                           /* Eb major */
    bad += check("Eb major DIA7 on Bb: Bb D F Ab (A#7)", chord_is(58, (const uint8_t[]){58, 62, 65, 68}) && name_is(58, "A#7"));
    t->p[P_SCALE] = 3; t->p[P_ROOT] = 2; t->p[P_CHRD] = CH_DIA3;   /* D dorian */
    bad += check("D dorian DIA3 on D: Dm, on G: G (the major IV)", name_is(62, "Dm") && name_is(67, "G") &&
                 chord_is(67, (const uint8_t[]){67, 71, 74, 0}));
    t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; t->p[P_CHBOR] = 1;
    bad += check("BORROW OFF: a key outside the scale plays the scale note below (C# in C major: C)", chord_is(61, (const uint8_t[]){60, 64, 67, 0}));
    t->p[P_CHBOR] = 0;
    t->p[P_SCALE] = 0; t->p[P_ROOT] = 7;                           /* CHR: the major scale of G */
    bad += check("CHR: the major scale of ROOT (G: on A, Am; on F#, F#dim)", name_is(69, "Am") && name_is(66, "F#dim"));
    t->p[P_SCALE] = 5; t->p[P_ROOT] = 0;                           /* C major pentatonic: every other note */
    bad += check("pentatonic DIA3: in the scale (C E A: C6)", chord_is(60, (const uint8_t[]){60, 64, 69, 0}) && name_is(60, "C6"));
    {   /* every scale, root, mode: every note in the scale */
        uint32_t s, r, n, k, note;
        uint8_t out[CHORD_MAX];
        ok = 1;
        for (s = 1; s <= (uint32_t)TP[P_SCALE].max; s++)
            for (r = 0; r < 12u; r++)
                for (note = 36; note < 96u; note++) {
                    t->p[P_SCALE] = (int16_t)s; t->p[P_ROOT] = (int16_t)r; t->p[P_CHBOR] = 1;   /* (BORROW OFF) */
                    t->p[P_CHRD] = (int16_t)(note & 1u ? CH_DIA3 : CH_DIA7);
                    n = chord_build(t, note, out);
                    ok &= n == (note & 1u ? 3u : 4u);
                    for (k = 0; k < n; k++)
                        ok &= (scale_mask(t) >> ((out[k] + 120u - r) % 12u)) & 1u;
                }
        bad += check("DIA3 / DIA7 of every scale and root: 3 / 4 notes, every one in key", ok);
        t->p[P_CHBOR] = 0;
    }
    return bad;
}

static int shapes_voicings(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    reset();
    t->p[P_CHRD] = CH_MAJ; bad += check("MAJ on C4: C E G", chord_is(60, (const uint8_t[]){60, 64, 67, 0}) && name_is(60, "C"));
    t->p[P_CHRD] = CH_MIN; bad += check("MIN: C Eb G (Cm)", chord_is(60, (const uint8_t[]){60, 63, 67, 0}) && name_is(60, "Cm"));
    t->p[P_CHRD] = CH_DOM7; bad += check("DOM7: C E G Bb (C7)", chord_is(60, (const uint8_t[]){60, 64, 67, 70}) && name_is(60, "C7"));
    t->p[P_CHRD] = CH_MAJ7; bad += check("MAJ7: C E G B (Cmaj7)", chord_is(60, (const uint8_t[]){60, 64, 67, 71}) && name_is(60, "Cmaj7"));
    t->p[P_CHRD] = CH_MIN7; bad += check("MIN7: C Eb G Bb (Cm7)", chord_is(60, (const uint8_t[]){60, 63, 67, 70}) && name_is(60, "Cm7"));
    t->p[P_CHRD] = CH_SUS4; bad += check("SUS4: C F G (Csus4)", chord_is(60, (const uint8_t[]){60, 65, 67, 0}) && name_is(60, "Csus4"));
    t->p[P_CHRD] = CH_POW; bad += check("POW: C G C (C5)", chord_is(60, (const uint8_t[]){60, 67, 72, 0}) && name_is(60, "C5"));
    t->p[P_CHRD] = CH_MIN; t->p[P_SCALE] = 1;
    bad += check("a fixed shape ignores the scale (MIN on D# in C major: D# F# A#)", chord_is(63, (const uint8_t[]){63, 66, 70, 0}));
    t->p[P_CHRD] = CH_MAJ;
    t->p[P_VOIC] = VC_OPEN; bad += check("OPEN: C G E (1-5-3)", chord_is(60, (const uint8_t[]){60, 67, 76, 0}));
    t->p[P_VOIC] = VC_INV1; bad += check("INV1: E G C", chord_is(60, (const uint8_t[]){64, 67, 72, 0}));
    t->p[P_VOIC] = VC_INV2; bad += check("INV2: G C E", chord_is(60, (const uint8_t[]){67, 72, 76, 0}));
    t->p[P_VOIC] = VC_BASS; bad += check("+OCT: C3 C E G", chord_is(60, (const uint8_t[]){48, 60, 64, 67}));
    t->p[P_CHRD] = CH_MAJ7;
    bad += check("+OCT of a seventh: C3 C E B (the fifth dropped, 4 notes)", chord_is(60, (const uint8_t[]){48, 60, 64, 71}));
    t->p[P_VOIC] = VC_OPEN; bad += check("OPEN seventh: C G B E", chord_is(60, (const uint8_t[]){60, 67, 71, 76}));
    t->p[P_VOIC] = VC_INV1; bad += check("INV1 seventh: E G B C", chord_is(60, (const uint8_t[]){64, 67, 71, 72}));
    t->p[P_VOIC] = VC_INV2; bad += check("INV2 seventh: G B C E", chord_is(60, (const uint8_t[]){67, 71, 72, 76}));
    t->p[P_CHRD] = CH_POW; t->p[P_VOIC] = VC_INV1;
    bad += check("INV1 of POW: G C (the doubled root once)", chord_is(60, (const uint8_t[]){67, 72, 0}));
    {   /* every mode, voicing and note: 1..4 notes, ascending, each once, inside 0..127; the name fits */
        uint32_t c, v, s, note, n, k;
        uint8_t out[CHORD_MAX];
        int32_t r;
        uint16_t mask;
        char b[12];
        ok = 1;
        for (s = 0; s <= (uint32_t)TP[P_SCALE].max; s++)
            for (c = 1; c <= (uint32_t)TP[P_CHRD].max; c++)
                for (v = 0; v <= (uint32_t)TP[P_VOIC].max; v++)
                    for (note = 0; note < 128u; note++) {
                        t->p[P_SCALE] = (int16_t)s; t->p[P_CHRD] = (int16_t)c; t->p[P_VOIC] = (int16_t)v;
                        n = chord_make(t, note, out, &r, &mask);
                        ok &= n >= 1u && n <= CHORD_MAX && (mask & 1u);
                        for (k = 1; k < n; k++) ok &= out[k] > out[k - 1u];
                        for (k = 0; k < n; k++) ok &= out[k] <= 127u;
                        chord_name(b, (uint32_t)r, mask);
                        ok &= str_len(b) >= 1u && str_len(b) <= 7u;
                    }
        bad += check("every scale x CHRD x VOIC x note: 1..4 notes, ascending, once each, 0..127, a name", ok);
    }
    t->p[P_CHRD] = CH_OFF;
    bad += check("OFF: the note alone", chord_is(61, (const uint8_t[]){61, 0}));
    return bad;
}

static int keys(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t before;
    reset();
    t->p[P_CHRD] = CH_DIA3;
    key_down(K_D4);
    bad += check("a key plays its chord (D4: D F A), MIDI OUT the three", gate_note(t, 62) && gate_note(t, 65) && gate_note(t, 69) &&
                 ngated(t) == 3u && mo_w == 3u && last_note == 62);
    bad += check("  chord_last: Dm for the CHORD page", chord_last[0].root == 62 && chord_last[0].n == 3u);
    t->p[P_CHRD] = CH_MIN7; t->p[P_VOIC] = VC_BASS;               /* changed while held */
    key_up(K_D4);
    bad += check("its release ends exactly those (CHRD / VOIC changed meanwhile), MIDI OUT their note-offs",
                 !ngated(t) && mo_w == 6u && ((midi_out_q[5] >> 8) & 0xF0u) == 0x80u);
    t->p[P_CHRD] = CH_DIA3; t->p[P_VOIC] = VC_CLOSE;
    mo_w = mo_r = 0;
    key_down(K_C4); key_down(K_E4);                                /* C E G and E G B: E and G shared */
    before = mo_w;
    bad += check("two chords: the shared notes sound once (C E G B)", ngated(t) == 4u && gate_note(t, 71) && before == 4u);
    key_up(K_C4);
    bad += check("  C up: C ends, E G stay for the E key", !gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67) && gate_note(t, 71));
    key_up(K_E4);
    bad += check("  E up: nothing left, MIDI OUT balanced", !ngated(t) && mo_w == 8u);
    t->p[P_VOICE] = V_MONO;
    key_down(K_C4 + 1u);                                           /* C#: BORROW's Db (OFF: DIA snaps to C) */
    bad += check("MONO: the root alone (C# in C major with BORROW: Db, C#)", ngated(t) == 1u && gate_note(t, 61));
    t->p[P_VOICE] = V_POLY;
    key_up(K_C4 + 1u);
    bad += check("  released (MONO switched off meanwhile)", !ngated(t));
    t->p[P_VOICE] = V_LEGATO;
    key_down(K_E4);
    bad += check("LEGATO: the root alone (E)", ngated(t) == 1u && gate_note(t, 64));
    key_up(K_E4);
    t->p[P_VOICE] = V_POLY;
    t->p[P_QUANT] = 2;                                             /* WHITE in A minor: D4 is the 4th degree */
    t->p[P_SCALE] = 2; t->p[P_ROOT] = 9;
    key_down(K_D4);                                                /* WHITE: C4 = A4 (the root), D4 = B4 */
    bad += check("QNT WHITE: the key mapped first (A minor, D4 -> B4), then its chord B D F",
                 kb_note[K_D4] == 71 && gate_note(t, 71) && gate_note(t, 74) && gate_note(t, 77));
    key_up(K_D4);
    bad += check("  released", !ngated(t));
    t->p[P_CHRD] = CH_OFF; t->p[P_QUANT] = 0;
    before = mo_w;
    key_down(K_C4);
    bad += check("OFF: one note, as before", ngated(t) == 1u && gate_note(t, 60) && mo_w == before + 1u);
    key_up(K_C4);
    return bad;
}

static int arp_rec(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i;
    reset();
    t->p[P_CHRD] = CH_DIA7;
    t->p[P_AMODE] = 1;
    key_down(K_C4);
    bad += check("ARP: one key, the chord as held notes (C E G B)", t->nheld == 4u && t->held[0] == 60 && t->held[1] == 64 &&
                 t->held[2] == 67 && t->held[3] == 71);
    key_up(K_C4);
    bad += check("  key up: nothing held", !t->nheld && !t->arp_phys);
    t->p[P_AMODE] = 0;
    for (i = 0; i < NSTEP; i++) t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0};
    t->p[P_CHRD] = CH_DIA3;
    song.rec = 1; song.playing = 1;
    t->seq_idx = 0; t->seq_pos = 0;
    key_down(K_D4);
    bad += check("live recording: the chord into one step (D F A)", t->step[0].n == 3u && t->step[0].time == ST_NOTE &&
                 t->step[0].note[0] == 62 && t->step[0].note[1] == 65 && t->step[0].note[2] == 69);
    key_up(K_D4);
    t->p[P_CHRD] = CH_MAJ7; t->p[P_VOIC] = VC_BASS;
    t->seq_idx = 4;
    key_down(K_C4);
    bad += check("  a 4-note voicing: 4 notes in the step (the most a step holds)", t->step[4].n == 4u && t->step[4].note[0] == 48);
    key_up(K_C4);
    song.rec = 0; song.playing = 0;
    t->p[P_VOICE] = V_MONO; t->p[P_CHRD] = CH_DIA3; t->p[P_VOIC] = VC_CLOSE;
    song.rec = 1; song.playing = 1; t->seq_idx = 8;
    key_down(K_E4);
    bad += check("  MONO records its root alone", t->step[8].n == 1u && t->step[8].note[0] == 64);
    key_up(K_E4);
    song.rec = 0; song.playing = 0;
    return bad;
}

static int midi_in(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    uint32_t i, free = 1;
    reset();
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 62, 100);
    bad += check("MIDI IN: a note plays its chord (D F A)", gate_note(t, 62) && gate_note(t, 65) && gate_note(t, 69) && ngated(t) == 3u);
    t->p[P_CHRD] = CH_OFF;
    midi(0x80, 62, 0);
    for (i = 0; i < MCHORD_N; i++) free &= !mchord[i].id;
    bad += check("  its note-off ends the chord (CHRD OFF meanwhile), the table empty", !ngated(t) && free);
    t->p[P_CHRD] = CH_DIA3;
    midi(0x90, 60, 100); key_down(K_E4);                           /* MIDI C E G, key E G B */
    bad += check("MIDI and a key share notes: C E G B once each", ngated(t) == 4u);
    midi(0x80, 60, 0);
    bad += check("  MIDI C off: C ends, E G stay for the key", !gate_note(t, 60) && gate_note(t, 64) && gate_note(t, 67));
    key_up(K_E4);
    bad += check("  key up: nothing left", !ngated(t));
    midi(0xB0, 64, 127); midi(0x90, 65, 100); midi(0x80, 65, 0);
    bad += check("pedal: the chord (F A C) held after its note-off", gate_note(t, 65) && gate_note(t, 69) && gate_note(t, 72));
    midi(0xB0, 64, 0);
    bad += check("  pedal up: released", !ngated(t));
    for (i = 0; i < MCHORD_N + 2u; i++) midi(0x90, 36 + i, 100);   /* more chords than the table holds */
    for (i = 0, free = 1; i < 2u; i++) free &= gate_note(t, 36 + MCHORD_N + i);   /* (the last two: their root alone) */
    for (i = 0; i < MCHORD_N + 2u; i++) midi(0x80, 36 + i, 0);
    bad += check("a full chord table: further notes play alone; all end", free && !ngated(t) && !t->nmono);
    midi(0x90, 62, 100); midi(0xB0, 120, 0);
    for (i = 0, free = 1; i < MCHORD_N; i++) free &= !mchord[i].id;
    bad += check("All Sound Off forgets the track's MIDI chords", free && !ngated(t));
    return bad;
}

static int kits(void)
{
    int bad = 0;
    track_t *d = &trk[3];
    uint8_t out[CHORD_MAX];
    reset();
    song.sel = 3;                                                  /* the DRUM track */
    d->p[P_CHRD] = CH_DIA7;
    key_down(K_C4);
    bad += check("DRUM: CHRD ignored (one hit per key)", chord_build(d, 36, out) == 1u && out[0] == 36 &&
                 kb_chn[K_C4] == 1u && mo_w == 1u);
    key_up(K_C4);
    midi(0x93, 38, 100);
    bad += check("  MIDI IN on the DRUM track: one hit, no chord kept", !mchord[0].id);
    midi(0x83, 38, 0);
    song.sel = 0;
    host_preset(&trk[0],ENGI_DRUM,0);trk[0].p[P_CHRD]=CH_DIA3;
    bad += check("DRUM ignores melodic chords",chord_kit(&trk[0]) && chord_build(&trk[0],38,out)==1u);
    return bad;
}

/* Real input ownership, scheduler and recorder, including batched advances. */
static int strumming(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    static const uint8_t den[] = {0, 2, 4, 8, 16, 32, 6, 12};
    for (uint32_t rate = 1; rate < NELEM(den); rate++) {
        reset(); t->p[P_CHRD] = CH_MAJ7; t->p[P_STRUM] = (int16_t)rate;
        uint32_t span = seq_beat_samples() / den[rate];
        key_down(K_C4);
        int exact = ngated(t) == 1 && gate_note(t, 60) && mo_w == 1;
        seq_advance(span / 3 - 1); exact &= ngated(t) == 1;
        seq_advance(1); exact &= ngated(t) == 2 && gate_note(t, 64);
        seq_advance(span * 2 / 3 - span / 3); exact &= ngated(t) == 3 && gate_note(t, 67);
        seq_advance(span - span * 2 / 3); exact &= ngated(t) == 4 && gate_note(t, 71);
        seq_advance(span * 2); exact &= mo_w == 4 && !kb_strum_pending;
        key_up(K_C4); exact &= !ngated(t) && mo_w == 8;
        bad += check("each strum division: exact first-to-last span, four ordered tones, one sweep, balanced MIDI", exact);
    }
    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 2;
    key_down(K_C4); key_up(K_C4); seq_advance(seq_beat_samples());
    bad += check("early release cancels pending tones without unmatched MIDI offs", !ngated(t) && mo_w == 2 && !kb_strum_pending);

    t->p[P_STROKE] = 1;
    key_down(K_C4);
    bad += check("UP starts at the highest voiced pitch", gate_note(t, 67) && ngated(t) == 1);
    t->p[P_STRUM] = 1; t->p[P_STROKE] = 0; t->p[P_CHRD] = CH_MIN7;
    seq_advance(seq_beat_samples() / 4);
    bad += check("held sweep keeps its original rate, shape and direction", ngated(t) == 3 && gate_note(t, 64) && gate_note(t, 60) && !gate_note(t, 63));
    key_up(K_C4);
    t->p[P_CHRD] = CH_MAJ; t->p[P_STROKE] = 2;
    key_down(K_C4); int alt = gate_note(t, 60); key_up(K_C4);
    key_down(K_C4); alt &= gate_note(t, 67); key_up(K_C4);
    bad += check("ALT alternates direction on successive chord presses", alt && !ngated(t));

    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 2;
    key_down(K_C4); key_down(K_E4); key_up(K_E4);
    seq_advance(seq_beat_samples() / 4);
    bad += check("an unplayed shared tone never steals or retains another key's ownership", ngated(t) == 3 && gate_note(t,60) && gate_note(t,64) && gate_note(t,67));
    key_up(K_C4);
    key_down(K_C4); seq_stop(); seq_advance(seq_beat_samples());
    bad += check("STOP cancels pending strings while the physically held first tone can ring", ngated(t) == 1 && !kb_strum_pending);
    key_up(K_C4);
    t->p[P_AMODE] = 1; key_down(K_C4);
    bad += check("ARP receives the complete chord immediately instead of a partial strum", t->nheld == 3 && !kb_strum_pending);
    key_up(K_C4);

    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 2;
    key_down(K_C4); panic_req = 1; events_block(0); seq_advance(seq_beat_samples());
    bad += check("preset/engine panic cancels keyboard strings and balances their MIDI output", !ngated(t) && !kb_strum_pending && !kb_chn[K_C4] && mo_w == 2);
    key_up(K_C4);

    for (uint32_t ext = 0; ext < 2; ext++) {
        reset(); midi_sync_active = (uint8_t)ext;
        t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 4;
        uint32_t span = seq_beat_samples() / 16;
        midi_note_event(0,60,87);
        int exact = ngated(t)==1 && gate_note(t,60);
        seq_advance(span / 2 - 1); exact &= ngated(t)==1;
        seq_advance(1); exact &= ngated(t)==2 && gate_note(t,64);
        seq_advance(span - span/2); exact &= ngated(t)==3 && gate_note(t,67);
        midi_note_event(0,60,0); seq_advance(span);
        bad += check("MIDI strums use internal/external beat units and release exactly their started tones", exact && !ngated(t) && !midi_strum_pending);
    }
    midi_sync_active = 0;
    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 2;
    midi_note_event(0,60,90); midi_note_event(0,60,0); seq_advance(seq_beat_samples());
    bad += check("MIDI early note-off cancels the rest of its sweep", !ngated(t) && !midi_strum_pending);
    midi_channel(0)->pedal = 1;
    midi_note_event(0,60,90); midi_note_event(0,60,0); seq_advance(seq_beat_samples()/4);
    bad += check("MIDI sustain lets the held sweep complete", ngated(t)==3 && !midi_strum_pending);
    midi_pedal_up(0);
    bad += check("pedal-up releases all strummed tones", !ngated(t));
    midi_note_event(0,60,90); midi_silence_track(0); seq_advance(seq_beat_samples());
    bad += check("All Sound Off cancels MIDI strings", !ngated(t) && !midi_strum_pending);
    midi_note_event(0,60,90); midi_in_overflow = 1; events_block(0); seq_advance(seq_beat_samples());
    bad += check("MIDI overflow cannot revive a pending string", !ngated(t) && !midi_strum_pending);

    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 2;
    song.g[G_BPM] = 120; key_down(K_C4);
    seq_advance(seq_beat_samples()/16); /* one quarter of the sweep */
    song.g[G_BPM] = 60;
    seq_advance(seq_beat_samples()/16);
    bad += check("tempo changes retain elapsed beat fraction", ngated(t)==2 && gate_note(t,64));
    midi_sync_active = 1;
    seq_advance(MIDI_BEAT_UNITS/8);
    bad += check("clock-domain changes retain the remaining sweep", ngated(t)==3 && gate_note(t,67));
    key_up(K_C4); midi_sync_active = 0;

    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 2;
    song.rec = 1; seq_start(); seq_advance(0); song.rec_pass = 0;
    uint32_t span = seq_beat_samples()/4;
    key_down(K_C4); seq_advance(span); key_up(K_C4);
    int recorded = recording[0].vel && recording[1].vel && recording[2].vel &&
                   recording_on(t,&recording[0],seq_div_samples(t->p[P_SDIV])) < recording_on(t,&recording[1],seq_div_samples(t->p[P_SDIV])) &&
                   recording_on(t,&recording[1],seq_div_samples(t->p[P_SDIV])) < recording_on(t,&recording[2],seq_div_samples(t->p[P_SDIV]));
    recorded_note_t saved[3]; memcpy(saved,recording,sizeof saved);
    seq_stop(); song.rec = 0; seq_start(); seq_advance(0);
    int replay = ngated(t)==1 && gate_note(t,60);
    seq_advance(span/2); replay &= gate_note(t,64) && !gate_note(t,67);
    /* Release was at the final onset, so the final tone has minimum duration. */
    seq_advance(span-span/2);
    bad += check("live recording captures separate strum onsets and replays their timing", recorded && replay && !memcmp(saved,recording,sizeof saved));
    seq_stop();
    t->p[P_STRUM] = 4; t->p[P_STROKE] = 2;
    apply_preset_to(t,0);
    bad += check("strum and stroke survive sound loads", t->p[P_STRUM]==4 && t->p[P_STROKE]==2);
    project_capture(&proj_scratch); t->p[P_STRUM]=0; t->p[P_STROKE]=0;
    project_restore_runtime(&proj_scratch);
    bad += check("project capture/restore retains strum controls", t->p[P_STRUM]==4 && t->p[P_STROKE]==2);
    return bad;
}

/* Recreate the previous parameter layout without changing the fixed native
 * patches or FX tail. Its engine slots must never become strum settings. */
static int strum_storage(void)
{
    reset(); track_t *t = &trk[0];
    t->p[P_STRUM]=4; t->p[P_STROKE]=2; t->p[P_E3]=23;
    t->p[P_FXTONE]=-20; song.g[G_MFXTONE]=17;
    project_capture(&proj_scratch);
    proj_scratch.motion.count=1; proj_scratch.motion.on=1;
    proj_scratch.motion.event[0]=(motion_event_t){0,P_E3,23};
    static project_store_t old;
    int bad=check("strum project serializes",proj_pack(&old,&proj_scratch));
    uint32_t src=68, dst=68;
    for(uint32_t k=0;k<NROWS;k++) {
        memmove(old.raw+dst,old.raw+src,114);dst+=114;src+=116;
        uint32_t tail=8+2+NSTEP*8;
        memmove(old.raw+dst,old.raw+src,tail);dst+=tail;src+=tail;
    }
    uint32_t motion_at=dst+sizeof(chain_legacy_t);
    memmove(old.raw+dst,old.raw+src,PROJ_TONE_WIRE-src);
    memset(old.raw+PROJ_TONE_WIRE-(src-dst),0,src-dst);
    /* Rewrite the source-era engine motion ID and count, then authenticate. */
    old.raw[motion_at+__builtin_offsetof(motion_store_t,event)+__builtin_offsetof(motion_event_t,param)]=117;
    old.raw[66]=122;
    uint32_t hash=proj_hash(old.raw,sizeof old-4);
    memcpy(old.raw+sizeof old-4,&hash,4);
    int ok=proj_import(&proj_scratch,&old,sizeof old);
    bad+=check("pre-strum project imports with OFF, original engine values/motion and FX tones",
        ok && !proj_scratch.t[0].p[P_STRUM] && !proj_scratch.t[0].p[P_STROKE] &&
        proj_scratch.t[0].p[P_E3]==23 && proj_scratch.motion.event[0].param==P_E3 &&
        proj_scratch.t[0].p[P_FXTONE]==-20 && proj_scratch.g[G_MFXTONE]==17);
    persist_old=tmpl_old=0; template_save();
    static tmpl4_t t4; tmpl4_capture(&t4);              /* (a TPLL template, the layout before six tracks) */
    static uint8_t blob[sizeof(tmpl4_t)]; memcpy(blob,&t4,sizeof t4);
    src=dst=2*PROJ_GLOBALS+2;
    for(uint32_t k=0;k<NROWS;k++) {
        uint32_t head=2+114*2;
        memmove(blob+dst,blob+src,head);dst+=head;src+=head+4;
        memmove(blob+dst,blob+src,16);dst+=16;src+=16;
    }
    memmove(blob+dst,blob+src,sizeof t4-src);
    uint32_t len=sizeof t4-(src-dst);
    memcpy(blob+len-8,&len,4);
    ok=tmpl_take(blob,len);
    bad+=check("pre-strum template imports with OFF and original native/engine/FX data",
        ok && !tmpl.t[0].p[P_STRUM] && !tmpl.t[0].p[P_STROKE] &&
        tmpl.t[0].p[P_E3]==23 && tmpl.t[0].p[P_FXTONE]==-20 && tmpl.master[14]==81);
    up_rec_t preset; memset(&preset,0,sizeof preset);
    preset.used=UP_USED; preset.ver=UP_VER_CZ; preset.engine=ENGI_CZ;
    preset.np=122; memcpy(preset.name,"OLD CZ",7); cz_patch_init(preset.packed);
    bad+=check("pre-strum CZ preset remains valid",up_valid(&preset));
    memset(&preset,0,sizeof preset); preset.np=125;
    int16_t defaults[P_COUNT],values[P_COUNT];
    for(uint32_t i=0;i<P_COUNT;i++)defaults[i]=TP[i].def;
    for(uint32_t i=0;i<125;i++)up_set_value(&preset,i,0);
    up_set_value(&preset,114,39);up_set_value(&preset,115,27);up_set_value(&preset,120,23);
    up_params(&preset,values,defaults);
    bad+=check("retired high-cut preset values cannot enable strum or change its direction",
        !values[P_STRUM] && !values[P_STROKE] && values[P_E3]==23);
    t->p[P_STRUM]=t->p[P_STROKE]=0;go_page(GR_CHORD);
    turn(EN_K3,4);turn(EN_K4,2);
    bad+=check("CHORD knobs 3 and 4 edit strum duration and stroke direction",t->p[P_STRUM]==4 && t->p[P_STROKE]==2);
    return bad;
}


/* EXTRA (Hookology, P_CHEXT on CHORD 2): 6 9 6/9 SUS2 SUS4 on DIA3 / DIA7 and the shapes, in key, at most 4 notes */
static int extras(void)
{
    int bad = 0, ok;
    track_t *t = &trk[0];
    reset();
    t->p[P_CHRD] = CH_DIA3; t->p[P_CHEXT] = EX_9;
    bad += check("EXTRA 9, DIA3 on C: C E G D (Cadd9)", chord_is(60, (const uint8_t[]){60, 64, 67, 74}) && name_is(60, "Cadd9"));
    bad += check("EXTRA 9, DIA3 on D: D F A E (Dmadd9)", chord_is(62, (const uint8_t[]){62, 65, 69, 76}) && name_is(62, "Dmadd9"));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("EXTRA 9, DIA7 on C: C E B D, no 5th (Cmaj9)", chord_is(60, (const uint8_t[]){60, 64, 71, 74}) && name_is(60, "Cmaj9"));
    bad += check("EXTRA 9, DIA7 on D: D F C E (Dm9); on G: G B F A (G9)", chord_is(62, (const uint8_t[]){62, 65, 72, 76}) &&
                 name_is(62, "Dm9") && chord_is(67, (const uint8_t[]){67, 71, 77, 81}) && name_is(67, "G9"));
    t->p[P_CHEXT] = EX_6; t->p[P_CHRD] = CH_DIA3;
    bad += check("EXTRA 6, DIA3 on F: F A C D (F6)", chord_is(65, (const uint8_t[]){65, 69, 72, 74}) && name_is(65, "F6"));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("EXTRA 6, DIA7 on F: F A E and D an octave up (Fmaj13); on G: G13",
                 chord_is(65, (const uint8_t[]){65, 69, 76, 86}) && name_is(65, "Fmaj13") && name_is(67, "G13"));
    t->p[P_CHEXT] = EX_69;
    bad += check("EXTRA 6/9 on C, DIA3 and DIA7: C E A D (C6/9)", chord_is(60, (const uint8_t[]){60, 64, 69, 74}) && name_is(60, "C6/9") &&
                 (t->p[P_CHRD] = CH_DIA3, chord_is(60, (const uint8_t[]){60, 64, 69, 74})));
    t->p[P_CHEXT] = EX_SUS4;
    bad += check("EXTRA SUS4, DIA3 on G: G C D (Gsus4)", chord_is(67, (const uint8_t[]){67, 72, 74, 0}) && name_is(67, "Gsus4"));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("EXTRA SUS4, DIA7 on G: G C D F (G7sus4)", chord_is(67, (const uint8_t[]){67, 72, 74, 77}) && name_is(67, "G7sus4"));
    t->p[P_CHEXT] = EX_SUS2;
    bad += check("EXTRA SUS2, DIA7 on A: A B E G (A7sus2); DIA3: A B E (Asus2)", chord_is(69, (const uint8_t[]){69, 71, 76, 79}) &&
                 name_is(69, "A7sus2") && (t->p[P_CHRD] = CH_DIA3, chord_is(69, (const uint8_t[]){69, 71, 76, 0})) && name_is(69, "Asus2"));
    t->p[P_CHRD] = CH_DOM7; t->p[P_CHEXT] = EX_9;
    bad += check("EXTRA 9 on the DOM7 shape: C E Bb D (C9)", chord_is(60, (const uint8_t[]){60, 64, 70, 74}) && name_is(60, "C9"));
    t->p[P_CHRD] = CH_DIA7; t->p[P_VOIC] = VC_BASS;
    bad += check("+OCT with DIA7 + 9 (no 5th): the root goes down, C under E B D", chord_is(60, (const uint8_t[]){48, 64, 71, 74}));
    t->p[P_CHRD] = CH_DIA3;
    bad += check("+OCT with DIA3 + 9: the 5th goes, C C E D", chord_is(60, (const uint8_t[]){48, 60, 64, 74}));
    t->p[P_VOIC] = VC_CLOSE; t->p[P_SCALE] = 5; t->p[P_CHEXT] = EX_SUS4;   /* C major pentatonic: no F */
    bad += check("pentatonic: SUS4 with no 4th in the scale leaves C E A", chord_is(60, (const uint8_t[]){60, 64, 69, 0}));
    {
        uint32_t s, r, x, note, n, k;
        uint8_t out[CHORD_MAX];
        ok = 1;
        for (x = EX_6; x <= EX_SUS4; x++)
            for (s = 1; s <= 15u; s++)
                for (r = 0; r < 12u; r += 5)
                    for (note = 48; note < 72u; note++) {
                        t->p[P_SCALE] = (int16_t)s; t->p[P_ROOT] = (int16_t)r; t->p[P_CHEXT] = (int16_t)x; t->p[P_CHBOR] = 1;
                        t->p[P_CHRD] = (int16_t)(note & 1u ? CH_DIA3 : CH_DIA7);
                        n = chord_build(t, note, out);
                        ok &= n >= 2u && n <= CHORD_MAX;
                        for (k = 0; k < n; k++)
                            ok &= ((scale_mask(t) >> ((out[k] + 120u - r) % 12u)) & 1u) && (!k || out[k] > out[k - 1u]);
                    }
        bad += check("EXTRA on DIA3 / DIA7, the 12-note scales and several roots: 2..4 notes, ascending, every one in key", ok);
    }
    t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; t->p[P_CHEXT] = EX_9; t->p[P_CHBOR] = 0;
    t->p[P_CHRD] = CH_HERA;
    {
        uint8_t a[CHORD_MAX], b[CHORD_MAX];
        uint32_t na = chord_build(t, 60, a);
        t->p[P_CHEXT] = 0;
        uint32_t nb = chord_build(t, 60, b);
        bad += check("HERA ignores EXTRA", na == nb && !memcmp(a, b, na));
    }
    {
        uint32_t pg = page_titled("CHORD 2");
        int on[4];
        t->p[P_CHRD] = CH_DIA7; on[0] = page_visible(pg);
        t->p[P_CHRD] = CH_POW; on[1] = page_visible(pg);
        t->p[P_CHRD] = CH_HERA; on[2] = page_visible(pg);
        t->p[P_CHRD] = CH_AUTO; on[3] = page_visible(pg) || (t->p[P_CHRD] = CH_OFF, page_visible(pg));
        bad += check("CHORD 2 (EXTRA) shows for DIA3 / DIA7 / the shapes only", pg < NPAGES && on[0] && on[1] && !on[2] && !on[3]);
    }
    bad += check("EXTRA is the track's: kept when a sound loads", param_kept(P_CHEXT) && !param_reserved(P_CHEXT));
    {
        project_t p;
        t->p[P_CHRD] = CH_DIA7; t->p[P_CHEXT] = EX_69; t->p[P_VOIC] = VC_LEAD;
        project_capture(&p);
        t->p[P_CHEXT] = 0; t->p[P_VOIC] = 0;
        p.sum = proj_sum(&p);
        bad += check("EXTRA and VOIC LEAD come back with the project", !project_restore_runtime(&p) &&
                     trk[0].p[P_CHEXT] == EX_69 && trk[0].p[P_VOIC] == VC_LEAD);
    }
    return bad;
}

/* VOIC LEAD (Hookology): the inversion closest to the last chord; root position after PLAY / STOP */
static int leading(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    chord_lead_forget();
    t->p[P_CHRD] = CH_DIA3; t->p[P_VOIC] = VC_LEAD;
    bad += check("LEAD: C F G C plays C E G, C F A, B D G, C E G",
                 chord_is(60, (const uint8_t[]){60, 64, 67, 0}) && chord_is(65, (const uint8_t[]){60, 65, 69, 0}) &&
                 chord_is(67, (const uint8_t[]){59, 62, 67, 0}) && chord_is(60, (const uint8_t[]){60, 64, 67, 0}));
    bad += check("LEAD: the names stay the chords' (F, G)", name_is(65, "F") && name_is(67, "G"));
    chord_is(65, (const uint8_t[]){60, 65, 69, 0});
    seq_stop();
    bad += check("LEAD: after STOP the next chord is in root position (G B D)", chord_is(67, (const uint8_t[]){67, 71, 74, 0}));
    {
        uint32_t i, n, k, ok = 1;
        uint8_t out[CHORD_MAX];
        static const uint8_t walk[] = {60, 72, 65, 77, 62, 74, 69, 81, 67, 79, 64, 76, 71, 83, 60, 48, 53, 41};
        t->p[P_CHRD] = CH_DIA7;
        for (i = 0; i < sizeof walk; i++) {
            n = chord_build(t, walk[i], out);
            ok &= n == 4u && out[0] + 12u >= walk[i] && out[0] < walk[i] + 12u && out[3] < walk[i] + 36u;
        }
        bad += check("LEAD: on any walk the lowest note stays within an octave of the key played, all four notes", ok);
    }
    t->p[P_CHEXT] = EX_9;
    chord_lead_forget();
    {
        uint8_t g[CHORD_MAX];
        int first = chord_is(62, (const uint8_t[]){62, 65, 72, 76});
        uint32_t n = chord_build(t, 67, g);
        bad += check("LEAD with EXTRA 9: Dm9 (D F C E), then G9 moves by step: F A B... within a 4th of it",
                     first && n == 4u && g[0] >= 58u && g[3] <= 80u);
    }
    return bad;
}


/* BORROW (Hookology, P_CHBOR 0 = ON, the default): DIA3 / DIA7 keys outside the scale play borrowed chords */
static int borrowed(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset();
    bad += check("BORROW is ON by default (stored 0), the track's, a value of CHORD 2",
                 TP[P_CHBOR].def == 0 && t->p[P_CHBOR] == 0 && param_kept(P_CHBOR) && !param_reserved(P_CHBOR) &&
                 PAGES[page_titled("CHORD 2")].id[1] == P_CHBOR);
    t->p[P_CHRD] = CH_DIA3;
    bad += check("C major DIA3: C# Db (Neapolitan), D# Eb, F# F#dim, G# Ab, A# Bb",
                 chord_is(61, (const uint8_t[]){61, 65, 68, 0}) && name_is(61, "C#") &&
                 chord_is(63, (const uint8_t[]){63, 67, 70, 0}) && name_is(63, "D#") &&
                 chord_is(66, (const uint8_t[]){66, 69, 72, 0}) && name_is(66, "F#dim") &&
                 chord_is(68, (const uint8_t[]){68, 72, 75, 0}) && name_is(68, "G#") &&
                 chord_is(70, (const uint8_t[]){70, 74, 77, 0}) && name_is(70, "A#"));
    bad += check("..the keys in the scale are as before (D: Dm, B: Bdim)", name_is(62, "Dm") && name_is(71, "Bdim"));
    t->p[P_CHRD] = CH_DIA7;
    bad += check("C major DIA7: Dbmaj7, Ebmaj7, F#dim7, Abmaj7, Bb7 (the backdoor dominant)",
                 chord_is(61, (const uint8_t[]){61, 65, 68, 72}) && name_is(61, "C#maj7") &&
                 chord_is(63, (const uint8_t[]){63, 67, 70, 74}) && name_is(63, "D#maj7") &&
                 chord_is(66, (const uint8_t[]){66, 69, 72, 75}) && name_is(66, "F#dim7") &&
                 chord_is(68, (const uint8_t[]){68, 72, 75, 79}) && name_is(68, "G#maj7") &&
                 chord_is(70, (const uint8_t[]){70, 74, 77, 80}) && name_is(70, "A#7"));
    t->p[P_SCALE] = 2; t->p[P_ROOT] = 9; t->p[P_CHRD] = CH_DIA3;   /* A minor: borrows from A major */
    bad += check("A minor DIA3: C# C#m, F# F#m, G# G#dim, Bb (Neapolitan), D# D#dim",
                 chord_is(61, (const uint8_t[]){61, 64, 68, 0}) && name_is(61, "C#m") &&
                 chord_is(66, (const uint8_t[]){66, 69, 73, 0}) && name_is(66, "F#m") &&
                 chord_is(68, (const uint8_t[]){68, 71, 74, 0}) && name_is(68, "G#dim") &&
                 chord_is(70, (const uint8_t[]){70, 74, 77, 0}) && name_is(70, "A#") &&
                 chord_is(63, (const uint8_t[]){63, 66, 69, 0}) && name_is(63, "D#dim"));
    t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; t->p[P_CHEXT] = EX_9;
    bad += check("EXTRA on a borrowed chord: Ab + 9 takes C minor's Bb (G#add9)",
                 chord_is(68, (const uint8_t[]){68, 72, 75, 82}) && name_is(68, "G#add9"));
    t->p[P_CHEXT] = 0; t->p[P_CHRD] = CH_MAJ;
    bad += check("the fixed shapes ignore BORROW (MAJ on C#: C# F G#)", chord_is(61, (const uint8_t[]){61, 65, 68, 0}));
    t->p[P_CHRD] = CH_DIA3; t->p[P_CHBOR] = 1;
    bad += check("BORROW OFF: Ab key plays G (the scale note below)", chord_is(68, (const uint8_t[]){67, 71, 74, 0}));
    track_select(0); t->p[P_QUANT] = Q_SNAP; t->p[P_CHBOR] = 1;
    ui.home = 0; ui.page = (uint8_t)page_titled("CHORD 2"); ui.force = 1;
    turn(EN_K2, -1);
    bad += check("BORROW turned ON while QNT is SNAP: BORROW NEEDS QNT OFF", t->p[P_CHBOR] == 0 && msg_is("BORROW NEEDS QNT OFF"));
    t->p[P_QUANT] = Q_OFF;
    t->p[P_CHBOR] = 0; t->p[P_VOIC] = VC_LEAD;
    chord_lead_forget();
    bad += check("LEAD into borrowed chords: C, Ab, Bb as C E G, C Eb Ab, Bb D F",
                 chord_is(60, (const uint8_t[]){60, 64, 67, 0}) && chord_is(68, (const uint8_t[]){60, 63, 68, 0}) &&
                 chord_is(70, (const uint8_t[]){58, 62, 65, 0}));
    return bad;
}

/* (Hookology, beta53) RANGE 2 OCT: the chord's strings and an octave above, in the stroke's order, released exactly;
 * SLOP: the strings early or late but each within its gap, the sweep's first and order kept; the ARP patterns */
static int strum_more(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 1; t->p[P_SRANGE] = 1;      /* (1/8: the sweep, six strings) */
    key_down(K_C4);
    int ok = kb_strum[K_C4].total == 6u && gate_note(t, 60) && ngated(t) == 1;
    seq_advance(seq_beat_samples());
    static const uint8_t want[6] = {60, 64, 67, 72, 76, 79};
    for (uint32_t i = 0; i < 6; i++) ok &= gate_note(t, want[i]) && chord_tone_at(kb_chord[K_C4], &kb_strum[K_C4], i) == want[i];
    ok &= ngated(t) == 6 && mo_w == 6;
    key_up(K_C4); ok &= !ngated(t) && mo_w == 12;
    bad += check("RANGE 2 OCT: C E G then an octave up, all six released with balanced MIDI", ok);
    reset(); t->p[P_CHRD] = CH_MAJ; t->p[P_STRUM] = 1; t->p[P_SRANGE] = 1; t->p[P_STROKE] = 1;
    key_down(K_C4); ok = gate_note(t, 79);
    seq_advance(seq_beat_samples());
    ok &= ngated(t) == 6 && chord_tone_at(kb_chord[K_C4], &kb_strum[K_C4], 5) == 60;
    key_up(K_C4); ok &= !ngated(t);
    bad += check("RANGE 2 OCT UP: from the top G down to C", ok);
    t->p[P_SRANGE] = 0; t->p[P_STROKE] = 0;
    /* SLOP: over 40 presses the strings' times vary, each within a gap of its even time, in order of firing */
    uint32_t span = seq_beat_samples() / 2u, gap = span / 3u, varied = 0, within = 1;   /* (STRUM 1: 1/8) */
    for (uint32_t press = 0; press < 40; press++) {
        reset(); t->p[P_CHRD] = CH_MAJ7; t->p[P_STRUM] = 1; t->p[P_SLOP] = 127;
        key_down(K_C4);
        uint32_t at[4] = {0}, n = 1, now = 0;
        while (now < 2u * span && n < 4u) {
            seq_advance(64); now += 64;
            while (n < ngated(t) && n < 4u) at[n++] = now;
        }
        for (uint32_t i = 1; i < 4; i++) {
            int32_t d = (int32_t)at[i] - (int32_t)(gap * i);
            if (d < -(int32_t)gap - 64 || d > (int32_t)gap + 64) within = 0;
            if (d < -200 || d > 200) varied = 1;
        }
        key_up(K_C4);
    }
    t->p[P_SLOP] = 0;
    bad += check("SLOP: the strings early or late at random, each within a gap of its time", within && varied);
    reset(); t->p[P_CHRD] = CH_MAJ7; t->p[P_STRUM] = 1;
    key_down(K_C4); seq_advance(gap - 1); ok = ngated(t) == 1; seq_advance(1); ok &= ngated(t) == 2; key_up(K_C4);
    bad += check("SLOP 0: the even sweep as before", ok);
    /* ARP patterns: PULSE 1 2 3 4 (the held notes from the lowest), WALTZ 1 3 2, SYNCO with its rests */
    static const struct { int16_t mode; uint8_t want[8]; } P[] = {
        {7, {60, 64, 67, 71, 60, 64, 67, 71}},                    /* pulse */
        {9, {60, 67, 64, 60, 67, 64, 60, 67}},                    /* waltz */
        {12, {60, 0, 64, 67, 0, 60, 67, 0}},                      /* syncopate */
    };
    for (uint32_t k = 0; k < NELEM(P); k++) {
        reset(); t->p[P_CHRD] = 0; t->p[P_AMODE] = P[k].mode; t->p[P_ARATE] = 2; t->p[P_AOCT] = 1; t->p[P_APROB] = 127;
        arp_add(t, 67); arp_add(t, 60); arp_add(t, 71); arp_add(t, 64);
        uint32_t period = seq_div_samples(2);
        ok = 1;
        for (uint32_t i = 0; i < 8; i++) {
            arp_step(t, i ? period : 0, i ? period : 0);
            ok &= t->arp_note == P[k].want[i];
        }
        while (t->nheld) arp_remove(t, t->held[0]);
        arp_step(t, period, period);
        t->p[P_AMODE] = 0;
        bad += check(k == 0 ? "ARP PULSE: the held notes 1 2 3 4 from the lowest" : k == 1 ? "ARP WALTZ: 1 3 2" : "ARP SYNCO: its rests silent", ok);
    }
    {   /* CHORD 2: EXTRA / BORROW where they apply, SLOP / RANGE while a strum is on (HERA too) */
        int16_t *vp;uint32_t pg = page_titled("CHORD 2");
        reset(); song.sel = 0; t->p[P_CHRD] = CHRD_HERA; t->p[P_STRUM] = 0;
        ok = !page_visible(pg);
        t->p[P_STRUM] = 2;
        ok &= page_visible(pg) && !page_desc(&PAGES[pg], 0, &vp) && !page_desc(&PAGES[pg], 1, &vp) &&
              !strcmp(page_desc(&PAGES[pg], 2, &vp)->label, "SLOP") && !strcmp(page_desc(&PAGES[pg], 3, &vp)->label, "RANGE");
        t->p[P_CHRD] = CH_DIA3; t->p[P_STRUM] = 0;
        ok &= page_visible(pg) && page_desc(&PAGES[pg], 0, &vp) && !page_desc(&PAGES[pg], 2, &vp);
        t->p[P_CHRD] = 0; t->p[P_STRUM] = 2; ok &= !page_visible(pg);
        t->p[P_STRUM] = 0;
        bad += check("CHORD 2: EXTRA / BORROW where they apply, SLOP / RANGE while a chord strums", ok);
    }
    bad += check("ARP MODE names: the 12 patterns after REPEAT", !strcmp(N_AMODE[7], "PULSE") && !strcmp(N_AMODE[18], "LADDR") && NELEM(N_AMODE) == 19u);
    return bad;
}

/* (Hookology, beta53) CHORD NAMES FLAT (the default): Ab, Bb, Eb, Db, Gb; SHARP as before; the slash bass */
static int names_flat(void)
{
    int bad = 0;
    char b[16];
    reset(); settings_sharps = 0;
    chord_name(b, 8, 0x91); int ok = !strcmp(b, "Ab");
    chord_name(b, 10, 0x489); ok &= !strcmp(b, "Bbm7");
    chord_name(b, 3, 0x891); ok &= !strcmp(b, "Ebmaj7");
    chord_name(b, 6, 0x49); ok &= !strcmp(b, "Gbdim");
    chord_name(b, 1, 0x91); ok &= !strcmp(b, "Db");
    bad += check("CHORD NAMES FLAT: Ab, Bbm7, Ebmaj7, Gbdim, Db", ok);
    settings_sharps = 1; chord_name(b, 8, 0x91); ok = !strcmp(b, "G#");
    bad += check("CHORD NAMES SHARP: G#", ok);
    ok = !strcmp(chord_note(4), "E") && !strcmp(chord_note(10), "A#");
    settings_sharps = 0; ok &= !strcmp(chord_note(10), "Bb");
    bad += check("the slash bass in the same spelling (E, A# or Bb)", ok);
    settings_sharps = 1;
    return bad;
}

int main(void)
{
    int bad = theory() + shapes_voicings() + keys() + arp_rec() + midi_in() + kits() + strumming() + strum_storage() + extras() + leading() + borrowed() + strum_more() + names_flat();
    printf("%s\n", bad ? "CHORD TEST FAILED" : "chord keys test passed");
    return bad != 0;
}
