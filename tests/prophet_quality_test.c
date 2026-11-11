/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* QUALITY (Hookology): a Prophet or CZ-1 chord rendered FULL and LIGHT on the host ("cz50": CZ-1 preset 50).
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
static double render_eng(const char *dir, uint32_t engine, uint32_t preset, uint32_t light)
{
    int32_t *pc=pcm[light];
    memset(trk,0,sizeof trk);trk_voices_bind();eng_state_reset();
    memset(p5_ready,0,sizeof p5_ready);
    host_tracks_init(); host_preset(&trk[0], engine, preset);
    track_t *t=&trk[0];
    t->p[P_VOICE]=V_POLY; t->p[P_P5Q]=(int16_t)light;
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
        char path[512];snprintf(path,sizeof path,"%s/%s%u_%s.pcm",dir,engine==ENGI_CZ?"cz":"",preset,light?"light":"full");
        FILE *f=fopen(path,"wb");fwrite(pc,sizeof pc[0],n,f);fclose(f);
        printf("%s%u,%s,%.1f\n",engine==ENGI_CZ?"cz":"",preset,light?"LIGHT":"FULL",cost);
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
    static const uint32_t E[]={ENGI_PROPHET,ENGI_PROPHET,ENGI_PROPHET,ENGI_CZ,ENGI_CZ};
    static const uint32_t P[]={23u,1u,120u,50u,2u};    /* Whiny Opener, It's a Prophet, a pad; CZ-1 FAT ENSEMBLE, BRASS 2 */
    static const double F[]={125,250,500,1000,2000,4000};
    int bad=0;
    for(uint32_t k=0;k<5u;k++){
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
        int ok=fabs(worst)<=1.0 && fabs(pd)<=2.0 && (full<=0.0 || light<(E[k]==ENGI_CZ?0.8:0.7)*full);
        printf("%s QUALITY: preset %u LIGHT within %.2f dB below 4 kHz, peak %+.2f dB, %.0f%% of FULL's host cost  %s\n",
               E[k]==ENGI_CZ?"CZ-1":"Prophet",P[k],worst,pd,full>0.0?100.0*light/full:0.0,ok?"ok":"FAIL");
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
        uint32_t cz=arg[0]=='c',preset=(uint32_t)atoi(arg+(cz?2:0)),e=cz?ENGI_CZ:ENGI_PROPHET;   /* ("cz50": CZ-1 50) */
        render_eng(argv[1],e,preset,0);render_eng(argv[1],e,preset,1);
        if(argc<=2)break;
    }
    return 0;
}
