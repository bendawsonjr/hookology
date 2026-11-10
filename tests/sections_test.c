/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* SEQ > SECTIONS: the section keys (play, copy, the question), the switch at the section's loop end (its longest track) with
 * every track from step 0, SONG REC from played sections, the song list's knobs and undo, PLAY there */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void put_note(uint32_t k, uint32_t len, uint32_t note)
{
    memset(trk[k].step, 0, sizeof trk[k].step);
    trk[k].p[P_SLEN] = (int16_t)len;
    trk[k].step[0] = (step_t){{(uint8_t)note}, 1, ST_NOTE, 0, 90};
    pattern_commit(&trk[k]);
}
static void setup(void)
{
    ui_power_on(); stop_transport();
    chain_defaults(&chain_config); memset(chain_patterns, 0, sizeof chain_patterns);
    put_note(0, 2, 36); put_note(1, 4, 48);              /* section A: track 1 two steps, track 2 four */
    go_page(GR_SECTIONS); ui.song_row = 0; frame();
    assert(cur_page()->graph == GR_SECTIONS && sections_keys_on());
}
static void sec_hold_key(uint32_t b) { key_down(SEC_KEY[b]); frames(1100u); key_up(SEC_KEY[b]); frame(); }
static uint32_t tick(void) { return seq_div_samples((uint32_t)trk[0].p[P_SDIV]); }

