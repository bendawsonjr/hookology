/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* (Hookology beta68) a DRUM track's lane pans (track_t lane_pan, voice.c spread_lr, mix.c mix_lr): through the real
 * mixer, with and without the second core's queue (BATCH): a kick centred plays the same on both sides as before;
 * panned hard left the right side is silent and the left as loud as centred; hard right the left nearly silent;
 * another lane's pan leaves the kick centred; every pan centred: the track's mono path (tests/golden.txt: exact); the SID
 * kit (one chip)
 * ignores its pans. The 808 and 909 (a voice a hit) and the CR78, FM and SAMPLE kits (one voice, panned inside). */
#define MELODEE_RAM_AUDIO 1
#ifndef BATCH
#define BATCH 1
#endif
#define MELODEE_DUAL_CORE BATCH
#define MELODEE_BATCH_AUDIO BATCH
static int audio_worker_online=1;
static int audio_worker_submit(void (*fn)(void *),void *ctx){(void)fn;(void)ctx;return 0;}
static int audio_worker_join(void){return 1;}
#define main hostsim_main
#include "hostsim.c"
#undef main
static int fails;
static void check(const char *what,int ok){printf("drum pan%s: %-70s %s\n",BATCH?"":" (one core)",what,ok?"ok":"FAIL");fails+=!ok;}
static const char *const KIT[10]={0,0,0,0,"808","909","SID","CR78","FM","SAMPLE"};

/* the track's left and right energy over the hit (blocks of mix_l / mix_r, the master's input), every other track
 * silent; lane pans pans[] */
static void hit(uint32_t kit,const int8_t *pans,double *l,double *r,int32_t *first)
{
    memset(trk,0,sizeof trk);trk_voices_bind();memset(&song,0,sizeof song);memset(resource,0,sizeof resource);host_tracks_init();
    host_preset(&trk[0],ENGI_DRUM,0);trk[0].p[P_E0]=(int16_t)kit;trk[0].p[P_MUTE]=0;trk[0].p[P_PAN]=0;
    for(uint32_t k=1;k<NROWS;k++)trk[k].p[P_MUTE]=1;
    memcpy(trk[0].lane_pan,pans,NLANE);
    int32_t o[2*CTL];
    mix_block(o,CTL);mix_block(o,CTL);                  /* (the kit ready) */
    trk_note_on(&trk[0],36,110);                        /* the kick (lane 0) */
    *l=*r=0;
    for(uint32_t b=0;b<200u;b++){
        mix_block(o,CTL);
        for(uint32_t i=0;i<CTL;i++){
            *l+=(double)mix_l[i]*mix_l[i];*r+=(double)mix_r[i]*mix_r[i];
            if(first && b<4u)first[b*CTL+i]=mix_l[i]^(mix_r[i]<<1);
        }
    }
}
static int near(double a,double b){return a>b*0.99 && a<b*1.01;}   /* (dither and noise run on between hits) */
int main(void)
{
    static const int8_t C[NLANE]={0},HL[NLANE]={-64},HR[NLANE]={63},SN[NLANE]={0,63,-40,0,20,0,0,0};
    for(uint32_t kit=4;kit<=9;kit++){
        double cl,cr,ll,lr,rl,rr,sl,sr;char w[96];
        int32_t base[4*CTL],again[4*CTL];
        hit(kit,C,&cl,&cr,base);
        hit(kit,HL,&ll,&lr,0);
        hit(kit,HR,&rl,&rr,0);
        hit(kit,SN,&sl,&sr,again);
        snprintf(w,sizeof w,"%s: centred, the kick plays alike on both sides",KIT[kit]);
        check(w,cl>1e6 && cl==cr);
        if(kit==6){
            check("SID: one chip, its pans ignored (as centred)",near(ll,cl) && near(lr,cr) && near(rl,cl) && near(rr,cr) && near(sl,cl) && near(sr,cr) && lr>cl*0.5);
            continue;
        }
        snprintf(w,sizeof w,"%s: hard left, the right silent, the left as loud as centred",KIT[kit]);
        check(w,lr<cl*1e-6 && ll>cl*0.97 && ll<cl*1.03);
        snprintf(w,sizeof w,"%s: hard right, the left nearly silent (PAN's 63: 1.6 %%), the right as centred",KIT[kit]);
        check(w,rl<cr*0.001 && rr>cr*0.97 && rr<cr*1.03);
        snprintf(w,sizeof w,"%s: other lanes panned, the kick still centred",KIT[kit]);
        check(w,sl>cl*0.97 && sl<cl*1.03 && sr>cr*0.97 && sr<cr*1.03);
        snprintf(w,sizeof w,"%s: centred again after panning: as centred (pans 0: the track's mono path)",KIT[kit]);
        trk[0].p[P_E0]=(int16_t)kit;
        hit(kit,C,&ll,&lr,again);
        check(w,near(ll,cl) && near(lr,cr) && !drum_pan_on(&trk[0]));
    }
    if(!fails)puts(BATCH?"drum pan: lanes panned through the mixer and the queue passed":"drum pan: lanes panned on one core passed");
    return fails!=0;
}
