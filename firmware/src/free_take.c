/* SPDX-License-Identifier: GPL-3.0-only */
/* REC MODE FREE (the menu), after SLOOP's free take (isod89, sloop-fm1): armed and stopped on a project with no notes,
 * the first note starts a take with no tempo and no grid. REC closes it: its length sets the loop (1, 2 or 4 bars, the
 * tempo nearest the one set; within 3 % of it that tempo stays), every track's current pattern takes the length, and the
 * notes land on QNT's grid (OFF: 1/16). The loop then plays and REC overdubs at once. PLAY drops the take; 24 s closes
 * it. Timed here, a block at a time, not by the UI, so the press is the loop's end. EDIT held undoes it: the track
 * through its recording undo, the tempo and the other tracks' lengths through ft_undo. Included by seq.c */
#define FT_MAX 192u
#define FT_BLOCKS (24u * FS / CTL)                      /* 24 s: 4 bars at 40 BPM */
#define FT_OPEN 0xFFFFu                                 /* the key is still down */
#define FT_KEEP 1030u                                   /* within 3 % the tempo set stays (a ratio x 1000) */
enum { FT_NONE, FT_CLOSED, FT_DROPPED, FT_NO_LOOP, FT_FULL };
typedef struct { uint16_t t, d; uint8_t note, vel; } ft_ev_t;   /* in blocks from the first note */
static ft_ev_t ft_ev[FT_MAX] __attribute__((section(".pool")));
static uint32_t ft_n, ft_t, ft_rec_mask, ft_play_mask, ft_prev;
static uint8_t ft_trk, ft_said_full;
static volatile uint8_t ft_ready;                       /* the UI: armed on a project with no notes */
static volatile uint8_t ft_msg, ft_bars;                /* what the take ended in, for the UI */
static volatile uint8_t ft_rec_eat, ft_play_eat;        /* a press the take took: not the UI's */

/* the tempo x 10 that makes T blocks last `bars` of 4 */
static uint32_t ft_bpm10(uint32_t T, uint32_t bars)
{
    return 2400u * bars * (uint32_t)FS / (T * CTL);
}
/* the bars T blocks fit, the tempo nearest the one set, *bpm its tempo; 0: none in 40..240 */
static uint32_t ft_fit(uint32_t T, uint32_t *bpm)
{
    static const uint8_t BARS[3] = {1, 2, 4};
    uint32_t best = 0, cur = (uint32_t)song.g[G_BPM] * 10u, err = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < 3u; i++) {
        uint32_t b = ft_bpm10(T, BARS[i]), e;
        if (b < 400u || b > 2400u) continue;
        e = b > cur ? b * 1000u / cur : cur * 1000u / b;
        if (e < err) {
            err = e;
            best = BARS[i];
            *bpm = e <= FT_KEEP ? cur / 10u : (b + 5u) / 10u;
        }
    }
    return best;
}

static void ft_start(track_t *t)
{
    ft_on = 1;
    ft_trk = (uint8_t)trk_index(t);
    ft_t = ft_n = 0;
    ft_said_full = 0;
    ft_undo.valid = 0;
}
static void ft_note_on(const track_t *t, uint32_t note, uint32_t vel)
{
    if (!ft_on || trk_index(t) != ft_trk) return;
    if (ft_n >= FT_MAX) {
        if (!ft_said_full) { ft_said_full = 1; ft_msg = FT_FULL; }
        return;
    }
    ft_ev[ft_n++] = (ft_ev_t){(uint16_t)ft_t, FT_OPEN, (uint8_t)note, (uint8_t)vel};
}
static void ft_note_off(const track_t *t, uint32_t note)
{
    if (!ft_on || trk_index(t) != ft_trk) return;
    for (uint32_t i = ft_n; i--;)
        if (ft_ev[i].note == note && ft_ev[i].d == FT_OPEN) {
            ft_ev[i].d = (uint16_t)(ft_t - ft_ev[i].t);
            return;
        }
}

/* QNT's grid in RECORD_UNIT of a 1/16 step (N_RECQ order; OFF: 1/16) */
static uint32_t ft_grid(const track_t *t)
{
    static const uint32_t GRID[11] = {RECORD_UNIT, 4u * RECORD_UNIT, 2u * RECORD_UNIT, RECORD_UNIT, RECORD_UNIT / 2u,
                                      4u * RECORD_UNIT / 3u, 2u * RECORD_UNIT / 3u, 8u * RECORD_UNIT, 16u * RECORD_UNIT,
                                      32u * RECORD_UNIT, 64u * RECORD_UNIT};
    return GRID[(uint32_t)clamp(t->p[P_RECQ], 0, 10)];
}
/* b blocks of a take of T, in RECORD_UNIT of a step of a loop of len: in two parts, 32 bits (no 64-bit division here) */
static uint32_t ft_units(uint32_t b, uint32_t T, uint32_t len)
{
    uint32_t x = b * len;
    return x / T * RECORD_UNIT + ((x % T) * RECORD_UNIT + T / 2u) / T;
}
/* a take's event -> a recorded note of track t, on the grid, in a loop of len steps T blocks long */
static void ft_place(track_t *t, const ft_ev_t *e, uint32_t T, uint32_t len, uint32_t grid, uint64_t *touched, uint32_t *slot)
{
    uint32_t loop = len * RECORD_UNIT, at = (ft_units(e->t, T, len) + grid / 2u) / grid * grid;
    uint32_t d = e->d == FT_OPEN || (uint32_t)e->t + e->d > T ? T - e->t : e->d, exp = 0;
    uint32_t dur = ft_units(d, T, len);
    uint32_t actual, on, view, j = *slot;
    recorded_note_t r;
    if (at >= loop) at = 0;
    actual = at / RECORD_UNIT;
    on = at % RECORD_UNIT;
    view = on >= RECORD_UNIT / 2u ? (actual + 1u) % len : actual;
    if (!dur) dur = 1;
    while (dur > 65535u && exp < 7u) { dur >>= 1; exp++; }
    memset(&r, 0, sizeof r);
    r.on = (uint16_t)on;
    recorded_set_note(&r, e->note);
    r.vel = e->vel ? e->vel : 1u;
    r.duration = (uint16_t)(dur > 65535u ? 65535u : dur);
    recorded_set_owner(&r, recording_owner(t));
    recorded_set_exponent(&r, exp);
    recorded_set_step(&r, actual, view != actual, !view && actual);
    recorded_set_gate(&r, drum_track(t));
    for (; j < RECORD_MAX && recording[j].vel && (recording_present(&recording[j]) || recording_sounding(j)); j++)
        ;
    *slot = j;
    if (j == RECORD_MAX) { recording_full = 1; return; }
    if (recording[j].vel) recording_unlink(j);
    recording_restore_note(t, j, r);
    t->step[view].time = ST_NOTE;
    t->step[view].flags |= SF_RECORDED;
    *touched |= 1ull << view;
}