static void keys_and_copy(void)
{
    setup();
    uint32_t before = TSEL->nheld;
    sec_hold_key(2);                                    /* C empty: A copied there, then C plays */
    assert(trk[0].pattern == 2 && trk[1].pattern == 2 && trk[2].pattern == 2 && trk[3].pattern == 2);
    assert(trk[0].step[0].note[0] == 36 && trk[1].step[0].note[0] == 48 && trk[1].p[P_SLEN] == 4);
    assert(!pattern_empty(0, 0) && !pattern_empty(1, 0) && TSEL->nheld == before);
    tap_key(SEC_KEY[1]); frame();                       /* B: stopped, at once */
    assert(trk[0].pattern == 1 && trk[3].pattern == 1 && pattern_empty(0, 1));
    tap_key(SEC_KEY[0]); frame();
    assert(trk[0].pattern == 0 && sec_playing() == 0);
    put_note(0, 2, 40);                                 /* A changed: copy it over C, which has notes: asked */
    sec_hold_key(2);
    assert(ui.confirm == CF_SEC_COPY && pattern_at(0, 2)->step[0].note[0] == 36);
    press(B_OCTDN);
    assert(!ui.confirm && trk[0].pattern == 0 && pattern_at(0, 2)->step[0].note[0] == 36);
    sec_hold_key(2); press(B_OCTUP);
    assert(!ui.confirm && trk[0].pattern == 2 && trk[0].step[0].note[0] == 40);
    sec_hold_key(2);                                    /* the section playing: nothing to copy */
    assert(!ui.confirm && msg_is("ALREADY IN C"));
    uint32_t dim, breath, lit = sections_leds(&dim, &breath);
    assert((lit >> SEC_KEY[2]) & 1u && (dim >> SEC_KEY[0]) & 1u && !((dim >> SEC_KEY[1]) & 1u));
    puts("SECTIONS: tap, copy, the question, the section playing and the key lights passed");
}
static void switch_at_loop_end(void)
{
    setup();
    sec_hold_key(1); tap_key(SEC_KEY[0]); frame();     /* B a copy of A; A plays */
    put_note(1, 4, 50);
    transport_req = 1; units_block(0);
    assert(song.playing && trk[0].pattern == 0);
    units_block(tick());                                /* track 1 on step 2 of 2, track 2 on step 2 of 4 */
    tap_key(SEC_KEY[1]); frame();
    assert(secs.next == 1 && sec_queued() == 1 && trk[0].pattern == 0);
    ui.home = 1; ui.home_pg = HP_TRACKS; ui.bpm_t = 0;   /* TRACKS' badge: A, B waiting; the tempo while SELECT turns it */
    assert(head_badge_on() && head_badge_sig() == 1u + 0u + 256u * 1u && !head_badge_rec());
    ui.bpm_t = 40; assert(!head_badge_on()); ui.bpm_t = 0;
    ui.home_pg = HP_STAGE; assert(!head_badge_on());
    go_page(GR_SECTIONS);
    assert(secs.lead == 1);                             /* the lead: track 2, A's longest loop */
    units_block(tick() / 2u);
    assert(trk[0].pattern == 0 && trk[1].pattern == 0);
    units_block(tick() / 2u + 3u);                       /* track 1's loop end: no switch, track 2 half through */
    assert(trk[0].pattern == 0 && trk[1].pattern == 0 && secs.next == 1);
    units_block(tick()); units_block(tick());           /* track 2's loop end: every track from step 0, 3 units in */
    assert(trk[0].pattern == 1 && trk[1].pattern == 1 && trk[2].pattern == 1);
    assert(trk[0].seq_idx == 0 && trk[1].seq_idx == 0 && trk[0].seq_pos == trk[1].seq_pos && trk[0].seq_pos == 3u);
    assert(secs.next == SEC_NONE);
    tap_key(SEC_KEY[0]); frame(); press(B_PLAY); units_block(0);
    assert(!song.playing && secs.next == SEC_NONE && trk[0].pattern == 1);
    puts("SECTIONS: a played section waits for the longest track's loop end, every track from step 0, STOP drops it passed");
}
/* the lead skips a longer track with no notes: track 1's loop end then */
static void empty_lead(void)
{
    setup();
    sec_hold_key(1); tap_key(SEC_KEY[0]); frame();
    memset(trk[1].step, 0, sizeof trk[1].step); trk[1].p[P_SLEN] = 16; pattern_commit(&trk[1]); scs.sig = 0;
    transport_req = 1; units_block(0); units_block(tick());
    tap_key(SEC_KEY[1]); frame();
    assert(secs.next == 1 && secs.lead == 0);
    units_block(tick() / 2u); units_block(tick() / 2u + 3u);
    assert(trk[0].pattern == 1 && trk[1].pattern == 1 && secs.next == SEC_NONE);
    puts("SECTIONS: an empty longer track does not hold the switch passed");
}
static void song_rec(void)
{
    setup();
    sec_hold_key(1); tap_key(SEC_KEY[0]); frame();
    uint32_t loop = 2u * tick();
    press(B_PLAY); units_block(0);                      /* playing: REC is SONG REC, from track 1's next loop */
    units_block(loop / 2u);
    press(B_REC); units_block(0);
    assert(song.playing && secs.rec && !scs.build && msg_is("SONG REC: NEXT LOOP"));
    units_block(loop / 2u);                             /* logged from here */
    units_block(loop); units_block(loop); units_block(loop);
    tap_key(SEC_KEY[1]); frame();
    units_block(loop / 2u); units_block(loop / 2u);      /* B waits for A's loop end (track 2's, two of track 1's) */
    assert(secs.next == 1 && trk[0].pattern == 0);
    units_block(loop / 2u); units_block(loop / 2u);
    assert(trk[0].pattern == 1);
    units_block(loop / 2u);
    press(B_REC);                                       /* finished: the song, the loop plays on */
    assert(!secs.rec && song.playing && chain_config.count == 2);
    assert(sec_row(0) == 0 && chain_config.row[0].repeat == 3 && sec_row(1) == 1 && chain_config.row[1].repeat == 1);   /* (A to its third end, B's tap waiting for it) */
    press(B_PLAY); units_block(0); frame();
    transport_req = 1; units_block(0); units_block(loop / 2u);   /* the loop playing (PLAY here: the song) */
    press(B_REC); units_block(0);                       /* over a song: asked first */
    assert(ui.confirm == CF_SEC_SONG && song.playing && !secs.rec);
    press(B_OCTUP); units_block(0);
    assert(secs.rec && song.playing);
    units_block(loop / 2u); units_block(loop); units_block(loop);
    press(B_PLAY); units_block(0); frame();             /* STOP finishes it too */
    assert(!secs.rec && chain_config.count == 1 && sec_row(0) == 1 && chain_config.row[0].repeat == 2);   /* (B still the loop: three of track 1's, B's two) */
    song_edit_history(0);                               /* SONG's undo brings back the song before */
    assert(chain_config.count == 2 && sec_row(1) == 1);
    puts("SECTIONS: SONG REC from played sections, its question, STOP and undo passed");
}
/* BUILD: REC stopped; taps add after the entry picked, the same section a repeat more; OCT- deletes and merges;
 * OCT- held clears; a hold copies nothing; undo; PLAY and REC end it */
