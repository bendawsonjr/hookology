/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* (Hookology beta69) EQ, a track's INSERT (fx_eq.h): its response measured with sines through fx_eq_run, both sides:
 * flat at every frequency; LOW, MID (at FREQ) and HIGH each +-12 dB where they should be and about 0 dB far from it;
 * FREQ's ends 200 Hz and 5 kHz, 1 kHz in the middle; a new EQ fading in over its first block; the display's values. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
static int fails;
static void check(const char *what,int ok){printf("EQ: %-74s %s\n",what,ok?"ok":"FAIL");fails+=!ok;}

/* the gain in dB at hz of an EQ with the slot's values p (TYPE MIX=LOW RATE=MID CHAR=FREQ) and TONE=HIGH */
static double gain(double hz,int32_t low,int32_t mid,int32_t freq,int32_t high)
{
    fx_state_t f;fx_zero(&f);f.type=FX_EQ;f.mix=32768;
    int16_t p[4]={FX_EQ,(int16_t)low,(int16_t)mid,(int16_t)freq};
    double in=0,out=0,ph=0,dp=2.0*M_PI*hz/FS;
    for(uint32_t b=0;b<400u;b++){                       /* (400 blocks: 0.29 s, settled; the last half measured) */
        int32_t x[CTL];
        for(uint32_t i=0;i<CTL;i++){x[i]=(int32_t)(200000.0*sin(ph));ph+=dp;fx_left[i]=fx_right[i]=x[i];}
        fx_eq_run(&f,p,high,CTL,32768);
        if(b>=200u)for(uint32_t i=0;i<CTL;i++){in+=(double)x[i]*x[i];out+=(double)fx_left[i]*fx_left[i];
            if(fx_left[i]!=fx_right[i])return 999.0;}  /* (both sides alike) */
    }
    return 10.0*log10(out/in);
}
static int near(double a,double b,double tol){return fabs(a-b)<=tol;}
int main(void)
{
    static const double HZ[]={40,100,300,1000,3000,8000,15000};
    int ok=1;char v[16];const char *u;
    for(uint32_t i=0;i<NELEM(HZ);i++)ok&=near(gain(HZ[i],64,64,64,0),0.0,0.05);
    check("flat (64, 64, FREQ, 0): 0 dB from 40 Hz to 15 kHz",ok);
    check("LOW +12: about +12 dB at 40 Hz, near 0 at 1 kHz and 8 kHz",near(gain(40,127,64,64,0),11.8,0.6) && near(gain(1000,127,64,64,0),0.0,0.4) && near(gain(8000,127,64,64,0),0.0,0.1));
    check("LOW -12: about -12 dB at 40 Hz",near(gain(40,0,64,64,0),-12.0,0.6));
    check("MID +12 at 1 kHz (FREQ 64): +12 dB at 1 kHz, near 0 at 100 Hz and 10 kHz",near(gain(1000,64,127,64,0),11.8,0.3) && near(gain(100,64,127,64,0),0.0,0.6) && near(gain(10000,64,127,64,0),0.0,0.6));
    check("MID -12 at 200 Hz (FREQ 0): -12 dB at 200 Hz, near 0 at 5 kHz",near(gain(200,64,0,0,0),-12.0,0.3) && near(gain(5000,64,0,0,0),0.0,0.3));
    check("MID +12 at 5 kHz (FREQ 127): +12 dB at 5 kHz",near(gain(5000,64,127,127,0),11.8,0.3));
    check("HIGH +12: about +12 dB at 15 kHz, near 0 at 300 Hz",near(gain(15000,64,64,64,63),12.0,0.8) && near(gain(300,64,64,64,63),0.0,0.1));
    check("HIGH -12: about -12 dB at 15 kHz",near(gain(15000,64,64,64,-63),-12.0,0.8));
    check("FREQ: 200 Hz, 1 kHz in the middle, 5 kHz",fabs(fx_eq_hz(0)-200.0f)<1.0f && fabs(fx_eq_hz(64)-1013.0f)<15.0f && fabs(fx_eq_hz(127)-5000.0f)<10.0f);
    {   /* a new EQ (mix 0) fades in: the first sample nearly dry, the last all wet */
        fx_state_t f;fx_zero(&f);f.type=FX_EQ;
        int16_t p[4]={FX_EQ,127,64,64};
        for(uint32_t i=0;i<CTL;i++)fx_left[i]=fx_right[i]=100000;
        fx_eq_run(&f,p,0,CTL,32768);
        check("a new EQ fades in over its first block (mix 0 -> all in)",f.mix==32768 && abs(fx_left[0]-100000)<10000);
    }
    {
        const param_desc_t d={0};param_desc_t lo=d,hi=d;lo.fmt=F_EQDB;lo.min=0;lo.max=127;hi.fmt=F_EQDB;hi.min=-63;hi.max=63;
        int s=1;
        param_format(&lo,64,v,&u);s&=str_eq(v,"0.0")&&str_eq(u,"dB");
        param_format(&lo,127,v,&u);s&=str_eq(v,"+11.8");
        param_format(&lo,0,v,&u);s&=str_eq(v,"-12.0");
        param_format(&hi,63,v,&u);s&=str_eq(v,"+12.0");
        lo.fmt=F_EQHZ;param_format(&lo,0,v,&u);s&=str_eq(v,"200")&&str_eq(u,"Hz");
        param_format(&lo,127,v,&u);s&=str_eq(v,"5.0")&&str_eq(u,"kHz");
        check("the display: 0.0 dB, +11.8, -12.0, HIGH +12.0; 200 Hz .. 5.0 kHz",s);
    }
    if(!fails)puts("EQ: response, fade and display passed");
    return fails!=0;
}
