/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, beta49) INSERT: a track's own audio effect (fx.c fx_insert, mix.c mix_insert_samples). The track's list
 * (OFF, DRIVE, CRUSH, FORMANT, TAPE, SPIT, NOTE ECHO, NOTE TAIL), an insert in each slot chained 1, 2, 3 (beta50),
 * the effect heard, OFF and MIX 0 running nothing and holding no memory, the insert before LEVEL / MUTE / the sends, its voice-budget units, ROOM off the shared list. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
static int32_t out[CTL*2];
/* track 1 an FM6 chord, the others silent; blocks of the real mixer -> the energy of the output */
static void voice(void)
{
    ui_power_on();fx_clear_all();
    for(uint32_t id=0;id<RES_COUNT;id++)if(id<RES_NOTE_HISTORY || (id>=RES_FX0 && id<RES_RECORD_UNDO))resource_release(id);
    memset(fx_state,0,sizeof fx_state);
    host_tracks_init();for(uint32_t k=0;k<NROWS;k++)host_preset(&trk[k],HOST_SYNTH,0);trk[0].p[P_SUS]=127;
    dith_r=DITH_SEED;trk_note_on(&trk[0],48,100);trk_note_on(&trk[0],55,100);
}
static uint64_t blocks(uint32_t n,int32_t *keep)
{
    uint64_t e=0;
    for(uint32_t k=0;k<n;k++){
        mix_block(out,CTL);
        for(uint32_t i=0;i<CTL*2u;i++){e+=(uint64_t)(out[i]<0?-out[i]:out[i]);if(keep)keep[k*CTL*2u+i]=out[i];}
    }
    return e;
}
static int insert_memory(uint32_t p)   /* track p's slots hold any */
{
    int any=0;for(uint32_t k=p*FX_SLOTS;k<(p+1u)*FX_SLOTS;k++)any|=resource[RES_FXS0+k].size || resource[RES_FXI0+k].size;
    return any;
}
int main(void)
{
    {   /* the list: OFF, the INSERTs (Dirt, Colour), the note effects */
        ui_power_on();const param_desc_t *d=&TP[P_FXTYPE];
        static const int16_t order[]={FX_OFF,FX_SOFT,FX_CRUSH,FX_FORMANT,FX_TAPE,FX_SPIT,FX_EQ,FX_GATE,FX_STUTTER,FX_MIDIDELAY,FX_MIDIVERB};   /* (EQ: beta69) */
        assert(enum_rows(d)==(int32_t)NELEM(order));
        for(uint32_t i=0;i<NELEM(order);i++)assert(enum_value(d,(int32_t)i)==order[i] && enum_rank(d,order[i])==(int32_t)i);
        for(int32_t t=0;t<FX_COUNT;t++)assert(fx_track_type(t)==(fx_is_midi(t) || (t>=FX_SOFT && t<=FX_CRUSH) || t==FX_FORMANT || t==FX_TAPE || t==FX_SPIT || t==FX_GATE || t==FX_STUTTER || t==FX_EQ?t:FX_OFF));
        for(int32_t i=0;i<enum_rows(&GP[G_MFXTYPE]);i++)assert(!fx_slicer_type(enum_value(&GP[G_MFXTYPE],i)));   /* (the SLICER: a track's only) */
        song.g[G_MFX2TYPE]=FX_GATE;shared_fx_normalize(song.g);assert(song.g[G_MFX2TYPE]==FX_OFF);
        /* ROOM is off the shared list; a song that has it still names it */
        for(int32_t i=0;i<enum_rows(&GP[G_MFXTYPE]);i++)assert(enum_value(&GP[G_MFXTYPE],i)!=FX_ROOM);
        assert(!strcmp(N_AUDIO_FX[FX_ROOM],"ROOM"));
    }
    {   /* an INSERT in each slot: KNOB 2 and the list reach them in every slot; a save keeps all three */
        ui_power_on();go_title("FX");
        TSEL->p[P_FXTYPE]=FX_TAPE;TSEL->p[P_FXMIX]=100;
        fx_rack_row[0]=1;edit_param(0,1);assert(TSEL->p[P_FX2TYPE]==FX_SOFT && TSEL->p[P_FX2MIX]==127);   /* (all in) */
        edit_param(0,1);assert(TSEL->p[P_FX2TYPE]==FX_CRUSH && TSEL->p[P_FXTYPE]==FX_TAPE);
        TSEL->p[P_FX3TYPE]=FX_OFF;TSEL->p[P_FX3MIX]=0;fx_rack_row[0]=2;
        vlist_open(ui.page,0);assert(vl.sel==FX_SOFT);turn(EN_K1,1);assert(vl.sel==FX_FORMANT);
        press(B_OCTUP);assert(!pop.on && TSEL->p[P_FX3TYPE]==FX_FORMANT && TSEL->p[P_FX2TYPE]==FX_CRUSH && TSEL->p[P_FXTYPE]==FX_TAPE);
        project_t q;project_store_t w;project_capture(&q);assert(proj_pack(&w,&q) && proj_import(&proj_scratch,&w,sizeof w));
        assert(proj_scratch.t[0].p[P_FXTYPE]==FX_TAPE && proj_scratch.t[0].p[P_FX2TYPE]==FX_CRUSH && proj_scratch.t[0].p[P_FX3TYPE]==FX_FORMANT);
    }
    static int32_t dry[64*CTL*2],wet[64*CTL*2];
    {   /* OFF and MIX 0: the dry mixer's path untouched (no insert run), no memory; CRUSH heard on a test tone */
        voice();blocks(16,0);assert(!fx_insert_active(&trk[0]) && !insert_memory(0));
        trk[0].p[P_FX2TYPE]=FX_CRUSH;trk[0].p[P_FX2MIX]=0;blocks(16,0);
        assert(!fx_insert_active(&trk[0]) && !insert_memory(0));
        trk[0].p[P_FX2MIX]=127;trk[0].p[P_FX2RATE]=10;trk[0].p[P_FX2CHAR]=20;spread_on=0;
        int32_t tone[CTL];double diff=0,sum=0;
        for(uint32_t k=0;k<64;k++){
            for(uint32_t i=0;i<CTL;i++)tone[i]=(int32_t)(sin((k*CTL+i)*2.0*M_PI*220.0/FS)*60000.0);
            assert(fx_insert(&trk[0],tone,CTL));
            if(k>=8)for(uint32_t i=0;i<CTL;i++){diff+=fabs((double)fx_left[i]-tone[i]);sum+=fabs((double)tone[i]);assert(fx_left[i]==fx_right[i]);}
        }
        assert(diff>sum*0.05 && resource[RES_FXS0+1].size && resource[RES_FXI0+1].size && !resource[RES_FXS0].size);
        trk[0].p[P_FX2MIX]=0;blocks(40,0);assert(!insert_memory(0));                   /* (MIX 0: faded, freed) */
        trk[0].p[P_FX2MIX]=127;blocks(4,0);assert(insert_memory(0));
        trk[0].p[P_FX2TYPE]=FX_OFF;blocks(40,0);assert(!insert_memory(0));              /* (OFF: freed) */
        printf("INSERT: CRUSH changes a 220 Hz tone by %.0f %%; OFF and MIX 0 run nothing, hold no memory\n",100.0*diff/sum);
    }
    for(int32_t type=FX_OFF;type<FX_COUNT;type++){   /* every INSERT: sounds, bounded, its memory freed */
        if(!fx_insert_type(type))continue;
        voice();trk[0].p[P_FXTYPE]=(int16_t)type;trk[0].p[P_FXMIX]=127;
        uint64_t e=blocks(64,wet);assert(e>0);
        for(uint32_t i=0;i<NELEM(wet);i++)assert(wet[i]>=-32767 && wet[i]<=32767);
        trk[0].p[P_FXTYPE]=FX_OFF;blocks(40,0);assert(!insert_memory(0));
    }
    {   /* before LEVEL, MUTE and the sends (the track's own peak: the master's DC blocker carries the last test's tail) */
        voice();trk[0].p[P_FXTYPE]=FX_SOFT;trk[0].p[P_FXMIX]=127;blocks(8,0);assert(trk[0].peak>1000);
        trk[0].p[P_LEVEL]=0;blocks(8,0);trk[0].peak=0;blocks(16,0);assert(!trk[0].peak);         /* (LEVEL 0: silent) */
        voice();trk[0].p[P_FXTYPE]=FX_SOFT;trk[0].p[P_FXMIX]=127;trk[0].p[P_MUTE]=1;blocks(8,0);
        trk[0].peak=0;blocks(16,0);assert(!trk[0].peak);                                          /* (MUTE: silent) */
        voice();trk[0].p[P_FXTYPE]=FX_SOFT;trk[0].p[P_FXMIX]=127;trk[0].p[P_FXSEND]=127;song.g[G_MFXTYPE]=FX_REVERB;song.g[G_MFXMIX]=127;
        blocks(8,0);uint64_t s=0;for(uint32_t i=0;i<CTL;i++)s+=(uint64_t)abs(fx_send_l[i]);assert(s>0);   /* (the send gets it) */
        song.g[G_MFXTYPE]=FX_OFF;
        voice();trk[0].p[P_FXTYPE]=FX_TAPE;trk[0].p[P_FXMIX]=127;trk[0].p[P_PAN]=-64;spread_on=0;   /* (PAN after the effect) */
        int32_t tone[CTL];uint64_t l=0;
        for(uint32_t k=0;k<16;k++){
            for(uint32_t i=0;i<CTL;i++)tone[i]=(int32_t)(sin((k*CTL+i)*2.0*M_PI*220.0/FS)*60000.0);
            assert(fx_insert(&trk[0],tone,CTL));for(uint32_t i=0;i<CTL;i++){l+=(uint64_t)abs(fx_left[i]);assert(!fx_right[i]);}
        }
        assert(l>0);
    }
    {   /* a chain: DRIVE then CRUSH, each slot its own state; one taken out, the other still runs */
        voice();trk[0].p[P_FXTYPE]=FX_HARD;trk[0].p[P_FXMIX]=127;trk[0].p[P_FX3TYPE]=FX_CRUSH;trk[0].p[P_FX3MIX]=127;
        trk[0].p[P_FX2TYPE]=FX_MIDIDELAY;trk[0].p[P_FX2MIX]=64;blocks(16,0);
        assert(resource[RES_FXS0].size && !resource[RES_FXS0+1].size && resource[RES_FXS0+2].size && trk[0].peak>1000);
        int32_t tone[CTL];uint64_t one=0,two=0;spread_on=0;
        for(uint32_t k=0;k<32;k++){for(uint32_t i=0;i<CTL;i++)tone[i]=(int32_t)(sin((k*CTL+i)*2.0*M_PI*220.0/FS)*60000.0);
            assert(fx_insert(&trk[0],tone,CTL));if(k>=8)for(uint32_t i=0;i<CTL;i++)two+=(uint64_t)abs(fx_left[i]-tone[i]);}
        trk[0].p[P_FX3TYPE]=FX_OFF;blocks(40,0);assert(resource[RES_FXS0].size && !resource[RES_FXS0+2].size);
        for(uint32_t k=0;k<32;k++){for(uint32_t i=0;i<CTL;i++)tone[i]=(int32_t)(sin((k*CTL+i)*2.0*M_PI*220.0/FS)*60000.0);
            assert(fx_insert(&trk[0],tone,CTL));if(k>=8)for(uint32_t i=0;i<CTL;i++)one+=(uint64_t)abs(fx_left[i]-tone[i]);}
        assert(one>0 && two>0 && one!=two);
    }
    {   /* SLICER GATE: pattern 7 (x...x...) at 120 BPM 1/16, stopped (its own clock from 0): steps 0, 4, 8, 12 open,
         * the rest closed; DEPTH 50 % halves them; the knobs' names; the chosen defaults */
        static const uint32_t STEP=5513u;                                    /* (1/16 at 120 BPM: 5512.5 samples) */
        for(uint32_t depth=0;depth<2;depth++){
            voice();song.g[G_BPM]=120;spread_on=0;
            trk[0].p[P_FXTYPE]=FX_GATE;trk[0].p[P_FXMIX]=depth?64:127;trk[0].p[P_FXRATE]=1;trk[0].p[P_FXCHAR]=6;
            double e[16]={0};int32_t tone[CTL];uint32_t at=0;
            while(at<16u*STEP){
                for(uint32_t i=0;i<CTL;i++)tone[i]=(int32_t)(sin((at+i)*2.0*M_PI*440.0/FS)*60000.0);
                assert(fx_insert(&trk[0],tone,CTL));
                for(uint32_t i=0;i<CTL;i++){uint32_t k=(at+i)/STEP,ph=(at+i)%STEP;if(k<16 && ph>300 && ph<STEP-300)e[k]+=fabs((double)fx_left[i]);}
                at+=CTL;
            }
            for(uint32_t k=1;k<16;k++){                           /* (step 0: the MIX fade in from the dry) */
                double rel=e[k]/e[4];
                if(k%4u==0)assert(rel>0.97 && rel<1.03);
                else if(depth)assert(rel>0.45 && rel<0.55);
                else assert(rel<0.01);
            }
        }
        ui_power_on();
        TSEL->p[P_FXTYPE]=FX_STUTTER;
        assert(!strcmp(track_desc(TSEL,P_FXMIX)->label,"DEPTH") && !strcmp(track_desc(TSEL,P_FXRATE)->label,"RATE"));
        {const param_desc_t *d=track_desc(TSEL,P_FXCHAR);assert(!strcmp(d->label,"PAT") && d->max==15 && !strcmp(d->names[6],"7"));}
        assert(!strcmp(track_desc(TSEL,P_FXRATE)->names[1],"1/16"));
        TSEL->p[P_FXTYPE]=FX_OFF;TSEL->p[P_FXRATE]=60;TSEL->p[P_FXCHAR]=64;go_title("FX");fx_rack_row[0]=0;
        vlist_open(ui.page,0);turn(EN_K1,1);turn(EN_K1,1);assert(vl.sel==FX_GATE);press(B_OCTUP);
        assert(TSEL->p[P_FXTYPE]==FX_GATE && TSEL->p[P_FXMIX]==127 && TSEL->p[P_FXRATE]==1 && TSEL->p[P_FXCHAR]==0);
    }
    {   /* STUTTER: the sound only on the open steps; a closed step repeats the last open one (GATE: silent there) */
        static const uint32_t STEP=5513u;
        double closed[2]={0};
        for(uint32_t stut=0;stut<2;stut++){
            voice();song.g[G_BPM]=120;spread_on=0;
            trk[0].p[P_FXTYPE]=(int16_t)(stut?FX_STUTTER:FX_GATE);trk[0].p[P_FXMIX]=127;trk[0].p[P_FXRATE]=1;trk[0].p[P_FXCHAR]=6;
            int32_t tone[CTL];uint32_t at=0;
            while(at<16u*STEP){
                for(uint32_t i=0;i<CTL;i++){uint32_t k=(at+i)/STEP;tone[i]=k%4u?0:(int32_t)(sin((at+i)*2.0*M_PI*440.0/FS)*60000.0);}
                assert(fx_insert(&trk[0],tone,CTL));
                for(uint32_t i=0;i<CTL;i++){uint32_t k=(at+i)/STEP,ph=(at+i)%STEP;if(k>=4 && k<16 && k%4u && ph>300 && ph<STEP-300)closed[stut]+=fabs((double)fx_left[i]);}
                at+=CTL;
            }
            assert(resource[RES_FXI0].size==fx_slicer_bytes(stut?FX_STUTTER:FX_GATE));
        }
        assert(closed[0]<1.0 && closed[1]>1e6);
        printf("SLICER: GATE closed steps %.0f, STUTTER repeats in them %.0f; STUTTER holds %u bytes\n",closed[0],closed[1],fx_slicer_bytes(FX_STUTTER));
    }
    {   /* playing: the track's song clock (step 0 at PLAY); its swing moves the odd steps */
        voice();song.g[G_BPM]=120;song.playing=1;trk[0].seq_pos=0;spread_on=0;
        trk[0].p[P_FXTYPE]=FX_GATE;trk[0].p[P_FXMIX]=127;trk[0].p[P_FXRATE]=1;trk[0].p[P_FXCHAR]=0;
        int32_t tone[CTL]={0};
        trk[0].seq_clock=6144u*5u+2000u;assert(fx_insert(&trk[0],tone,CTL));assert(fx_slicer_now(&trk[0],0)==5);
        trk[0].seq_clock=6144u*16u*3u+6144u*11u+100u;assert(fx_insert(&trk[0],tone,CTL));assert(fx_slicer_now(&trk[0],0)==11);
        trk[0].p[P_SSWING]=50;uint32_t a=swing_step_len(&trk[0],6144u,0);assert(a>6144u);
        trk[0].seq_clock=a-100u+35u;assert(fx_insert(&trk[0],tone,CTL));assert(fx_slicer_now(&trk[0],0)==0);   /* (the long even step) */
        trk[0].seq_clock=a+60u;assert(fx_insert(&trk[0],tone,CTL));assert(fx_slicer_now(&trk[0],0)==1);
        song.playing=0;trk[0].p[P_SSWING]=0;
    }
    {   /* the voice budget: each INSERT takes 5 units, at MIX 0 none; a note effect none */
        ui_power_on();uint32_t base=voice_fx_units();
        trk[2].p[P_FX3TYPE]=FX_FUZZ;trk[2].p[P_FX3MIX]=90;assert(voice_fx_units()==base+5u);
        trk[2].p[P_FXTYPE]=FX_TAPE;trk[2].p[P_FXMIX]=40;assert(voice_fx_units()==base+10u);
        trk[2].p[P_FX2TYPE]=FX_MIDIVERB;trk[2].p[P_FX2MIX]=90;assert(voice_fx_units()==base+10u);
        trk[2].p[P_FX3MIX]=0;trk[2].p[P_FXMIX]=0;assert(voice_fx_units()==base);
    }
    puts("INSERT: the track's list, one a slot chained, OFF / MIX 0 free, every insert bounded, LEVEL / MUTE / sends / PAN after it, its budget: OK");
    return 0;
}
