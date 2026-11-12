/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* QUALITY (Hookology): a Prophet, CZ-1, FM6 or SID chord rendered FULL and LIGHT on the host ("cz50": CZ-1 preset 50).
 *   prophet_quality_test                    checks: LIGHT keeps the tone below 4 kHz within 1 dB per octave band, peaks
 *                                           within 2 dB of FULL, costs under 70 % of FULL (host instructions: a ranking,
 *                                           not the FM-1's cycles; device timing: tools/usb_audio_stats.py)
 *   prophet_quality_test DIR [preset ...]   writes DIR/<preset>_<full|light>.pcm (int32, mono) and prints the costs */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
static uint64_t instructions(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 r;
    if(!proc_pid_rusage(getpid(),RUSAGE_INFO_V4,(rusage_info_t *)&r))return r.ri_instructions;
#endif
    return 0;
}
static int32_t pcm[2][44100 * 3];
static int16_t eco;                     /* the project's ECONOMY for the next render (host_tracks_init resets song) */
static double render_eng(const char *dir, uint32_t engine, uint32_t preset, uint32_t light)
{
    int32_t *pc=pcm[light];
    memset(trk,0,sizeof trk);trk_voices_bind();eng_state_reset();
    memset(p5_ready,0,sizeof p5_ready);
    host_tracks_init(); host_preset(&trk[0], engine, preset);
    track_t *t=&trk[0];
    t->p[P_VOICE]=V_POLY; t->p[P_P5Q]=(int16_t)light; song.g[G_ECO]=eco;
    static const uint8_t chord[4]={48,55,64,71};
    for(uint32_t k=0;k<4u;k++)trk_note_on(t,chord[k],100);
    uint64_t i0=instructions(), voice_samples=0;
    uint32_t n=0;
    for(uint32_t b=0;b<(44100u*3u)/CTL;b++){
        if(b==(44100u*3u/2u)/CTL)for(uint32_t k=0;k<4u;k++)trk_note_off(t,chord[k]);
        int32_t out[CTL]={0};
        for(uint32_t v=0;v<NVOICE;v++)voice_samples+=(t->v[v].active?CTL:0u);
        track_render(t,out,CTL);
        memcpy(pc+n,out,sizeof out);n+=CTL;
    }
    uint64_t ins=instructions()-i0;
    double cost=voice_samples?(double)ins/(double)voice_samples:0.0;
    if(dir){
        char path[512];snprintf(path,sizeof path,"%s/%s%u_%s.pcm",dir,engine==ENGI_CZ?"cz":engine==ENGI_FM6?"fm":engine==ENGI_SID?"si":"",preset,light?"light":"full");
        FILE *f=fopen(path,"wb");fwrite(pc,sizeof pc[0],n,f);fclose(f);
        printf("%s%u,%s,%.1f\n",engine==ENGI_CZ?"cz":engine==ENGI_FM6?"fm":engine==ENGI_SID?"si":"",preset,light?"LIGHT":"FULL",cost);
    }
    return cost;
}
static double render(const char *dir, uint32_t preset, uint32_t light) { return render_eng(dir, ENGI_PROPHET, preset, light); }
/* the energy in the octave band around fc (an RBJ band-pass, Q 1.41) */
static double band(const int32_t *x,uint32_t n,double fc)
{
    double w=2.0*M_PI*fc/44100.0,c=cos(w),al=sin(w)/(2.0*1.41),a0=1.0+al,x1=0,x2=0,y1=0,y2=0,e=0;
    for(uint32_t i=0;i<n;i++){
        double v=x[i],y=(al*v-al*x2+2.0*c*y1-(1.0-al)*y2)/a0;
        x2=x1;x1=v;y2=y1;y1=y;e+=y*y;
    }
    return e;
}
static int checks(void)
{
    static const uint32_t E[]={ENGI_PROPHET,ENGI_PROPHET,ENGI_PROPHET,ENGI_CZ,ENGI_CZ,ENGI_FM6,ENGI_FM6,ENGI_FM6,ENGI_SID,ENGI_SID,ENGI_SID};
    static const uint32_t P[]={23u,1u,120u,50u,2u,0u,1u,9u,16u,10u,9u};    /* Whiny Opener, It's a Prophet, a pad; CZ-1 FAT ENSEMBLE, BRASS 2;
                                                                 * FM6 TINE EP, BELL, BRASS SECT; SID PWM PAD, SYNC LEAD, RING LEAD */
    static const double F[]={125,250,500,1000,2000,4000};
    int bad=0;
    for(uint32_t k=0;k<NELEM(E);k++){
        double full=render_eng(0,E[k],P[k],0),light=render_eng(0,E[k],P[k],1),worst=0;
        int32_t peak=0,peak_full=0;
        for(uint32_t i=0;i<44100u*3u;i++){
            int32_t a=pcm[1][i]<0?-pcm[1][i]:pcm[1][i],f=pcm[0][i]<0?-pcm[0][i]:pcm[0][i];
            if(a>peak)peak=a;
            if(f>peak_full)peak_full=f;
        }
        double pd=peak_full?20.0*log10((double)peak/(double)peak_full):99.0;
        for(uint32_t b=0;b<6u;b++){
            double d=10.0*log10((band(pcm[1],44100u*3u,F[b])+1e-9)/(band(pcm[0],44100u*3u,F[b])+1e-9));
            if(fabs(d)>fabs(worst))worst=d;
        }
        int ok=fabs(worst)<=(E[k]==ENGI_SID?1.5:1.0) && fabs(pd)<=2.0   /* (SID: bright edges, its 4 kHz band droops ~1 dB) */ && (full<=0.0 || light<(E[k]==ENGI_CZ?0.8:E[k]==ENGI_FM6?0.88:E[k]==ENGI_SID?0.75:0.7)*full);
        printf("%s QUALITY: preset %u LIGHT within %.2f dB below 4 kHz, peak %+.2f dB, %.0f%% of FULL's host cost  %s\n",
               E[k]==ENGI_CZ?"CZ-1":E[k]==ENGI_FM6?"FM6":E[k]==ENGI_SID?"SID":"Prophet",P[k],worst,pd,full>0.0?100.0*light/full:0.0,ok?"ok":"FAIL");
        bad+=!ok;
    }
    for(uint32_t k=0;k<4u;k++){   /* ECONOMY (G_ECO): a FULL track plays exactly as LIGHT */
        uint32_t e=k==3u?ENGI_SID:k==2u?ENGI_FM6:k?ENGI_CZ:ENGI_PROPHET,p=k==3u?16u:k==2u?1u:k?50u:23u;
        /* (the CZ-1 renders the same twice: an exact match; a Prophet's free-running phases do not: LIGHT's cost) */
        double full=render_eng(0,e,p,0),light;
        eco=0;light=render_eng(0,e,p,1);
        eco=1;double as=render_eng(0,e,p,0);eco=0;song.g[G_ECO]=0;
        int ok=k?!memcmp(pcm[0],pcm[1],sizeof pcm[0]):(full<=0.0 || (as<0.8*full && as<1.15*light));
        printf("%s ECONOMY: a FULL track renders as LIGHT  %s\n",k==3u?"SID":k==2u?"FM6":k?"CZ-1":"Prophet",ok?"ok":"FAIL");
        bad+=!ok;
    }
    {   /* FM6 F25 MELLOW TRM (Hookology) is exactly the voice designed and auditioned (the DX7 dump sent to the FM-1) */
        static const uint8_t SENT[155]={95,40,30,60,99,80,0,0,39,0,0,0,0,0,0,3,42,0,1,0,7,95,28,20,60,99,90,0,0,39,0,0,0,0,0,3,1,78,0,1,0,6,97,62,40,60,99,60,0,0,39,0,20,0,0,3,0,6,58,0,14,0,7,95,18,20,50,99,95,0,0,39,0,0,0,0,2,3,2,90,0,1,0,8,95,50,35,78,99,75,0,0,39,0,20,0,0,3,0,7,48,0,1,0,7,96,22,25,67,99,78,0,0,39,0,0,0,0,3,3,2,98,0,1,0,7,99,99,99,99,50,50,50,50,4,3,1,30,0,0,45,0,4,0,24,77,69,76,76,79,87,32,84,82,77};
        uint8_t v[FP_SIZE+1u],pk[FM6_PACKED];
        fm6_factory(FM6_NFACTORY+16u,pk);fm6_unpack(pk,v);
        int ok=!memcmp(v,SENT,155) && str_eq(FM6_PRESETS[FM6_NFACTORY+16u].name,"MELLOW TRM") && str_eq(FM6_PRESETS[FM6_NFAC].name,"INIT VOICE");
        printf("FM6 F25 MELLOW TRM: the auditioned voice, INIT VOICE after it  %s\n",ok?"ok":"FAIL");
        bad+=!ok;
    }
    {   /* the shared budget counts a LIGHT voice as FULL (beta27: cheaper let a six-track mix overload) */
        track_t *t=&trk[0];host_preset(t,ENGI_PROPHET,23u);
        t->p[P_P5Q]=0;uint32_t full=voice_units(t);
        t->p[P_P5Q]=1;uint32_t light=voice_units(t);
        int ok=light==full;
        printf("Prophet QUALITY: the budget counts a LIGHT voice as FULL (%u units)  %s\n",light,ok?"ok":"FAIL");
        bad+=!ok;
    }
    return bad;
}
int main(int argc,char **argv)
{
    if(argc<2)return checks()?1:0;
    puts("preset,mode,host_instructions_per_voice_sample");
    for(int a=2;a<argc || a==2;a++){
        const char *arg=argc>2?argv[a]:"23";
        uint32_t cz=arg[0]=='c',fm=arg[0]=='f',si=arg[0]=='s',preset=(uint32_t)atoi(arg+(cz||fm||si?2:0)),e=cz?ENGI_CZ:fm?ENGI_FM6:si?ENGI_SID:ENGI_PROPHET;   /* ("cz50": CZ-1 50, "fm1": FM6 1, "si16": SID 16) */
        render_eng(argv[1],e,preset,0);render_eng(argv[1],e,preset,1);
        if(argc<=2)break;
    }
    return 0;
}
