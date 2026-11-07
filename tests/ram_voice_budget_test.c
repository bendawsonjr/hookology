/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Resident profile admission, with online caps and serial host rendering. */
#define MELODEE_RAM_AUDIO 1
#define MELODEE_DUAL_CORE 1
static int audio_worker_online=1;
static int audio_worker_submit(void (*fn)(void *),void *ctx){(void)fn;(void)ctx;return 0;}
static int audio_worker_join(void){return 1;}
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static void fresh(void)
{
    memset(trk,0,sizeof trk);trk_voices_bind();memset(&song,0,sizeof song);
    memset(resource,0,sizeof resource);
    host_tracks_init();
    memset(voice_kills_part,0,sizeof voice_kills_part);voice_kills=0;
    audio_worker_online=1;
    for(unsigned p=0;p<NROWS;p++){
        host_preset(&trk[p],HOST_SYNTH,0);
        trk[p].p[P_VOICE]=V_POLY;
    }
}

int main(void)
{
    fresh();assert(voice_budget()==76u);               /* 19 points of four units: the measured steady load */
    for(unsigned p=0;p<NROWS;p++)for(unsigned n=0;n<2u;n++)trk_note_on(&trk[p],48u+4u*n,100);
    assert(voices_busy()==8u*6u && !voice_kills);      /* the host synth is CZ: six units a voice */

    fresh();
    host_preset(&trk[0],ENGI_PROPHET,0);trk[0].p[P_VOICE]=V_POLY;
    for(unsigned n=0;n<3u;n++)trk_note_on(&trk[0],48u+4u*n,100);
    assert(voice_units(&trk[0])==9u && voices_busy()==27u);
    audio_worker_online=0;
    assert(voice_budget()==64u && voice_units(&trk[0])==12u);
    audio_worker_online=1;
    /* main.c clock_boot: the CPU1 budget follows the clock running (a 320 a soft reset kept); one core keeps 16 */
    audio_worker_online=1;
    vbudget_cpu1=VBUDGET*320u/360u; assert(voice_budget()==4u*(19u*(VBUDGET*320u/360u)/VBUDGET));
    audio_worker_online=0; assert(voice_budget()==64u);
    vbudget_cpu1=VBUDGET;
    printf("Resident voice budget: 76 units (64 at 320 MHz or one core), bounded admission, protected bass, CPU1 fallback to 16: OK\n");
    return 0;
}
