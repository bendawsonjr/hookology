/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * SLOOP stereo feedback network and freeze by isod89 (f2b44c2, d30d655).
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * (retained Felucca input diffuser). Adapted from isod89/sloop-fm1 fx.c,
 * 4ec1ac829fd2bdf2c571ea215fb3d62477d01220. */
/* Full-rate mono input, stereo return. One optional tank per live slot;
 * normal inserts and CHAIN/SPLIT buses use the same wet processor. No dry
 * signal is hidden in the return. SIZE changes decay, DAMP the feedback tone.
 * Four-times internal headroom; original SLOOP delay lengths and mixing. */
#define FX_SLOOP_REV_MOD 12u
#define FX_SLOOP_REV_LINES (1559u+1931u+2389u+2791u+FX_SLOOP_REV_MOD+2u)
#define FX_SLOOP_REV_AP (556u+441u)
typedef struct {
    int16_t line[FX_SLOOP_REV_LINES],ap[FX_SLOOP_REV_AP];
    uint16_t pos[4],ap_pos[2];
    int32_t low[4];
    uint32_t phase;
} fx_sloop_reverb_t;
_Static_assert(sizeof(fx_sloop_reverb_t)<20u*1024u,"SLOOP reverb fits 20 KiB");
static const uint16_t SR_LEN[4] __attribute__((section(".data.fx_sloop")))={1573,1931,2389,2791};
static const uint16_t SR_BASE[4] __attribute__((section(".data.fx_sloop")))={0,1573,3504,5893};
static const uint16_t SR_AP[2] __attribute__((section(".data.fx_sloop")))={556,441};
static __attribute__((noinline,section(".audio_text"))) void fx_sloop_reverb_run_mode(
    fx_state_t *restrict f,fx_sloop_reverb_t *restrict s,uint32_t n,int32_t target,
    int32_t *restrict l,int32_t *restrict r,uint32_t capture)
{
    if(!n)return;
    int32_t gain=17000+f->rate*104,damp=32767-f->character*200,mix=f->mix;
    /* Drift is designed twice per block and interpolated with an add per
     * sample. A held tank has fixed taps, unity feedback and no damping. */
    int32_t ma=0,dm=0;
    if(!capture){
        ma=(12<<8)+((sine_i(s->phase)*12)>>7);
        s->phase+=LFO_INC[30]*n/CTL;
        int32_t mb=(12<<8)+((sine_i(s->phase)*12)>>7);
        dm=(mb-ma)*256/(int32_t)n;
    }
    int32_t drift=ma*256;
    for(uint32_t i=0;i<n;i++,drift+=dm){
        int32_t dry_l=l[i],dry_r=r[i];
        int32_t mono=fx_div_pow2(clamp(dry_l,-1048575,1048575)+clamp(dry_r,-1048575,1048575),1);
        int32_t input=capture==2u?0:fx_tone_input(f,mono,1.0f/(0.010f*FS));
        int32_t a=fx_div_pow2(clamp(input,-262143,262143),capture?4u:2u);
        a=fx_div_pow2(a*13000,15);
        uint32_t off=0;
        for(uint32_t k=0;k<2u;k++){
            int32_t b=s->ap[off+s->ap_pos[k]],v=a+(b>>1);
            s->ap[off+s->ap_pos[k]]=(int16_t)clamp(v,-32768,32767);
            a=b-(v>>1);
            if(++s->ap_pos[k]==SR_AP[k])s->ap_pos[k]=0;
            off+=SR_AP[k];
        }
        uint32_t tap=s->pos[0]+((uint32_t)drift>>16);
        if(tap>=SR_LEN[0])tap-=SR_LEN[0];
        uint32_t next=tap+1u==SR_LEN[0]?0u:tap+1u;
        int32_t o0=s->line[tap];
        o0+=((s->line[next]-o0)*((drift>>8)&255)+(capture?128:0))>>8;
        int32_t o1=s->line[SR_BASE[1]+s->pos[1]],o2=s->line[SR_BASE[2]+s->pos[2]],o3=s->line[SR_BASE[3]+s->pos[3]];
        int32_t s0=o0+o1,s1=o2+o3,d0=o0-o1,d1=o2-o3;
        int32_t v[4]={s0+s1,d0+d1,s0-s1,d0-d1};
        for(uint32_t k=0;k<4u;k++){
            int32_t fb=fx_div_pow2(v[k],1);
            if(!capture){
                s->low[k]+=fx_div_pow2(clamp(fb-s->low[k],-65535,65535)*damp,15);
                fb=fx_div_pow2(s->low[k]*gain,15);
            }else s->low[k]=fb;
            s->line[SR_BASE[k]+s->pos[k]]=(int16_t)clamp(fb+((k&1u)?-a:a),-32768,32767);
            if(++s->pos[k]==SR_LEN[k])s->pos[k]=0;
        }
        int32_t wl=(o0+o2)*(capture?8:4),wr=(o1-o3)*(capture?8:4);
        if(mix!=target)mix+=clamp(target-mix,-128,128);
        l[i]=dry_l+fx_scale(clamp(wl,-262143,262143)-dry_l,(uint32_t)mix*2u);
        r[i]=dry_r+fx_scale(clamp(wr,-262143,262143)-dry_r,(uint32_t)mix*2u);
    }
    f->mix=mix;
}
static void fx_sloop_reverb_run(fx_state_t *f,fx_sloop_reverb_t *s,uint32_t n,int32_t target,int32_t *l,int32_t *r)
{fx_sloop_reverb_run_mode(f,s,n,target,l,r,0);}

