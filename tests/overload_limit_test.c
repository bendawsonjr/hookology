/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Hookology's overload limit: measured costs, effects off the budget, the top of the fullest chord, note pacing. */
#define MELODEE_RAM_AUDIO 1
#define MELODEE_DUAL_CORE 1
static int audio_worker_online=1;
static int audio_worker_submit(void (*fn)(void *),void *ctx){(void)fn;(void)ctx;return 0;}
static int audio_worker_join(void){return 1;}
#define main hostsim_main
#include "hostsim.c"
#undef main
static int fails;
static void check(const char *what,int ok){printf("overload limit: %-72s %s\n",what,ok?"ok":"FAIL");fails+=!ok;}
static void fresh(void)
{
    memset(trk,0,sizeof trk);trk_voices_bind();memset(&song,0,sizeof song);memset(resource,0,sizeof resource);host_tracks_init();
    memset(settings_vpoints,0,sizeof settings_vpoints);
    audio_worker_online=1;voice_kills=voice_refusals=0;pace_n=pace_used=pace_block=0;pace_late=0;
}
static uint32_t gated(const track_t *t,uint32_t note)
{
    for(uint32_t i=0;i<NVOICE;i++)if(t->v[i].active&&t->v[i].gate&&t->v[i].note==note)return 1;
    return 0;
}
static uint32_t held(const track_t *t){uint32_t n=0;for(uint32_t i=0;i<NVOICE;i++)n+=t->v[i].active&&t->v[i].gate;return n;}
static void poly(uint32_t k,uint32_t engine,uint32_t preset){host_preset(&trk[k],engine,preset);trk[k].p[P_VOICE]=V_POLY;trk[k].p[P_MUTE]=0;}
static void blocks(uint32_t n){int32_t o[2*CTL];while(n--)mix_block(o,CTL);}