static void build(void)
{
    setup();
    sec_hold_key(1); sec_hold_key(2); tap_key(SEC_KEY[0]); frame();   /* B and C copies of A; A the loop */
    press(B_REC); frame();
    assert(scs.build && !secs.rec && !song.playing && msg_is("BUILD") && sections_keys_on());
    tap_key(SEC_KEY[0]); frame(); tap_key(SEC_KEY[0]); frame();
    assert(chain_config.count == 1 && sec_row(0) == 0 && chain_config.row[0].repeat == 2 && msg_is("A x2"));
    tap_key(SEC_KEY[1]); frame(); tap_key(SEC_KEY[2]); frame(); tap_key(SEC_KEY[1]); frame();
    assert(chain_config.count == 4 && sec_row(1) == 1 && sec_row(2) == 2 && sec_row(3) == 1 && ui.song_row == 3);
    assert(trk[0].pattern == 0);                        /* (taps never switched the loop) */
    tap_key(SEC_KEY[5]); frame();                       /* F empty: not added */
    assert(chain_config.count == 4 && msg_is("F IS EMPTY"));
    turn(EN_K1, -3); tap_key(SEC_KEY[2]); frame();     /* after A: C, a new entry */
    assert(chain_config.count == 5 && sec_row(1) == 2 && ui.song_row == 1);   /* A2 C B C B */
    tap_key(SEC_KEY[1]); frame();                       /* after C, B already next: a repeat more there */
    assert(chain_config.count == 5 && sec_row(2) == 1 && chain_config.row[2].repeat == 2 && ui.song_row == 2);
    turn(EN_K1, 1); press(B_OCTDN); frame();            /* C deleted: B2 and B one entry, B x3; B picked */
    assert(chain_config.count == 3 && sec_row(2) == 1 && chain_config.row[2].repeat == 3 && ui.song_row == 2);
    assert(msg_is("ENTRY DELETED") && cur_page()->graph == GR_SECTIONS);
    hold(B_EDIT);                                       /* undo: C back */
    assert(chain_config.count == 5 && sec_row(3) == 2 && sec_row(4) == 1);
    sec_hold_key(3);                                    /* a hold: no copy, no question */
    assert(!ui.confirm && pattern_empty(0, 3) && chain_config.count == 5);
    btn_down(B_OCTDN); frame(); frames(1100u); btn_up(B_OCTDN); frame();   /* held: clear, asked */
    assert(ui.confirm == CF_CLEAR_SONG && chain_config.count == 5);
    press(B_OCTUP); frame();
    assert(!ui.confirm && chain_config.count == 0 && scs.build);
    tap_key(SEC_KEY[2]); frame();
    assert(chain_config.count == 1 && sec_row(0) == 2);
    press(B_REC); frame();
    assert(!scs.build && msg_is("BUILD DONE"));
    press(B_REC); frame(); press(B_PLAY); units_block(0); frame();   /* PLAY ends it: the song plays */
    assert(!scs.build && chain.running);
    press(B_PLAY); units_block(0); frame();
    press(B_REC); frame(); go_page(GR_SONG); frame();   /* leaving the page ends it */
    assert(!scs.build);
    puts("SECTIONS: BUILD adds, repeats, deletes, merges, clears, undoes and ends passed");
}
/* a song entry's repeat is the whole section: its longest track's loop (track 2's four steps here, track 1's two) */
static void whole_section(void)
{
    uint32_t loop1;
    setup();
    sec_hold_key(1); tap_key(SEC_KEY[0]); frame();     /* B a copy of A; A the loop */
    loop1 = 2u * tick();
    press(B_REC); frame(); tap_key(SEC_KEY[0]); frame(); tap_key(SEC_KEY[0]); frame(); tap_key(SEC_KEY[1]); frame();
    press(B_REC); frame();
    assert(chain_config.count == 2 && chain_config.row[0].repeat == 2);
    press(B_PLAY); units_block(0); frame();
    assert(chain.running && chain.row == 0 && chain.lead[0] == 1u);
    {                                                   /* A x2: four of track 1's loops (eight steps), then B */
        uint32_t q = tick() / 4u, i, at = 0;
        for (i = 0; i < 40u && !at; i++) { units_block(q); frame(); if (chain.row == 1u) at = i + 1u; }
        assert(at == 32u && trk[0].pattern == 1u);
        (void)loop1;
    }
    press(B_PLAY); units_block(0);
    puts("SECTIONS: a song entry's repeat is the whole section (its longest track) passed");
}
static void song_list(void)
{
    setup();
    turn(EN_K2, 1);                                     /* entry 1 (new): A */
    assert(chain_config.count == 1 && sec_row(0) == 0);
    turn(EN_K2, 1); turn(EN_K3, 1); turn(EN_K3, 1);
    assert(sec_row(0) == 1 && chain_config.row[0].repeat == 3);
    turn(EN_K4, 1); turn(EN_K4, 1);                     /* the song longer: the last entry again */
    assert(chain_config.count == 3 && sec_row(2) == 1 && chain_config.row[2].repeat == 3);
    turn(EN_K1, 2); turn(EN_K2, -1);
    assert(ui.song_row == 2 && sec_row(2) == 0 && sec_row(1) == 1);
    turn(EN_K4, -1);
    assert(chain_config.count == 2 && ui.song_row == 2);
    hold(B_EDIT);                                       /* undo: the entry back */
    assert(chain_config.count == 3 && sec_row(2) == 0);
    chain_patterns[1][1] = 5;                           /* an entry SONG changed on one track: '*' */
    assert(sec_row(1) == SEC_NONE);
    chain_patterns[1][1] = 1;
    turn(EN_K1, -1); press(B_PLAY); units_block(0);     /* PLAY: the song from the top, whatever the entry */
    assert(chain.running && chain.row == 0);
    tap_key(SEC_KEY[2]); frame();
    assert(msg_is("STOP SONG TO SWITCH"));
    press(B_PLAY); units_block(0);
    assert(!chain.running && !song.playing);
    chain_defaults(&chain_config); chain_config.count = 0;
    press(B_PLAY); units_block(0);                      /* no song: the loop */
    assert(song.playing && !chain.running);
    press(B_PLAY); units_block(0);
    puts("SECTIONS: the song list's knobs, '*', undo and PLAY passed");
}
/* the build's SEQ (Hookology): PATTERNS and SONG hidden, SEQ held opens SECTIONS, TEMPO on GLO after MIXER */
static void seq_pages(void)
{
    uint32_t seen = 0, i;
    ui_power_on(); page_patterns_on = HOOKOLOGY_PATTERNS_PAGE; page_song_on = HOOKOLOGY_SONG_PAGE;
    go_home(); press(B_SEQ);
    for (i = 0; i < 12u; i++) {
        uint32_t g = cur_page()->graph;
        assert(cur_page()->fam == FAM_SEQ && g != GR_PATGRID && g != GR_SONG && !str_eq(cur_page()->title, "TEMPO"));
        if (g == GR_SECTIONS) seen |= 1u;
        press(B_SEQ);
    }
    assert(seen == 1u);
    go_home(); hold(B_SEQ); assert(!ui.home && cur_page()->graph == GR_SECTIONS);
    go_home(); press(B_GLO); assert(cur_page()->graph == GR_TRK);
    press(B_GLO); assert(str_eq(cur_page()->title, "TEMPO"));
    press(B_GLO); assert(str_eq(cur_page()->title, "GLOBAL"));
    page_patterns_on = page_song_on = 1;
    puts("SEQ: PATTERNS and SONG hidden, SEQ held opens SECTIONS, TEMPO after MIXER on GLO passed");
}
/* a DAW's Start on SECTIONS (CLK USB): the song from its first entry, in time; elsewhere, or CLK INT: the loop */
static void daw(uint32_t status) { midi_enqueue(0xFu | status << 8, 1u); units_block(0); }
static void daw_start(void)
{
    setup();
    sec_hold_key(1); tap_key(SEC_KEY[0]); frame();     /* B a copy of A */
    press(B_REC); frame(); tap_key(SEC_KEY[0]); frame(); tap_key(SEC_KEY[1]); frame(); press(B_REC); frame();
    assert(chain_config.count == 2 && !scs.build);
    frames(300u); assert(!chain_cued);                 /* CLK INT: nothing cued */
    song.g[G_CLOCK] = 1; frames(32u);
    assert(chain_cued && !chain.armed && !song.playing && !transport_busy());
    daw(0xFAu);
    assert(song.playing && chain.running && chain.row == 0 && !chain_cued);
    daw(0xFCu); frame();
    assert(!song.playing && !chain.running && trk[0].pattern == 0);
    frames(300u); assert(chain_cued);                  /* cued again for the next take */
    daw(0xFCu); frames(32u); assert(chain_cued && !song.playing);   /* a Stop while stopped keeps it */
    midi_enqueue(0x3u | 0xF2u << 8, 1u); daw(0xFBu); daw(0xF8u);   /* Continue from the top: as Start */
    assert(song.playing && chain.running && chain.row == 0);
    daw(0xFCu); frame();
    go_page(GR_SONG); frames(32u); assert(!chain_cued);  /* another page: the loop */
    daw(0xFAu); assert(song.playing && !chain.running);
    daw(0xFCu); frame();
    go_page(GR_SECTIONS); frames(300u); assert(chain_cued);
    press(B_PLAY); units_block(0); frame();             /* PLAY there: the song, as before */
    assert(song.playing && chain.running && !chain_cued);
    press(B_PLAY); units_block(0); frame();
    song.g[G_CLOCK] = 0; frames(32u); assert(!chain_cued && !song_sounds.mask);
    puts("SECTIONS: a DAW's Start (or Continue from the top) plays the song; other pages and CLK INT the loop passed");
}
int main(void)
{
    seq_pages();
    keys_and_copy();
    switch_at_loop_end();
    empty_lead();
    song_rec();
    build();
    whole_section();
    song_list();
    daw_start();
    puts("SECTIONS tests passed");
    return 0;
}
