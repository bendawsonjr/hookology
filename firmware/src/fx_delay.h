/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, v1.1-beta48) DELAY: the shared audio delay, one in the rack (fx_single). Mono in (the pair's / four
 * samples' average), a line at half the rate (ECONOMY: a quarter), stereo out: the left echo at TIME, the right 12 ms
 * later. TIME is a beat division (N_DTIME) at the song's tempo (beat_samples: MIDI clock too), halved while longer
 * than the line (1 s); a change glides (a tape's pitch bend, no click). Each repeat goes back through TONE (fx_tone_input:
 * the slot's low and high cut), so the echoes darken (or thin) as they go; FDBK (CHAR) 0..127 is 0..85 % feedback, the
 * line int16 and clamped: it never runs away. MIX 127 in a shared slot: all wet, no dry work. Included by fx.c. */
#define FX_DLY_SPREAD_MS 12u
static const uint8_t FX_DTIME_12[8]={3,4,6,9,8,12,18,24};   /* N_DTIME in twelfths of a beat */
typedef struct {
    uint32_t w,sub,len,spread;
    int32_t d_q8,acc;
    int32_t wb_l,wb_r,cur_l,cur_r,dl_l,dl_r;
    int16_t line[];
} fx_delay_t;
static uint32_t fx_delay_div(uint32_t light){return light?4u:2u;}
static uint32_t fx_delay_spread(uint32_t light){return (uint32_t)FS/fx_delay_div(light)*FX_DLY_SPREAD_MS/1000u;}
static uint32_t fx_delay_len(uint32_t light){return (uint32_t)FS/fx_delay_div(light)+fx_delay_spread(light)+4u;}
static uint32_t fx_delay_bytes(uint32_t light){return (uint32_t)sizeof(fx_delay_t)+fx_delay_len(light)*2u;}
static uint32_t beat_samples(void);                      /* (mix.c) */
/* the delay TIME stands for, in line samples (Q8): halved while it would not fit (a slow tempo's 1/2: a 1/4) */
static int32_t fx_delay_target(uint32_t time,uint32_t light)
{
    uint32_t div=fx_delay_div(light),max=(fx_delay_len(light)-fx_delay_spread(light)-3u)<<8;
    uint32_t q8=beat_samples()*FX_DTIME_12[time<8u?time:7u]/12u*256u/div;
    while(q8>max)q8>>=1;
    return (int32_t)(q8<512u?512u:q8);
}
static inline __attribute__((always_inline)) int32_t fx_delay_read(const fx_delay_t *s,int32_t d_q8)
{
    uint32_t di=(uint32_t)d_q8>>8,len=s->len,a=s->w+len-di,b=a+len-1u;   /* d steps ago, and one more */
    if(a>=len)a-=len;
    if(b>=len)b-=len;if(b>=len)b-=len;
    int32_t x=s->line[a],y=s->line[b];
    return x+(((y-x)*(d_q8&255))>>8);
}
static __attribute__((noinline,section(".audio_text"))) void fx_delay_run(fx_state_t *restrict f,fx_delay_t *restrict s,uint32_t n,
    int32_t target,int32_t *restrict l,int32_t *restrict r)
{
    if(!n)return;
    uint32_t light=f->light,div=fx_delay_div(light),shift=light?2u:1u;
    if(!s->len){s->len=fx_delay_len(light);s->spread=fx_delay_spread(light)<<8;}
    int32_t want=f->rev_l,fb=f->character*220,mix=f->mix;   /* (rev_l: the target, fx_slot_prepare) */
    if(!s->d_q8)s->d_q8=want;
    float slew=1.0f/(0.010f*(float)(FS/div));
    for(uint32_t i=0;i<n;i++){
        int32_t dry_l=l[i],dry_r=r[i];
        int32_t wl=s->cur_l,wr=s->cur_r;
        s->cur_l+=s->dl_l;s->cur_r+=s->dl_r;
        s->acc+=fx_div_pow2(clamp(dry_l,-1048575,1048575)+clamp(dry_r,-1048575,1048575),1);
        if(++s->sub==div){
            s->sub=0u;
            int32_t in=fx_div_pow2(s->acc,shift);s->acc=0;
            int32_t dd=want-s->d_q8;                       /* the time glides: a 512th of the way a step, at least 1/256 */
            s->d_q8+=dd>0?(dd>>9)+1:dd<0?-((-dd)>>9)-1:0;
            int32_t tl=fx_delay_read(s,s->d_q8),tr=fx_delay_read(s,s->d_q8+(int32_t)s->spread);
            int32_t x=fx_div_pow2(clamp(in,-262143,262143),3)+fx_div_pow2(tl*fb,15);
            x=fx_tone_input(f,x,slew);                     /* (each repeat through TONE again) */
            s->line[s->w]=(int16_t)clamp(x,-32767,32767);
            if(++s->w==s->len)s->w=0;
            int32_t nl=tl*8,nr=tr*8;
            s->cur_l=s->wb_l;s->cur_r=s->wb_r;
            s->dl_l=(nl-s->wb_l)>>shift;s->dl_r=(nr-s->wb_r)>>shift;
            s->wb_l=nl;s->wb_r=nr;
        }
        if(mix==32768 && target==32768){l[i]=wl;r[i]=wr;continue;}   /* (all wet: no dry) */
        if(mix!=target)mix+=clamp(target-mix,-128,128);
        l[i]=dry_l+fx_scale(wl-dry_l,(uint32_t)mix*2u);
        r[i]=dry_r+fx_scale(wr-dry_r,(uint32_t)mix*2u);
    }
    f->mix=mix;
}
