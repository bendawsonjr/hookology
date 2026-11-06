/* SPDX-License-Identifier: GPL-3.0-only */
/* KIT SAMPLE (src/drum_sample.c): every drum sounds and ends, the same twice; levels beside the 808's; ghost and hard
 * hits; the knobs; a closed hat chokes the open one; every GM note plays; OCT- / OCT+ on the keys; its presets. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#define P_ACOUSTIC 6u
static const uint8_t NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};   /* one for each sample */
static const char *const NAME[8] = {"KICK", "SNARE", "CLAP", "HATC", "HATO", "TOM", "RIM", "BELL"};
static track_t *kit(uint32_t preset)
{
    ui_power_on(); track_t *t = TSEL;
    for (uint32_t p = 0; p < NROWS; p++) memset(trk[p].v, 0, NVOICE * sizeof *trk[p].v);
    eng_state_reset();
    host_preset(t, ENGI_DRUM, preset);
    return t;
}
static int32_t run(track_t *t, int32_t *buf, uint32_t nb)
{
    int32_t o[CTL], pk = 0;
    for (uint32_t i = 0; i < nb; i++) {
        track_render(t, o, CTL);
        for (uint32_t k = 0; k < CTL; k++) {
            if (buf) buf[i * CTL + k] = o[k];
            pk = abs(o[k]) > pk ? abs(o[k]) : pk;
        }
    }
    return pk;
}
static uint32_t voices(const track_t *t) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active; return n; }
static double energy(const int32_t *x, uint32_t n) { double e = 0; for (uint32_t i = 0; i < n; i++) e += (double)x[i] * x[i]; return e; }
static double high(const int32_t *x, uint32_t n) { double e = 0; for (uint32_t i = 1; i < n; i++) e += (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]); return e; }
static uint32_t rings(track_t *t) { uint32_t b = 0; while (b < 4000u && voices(t)) { run(t, 0, 1); b++; } return b; }
static uint32_t zero_x(const int32_t *x, uint32_t n) { uint32_t c = 0; for (uint32_t i = 1; i < n; i++) c += (x[i - 1] < 0) != (x[i] < 0); return c; }