/* a step's overview from the recorded notes on it (as seq_notes.c notes_rebuild; a take's steps hold notes) */
static void ft_overview(track_t *t, uint32_t view)
{
    step_t *st = &t->step[view];
    st->n = st->hit = st->acc = st->vel = 0;
    for (uint32_t i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) {
        const recorded_note_t *r = &recording[i];
        uint32_t note = recorded_note(r), lane = drum_lane(note), j;
        if (!recording_active(t, i) || recording_view(t, r) != view) continue;
        if (r->vel > st->vel) st->vel = r->vel;
        if (drum_track(t) && note == DRUM_LANE_NOTE[lane]) {
            st->hit |= (uint8_t)(1u << lane);
            if (r->vel > 110u) st->acc |= (uint8_t)(1u << lane);
        } else {
            for (j = 0; j < st->n && st->note[j] != note; j++);
            if (j == st->n && st->n < 4u) st->note[st->n++] = (uint8_t)note;
        }
    }
    if (st->vel > 110u) st->flags |= SF_ACCENT;
}

static void ft_close(void)
{
    track_t *t = &trk[ft_trk % NTRK];
    uint32_t T = ft_t, bars, bpm = 0, len, grid, slot = 0, k;
    uint64_t touched = 0;
    ft_on = 0;
    bars = ft_n && T >= FS / CTL / 2u ? ft_fit(T, &bpm) : 0u;
    if (!bars) {
        ft_msg = FT_NO_LOOP;
        return;
    }
    len = 16u * bars;
    ft_undo.bpm = song.g[G_BPM];
    ft_undo.trk = ft_trk;
    for (k = 0; k < NTRK; k++) {
        ft_undo.slen[k] = trk[k].p[P_SLEN];
        ft_undo.sdiv[k] = trk[k].p[P_SDIV];
    }
    recording_undo_capture(t);
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = (int16_t)len;
        trk[k].p[P_SDIV] = FT_DIV;
    }
    song.g[G_BPM] = (int16_t)bpm;
    grid = ft_grid(t);
    for (k = 0; k < ft_n; k++)
        ft_place(t, &ft_ev[k], T, len, grid, &touched, &slot);
    for (k = 0; k < NSTEP; k++)
        if ((touched >> k) & 1u) ft_overview(t, k);
    t->seq_active = 1;
    rec_disarm(t);
    ft_undo.valid = recording_undo_state() != 0;
    ft_bars = (uint8_t)bars;
    ft_msg = FT_CLOSED;
    if (midi_clock_local_start(fm1_ms)) midi_clock_transport(0xFAu, fm1_ms);   /* this is the downbeat */
    else seq_start();
}

/* a take, once per block: REC closes it, PLAY drops it */
static void ft_block(void)
{
    uint32_t b = fm1_in.buttons & (ft_rec_mask | ft_play_mask), press = b & ~ft_prev;
    ft_prev = b;
    if (!ft_on) return;
    ft_t++;
    if ((press & ft_play_mask) || song.playing || chain.running) {
        ft_on = 0;
        ft_play_eat = (press & ft_play_mask) != 0u;
        ft_msg = FT_DROPPED;
    } else if (press & ft_rec_mask) {
        ft_rec_eat = 1;
        ft_close();
    } else if (ft_t >= FT_BLOCKS) {
        ft_close();
    }
}

/* the take's tempo and the other tracks' lengths swap with their copies (EDIT held, EDIT + OCT+) */
static void ft_undo_swap(void)
{
    if (!ft_undo.valid) return;
    int16_t x = song.g[G_BPM];
    song.g[G_BPM] = ft_undo.bpm;
    ft_undo.bpm = x;
    for (uint32_t k = 0; k < NTRK; k++) {
        if (k == ft_undo.trk) continue;
        x = trk[k].p[P_SLEN]; trk[k].p[P_SLEN] = ft_undo.slen[k]; ft_undo.slen[k] = x;
        x = trk[k].p[P_SDIV]; trk[k].p[P_SDIV] = ft_undo.sdiv[k]; ft_undo.sdiv[k] = x;
    }
}
