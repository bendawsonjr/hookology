/* SPDX-License-Identifier: GPL-3.0-only */
/* HERA chord sets (chord.c CH_HERA): the keys, voicings, every source of notes, and the stored project layout. */
#include <assert.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
_Static_assert(P_COUNT == 124u, "HERA adds no stored parameter: stock projects must load");
_Static_assert(P_CHSET < P_E0 && CH_MELODY == 11 && CH_HERA == 12, "stored IDs");
static track_t *setup(void)
{
    ui_power_on(); track_t *t=TSEL; set_engine_of(t,HOST_SYNTH);
    t->p[P_VOICE]=V_POLY; t->p[P_SCALE]=1; t->p[P_ROOT]=t->p[P_TRANS]=0; t->p[P_QUANT]=Q_OFF;
    t->p[P_CHRD]=CH_HERA; t->p[P_CHSET]=0; t->p[P_VOIC]=VC_CLOSE; t->p[P_STRUM]=0; t->p[P_AMODE]=0;
    events_block(CTL); return t;
}
static void same(const uint8_t *got,uint32_t n,uint32_t want_n,const uint8_t *want)
{
    if(n!=want_n||memcmp(got,want,n)){printf("got %u notes:",n);for(uint32_t i=0;i<n;i++)printf(" %u",got[i]);puts("");assert(0);}
}
static void table(void)
{
    track_t *t=setup(); uint8_t o[4]; int32_t r; uint16_t m;
    /* set 1 as HERA lists it: C, D, F; a key an octave up or down moves the chord an octave */
    same(o,chord_make(t,60,o,&r,&m),4,(uint8_t[]){48,55,62,64});
    same(o,chord_make(t,62,o,&r,&m),4,(uint8_t[]){50,57,60,65});
    same(o,chord_make(t,65,o,&r,&m),4,(uint8_t[]){41,57,64,67});
    same(o,chord_make(t,72,o,&r,&m),4,(uint8_t[]){60,67,74,76});
    same(o,chord_make(t,53,o,&r,&m),4,(uint8_t[]){29,45,52,55});
    t->p[P_CHSET]=1; same(o,chord_make(t,60,o,&r,&m),4,(uint8_t[]){48,52,59,62});
    /* TRN moves the chord, it does not pick another key's */
    t->p[P_CHSET]=0; t->p[P_TRANS]=2; same(o,chord_make(t,62,o,&r,&m),4,(uint8_t[]){50,57,64,66});
    t=setup();
    /* every chord of every set, every voicing: 1..4 notes, ascending, in range */
    for(uint32_t set=0;set<100;set++) for(uint32_t key=0;key<12;key++) for(uint32_t v=VC_CLOSE;v<=VC_BASS;v++) for(uint32_t oc=0;oc<3;oc++) {
        t->p[P_CHSET]=(int16_t)set; t->p[P_VOIC]=(int16_t)v;
        uint32_t n=chord_make(t,48+12*oc+key,o,&r,&m);
        assert(n>=1 && n<=CHORD_MAX);
        for(uint32_t i=1;i<n;i++) assert(o[i-1]<o[i]);
        if(v==VC_CLOSE) { uint32_t want=0; const uint8_t *c=&HERA_NOTES[(set*12+key)*4]; for(uint32_t i=0;i<4;i++) if(c[i]&&(!i||c[i]!=c[i-1])) want++; assert(n==want); }
    }
    /* a value outside 0..99 is still a set; the slot shows 1..100 */
    t=setup(); t->p[P_CHSET]=-1; assert(hera_set(t)==0); t->p[P_CHSET]=127; assert(hera_set(t)==99);
    { char v[8]; const char *u; param_format(&TP[P_CHSET],0,v,&u); assert(!strcmp(v,"1")); param_format(&TP[P_CHSET],99,v,&u); assert(!strcmp(v,"100")); }
    char b[HERA_NAME_MAX+1]; assert(hera_name(b,0,0) && !strcmp(b,"Cadd9") && !strcmp(hera_genre(0),"Pop"));
    assert(hera_name(b,0,11) && !strcmp(b,"G/B"));
    assert(hera_name(b,13,0) && !strcmp(b,"C oct") && hera_name(b,15,1) && !strcmp(b,"C# 5th"));   /* the stack sets */
    puts("HERA: 100 sets x 12 keys x 5 voicings x 3 octaves, TRN, names passed");
}
static void keys_ignore_the_scale(void)
{
    static const uint8_t q[]={Q_OFF,Q_SNAP,Q_WHITE,Q_ALL};
    for(uint32_t i=0;i<NELEM(q);i++) {
        track_t *t=setup(); t->p[P_QUANT]=q[i];
        for(uint32_t k=0;k<27;k++) assert(kb_map(t,k)==53u+k+12u*(uint32_t)input_octave(t));
    }
    puts("HERA: every key its own chord under every QNT passed");
}
static void sources(void)
{
    track_t *t=setup(); uint8_t started[4];
    key_down(7); assert(gates()==4); memcpy(started,kb_chord[7],4);
    t->p[P_CHSET]=1; assert(!memcmp(started,kb_chord[7],4)); key_up(7); assert(!gates());   /* browsing leaves nothing hanging */
    t=setup(); t->p[P_STRUM]=2; key_down(7); assert(gates()==1); seq_advance(beat_samples()); assert(gates()==4); key_up(7); assert(!gates());
    t=setup(); t->p[P_AMODE]=1; key_down(7); assert(t->nheld==4); key_up(7); assert(!t->nheld);
    t=setup(); t->p[P_VOICE]=V_MONO; key_down(7); assert(gates()==1); key_up(7); assert(!gates());
    t=setup(); song.rec=song.playing=1; key_down(7);
    assert(t->step[0].n==4 && !memcmp(t->step[0].note,(uint8_t[]){48,55,62,64},4)); key_up(7);
    assert(chord_last[0].hera==1 && chord_last[0].set==0);
    t=setup(); uint8_t o[4]; host_preset(t,ENGI_DRUM,0); t->p[P_CHRD]=CH_HERA; assert(chord_build(t,36,o)==1 && o[0]==36);
    t=setup(); t->p[P_SCALE]=31; t->p[P_QUANT]=Q_ALL; assert(chord_build(t,60,o)==1 && o[0]==60);
    puts("HERA: frozen ownership, strum, ARP, mono, recording, kits and microtonal bypass passed");
}
static void stored(void)
{
    track_t *t=setup(); t->p[P_CHSET]=42;
    static project_t before,after; static project_store_t wire;
    project_capture(&before); assert(proj_pack(&wire,&before) && proj_unpack(&after,wire.raw));
    assert(after.t[0].p[P_CHRD]==CH_HERA && after.t[0].p[P_CHSET]==42);
    assert(!project_save(0)); t->p[P_CHSET]=7; project_load(0); assert(TSEL->p[P_CHRD]==CH_HERA && TSEL->p[P_CHSET]==42);
    /* an older project: 0 in the slot, set 1 */
    t=setup(); assert(!project_save(1)); t->p[P_CHSET]=9; project_load(1); assert(TSEL->p[P_CHSET]==0 && hera_set(TSEL)==0);
    /* the slot's old motion events stay inert and readable; the other retired slots stay retired */
    assert(param_retired(P_CHSET) && !param_reserved(P_CHSET) && param_reserved(P_RESERVED45) && param_reserved(P_RESERVED33) && !param_reserved(P_VMAX) && !param_reserved(P_P5Q));
    assert(param_kept(P_CHSET));
    puts("HERA: set saved in the project, older projects read set 1, retired slots unchanged passed");
}
int main(void)
{
    table(); keys_ignore_the_scale(); sources(); stored();
    puts("HERA chord tests passed");
    return 0;
}