static void sounds(void)
{
    static int32_t a[32768], b[32768];
    uint32_t nb = 32768u / CTL;
    int32_t top = 0;
    for (uint32_t i = 0; i < 8u; i++) {
        track_t *t = kit(P_ACOUSTIC);
        trk_note_on(t, NOTE[i], 100); int32_t pk = run(t, a, nb);
        t = kit(P_ACOUSTIC);
        trk_note_on(t, NOTE[i], 100); run(t, b, nb);
        track_t *r = kit(0); trk_note_on(r, NOTE[i], 100); int32_t ref = run(r, 0, nb);
        printf("SAMPLE %-6s peak %6d (808: %6d)\n", NAME[i], pk, ref);
        assert(pk > 1500 && !memcmp(a, b, sizeof a));
        assert(!a[32768u / CTL * CTL - 1u]);                 /* it has ended */
        top = pk > top ? pk : top;
    }
    assert(top < 60000);
    puts("SAMPLE: all eight sound, the same twice, and end passed");
}
static void levels(void)
{
    int32_t pk[3]; static const uint8_t V[3] = {42, 100, 127};
    for (uint32_t i = 0; i < 3u; i++) { track_t *t = kit(P_ACOUSTIC); trk_note_on(t, 38, V[i]); pk[i] = run(t, 0, 200); }
    printf("SAMPLE ghost %d normal %d hard %d\n", pk[0], pk[1], pk[2]);
    assert(pk[0] < pk[1] * 3 / 4 && pk[1] < pk[2]);
    puts("SAMPLE: ghost, normal and hard hits passed");
}
static void knobs(void)
{
    static int32_t a[16384];
    uint32_t nb = 16384u / CTL;
    track_t *t = kit(P_ACOUSTIC); t->p[P_E1] = 30; trk_note_on(t, 45, 110); run(t, a, nb); uint32_t lo = zero_x(a, 16384);
    t = kit(P_ACOUSTIC); t->p[P_E1] = 100; trk_note_on(t, 45, 110); run(t, a, nb); assert(zero_x(a, 16384) > lo);
    t = kit(P_ACOUSTIC); t->p[P_E2] = 8; trk_note_on(t, 38, 110); run(t, a, nb); double dark = high(a, 16384) / energy(a, 16384);
    t = kit(P_ACOUSTIC); trk_note_on(t, 38, 110); run(t, a, nb); double mid = high(a, 16384) / energy(a, 16384);
    t = kit(P_ACOUSTIC); t->p[P_E2] = 127; trk_note_on(t, 38, 110); run(t, a, nb); double bright = high(a, 16384) / energy(a, 16384);
    assert(dark < mid && mid < bright);
    t = kit(P_ACOUSTIC); trk_note_on(t, 46, 110); uint32_t whole = rings(t);
    t = kit(P_ACOUSTIC); t->p[P_E3] = 4; trk_note_on(t, 46, 110); assert(rings(t) < whole / 2u);
    puts("SAMPLE: TUNE, TONE and DECY passed");
}
static void lanes(void)
{
    track_t *t = kit(P_ACOUSTIC); assert(drum_islane(t) && drum_cap(t) == 1u);
    trk_note_on(t, 46, 110); run(t, 0, 8); assert(dlk_part(0)->ch[DV_HATO].on && !dlk_part(0)->ch[DV_HATO].fade);
    trk_note_on(t, 42, 110); run(t, 0, 1); assert(dlk_part(0)->ch[DV_HATO].fade || !dlk_part(0)->ch[DV_HATO].on);
    assert(voices(t) == 1u);
    for (uint32_t n = 35; n <= 81; n++) { trk_note_on(t, n, 100); run(t, 0, 2); }
    assert(voices(t) == 1u);
    puts("SAMPLE: the choke, one voice and every GM note passed");
}
static void keys(void)
{
    track_t *t = kit(P_ACOUSTIC);
    int32_t base = drum_keys(t, 7);
    ui.home = 1; ui.home_pg = HP_TRACKS; assert(oct_hits());
    song.octave = 2; assert(drum_keys(t, 7) == base); song.octave = 0;
    int8_t oct = *octave_control();
    fm1_in.buttons |= 1u << panel.btn[B_OCTDN]; ui_input(); assert(key_hit_vel == HIT_GHOST);
    key_down(7); assert(t->v[0].active && t->v[0].vel == HIT_GHOST); key_up(7);
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTDN]); ui_input(); assert(!key_hit_vel && *octave_control() == oct);
    fm1_in.buttons |= 1u << panel.btn[B_OCTUP]; ui_input(); assert(key_hit_vel == HIT_HARD);
    fm1_in.buttons &= ~(1u << panel.btn[B_OCTUP]); ui_input(); assert(!key_hit_vel && *octave_control() == oct);
    /* a recording keeps the level */
    t = kit(P_ACOUSTIC); ui.home = 1; ui.home_pg = HP_TRACKS; song.rec = 1; song.playing = 1; key_hit_vel = HIT_GHOST;
    key_down(7); key_up(7); key_hit_vel = 0;
    { uint32_t found = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) found += recording[i].vel == HIT_GHOST; assert(found == 1u); }
    song.rec = song.playing = 0;
    /* the 808 keeps its octave buttons */
    t = kit(0); ui.home = 1; assert(!oct_hits()); song.octave = 2; assert(drum_keys(t, 7) != base); song.octave = 0;
    puts("SAMPLE: OCT- and OCT+ as hit levels on this kit alone passed");
}
static void presets(void)
{
    assert(ENG_DRUM.edit[0].max == DK_SAMPLE && !strcmp(N_DRUM_KIT[DK_SAMPLE], "SAMPLE") && ENG_DRUM.npresets == 11u);
    static const char *const N[5] = {"ACOUSTIC KIT", "DEEP KIT", "TIGHT KIT", "BRIGHT KIT", "DUST KIT"};
    for (uint32_t i = 0; i < 5u; i++) {
        assert(!strcmp(ENG_DRUM.presets[P_ACOUSTIC + i].name, N[i]) && ENG_DRUM.presets[P_ACOUSTIC + i].e[0] == DK_SAMPLE);
        track_t *t = kit(P_ACOUSTIC + i); trk_note_on(t, 36, 110); assert(run(t, 0, 200) > 1500);
    }
    puts("SAMPLE: its five presets passed");
}
int main(void)
{
    sounds(); levels(); knobs(); lanes(); keys(); presets();
    puts("KIT SAMPLE tests passed");
    return 0;
}