static uint32_t sounding(const track_t *t){uint32_t n=0;for(uint32_t i=0;i<NVOICE;i++)n+=t->v[i].active&&t->v[i].stage!=4u;return n;}
/* (Hookology beta72) ECO caps a synth track at 4 voices; a lower VOICES wins; drums keep theirs; ECO off: the track's own */
static void eco_voices(void)
{
    static const uint8_t SIX[6]={48,52,55,59,62,65};
    uint32_t ok=1;int16_t budget=settings_budget;
    for(uint32_t eng=0;eng<3u;eng++){
        uint32_t e=eng==0?ENGI_CZ:eng==1?ENGI_FM6:ENGI_PROPHET;
        fresh();settings_budget=32;poly(0,e,0);song.g[G_ECO]=1;
        for(uint32_t i=0;i<6u;i++)trk_note_on(&trk[0],SIX[i],100);
        ok&=held(&trk[0])==4u&&gated(&trk[0],65)&&gated(&trk[0],62)&&(e==ENGI_FM6?gated(&trk[0],55):gated(&trk[0],48));   /* (the
                                                         * newest; the lowest kept, FM6 as Dexed: the newest four) */
        fresh();settings_budget=32;poly(0,e,0);song.g[G_ECO]=1;trk[0].p[P_VMAX]=2;
        for(uint32_t i=0;i<6u;i++)trk_note_on(&trk[0],SIX[i],100);
        ok&=held(&trk[0])==2u;
        fresh();settings_budget=32;poly(0,e,0);song.g[G_ECO]=0;
        for(uint32_t i=0;i<6u;i++)trk_note_on(&trk[0],SIX[i],100);
        ok&=held(&trk[0])==6u;
    }
    check("ECO: a synth track at most 4 voices (CZ, FM6, Prophet); VOICES 2 still 2; ECO off: all six",ok);
    static const uint8_t HITS[6]={36,38,39,42,45,37};   /* (six lanes: KICK SNARE CLAP HATCL TOM RIM) */
    uint32_t n[2];
    for(uint32_t eco=0;eco<2u;eco++){
        fresh();settings_budget=32;host_preset(&trk[3],ENGI_DRUM,0);trk[3].p[P_E0]=4;song.g[G_ECO]=(int16_t)eco;   /* (the 808: a hit a voice) */
        for(uint32_t i=0;i<6u;i++)trk_note_on(&trk[3],HITS[i],100);
        n[eco]=sounding(&trk[3]);
    }
    check("ECO: a drum track keeps its voices (six 808 hits ring together, as without)",n[1]==n[0]&&n[1]==6u);
    song.g[G_ECO]=0;settings_budget=budget;
}
int main(void)
{
    eco_voices();
    fresh();
    poly(0,ENGI_PROPHET,0);poly(1,ENGI_FM6,0);poly(2,ENGI_CZ,0);poly(3,ENGI_DRUM,0);
    check("a Prophet voice costs most, FM6 least of the synths",voice_units(&trk[0])==9u&&voice_units(&trk[1])==4u&&voice_units(&trk[2])==6u);
    /* (beta64) the drum kits: 808 3, 909 5, SID 6 a voice; a lane kit by its drums ringing (3, then 2 a drum; FM 3) */
    trk[3].p[P_E0]=5;check("a 909 drum costs 5",voice_units(&trk[3])==5u);
    trk[3].p[P_E0]=6;check("the SID kit's one voice costs 6",voice_units(&trk[3])==6u);
    trk[3].p[P_E0]=4;check("an 808 drum costs 3",voice_units(&trk[3])==3u);
    for(uint32_t kit=7;kit<=9;kit++){
        trk[3].p[P_E0]=(int16_t)kit;
        uint32_t one=voice_units(&trk[3]);
        trk_note_on(&trk[3],36,110);trk_note_on(&trk[3],38,110);trk_note_on(&trk[3],42,110);trk_note_on(&trk[3],49,110);
        blocks(2);
        uint32_t four=voice_units(&trk[3]),per=kit==8?3u:2u;
        check(kit==7?"CR78: 3 for a drum, 2 more for each further one ringing":kit==8?"FM kit: 3, then 3 for each further drum":"SAMPLE kit: 3, then 2 for each further drum",
              one==3u&&four==3u+3u*per&&voices_busy()>=four);
        trk_note_off(&trk[3],36);trk_note_off(&trk[3],38);trk_note_off(&trk[3],42);trk_note_off(&trk[3],49);
        for(uint32_t i=0;i<NVOICE;i++)trk[3].v[i].active=0;
        dlk_part_t *K=dlk_part(3);if(K)memset(K->ch,0,sizeof K->ch);
    }
    trk[3].p[P_E0]=4;
    check("no shared effect: the whole budget",voice_budget()==80u);
    song.g[G_MFXTYPE]=FX_ROOM;check("a shared effect takes 8 units off",voice_budget()==72u);
    song.g[G_MFXTYPE]=FX_REVERB;check("REVERB takes 15 units off",voice_budget()==65u);
    song.g[G_MFX2TYPE]=FX_CHORUS;song.g[G_MFX3TYPE]=FX_FLANGER;check("every shared slot counts",voice_budget()==49u);

    /* a chord over a full budget keeps its low notes and a single-note track keeps its note */
    fresh();
    poly(0,ENGI_PROPHET,0);poly(1,ENGI_FM6,0);poly(2,ENGI_FM6,0);
    settings_budget=9;                                  /* 36 units: three Prophet notes and the two tracks' single notes */
    trk_note_on(&trk[1],72,100);trk_note_on(&trk[2],76,100);
    static const uint8_t CHORD[4]={48,55,60,64};
    for(uint32_t i=0;i<4u;i++)trk_note_on(&trk[0],CHORD[i],100);
    check("the top of a chord over the budget does not start",gated(&trk[0],48)&&gated(&trk[0],55)&&gated(&trk[0],60)&&!gated(&trk[0],64));
    check("a refused note is counted",voice_refusals==1u&&!voice_kills);
    check("single-note tracks keep their notes",gated(&trk[1],72)&&gated(&trk[2],76));
    check("the budget holds",voices_busy()<=voice_budget());
    trk_note_on(&trk[1],84,100);
    check("another track's new note takes the chord's top, not its bass",gated(&trk[1],84)&&gated(&trk[0],48)&&!gated(&trk[0],60));
    check("every single-note track still sounds",gated(&trk[2],76));

    /* a tail goes before any held note */
    fresh();
    poly(0,ENGI_PROPHET,0);poly(1,ENGI_FM6,0);
    settings_budget=9;
    trk_note_on(&trk[1],72,100);trk_note_on(&trk[1],76,100);trk_note_off(&trk[1],76);
    for(uint32_t i=0;i<4u;i++)trk_note_on(&trk[0],CHORD[i],100);
    check("the tail goes, the chord's three notes stay and its top is refused",gated(&trk[0],48)&&gated(&trk[0],55)&&gated(&trk[0],60)&&!gated(&trk[0],64)&&voice_kills==1u&&voice_refusals==1u);
    check("the track keeps its held note",gated(&trk[1],72));

    /* pacing */
    fresh();
    for(uint32_t k=0;k<6u;k++)poly(k,ENGI_FM6,0);
    host_preset(&trk[3],ENGI_DRUM,0);
    song.cpu_q8=100u;                                   /* a light song: every note on time */
    for(uint32_t i=0;i<4u;i++)trk_note_on(&trk[0],CHORD[i],100);
    trk_note_on(&trk[1],72,100);trk_note_on(&trk[2],76,100);
    check("a light song is never paced",held(&trk[0])==4u&&gated(&trk[1],72)&&gated(&trk[2],76)&&!pace_n);
    fresh();
    for(uint32_t k=0;k<6u;k++)poly(k,ENGI_FM6,0);
    host_preset(&trk[3],ENGI_DRUM,0);
    song.cpu_q8=PACE_BUSY_Q8;
    for(uint32_t i=0;i<4u;i++)trk_note_on(&trk[0],CHORD[i],100);
    trk_note_on(&trk[1],72,100);
    trk_note_on(&trk[3],36,110);
    check("a busy half starts two notes",held(&trk[0])==2u&&!gated(&trk[1],72)&&pace_n==3u);
    check("a drum starts at once",trk[3].v[0].active||trk[3].v[1].active||trk[3].v[2].active||trk[3].v[3].active||trk[3].v[4].active||trk[3].v[5].active||trk[3].v[6].active||trk[3].v[7].active);
    trk_note_on(&trk[0],67,100);
    check("a track's later note stays behind its waiting note",pace_n==4u);
    blocks(PACE_BLOCKS);
    check("the next half starts the next two",gated(&trk[0],60)&&gated(&trk[0],64)&&!gated(&trk[1],72)&&pace_n==2u);
    blocks(PACE_BLOCKS);
    check("the half after starts the rest, in order",gated(&trk[1],72)&&gated(&trk[0],67)&&!pace_n);
    fresh();
    poly(0,ENGI_FM6,0);poly(1,ENGI_FM6,0);
    song.cpu_q8=PACE_BUSY_Q8;pace_used=PACE_HALF;
    trk_note_on(&trk[0],60,100);trk_note_on(&trk[0],64,100);trk_note_on(&trk[1],72,100);
    trk_note_off(&trk[0],60);
    check("a note released before it starts never starts",pace_n==2u);
    trk_all_off(&trk[0]);
    check("all off drops the track's waiting notes",pace_n==1u);
    blocks(PACE_BLOCKS);
    check("a waiting note of another track still starts",gated(&trk[1],72)&&!gated(&trk[0],60)&&!gated(&trk[0],64));
    fresh();
    poly(0,ENGI_FM6,0);trk[0].p[P_MUTE]=1;
    song.cpu_q8=PACE_BUSY_Q8;pace_used=PACE_HALF;
    trk_note_on(&trk[0],60,100);blocks(PACE_BLOCKS);
    check("a track muted while its note waits stays silent",!held(&trk[0]));
    fresh();
    poly(0,ENGI_FM6,0);
    song.cpu_q8=PACE_BUSY_Q8;pace_used=PACE_HALF;
    for(uint32_t i=0;i<40u;i++)trk_note_on(&trk[0],40u+i,100);
    check("a full queue never loses control: it is bounded",pace_n==PACE_QUEUE);
    trk_all_off(&trk[0]);
    for(uint32_t i=0;i<LOAD_HALVES*3u;i++)load_note(40u);
    check("LOAD shows a steady load",load_percent()==40u);
    load_note(120u);check("LOAD holds a spike",load_percent()==120u);
    for(uint32_t i=0;i<LOAD_HALVES;i++)load_note(40u);
    check("LOAD still shows it for the next second",load_percent()==120u);
    for(uint32_t i=0;i<LOAD_HALVES;i++)load_note(40u);
    check("LOAD lets it go after that",load_percent()==40u);
    printf(fails?"overload limit: FAILED\n":"overload limit: all passed\n");
    return fails!=0;
}
