/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, beta48) DELAY (fx_delay.h): the echo at TIME (120 BPM 1/4: 0.5 s), the right one 12 ms later, the next
 * at FDBK's level, FDBK 127 bounded and dying away, a slow tempo's long TIME halved to fit, ECONOMY's quarter rate
 * (half the memory, the same timing), the knobs' names, one DELAY in the rack, the memory back at MIX 0, its cost.
 * DELAY_WAV=dir writes delay.wav (a chord stab through it, MIX 64). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <math.h>
static fx_plan_t plan;
static int16_t pp[4];
static void setup(uint32_t bpm,int eco,int32_t time,int32_t fdbk,int32_t mix)
{
    ui_power_on();fx_clear_all();for(uint32_t k=0;k<FX_STATE_COUNT;k++)resource_release(RES_FX0+k);memset(fx_state,0,sizeof fx_state);
    song.g[G_BPM]=(int16_t)bpm;song.g[G_ECO]=(int16_t)eco;song.g[G_MFXTONE]=0;
    pp[0]=FX_DELAY;pp[1]=(int16_t)mix;pp[2]=(int16_t)time;pp[3]=(int16_t)fdbk;
}
/* n samples of input (burst: a 1 kHz burst at 0 for 10 ms) -> out_l / out_r */
static void run(float *out_l,float *out_r,uint32_t n,int burst,int noise)
{
    uint32_t rng=1;
    for(uint32_t at=0;at<n;at+=CTL){
        for(uint32_t i=0;i<CTL;i++){
            uint32_t t=at+i;int32_t x=0;
            if(burst && t<FS/100u)x=(int32_t)(sin(t*2.0*M_PI*1000.0/FS)*100000.0);
            if(noise && t<n/2u){rng=rng*1103515245u+12345u;x=(int32_t)(rng>>12)-524288;}
            fx_left[i]=fx_right[i]=x;
        }
        fx_slot_prepare(&plan,0,pp);
        if(plan.active){plan.mem=resource[RES_FX0].ptr;fx_slot_run(&plan,CTL);}
        for(uint32_t i=0;i<CTL && at+i<n;i++){
            assert(abs(fx_left[i])<=600000 && abs(fx_right[i])<=600000);
            if(out_l){out_l[at+i]=(float)fx_left[i];out_r[at+i]=(float)fx_right[i];}
        }
    }
}
/* the energy of x over [a, a+w) */
static double win(const float *x,uint32_t a,uint32_t w){double e=0;for(uint32_t i=a;i<a+w;i++)e+=(double)x[i]*x[i];return e;}
/* where the energy of x peaks (in 1 ms windows) between a and b */
static uint32_t peak(const float *x,uint32_t a,uint32_t b){uint32_t best=a;double bv=-1;for(uint32_t t=a;t+FS/1000u<b;t+=FS/1000u){double e=win(x,t,FS/1000u);if(e>bv){bv=e;best=t;}}return best;}
static void wav(const char *path,const float *l,const float *r,uint32_t frames)
{
    FILE *f=fopen(path,"wb");if(!f)return;
    uint32_t data=frames*4u,riff=36u+data,rate=FS,byterate=FS*4u,sixteen=16;uint16_t fmt=1,ch=2,align=4,bits=16;
    fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&sixteen,4,1,f);fwrite(&fmt,2,1,f);fwrite(&ch,2,1,f);
    fwrite(&rate,4,1,f);fwrite(&byterate,4,1,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&data,4,1,f);
    for(uint32_t i=0;i<frames;i++){int16_t s[2]={(int16_t)clamp((int32_t)(l[i]/4),-32768,32767),(int16_t)clamp((int32_t)(r[i]/4),-32768,32767)};fwrite(s,4,1,f);}
    fclose(f);
}
int main(void)
{
    static float l[4*FS],r[4*FS];
    for(int eco=0;eco<2;eco++){
        setup(120,eco,5,50,127);run(l,r,3*FS,1,0);
        uint32_t e1=peak(l,FS/20u,FS*3u/4u),e1r=peak(r,FS/20u,FS*3u/4u),e2=peak(l,FS*3u/4u,FS*5u/4u);
        double a1=win(l,e1,FS/100u),a2=win(l,e2,FS/100u),ratio=sqrt(a2/a1);
        printf("DELAY %s: 1/4 at 120 BPM: echo %.1f ms (right %.1f ms), second %.1f ms at %.2f of the first; %u bytes\n",eco?"LIGHT":"FULL ",
               e1*1000.0/FS,e1r*1000.0/FS,e2*1000.0/FS,ratio,resource[RES_FX0].size);
        assert(fabs(e1*1000.0/FS-500.0)<6.0 && fabs((e1r-e1)*1000.0/FS-12.0)<3.0 && fabs(e2*1000.0/FS-1000.0)<8.0);
        assert(ratio>0.2 && ratio<0.45);                                    /* FDBK 50: 0.34 */
        assert(resource[RES_FX0].size<=fx_delay_bytes(eco)+4u);
        assert(win(l,FS/50u,FS/5u)<a1*0.01);                                 /* (nothing between the burst and the echo) */
    }
    assert(fx_delay_bytes(1)*2u<fx_delay_bytes(0)+64u && fx_delay_bytes(0)<46u*1024u);
    setup(120,0,5,127,127);run(l,r,4*FS,0,1);
    {   double end=win(l,4*FS-FS/4u,FS/4u),mid=win(l,FS,FS/4u);assert(end<mid);   /* FDBK 127: bounded, dying once the input stops */
        printf("DELAY FDBK 127: bounded; last quarter second %.2g of the loud part\n",end/mid); }
    setup(40,0,7,0,127);run(l,r,2*FS,1,0);
    {   uint32_t e=peak(l,FS/20u,2*FS-FS/50u);printf("DELAY 1/2 at 40 BPM (3 s): halved to %.0f ms\n",e*1000.0/FS);assert(fabs(e*1000.0/FS-750.0)<8.0); }
    {   /* the knobs, one DELAY, the defaults when chosen, MIX 0 frees it, its cost */
        ui_power_on();shared_fx_select(song.g,0,FX_DELAY);
        assert(song.g[G_MFXTYPE]==FX_DELAY && song.g[G_MFXRATE]==5 && song.g[G_MFXCHAR]==50);
        param_desc_t t=*global_desc(G_MFXRATE),c=*global_desc(G_MFXCHAR);   /* (one static descriptor: copied) */
        assert(!strcmp(t.label,"TIME") && t.fmt==F_ENUM && t.max==7 && !strcmp(t.names[5],"1/4") && !strcmp(c.label,"FDBK"));
        shared_fx_select(song.g,1,FX_DELAY);assert(song.g[G_MFXTYPE]==FX_OFF && song.g[G_MFX2TYPE]==FX_DELAY);
        song.g[G_MFX2TYPE]=FX_OFF;song.g[G_MFXTYPE]=FX_DELAY;song.g[G_MFX3TYPE]=FX_OFF;assert(voice_fx_units()==4u);song.g[G_ECO]=1;assert(voice_fx_units()==3u);song.g[G_ECO]=0;
        setup(120,0,5,50,127);run(0,0,FS/10u,1,0);assert(resource[RES_FX0].size);
        pp[1]=0;run(0,0,FS/10u,0,0);assert(!resource[RES_FX0].size);
        assert(FX_ORDER[3]==FX_DELAY && fx_family(FX_DELAY)==FXF_SPACE && !strcmp(N_AUDIO_FX[FX_DELAY],"DELAY"));
    }
    const char *dir=getenv("DELAY_WAV");
    if(dir){
        setup(100,0,3,60,64);song.g[G_MFXTONE]=-20;
        static const double hz[3]={293.66,369.99,440.0};
        for(uint32_t at=0;at<4*FS;at+=CTL){
            for(uint32_t i=0;i<CTL;i++){double t=(double)(at+i)/FS,env=t<0.4?exp(-t*6.0):0,s=0;for(int k=0;k<3;k++)s+=sin(2*M_PI*hz[k]*t)+0.3*sin(6*M_PI*hz[k]*t);fx_left[i]=fx_right[i]=(int32_t)(s*env*35000.0);}
            fx_slot_prepare(&plan,0,pp);if(plan.active){plan.mem=resource[RES_FX0].ptr;fx_slot_run(&plan,CTL);}
            for(uint32_t i=0;i<CTL;i++){l[at+i]=(float)fx_left[i];r[at+i]=(float)fx_right[i];}
        }
        char path[512];snprintf(path,sizeof path,"%s/delay.wav",dir);wav(path,l,r,4*FS);
    }
    puts("DELAY test passed");
    return 0;
}
