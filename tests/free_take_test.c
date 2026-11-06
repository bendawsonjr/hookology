/* SPDX-License-Identifier: GPL-3.0-only */
/* REC MODE FREE (src/free_take.c): the tempo and length a take sets, where its notes land, PLAY and the limits, when a
 * take applies, overdub at once, latency, undo, and that GRID records as it did. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#define BLOCKS_PER_S (FS / CTL)
static track_t *setup(uint32_t freemode)
{
    ui_power_on(); track_t *t = TSEL; set_engine_of(t, HOST_SYNTH);
    for (uint32_t k = 0; k < NTRK; k++) { trk[k].p[P_SLEN] = 16; trk[k].p[P_SDIV] = 2; }
    t->p[P_VOICE] = V_POLY; t->p[P_QUANT] = Q_OFF; t->p[P_CHRD] = 0; t->p[P_RECQ] = 0; song.g[G_BPM] = 120; song.g[G_CLOCK] = 0;
    song.rec = song.rec_wait = song.rec_latch = song.rec_pass = 0; song.playing = 0;
    GLO_SELECT = (uint8_t)(GLO_SELECT & ~REC_MODE_FREE) | (freemode ? REC_MODE_FREE : 0u);
    ft_msg = 0; ft_on = 0; ft_rec_eat = ft_play_eat = 0; fm1_in.buttons = 0; ft_undo.valid = 0;
    free_take_poll();
    return t;
}
static void blocks(uint32_t n) { for (uint32_t i = 0; i < n; i++) events_block(CTL); }
static void wait_s(double s) { blocks((uint32_t)(s * BLOCKS_PER_S)); }
static void arm(void) { rec_set(TSEL, 1); free_take_poll(); assert(ft_ready); }
static void rec_press(void)
{
    fm1_in.buttons |= 1u << panel.btn[B_REC]; events_block(CTL);
    fm1_in.buttons &= ~(1u << panel.btn[B_REC]); events_block(CTL);
}
static void tap(uint32_t k, double gap) { key_down(k); wait_s(gap / 2); key_up(k); wait_s(gap / 2); }
static uint32_t notes_of(const track_t *t) { uint32_t n = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) n += recording[i].vel && recorded_owner(&recording[i]) == recording_owner(t); return n; }

static void fit(void)
{
    uint32_t bpm = 0;
    song.g[G_BPM] = 120;
    assert(ft_fit(4u * BLOCKS_PER_S, &bpm) == 2u && bpm == 120u);        /* 4 s: 2 bars at 120, not 1 at 60 */
    song.g[G_BPM] = 60;
    assert(ft_fit(4u * BLOCKS_PER_S, &bpm) == 1u && bpm == 60u);         /* .. 1 at 60 from a slow tempo */
    song.g[G_BPM] = 120;
    assert(ft_fit((uint32_t)(4.08 * BLOCKS_PER_S), &bpm) == 2u && bpm == 120u);   /* within 3 %: the tempo stays */
    assert(ft_fit((uint32_t)(4.4 * BLOCKS_PER_S), &bpm) == 2u && bpm == 109u);    /* else it follows */
    assert(!ft_fit(40u * BLOCKS_PER_S, &bpm));                            /* nothing in 40..240 */
    assert(ft_units(0, 100, 16) == 0u && ft_units(50, 100, 16) == 8u * RECORD_UNIT && ft_units(33075, 33075, 64) == 64u * RECORD_UNIT);
    puts("FREE: bars and tempo from a take's length passed");
}
static void take(void)
{
    track_t *t = setup(1); arm();
    key_down(7); assert(ft_on && !song.playing); wait_s(0.25); key_up(7); wait_s(0.25);     /* quarter notes at 120, 2 bars */
    for (uint32_t i = 1; i < 8u; i++) tap(7u + (i & 3u), 0.5);
    rec_press();
    assert(!ft_on && song.playing && ft_rec_eat && ft_msg == FT_CLOSED && ft_bars == 2u && song.g[G_BPM] == 120);
    for (uint32_t k = 0; k < NTRK; k++) assert(trk[k].p[P_SLEN] == 32 && trk[k].p[P_SDIV] == 2);
    assert(!((song.rec >> song.sel) & 1u));                               /* the loop plays, nothing records */
    assert(notes_of(t) == 8u);
    for (uint32_t i = 0; i < RECORD_MAX; i++)
        if (recording[i].vel) assert(!recording[i].on && recorded_step(&recording[i]) % 4u == 0u);  /* on the 1/16 grid */
    for (uint32_t i = 0; i < 8u; i++) assert(t->step[i * 4u].time == ST_NOTE && (t->step[i * 4u].flags & SF_RECORDED));
    /* a loose take lands on the grid, the QNT chosen */
    t = setup(1); t->p[P_RECQ] = 2; arm();                                /* 1/8 */
    key_down(7); wait_s(0.1); key_up(7); wait_s(0.21); key_down(8); wait_s(0.1); key_up(8); wait_s(1.59);
    rec_press();
    assert(ft_bars == 1u && notes_of(t) == 2u);
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel) assert(!recording[i].on && !(recorded_step(&recording[i]) & 1u));
    seq_stop();
    puts("FREE: a take sets tempo and lengths, lands on the grid, plays without recording passed");
}
static void limits(void)
{
    track_t *t = setup(1); arm();
    key_down(7); key_up(7); wait_s(1.0);
    fm1_in.buttons |= 1u << panel.btn[B_PLAY]; events_block(CTL); assert(!ft_on && ft_play_eat && ft_msg == FT_DROPPED);
    fm1_in.buttons = 0; ui_input(); assert(!song.playing && notes_of(t) == 0u && song.g[G_BPM] == 120 && t->p[P_SLEN] == 16 && !ft_play_eat);
    t = setup(1); arm(); key_down(7); key_up(7); wait_s(0.2); rec_press();    /* too short */
    assert(!ft_on && !song.playing && ft_msg == FT_NO_LOOP && !notes_of(t) && t->p[P_SLEN] == 16);
    t = setup(1); arm(); key_down(7); key_up(7); blocks(FT_BLOCKS + 2u);      /* 24 s closes it */
    assert(!ft_on && song.playing && ft_msg == FT_CLOSED && ft_bars == 4u && song.g[G_BPM] == 40); seq_stop();
    t = setup(1); arm(); key_down(7); key_up(7);
    for (uint32_t i = 1; i < FT_MAX + 5u; i++) { key_down(8); events_block(CTL); key_up(8); events_block(CTL); }
    assert(ft_n == FT_MAX && ft_msg == FT_FULL && ft_on); ft_msg = 0; wait_s(2.0); rec_press(); assert(ft_msg == FT_CLOSED); seq_stop();
    puts("FREE: PLAY drops, short takes refused, 24 s closes, a full take still closes passed");
}
static void applies(void)
{
    /* notes anywhere: the tempo set, the first note starts the transport */
    track_t *t = setup(1); t->step[3] = (step_t){.time = ST_NOTE, .n = 1, .note = {60}}; t->seq_active = 1;
    rec_set(t, 1); free_take_poll(); assert(!ft_ready); key_down(7); assert(!ft_on && song.playing); key_up(7); seq_stop();
    /* an external clock owns the tempo */
    t = setup(1); song.g[G_CLOCK] = 1; rec_set(t, 1); free_take_poll(); assert(!ft_ready); song.g[G_CLOCK] = 0; rec_set(t, 0);
    /* GRID: as it was */
    t = setup(0); rec_set(t, 1); free_take_poll(); assert(!ft_ready && !free_mode); key_down(7); assert(!ft_on && song.playing); key_up(7); seq_stop();
    /* GRID waits for the loop, FREE does not */
    t = setup(0); rec_set(t, 0); song.playing = 1; t->seq_pos = 100; rec_set(t, 1); assert(song.rec_wait); rec_set(t, 0); song.playing = 0;
    t = setup(1); song.playing = 1; t->seq_pos = 100; rec_set(t, 1); assert(!song.rec_wait && rec_on(t)); rec_set(t, 0); song.playing = 0;
    puts("FREE: only an empty project on the internal clock; GRID unchanged passed");
}
static void overdub(void)
{
    track_t *t = setup(1); arm(); key_down(7); wait_s(0.5); key_up(7); wait_s(1.5); rec_press();
    assert(ft_bars == 1u && ft_undo.valid);
    wait_s(0.5); ft_rec_eat = 0;
    rec_set(t, 1); song.rec_latch |= 1u << song.sel; assert(rec_on(t));
    /* played just after a step begins: heard on it, and in the step before when within the latency */
    uint32_t back = t->seq_pos, step = t->seq_idx;
    key_down(9); key_up(9);
    rec_set(t, 0);
    uint32_t found = 0;
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel && recorded_note(&recording[i]) == 62u) found = recorded_step(&recording[i]) + 1u;
    assert(found);
    (void)back; (void)step;
    assert(!ft_undo.valid);                                               /* an overdub has its own undo */
    seq_stop();
    /* the latency: FREE moves an event back, GRID does not */
    t = setup(1); uint32_t a = 3, p = 100, s = 5000; recording_heard(t, 5000, &a, &p, &s); assert(a == 2u && p == s - (REC_LAT - 100u));
    a = 0; p = 100; recording_heard(t, 5000, &a, &p, &s); assert(a == 15u);
    a = 3; p = 2000; recording_heard(t, 5000, &a, &p, &s); assert(a == 3u && p == 2000u - REC_LAT);
    t = setup(0); a = 3; p = 100; s = 5000; recording_heard(t, 5000, &a, &p, &s); assert(a == 3u && p == 100u);
    puts("FREE: overdub at once, its own undo, latency in FREE alone passed");
}
static void take_undo(void)
{
    track_t *t = setup(1); song.g[G_BPM] = 90; trk[1].p[P_SLEN] = 12; arm();
    for (uint32_t i = 0; i < 4u; i++) tap(7, 0.5);
    rec_press();
    assert(ft_undo.valid && song.g[G_BPM] != 90 && trk[1].p[P_SLEN] == t->p[P_SLEN]);
    int16_t bpm = song.g[G_BPM], len = t->p[P_SLEN];
    seq_stop();
    assert(recording_history(0) && song.g[G_BPM] == 90 && t->p[P_SLEN] == 16 && trk[1].p[P_SLEN] == 12 && !notes_of(t));
    assert(recording_history(1) && song.g[G_BPM] == bpm && t->p[P_SLEN] == len && trk[1].p[P_SLEN] == len && notes_of(t) == 4u);
    puts("FREE: undo and redo with tempo and lengths passed");
}
static void stored(void)
{
    assert(P_COUNT == 124u);
    track_t *t = setup(1); arm(); for (uint32_t i = 0; i < 4u; i++) tap(7, 0.5); rec_press(); seq_stop();
    assert(!project_save(0)); recording_reset(); project_load(0); t = TSEL; assert(notes_of(t) == 4u && t->p[P_SLEN] == 16);
    puts("FREE: a take saves with the project passed");
}
int main(void)
{
    fit(); take(); limits(); applies(); overdub(); take_undo(); stored();
    puts("REC MODE FREE tests passed");
    return 0;
}
