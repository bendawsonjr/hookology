/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Hookology) the 909's sound, bit for bit: every voice, every panel control at five settings and every distortion
 * type, played the way eng_drum.c plays it (drum909_render_voice on the shared noise, mono) and through the kit's
 * own stereo render with pans, for several seconds with retriggers. The output's bits are hashed and compared with
 * the recorded hash, so a change to the 909's code cannot change a single sample unnoticed. Recorded with beta66's
 * 80 dB end (each drum stops once faded 80 dB); with HOOKOLOGY_D9_QUIET_END=0 it is 1.0's sound, f6ec1ca263014e25 /
 * 26f22b68434c2f35. Hashes with -ffp-contract=off, as the suite and the target build.
 *   drum909_exact_test [--print]    --print shows the hashes (to record them after an intended change) */
#include <stdio.h>
#include <string.h>
#define __attribute__(x)                        /* the tables' target-only section attributes */
#include "melodee_tables.h"
#undef __attribute__
#include "x0x/drum909.c"

static uint64_t H = 1469598103934665603ull;
static void hash(const float *x, int n)
{
    for (int i = 0; i < n; i++) {
        uint32_t u; memcpy(&u, &x[i], 4);
        for (int b = 0; b < 4; b++) { H ^= (u >> (8 * b)) & 0xFFu; H *= 1099511628211ull; }
    }
}

static drum909_t D;

/* one voice, eng_drum.c's way: noise per block, the voice rendered alone into a mono buffer */
static void play_voice(int voice, float vel, int blocks, int retrig)
{
    drum909_trigger(&D, voice, vel);
    for (int b = 0; b < blocks; b++) {
        if (retrig && b == retrig) drum909_trigger(&D, voice, vel * 0.5f);
        float *nz = d9_noise_block(&D, CTL), f[CTL] = {0};
        drum909_render_voice(&D, voice, nz, f, CTL);
        hash(f, CTL);
    }
}

int main(int argc, char **argv)
{
    static const int VALS[] = {0, 32, 64, 100, 127};
    uint64_t voice_h, kit_h;
    drum909_init(&D);
    for (int v = 0; v < D9_NUM; v++) {
        int np = drum909_nparams(v);
        play_voice(v, 1.0f, 80, 0);
        for (int i = 0; i < np; i++) {
            const x0x_param_t *p = drum909_param(v, i);
            int def = drum909_get(&D, v, i);
            if (p->max == 127) {
                for (unsigned k = 0; k < sizeof VALS / sizeof VALS[0]; k++) {
                    drum909_set(&D, v, i, VALS[k]);
                    play_voice(v, k & 1 ? 0.4f : 1.0f, 60, k == 2 ? 20 : 0);
                }
            } else {
                for (int k = 0; k <= p->max; k++) {
                    drum909_set(&D, v, i, k);
                    play_voice(v, 0.8f, 60, 0);
                }
            }
            drum909_set(&D, v, i, def);
        }
        play_voice(v, 0.7f, 2000, 400);          /* the whole decay, a retrigger in it */
    }
    voice_h = H;

    /* the kit's own render, mono then stereo with pans gliding, every voice at once */
    H = 1469598103934665603ull;
    drum909_init(&D);
    for (int pass = 0; pass < 2; pass++)
        for (int b = 0; b < 3000; b++) {
            if (b % 97 == 0) for (int v = 0; v < D9_NUM; v++) if ((b / 97 + v) % 3 == 0) drum909_trigger(&D, v, 0.3f + 0.06f * v);
            if (pass && b % 500 == 0)
                for (int v = 0; v < D9_NUM; v++) x0x_pan_gains((b / 500 * 37 + v * 11) % 128, &D.pan_l[v], &D.pan_r[v]);
            float l[CTL] = {0}, r[CTL] = {0};
            if (pass) { drum909_render_st(&D, l, r, CTL); hash(l, CTL); hash(r, CTL); }
            else { drum909_render(&D, l, CTL); hash(l, CTL); }
        }
    kit_h = H;

    if (argc > 1 && !strcmp(argv[1], "--print")) {
        printf("voices %016llx\nkit    %016llx\n", (unsigned long long)voice_h, (unsigned long long)kit_h);
        return 0;
    }
    const uint64_t WANT_VOICE = 0x8e1c9600c829149bull, WANT_KIT = 0x9dad9665d4e50c58ull;
    int ok = voice_h == WANT_VOICE && kit_h == WANT_KIT;
    printf("909 bit-exact: voices %016llx %s, kit %016llx %s\n", (unsigned long long)voice_h,
           voice_h == WANT_VOICE ? "ok" : "CHANGED", (unsigned long long)kit_h, kit_h == WANT_KIT ? "ok" : "CHANGED");
    return !ok;
}
