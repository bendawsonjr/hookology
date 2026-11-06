/* SPDX-License-Identifier: GPL-3.0-only */
/* A project saved by unmodified Melodee 1.6.1 (tests/fixtures/stock-1.6.1-project.bin, written by that source, commit
 * 165d177) loads here with its tempo, swing, lengths, chords, steps and recorded notes; HERA's set reads as set 1. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
int main(void)
{
    static project_t q;
    static project_store_t w;
    FILE *f = fopen("tests/fixtures/stock-1.6.1-project.bin", "rb");
    assert(f && fread(&w, 1, sizeof w, f) == sizeof w);
    fclose(f);
    ui_power_on();
    assert(proj_unpack(&q, w.raw) && !project_restore_runtime(&q));
    track_t *t = &trk[0];
    assert(song.g[G_BPM] == 97 && song.g[G_SWING] == 11 && t->p[P_SSWING] == 33 && t->p[P_SLEN] == 24);
    assert(t->p[P_PAN] == -20 && t->p[P_LEVEL] == 90 && t->p[P_CHRD] == CH_AUTO && t->p[P_VOIC] == VC_OPEN);
    assert(t->step[3].n == 1 && t->step[3].note[0] == 60 && t->step[0].time == ST_NOTE && t->p[P_CHSET] == 0);
    assert(trk[3].p[P_E0] == DK_909 && trk[3].p[P_SLEN] == 32);
    uint32_t r = 0;
    for (uint32_t i = 0; i < RECORD_MAX; i++) r += recording[i].vel != 0;
    assert(r == 3u);
    puts("a stock 1.6.1 project loads passed");
    return 0;
}
