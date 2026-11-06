/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME's TRACKS page: its knobs, STEPS at the loop end, and that it adds nothing to the stored project. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
_Static_assert(P_COUNT == 124u, "TRACKS adds no stored parameter: stock projects must load");
static track_t *setup(void)
{
    ui_power_on(); track_t *t=TSEL; set_engine_of(t,HOST_SYNTH);
    ui.home=1; ui.home_pg=HP_TRACKS; t->p[P_SLEN]=16; t->p[P_SDIV]=2; t->p[P_SSWING]=0; t->slen_next=0; song.g[G_SWING]=0;
    events_block(CTL); return t;
}
static void loop_end(const track_t *t)
{
    for(uint32_t i=0;i<200 && t->seq_idx;i++) seq_advance(beat_samples()/4u);
}
static void landing(void)
{
    assert(steps_land(7,40)==8 && steps_land(8,40)==16 && steps_land(16,40)==32 && steps_land(32,64)==64);
    assert(steps_land(40,3)==32 && steps_land(32,3)==16 && steps_land(9,3)==8 && steps_land(3,5)==5 && steps_land(16,17)==17);
    puts("TRACKS: fast turns stop at 8, 16, 32 and 64 passed");
}
static void knobs(void)
{
    track_t *t=setup(), *other=&trk[1];
    int16_t ol=other->p[P_LEVEL], op=other->p[P_PAN], os=other->p[P_SSWING], lv=t->p[P_LEVEL], pan=t->p[P_PAN];
    tracks_knob(0,1); assert(t->p[P_SSWING]==1);
    tracks_knob(1,-1); assert(t->p[P_LEVEL]==lv-1);
    tracks_knob(3,1); assert(t->p[P_PAN]==pan+1);
    tracks_knob(2,1); assert(t->p[P_SLEN]==17 && !t->slen_next);
    assert(other->p[P_LEVEL]==ol && other->p[P_PAN]==op && other->p[P_SSWING]==os && !song.g[G_SWING]);
    /* the track's swing and the song's add up, as before, and only this track's clock moves */
    t->p[P_SSWING]=30; song.g[G_SWING]=20; assert(track_swing(t)==50 && track_swing(other)==20+other->p[P_SSWING]);
    t->p[P_SSWING]=100; assert(track_swing(t)==SWING_MAX);
    puts("TRACKS: swing, level, steps and pan of the selected track alone passed");
}
static void at_loop_end(void)
{
    track_t *t=setup();
    t->step[12]=(step_t){.time=ST_NOTE,.n=1,.note={60}};
    t->p[P_SLEN]=16; song.playing=1; seq_advance(beat_samples()); assert(t->seq_idx);
    tracks_knob(2,-1); assert(t->p[P_SLEN]==16 && t->slen_next==15);
    loop_end(t); assert(t->p[P_SLEN]==15 && !t->slen_next);
    /* back to the length playing: nothing waits */
    seq_advance(beat_samples()); tracks_knob(2,1); assert(t->slen_next==16); tracks_knob(2,-1); assert(!t->slen_next);
    /* STOP does it at once */
    tracks_knob(2,-1); assert(t->slen_next==14); seq_stop(); assert(t->p[P_SLEN]==14 && !t->slen_next);
    /* shorter hides the steps, longer brings them back */
    t->p[P_SLEN]=8; tracks_knob(2,1); assert(t->p[P_SLEN]==9); t->p[P_SLEN]=16; assert(t->step[12].n==1 && t->step[12].note[0]==60);
    puts("TRACKS: STEPS at the loop end, cancelled, on STOP, and hidden steps kept passed");
}
static void one_length(void)
{
    track_t *t=setup(); t->p[P_SLEN]=16; song.playing=1; seq_advance(beat_samples());
    tracks_knob(2,1); assert(t->slen_next==17);
    assert(!pattern_half(t,t->pattern,0)); pattern_half(t,t->pattern,1);
    assert(t->p[P_SLEN]==8 && !t->slen_next && tr_steps(t)==8);        /* halve: the shortcut's length wins and both agree */
    seq_stop();
    t=setup(); assert(tr_steps(t)==16); t->p[P_SLEN]=32; assert(tr_steps(t)==32);
    puts("TRACKS: one length with the halve and double shortcut passed");
}
static void pages(void)
{
    track_t *t=setup(); int16_t e=t->p[P_E0];
    ui.home_pg=HP_STAGE; assert(ui.home); page_knob(0,1);
    assert(t->p[P_SSWING]==0);                                         /* Stage's knobs stay the engine's */
    (void)e;
    ui.home_pg=HP_TRACKS; ui.force=1; ui_draw(); ui_draw();            /* the page draws, on a track of each kind */
    song.sel=3; ui.force=1; ui_draw(); song.sel=0;
    puts("TRACKS: Stage keeps its knobs, the page draws passed");
}
static void stored(void)
{
    track_t *t=setup(); t->p[P_SSWING]=33; t->p[P_PAN]=-20; t->p[P_LEVEL]=90; t->p[P_SLEN]=24;
    assert(!project_save(0)); t->p[P_SSWING]=0; t->p[P_PAN]=0; t->p[P_LEVEL]=10; t->p[P_SLEN]=8; project_load(0);
    t=TSEL; assert(t->p[P_SSWING]==33 && t->p[P_PAN]==-20 && t->p[P_LEVEL]==90 && t->p[P_SLEN]==24 && !t->slen_next);
    puts("TRACKS: per-track values saved with the project passed");
}
int main(void)
{
    landing(); knobs(); at_loop_end(); one_length(); pages(); stored();
    puts("TRACKS tests passed");
    return 0;
}
