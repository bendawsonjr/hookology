/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, beta47) REVERB LIGHT: ECONOMY runs the SLOOP reverb at half rate (fx_sloop_reverb.h
 * fx_sloop_reverb_light_run). Against FULL: half the memory, the same decay (energy in each window of the tail within
 * 2 dB), the size and damping still act, bounded on hot input, no runaway, ECONOMY switched while it plays fades
 * across, and fewer instructions. REVERB_WAV=dir writes full.wav / light.wav (a chord stab, the reverb at MIX 64). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "fx_kernel_fixture.h"
#include <assert.h>
#include <math.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
static uint64_t instructions(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 r;if(!proc_pid_rusage(getpid(),RUSAGE_INFO_V4,(rusage_info_t *)&r))return r.ri_instructions;
#endif
    return 0;
}
static int32_t b[CTL];
static void reset(int eco){ui_power_on();punch_reset();fx_clear_all();for(uint32_t k=0;k<FX_STATE_COUNT;k++)resource_release(RES_FX0+k);memset(fx_state,0,sizeof fx_state);spread_on=0;song.g[G_ECO]=(int16_t)eco;}
/* the impulse's tail energy in windows: [0] 0..0.25 s, [1] 0.25..1 s, [2] 1..2 s, [3] 2..4 s */
static void tail(int eco,uint32_t size,uint32_t damping,double *w)
{
    reset(eco);TSEL->p[P_FXTYPE]=FX_REVERB;TSEL->p[P_FXMIX]=127;TSEL->p[P_FXRATE]=(int16_t)size;TSEL->p[P_FXCHAR]=(int16_t)damping;
    static const uint32_t edge[5]={0,FS/4,FS,2*FS,4*FS};
    uint64_t last=0;for(uint32_t k=0;k<4;k++)w[k]=0;
    for(uint32_t at=0;at<10*FS;at+=CTL){
        memset(b,0,sizeof b);if(!at)for(uint32_t i=0;i<CTL;i++)b[i]=(int32_t)(sin(i*2.0*M_PI*700.0/FS)*24000.0);   /* (a 700 Hz burst) */
        assert(fx_kernel_process(TSEL,b,CTL));
        for(uint32_t i=0;i<CTL;i++){
            assert(abs(fx_left[i])<=262143 && abs(fx_right[i])<=262143);
            double e=(double)fx_left[i]*fx_left[i]+(double)fx_right[i]*fx_right[i];
            for(uint32_t k=0;k<4;k++)if(at+i>=edge[k] && at+i<edge[k+1])w[k]+=e;
            if(at>9*FS)last+=(uint64_t)(abs(fx_left[i])+abs(fx_right[i]));
        }
    }
    assert(last<2000);
}
static double db(double a,double b){return 10.0*log10((a+1.0)/(b+1.0));}
static void wav(const char *path,const int16_t *pcm,uint32_t frames)
{
    FILE *f=fopen(path,"wb");if(!f)return;
    uint32_t data=frames*4u,riff=36u+data,rate=FS,byterate=FS*4u;uint16_t fmt=1,ch=2,align=4,bits=16;uint32_t sixteen=16;
    fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&sixteen,4,1,f);fwrite(&fmt,2,1,f);fwrite(&ch,2,1,f);
    fwrite(&rate,4,1,f);fwrite(&byterate,4,1,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&data,4,1,f);
    fwrite(pcm,4,frames,f);fclose(f);
}
static void render(int eco,const char *dir)
{
    static int16_t pcm[6*FS*2];
    reset(eco);TSEL->p[P_FXTYPE]=FX_REVERB;TSEL->p[P_FXMIX]=64;TSEL->p[P_FXRATE]=90;TSEL->p[P_FXCHAR]=30;
    static const double hz[4]={261.63,329.63,392.0,523.25};
    for(uint32_t at=0;at<6*FS;at+=CTL){
        for(uint32_t i=0;i<CTL;i++){
            double t=(double)(at+i)/FS,env=t<0.6?exp(-t*3.0):0.0,s=0;
            for(int k=0;k<4;k++)s+=sin(2*M_PI*hz[k]*t)+0.3*sin(4*M_PI*hz[k]*t);
            b[i]=(int32_t)(s*env*9000.0);
        }
        assert(fx_kernel_process(TSEL,b,CTL));
        for(uint32_t i=0;i<CTL;i++){pcm[(at+i)*2]=(int16_t)clamp(fx_left[i]>>2,-32768,32767);pcm[(at+i)*2+1]=(int16_t)clamp(fx_right[i]>>2,-32768,32767);}
    }
    char path[512];snprintf(path,sizeof path,"%s/%s.wav",dir,eco?"light":"full");wav(path,pcm,6*FS);
}
int main(void)
{
    assert(sizeof(fx_sloop_reverb_light_t)*2u<=sizeof(fx_sloop_reverb_t)+256u);
    static const uint32_t sizes[3]={0,64,127};
    for(uint32_t s=0;s<3;s++)for(uint32_t d=0;d<2;d++){
        double f[4],l[4];tail(0,sizes[s],d?100:0,f);tail(1,sizes[s],d?100:0,l);
        printf("REVERB LIGHT size %3u damp %3u: dB vs FULL %+.2f %+.2f %+.2f %+.2f\n",sizes[s],d?100:0,db(l[0],f[0]),db(l[1],f[1]),db(l[2],f[2]),db(l[3],f[3]));
        printf("  full %.3g %.3g %.3g %.3g light %.3g %.3g %.3g %.3g\n",f[0],f[1],f[2],f[3],l[0],l[1],l[2],l[3]);
        for(uint32_t k=0;k<4;k++)if(f[k]>f[0]*1e-4)assert(fabs(db(l[k],f[k]))<(k<3?2.0:3.0));   /* (the audible tail: within 40 dB) */
    }
    {   double a[4],c[4];tail(1,127,0,a);tail(1,0,0,c);assert(db(a[2],c[2])>6.0);   /* SIZE acts */
        tail(1,127,127,c);assert(db(a[2],c[2])>1.0); }                            /* DAMP acts */
    reset(1);TSEL->p[P_FXTYPE]=FX_REVERB;TSEL->p[P_FXMIX]=127;TSEL->p[P_FXRATE]=127;
    for(uint32_t j=0;j<3000;j++){
        for(uint32_t i=0;i<CTL;i++)b[i]=((i+j)&1u)?1048575:-1048575;
        assert(fx_kernel_process(TSEL,b,CTL));
        for(uint32_t i=0;i<CTL;i++)assert(abs(fx_left[i])<=262143 && abs(fx_right[i])<=262143);
    }
    assert(resource[RES_FX0].size && resource[RES_FX0].size<=sizeof(fx_sloop_reverb_light_t)+4u);
    {   /* ECONOMY switched while it plays: a fade, then the other tank (no jump bigger than the signal allows) */
        reset(0);TSEL->p[P_FXTYPE]=FX_REVERB;TSEL->p[P_FXMIX]=127;TSEL->p[P_FXRATE]=100;
        int32_t prev=0,jump=0;
        for(uint32_t j=0;j<400;j++){
            if(j==200)song.g[G_ECO]=1;
            for(uint32_t i=0;i<CTL;i++)b[i]=(int32_t)(sin((j*CTL+i)*0.05)*20000.0);
            assert(fx_kernel_process(TSEL,b,CTL));
            for(uint32_t i=0;i<CTL;i++){int32_t d=abs(fx_left[i]-prev);if(j>=190&&j<230&&d>jump)jump=d;prev=fx_left[i];}
        }
        assert(fx_state[0].light==1 && resource[RES_FX0].size<=sizeof(fx_sloop_reverb_light_t)+4u);
        printf("REVERB LIGHT: ECONOMY switched playing, largest step %d\n",jump);
        assert(jump<12000);
    }
    {   uint64_t cost[2];
        for(int eco=0;eco<2;eco++){
            reset(eco);TSEL->p[P_FXTYPE]=FX_REVERB;TSEL->p[P_FXMIX]=127;TSEL->p[P_FXRATE]=100;
            for(uint32_t i=0;i<CTL;i++)b[i]=(int32_t)(i*97%2000)-1000;
            fx_kernel_process(TSEL,b,CTL);
            uint64_t a=instructions();for(uint32_t j=0;j<4000;j++)fx_kernel_process(TSEL,b,CTL);cost[eco]=instructions()-a;
        }
        printf("REVERB LIGHT: %zu bytes (FULL %zu); host instructions LIGHT/FULL %.2f\n",sizeof(fx_sloop_reverb_light_t),sizeof(fx_sloop_reverb_t),cost[0]?(double)cost[1]/cost[0]:0.0);
    }
    {   /* the overload limit counts REVERB LIGHT a third (voice.c voice_fx_units, measured on the FM-1) */
        reset(0);song.g[G_MFXTYPE]=FX_REVERB;song.g[G_MFXTYPE+FX_PARAMS]=FX_OFF;song.g[G_MFXTYPE+2*FX_PARAMS]=FX_OFF;
        uint32_t full=voice_fx_units();song.g[G_ECO]=1;uint32_t light=voice_fx_units();
        assert(full==15u && light==5u);
    }
    const char *dir=getenv("REVERB_WAV");if(dir){render(0,dir);render(1,dir);}
    puts("REVERB LIGHT test passed");
    return 0;
}