/* (Hookology) REVERB LIGHT, with ECONOMY (G_ECO): the same network at a quarter of the rate, every line and diffuser a
 * quarter as long (the same times): about a third of FULL's work and a quarter of its memory (fx_slot_prepare
 * allocates the light tank). The input is the average of four samples, the damping (1-(1-a)^4) and the drift scaled to
 * the rate, the wet linearly interpolated back to the full rate (one step late). The tail keeps what lies below
 * ~5 kHz. FULL (fx_sloop_reverb_run_mode) is unchanged. */
#define FX_SRL_D 4u
#define FX_SRL_LINES (394u+483u+597u+698u)
#define FX_SRL_AP (139u+110u)
typedef struct {
    int16_t line[FX_SRL_LINES],ap[FX_SRL_AP];
    uint16_t pos[4],ap_pos[2];
    int32_t low[4];
    uint32_t phase,sub;
    int32_t acc,wb_l,wb_r,cur_l,cur_r,dl_l,dl_r;
} fx_sloop_reverb_light_t;
static const uint16_t SRL_LEN[4] __attribute__((section(".data.fx_sloop")))={394,483,597,698};
static const uint16_t SRL_BASE[4] __attribute__((section(".data.fx_sloop")))={0,394,877,1474};
static const uint16_t SRL_AP[2] __attribute__((section(".data.fx_sloop")))={139,110};
static __attribute__((noinline,section(".audio_text"))) void fx_sloop_reverb_light_run(
    fx_state_t *restrict f,fx_sloop_reverb_light_t *restrict s,uint32_t n,int32_t target,
    int32_t *restrict l,int32_t *restrict r)
{
    if(!n)return;
    int32_t gain=17000+f->rate*104,a1=32767-f->character*200,mix=f->mix;
    int32_t keep=32768-a1,keep2=(keep*keep)>>15,damp=32768-((keep2*keep2)>>15);   /* (one pole at a quarter rate: 1-(1-a)^4) */
    if(damp>32767)damp=32767;
    int32_t ma=(3<<8)+((sine_i(s->phase)*3)>>7);
    s->phase+=LFO_INC[30]*n/CTL;
    int32_t mb=(3<<8)+((sine_i(s->phase)*3)>>7);
    uint32_t steps=(n+FX_SRL_D-1u)/FX_SRL_D;
    int32_t dm=(mb-ma)*256/(int32_t)steps,drift=ma*256;
    for(uint32_t i=0;i<n;i++){
        int32_t dry_l=l[i],dry_r=r[i];
        int32_t mono=fx_div_pow2(clamp(dry_l,-1048575,1048575)+clamp(dry_r,-1048575,1048575),1);
        /* the wet: stepped from the last tank output to the newest, a quarter a sample (one step late) */
        int32_t wl=s->cur_l,wr=s->cur_r;
        s->cur_l+=s->dl_l;s->cur_r+=s->dl_r;
        s->acc+=mono;
        if(++s->sub==FX_SRL_D){
            s->sub=0u;
            int32_t input=fx_tone_input(f,fx_div_pow2(s->acc,2),1.0f/(0.010f*(FS/FX_SRL_D)));
            s->acc=0;
            int32_t a=fx_div_pow2(clamp(input,-262143,262143),2u);
            a=fx_div_pow2(a*13000,15);
            uint32_t off=0;
            for(uint32_t k=0;k<2u;k++){
                int32_t b=s->ap[off+s->ap_pos[k]],v=a+(b>>1);
                s->ap[off+s->ap_pos[k]]=(int16_t)clamp(v,-32768,32767);
                a=b-(v>>1);
                if(++s->ap_pos[k]==SRL_AP[k])s->ap_pos[k]=0;
                off+=SRL_AP[k];
            }
            uint32_t tap=s->pos[0]+((uint32_t)drift>>16);
            if(tap>=SRL_LEN[0])tap-=SRL_LEN[0];
            uint32_t next=tap+1u==SRL_LEN[0]?0u:tap+1u;
            int32_t o0=s->line[tap];
            o0+=((s->line[next]-o0)*((drift>>8)&255))>>8;
            drift+=dm;
            int32_t o1=s->line[SRL_BASE[1]+s->pos[1]],o2=s->line[SRL_BASE[2]+s->pos[2]],o3=s->line[SRL_BASE[3]+s->pos[3]];
            int32_t s0=o0+o1,s1=o2+o3,d0=o0-o1,d1=o2-o3;
            int32_t v[4]={s0+s1,d0+d1,s0-s1,d0-d1};
            for(uint32_t k=0;k<4u;k++){
                int32_t fb=fx_div_pow2(v[k],1);
                s->low[k]+=fx_div_pow2(clamp(fb-s->low[k],-65535,65535)*damp,15);
                fb=fx_div_pow2(s->low[k]*gain,15);
                s->line[SRL_BASE[k]+s->pos[k]]=(int16_t)clamp(fb+((k&1u)?-a:a),-32768,32767);
                if(++s->pos[k]==SRL_LEN[k])s->pos[k]=0;
            }
            int32_t nl=clamp((o0+o2)*4,-262143,262143),nr=clamp((o1-o3)*4,-262143,262143);
            s->cur_l=s->wb_l;s->cur_r=s->wb_r;
            s->dl_l=fx_div_pow2(nl-s->wb_l,2);s->dl_r=fx_div_pow2(nr-s->wb_r,2);
            s->wb_l=nl;s->wb_r=nr;
        }
        if(mix==32768 && target==32768){l[i]=wl;r[i]=wr;continue;}   /* (all wet, as a shared slot at MIX 127: no dry) */
        if(mix!=target)mix+=clamp(target-mix,-128,128);
        l[i]=dry_l+fx_scale(wl-dry_l,(uint32_t)mix*2u);
        r[i]=dry_r+fx_scale(wr-dry_r,(uint32_t)mix*2u);
    }
    f->mix=mix;
}
