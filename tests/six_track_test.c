/* SPDX-License-Identifier: GPL-3.0-only */
/* Six sound tracks and two MIDI tracks: tracks 5 and 6 play their own engines, and their sounds, patterns, recordings and
 * motion survive a save and reload; 7 and 8 stay MIDI; a screen shows four; an older file keeps a MIDI Track on 5. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
_Static_assert(NPART == 6 && NROWS == 4 && NMIDI == 2 && NTRK == 8 && PROJ_TRK == 4, "six sound tracks, two MIDI");
static uint32_t held(const track_t *t) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active; return n; }
static track_t *six(void)
{
    ui_power_on(); project_new_blank(); assert(trk_n() == NROWS);
    track_add(); track_add(); assert(trk_n() == 6u);
    events_block(CTL);
    return &trk[4];
}
static void defaults(void)
{
    ui_power_on(); project_new_blank();
    assert(trk_n() == 4u && song.ntrk == 4u);
    track_add(); assert(trk[4].eng_req == ENGI_FM6 && !midi_track_only(&trk[4]) && !trk_midi_slot(4));
    track_add(); assert(trk[5].eng_req == ENGI_PROPHET && !midi_track_only(&trk[5]));
    track_add(); assert(trk[6].eng_req == ENGI_MIDI && midi_track_only(&trk[6]) && trk_midi_slot(6));
    track_add(); assert(trk[7].eng_req == ENGI_MIDI && trk_midi_slot(7) && trk_n() == NTRK);
    assert(trk_npat(4) == NPAT && trk_npat(5) == NPAT && trk_npat(6) == NPAT_MIDI);
    for (uint32_t k = 0; k < 4u; k++) { song.sel = (uint8_t)(k + 4u); assert(trk_group() == 4u); song.sel = (uint8_t)k; assert(!trk_group()); }
    puts("SIX: a project starts with four, tracks 5 and 6 add as sounds, 7 and 8 as MIDI passed");
}
static void sounding(void)
{
    track_t *t = six(), *u = &trk[5];
    trk_note_on(t, 60, 100); trk_note_on(u, 64, 100); events_block(CTL);
    assert(held(t) && held(u));
    int32_t o[CTL]; track_render(t, o, CTL); track_render(u, o, CTL);
    int32_t pk = 0; for (uint32_t i = 0; i < 40u; i++) { track_render(t, o, CTL); for (uint32_t k = 0; k < CTL; k++) pk = abs(o[k]) > pk ? abs(o[k]) : pk; }
    assert(pk > 100);
    trk_note_off(t, 60); trk_note_off(u, 64);
    /* solo of the fifth track: the others go silent, it stays */
    assert(solo_gain[4] == 32768 && solo_gain[5] == 32768);
    puts("SIX: tracks 5 and 6 play their own engines, solo starts open passed");
}
static void saved(void)
{
    track_t *t = six(), *u = &trk[5];
    set_engine_of(t, ENGI_CZ); events_block(CTL); apply_preset_to(t, 12); t->p[P_LEVEL] = 77; t->p[P_PAN] = -9;
    t->step[3] = (step_t){{50}, 1, ST_NOTE, 0, 96, 0, 0}; t->p[P_SLEN] = 24; t->seq_active = 1;
    apply_preset_to(u, 7); u->p[P_E2] = 120; u->p[P_LEVEL] = 66;   /* (E2: past Prophet's range, a stray value) */
    u->step[9] = (step_t){{70}, 1, ST_NOTE, 0, 88, 0, 0}; u->seq_active = 1;
    pattern_t p; pattern_read(5, 3, &p); p.step[2] = (step_t){{71}, 1, ST_NOTE, 0, 80, 0, 0}; pattern_write(5, 3, &p);
    trk[6].p[P_E0] = 11;
    recorded_note_t r = {0}; recorded_set_note(&r, 61); r.vel = 77; r.duration = 100; recorded_set_owner(&r, 4u * NPAT);
    recorded_set_step(&r, 1, 0, 0); recording[RECORD_MAX - 1u] = r; recording_reindex();
    static uint8_t cz[sizeof(cz_patch_t)], p5[sizeof(p5_patch_t)];
    memcpy(cz, cz_patch_of(4), sizeof cz); memcpy(p5, p5_patch_of(u), sizeof p5);
    uint8_t ps = u->preset;
    assert(!project_save_as(0, "SIX"));
    project_new_blank(); assert(trk_n() == NROWS);
    project_load(0); events_block(CTL);
    t = &trk[4]; u = &trk[5];
    assert(trk_n() == 6u);
    assert(t->eng_req == ENGI_CZ && t->p[P_LEVEL] == 77 && t->p[P_PAN] == -9 && t->p[P_SLEN] == 24 && !memcmp(cz, cz_patch_of(4), sizeof cz));
    assert(t->step[3].n == 1 && t->step[3].note[0] == 50);
    assert(u->eng_req == ENGI_PROPHET && u->preset == ps && u->p[P_LEVEL] == 66 && !memcmp(p5, p5_patch_of(u), sizeof p5));
    assert(u->step[9].note[0] == 70);
    pattern_read(5, 3, &p); assert(p.step[2].note[0] == 71);
    { uint32_t n = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) n += recording[i].vel == 77u && recorded_owner(&recording[i]) == 4u * NPAT; assert(n == 1u); }
    trk_note_on(t, 60, 100); events_block(CTL); assert(held(t)); trk_note_off(t, 60);
    puts("SIX: sounds, levels, patterns and recordings of tracks 5 and 6 survive a reload passed");
}
static void every_sound(void)
{
    static const uint8_t E[] = {ENGI_PROPHET, ENGI_FM6, ENGI_CZ, 3, ENGI_DRUM};
    uint32_t total = 0;
    track_t *t = &six()[1];
    for (uint32_t e = 0; e < NELEM(E); e++) {
        set_engine_of(t, E[e]); events_block(CTL);
        for (uint32_t i = 0; i < ENGINES[E[e]]->npresets; i++, total++) {
            song_sound_t s; apply_preset_to(t, i); song_sound_capture(&s, t, "TRACK"); assert(song_sound_valid(&s));
        }
    }
    assert(total > 300u);
    puts("SIX: every factory sound of every engine can be stored on tracks 5 and 6 passed");
}
static void older(void)
{
    /* a project that keeps a MIDI Track on track 5, as older firmware's: no sound stored, still a MIDI Track */
    track_t *t = six(); set_engine_of(t, ENGI_MIDI); events_block(CTL); t->p[P_E0] = 9;
    t->step[1] = (step_t){{40}, 1, ST_NOTE, 0, 90, 0, 0}; t->seq_active = 1;
    assert(!project_save_as(1, "OLD"));
    uint8_t *raw = proj_bank_slot[1]; assert(bank_valid(raw, BANK_STORE_SIZE));
    { uint32_t any = 0; for (uint32_t i = 0; i < BANK_PART_BYTES / 2u; i++) any |= raw[BANK_ACTIVE_OFF + BANK_PART_OFF + i]; assert(!any); }
    project_new_blank(); project_load(1); events_block(CTL);
    assert(trk[4].eng_req == ENGI_MIDI && trk[4].p[P_E0] == 9 && trk[4].step[1].note[0] == 40);
    puts("SIX: a MIDI Track on track 5 stays one passed");
}
static void screens(void)
{
    six(); go_home(); ui.home = 1;
    for (uint32_t pg = 0; pg < 3u; pg++) {
        ui.home_pg = (uint8_t)pg;
        for (uint32_t k = 0; k < 6u; k++) { track_select(k); ui.force = 1; ui_draw(); ui_draw(); assert(song.sel == k); }
    }
    puts("SIX: every HOME page draws on both pages of tracks passed");
}
int main(void)
{
    defaults(); sounding(); every_sound(); saved(); older(); screens();
    puts("Six-track tests passed");
    return 0;
}
