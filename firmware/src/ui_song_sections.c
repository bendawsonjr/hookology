/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Hookology contributors */
/* SEQ > SECTIONS (Hookology): build the song from whole sections, after SLOOP's sections by isod89 (no SLOOP code).
 * Section A..H is pattern 1..8 on every track in use (sections.c). On this page, with SEQ not held:
 *   the white keys F3 .. F4 are A .. H: a tap plays that section (playing: every track at track 1's loop end;
 *   stopped: at once); held a second, what plays now is copied into it (over a section with notes: the question
 *   first) and it plays next;
 *   REC records the song: from the next loop of track 1 every section played, and how many times, is logged (the
 *   jam, seq.c); REC again or STOP makes it the song (over a song: the question before it starts);
 *   KNOB 1 the song entry, 2 its section, 3 its repeats, 4 the song's length; PLAY plays the song from the entry
 *   (no song: the loop); EDIT held undoes the song edits, as on SONG.
 * The song is SONG's own (64 rows): an entry where every track plays one section shows its letter, else '*'; SONG
 * stays for rests, single tracks, presets and punch FX. Included by ui_draw.c, after ui_patterns.c */

#define SEC_HOLD_MS 1000u
static const uint8_t SEC_KEY[SEC_N] = {0, 2, 4, 6, 7, 9, 11, 12};   /* F3 .. F4, the first eight white keys */
static struct {
    uint8_t key, did;                                   /* the section key held (+1), its hold already acted */
    uint32_t ms;                                        /* when it went down */
    uint32_t sig;                                       /* what the music mask was computed from */
    uint8_t used[SEC_N];                                /* section b: the tracks in use whose pattern b has notes */
    uint32_t tile[SEC_N], song, foot;                   /* what each part last drew */
} scs;
static void jam_take(void);                             /* ui_input.c */
static int sec_pop_on(void);                            /* ui_input.c: a popup open (ui_popup.c's) */

static int sections_page(void) { return !ui.home && cur_page()->graph == GR_SECTIONS; }
/* the section keys act: on SECTIONS, no SEQ held (its pattern keys), no dialog, menu, layer or popup */
static int sections_keys_on(void)
{
    return sections_page() && !ui.menu && !ui.confirm && !name_on() && !ui.ly && !ui.uboot && !sec_pop_on() &&
        !(fm1_in.buttons & (1u << panel.btn[B_SEQ]));
}
/* which tracks hold notes in each section, again when anything a pattern holds may have changed */
static void sec_sync(void)
{
    uint32_t h = 2166136261u, k, b, n = trk_n();
    for (k = 0; k < NTRK; k++)
        h = fnv1(fnv1(fnv1(h, trk[k].pattern_gen), trk[k].pattern), trk[k].pattern_next);
    h = fnv1(fnv1(fnv1(fnv1(h, pattern_pool_used), pattern_epoch), motion.count), n + 64u * recording_generation);
    if (h == scs.sig && scs.sig) return;
    scs.sig = h;
    for (b = 0; b < SEC_N; b++) {
        scs.used[b] = 0;
        for (k = 0; k < n; k++)
            if (!pattern_empty(k, b)) scs.used[b] |= (uint8_t)(1u << k);
    }
}
/* the section track k waits for (SEC_NONE: none) */
static uint32_t sec_queued(void)
{
    uint32_t k, n = trk_n(), b;
    if (secs.next < SEC_N) return secs.next;
    if (!song.playing) return SEC_NONE;
    b = trk[0].pattern_next;
    for (k = 1; b < SEC_N && k < n; k++)
        if (trk[k].pattern_next != b) return SEC_NONE;
    return b < SEC_N ? b : SEC_NONE;
}
/* section b plays: playing, every track at track 1's loop end; stopped, at once */
static void sec_play(uint32_t b)
{
    uint32_t k, n = trk_n(), f;
    char m[12] = "SECTION A";
    if (b >= SEC_N) return;
    if (chain.running || chain.armed) { ui_message("STOP SONG TO SWITCH"); return; }
    if (song.playing && song.rec) { ui_message("STOP RECORDING FIRST"); return; }
    m[8] = (char)('A' + b);
    f = motion_guard();
    if (song.playing) {
        for (k = 0; k < NTRK; k++) trk[k].pattern_next = 0xFFu;
        secs.next = (uint8_t)b;
    } else {
        secs.next = SEC_NONE;
        for (k = 0; k < n; k++)
            if (trk[k].pattern != b) pattern_switch(&trk[k], b);
    }
    motion_unguard(f);
    if (!song.playing) { ui.entry_open = 0; cursor_set(0); }
    ui_message(song.playing ? (sec_playing() == b ? "FROM THE TOP" : m) : m);
    ui.force = 1;
}
/* what every track plays now copied into section b (each track's pattern into its pattern b), then b plays */
static void sec_copy(uint32_t b)
{
    uint32_t k, n = trk_n(), rc = 0;
    char m[12] = "COPIED TO A";
    if (b >= SEC_N) return;
    if (chain.running || chain.armed) { ui_message("STOP SONG FIRST"); return; }
    for (k = 0; k < n && !rc; k++)
        if (trk[k].pattern != b) rc = (uint32_t)pattern_copy(&trk[k], trk[k].pattern, b);
    scs.sig = 0;
    if (rc) {
        ui_message(rc == 4u ? "PATTERN STORAGE FULL" : rc == 2u ? "PATTERN DATA FULL" : "STOP TO COPY");
        ui.force = 1;
        return;
    }
    sec_play(b);
    m[10] = (char)('A' + b);
    ui_message(m);
}
/* a section key held: the copy, over notes the question first; the section playing already is that loop */
static void sec_hold(uint32_t b)
{
    sec_sync();
    if (sec_playing() == b) { char m[16] = "ALREADY IN A"; m[11] = (char)('A' + b); ui_message(m); return; }
    if (chain.running || chain.armed) { ui_message("STOP SONG FIRST"); return; }
    if (scs.used[b]) confirm_open(CF_SEC_COPY, b);
    else sec_copy(b);
}

/* -------------------------------------------------------- SONG REC --- */
static void sec_rec_start(void)
{
    uint32_t f;
    if (chain.running || chain.armed) { ui_message("STOP SONG FIRST"); return; }
    f = motion_guard();
    secs.rec = 1;
    if (song.playing) jam.n = 0;                        /* (logged from track 1's next loop) */
    motion_unguard(f);
    if (!song.playing && !seq_counting()) transport_req = 1;   /* (the start logs the first loop, seq_start) */
    ui_message(song.playing ? "SONG REC: NEXT LOOP" : "SONG REC");
    ui.force = 1;
}
static void sec_rec_finish(void)
{
    secs.rec = 0;
    if (!jam.n) ui_message("NOTHING RECORDED");
    else jam_take();                                    /* (SONG's undo: EDIT held) */
    ui.force = 1;
}
/* REC on SECTIONS: start (over a song: the question), or finish */
static void sec_rec_tap(void)
{
    if (secs.rec) sec_rec_finish();
    else if (chain.running || chain.armed) ui_message("STOP SONG FIRST");
    else if (chain_config.count) confirm_open(CF_SEC_SONG, 0);
    else sec_rec_start();
}
/* every frame: STOP finishes a song recording; the song playing ends it unfinished */
static void sec_poll(void)
{
    if (secs.rec && !song.playing && !seq_counting() && transport_req != 1u) sec_rec_finish();
}

/* ---------------------------------------------------- the song list --- */
/* song row r's section, every track in use on one of A..H (SEC_NONE: anything else, '*') */
static uint32_t sec_row(uint32_t r)
{
    uint32_t k, n = trk_n(), b;
    if (r >= chain_config.count) return SEC_NONE;
    b = chain_patterns[r][0];
    if (b >= SEC_N) return SEC_NONE;
    for (k = 0; k < n; k++)
        if (chain_patterns[r][k] != b || chain_config.sound[r][k]) return SEC_NONE;
    return b;
}
static void sec_row_set(uint32_t r, uint32_t b)         /* (inside song_edit_begin / end) */
{
    uint32_t k;
    song_extend_rows(r + 1u);
    song_materialize(r + 1u);                           /* (a block continued from here keeps its own pattern) */
    for (k = 0; k < NTRK; k++) {
        chain_patterns[r][k] = (uint8_t)b;
        chain_config.sound[r][k] = 0;
    }
    chain_config.row[r].slot = (uint8_t)b;
}
/* KNOB 1 the entry (one past the last: a new one), 2 its section, 3 its repeats, 4 the song's length */
static void sections_edit(uint32_t slot, int32_t steps)
{
    uint32_t r = ui.song_row, n = chain_config.count, b;
    int32_t d = steps > 0 ? 1 : -1;
    if (slot == 0u) {
        ui.song_row = (uint8_t)clamp((int32_t)r + steps, 0, n < CHAIN_ROWS ? (int32_t)n : CHAIN_ROWS - 1);
        ui.force = 1;
        return;
    }
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    if (slot == 1u) {
        if (r >= CHAIN_ROWS) return;
        b = sec_row(r);
        b = b < SEC_N ? (uint32_t)clamp((int32_t)b + d, 0, SEC_N - 1) :
            r < n && chain_patterns[r][0] < SEC_N ? chain_patterns[r][0] : 0u;
        if (r < n && sec_row(r) == b) return;
        if (!song_edit_begin()) return;
        sec_row_set(r, b);
        song_edit_end();
    } else if (slot == 2u) {
        if (r >= n) { ui_message("CHOOSE A SECTION"); return; }
        int32_t v = clamp((int32_t)chain_config.row[r].repeat + d, 1, 16);
        if (v == chain_config.row[r].repeat || !song_edit_begin()) return;
        chain_config.row[r].repeat = (uint8_t)v;
        song_edit_end();
    } else if (slot == 3u) {
        if (d < 0) {
            if (!n || !song_edit_begin()) return;
            song_rows_remove(n - 1u, 1);
        } else {
            if (n >= CHAIN_ROWS) { ui_message("SONG FULL"); return; }
            b = n ? sec_row(n - 1u) : 0u;
            if (!song_edit_begin()) return;
            sec_row_set(n, b < SEC_N ? b : 0u);
            if (n) chain_config.row[n].repeat = chain_config.row[n - 1u].repeat;
        }
        if (ui.song_row > chain_config.count) ui.song_row = chain_config.count;
        song_edit_end();
    }
    ui.force = 1;
}
/* PLAY on SECTIONS: the song from the entry; no song, the loop */
static void sections_play(void)
{
    if (song.playing || seq_counting() || chain_busy()) transport_req = 2;
    else if (chain_config.count) chain_play_ui();
    else transport_req = 1;
}

/* ------------------------------------------------------- keys, lights --- */
static void sections_keys(uint32_t notes)
{
    static uint32_t seen;
    uint32_t b;
    if (fm1_ms - seen > 100u) scs.key = scs.did = 0;   /* (frames skipped: a dialog, the menu; no stale tap after) */
    seen = fm1_ms;
    if (scs.key && (!sections_keys_on() || !(fm1_in.notes & (1u << SEC_KEY[scs.key - 1u])))) {
        if (!scs.did && sections_keys_on()) sec_play(scs.key - 1u);   /* (released before the hold: a tap) */
        scs.key = scs.did = 0;
    }
    if (!sections_keys_on()) return;
    for (b = 0; b < SEC_N && !scs.key; b++)
        if (notes & (1u << SEC_KEY[b])) { scs.key = (uint8_t)(b + 1u); scs.did = 0; scs.ms = fm1_ms; }
    if (scs.key && !scs.did && fm1_ms - scs.ms >= SEC_HOLD_MS) {
        scs.did = 1;
        sec_hold(scs.key - 1u);
    }
}
/* bright the section playing (and the key held), breathing the one waiting, dim the others with notes */
static uint32_t sections_leds(uint32_t *dim, uint32_t *breath)
{
    uint32_t b, m = 0, p = sec_playing(), q = sec_queued();
    sec_sync();
    *dim = *breath = 0;
    for (b = 0; b < SEC_N; b++) {
        if (b == p) m |= 1u << SEC_KEY[b];
        else if (b == q) *breath |= 1u << SEC_KEY[b];
        else if (scs.used[b]) *dim |= 1u << SEC_KEY[b];
    }
    if (scs.key) m |= 1u << SEC_KEY[scs.key - 1u];
    *dim &= ~(m | *breath);
    *breath &= ~m;
    return m;
}

/* -------------------------------------------------------------- draw --- */
#define SC_TILE_Y H_HEAD + 4                            /* two rows of four sections: 54 x 50, to 140 */
#define SC_TILE_W 54
#define SC_TILE_H 50
#define SC_SONG_Y 146                                   /* the song: 146 .. 209 */
#define SC_FOOT_Y 212                                   /* the hint: 212 .. 239 */
static int32_t sc_tile_x(uint32_t b) { return 6 + 58 * (int32_t)(b % 4u); }
static int32_t sc_tile_y(uint32_t b) { return SC_TILE_Y + 54 * (int32_t)(b / 4u); }
static void sc_tile(uint32_t b, uint32_t p, uint32_t q)
{
    uint32_t k, n = trk_n(), on = b == p, used = scs.used[b], held = scs.key == b + 1u;
    uint32_t h = fnv1(fnv1(fnv1(2166136261u, used + 256u * n), on + 2u * (b == q) + 4u * held), ux.gen * 977u + ux.pal * 31u);
    uint16_t fill = on ? T_THEME : used ? T_SURF : T_BG, ink = on ? T_INK : used ? T_TEXT : T_DIM;
    char l[2] = {(char)('A' + b), 0};
    if (!ui.force && h == scs.tile[b]) return;
    scs.tile[b] = h;
    cv_begin(SC_TILE_W, SC_TILE_H, T_BG);
    cv_rrect(0, 0, SC_TILE_W, SC_TILE_H, 6, held && !on ? T_TEXT : fill == T_BG ? T_LINE : fill, T_BG);
    if (fill == T_BG || (held && !on))
        cv_rrect(1, 1, SC_TILE_W - 2, SC_TILE_H - 2, 5, fill, held && !on ? T_TEXT : T_LINE);
    if (b == q && !on)                                  /* waiting for track 1's loop end: dashed */
        cv_dashed(0, 0, SC_TILE_W, SC_TILE_H, 6, T_THEME);
    cv_text_c(SC_TILE_W / 2, ty_y(19, TY_TX), TY_TX, l, ink, fill);
    for (k = 0; k < n; k++) {                           /* a dot a track: filled, its pattern b holds notes */
        int32_t x = SC_TILE_W / 2 - 4 * (int32_t)n + 1 + 8 * (int32_t)k, y = 38;
        uint16_t c = on ? T_INK : T_TRK(k);
        if ((used >> k) & 1u) cv_rrect(x, y, 6, 6, 3, c, fill);
        else { cv_rrect(x, y, 6, 6, 3, on ? T_INK : T_LINE, fill); cv_rrect(x + 1, y + 1, 4, 4, 2, fill, on ? T_INK : T_LINE); }
    }
    cv_blit((uint32_t)sc_tile_x(b), (uint32_t)sc_tile_y(b));
}
/* the song as entries (a letter, its repeats), six around the one picked, one more to add; the one playing marked */
static void sc_song(void)
{
    uint32_t n = chain_config.count, cur = ui.song_row, play = chain.running ? chain.row : 0xFFu, i, first, slots = n + (n < CHAIN_ROWS);
    uint32_t h = 2166136261u, r;
    for (r = 0; r < n; r++) h = fnv1(fnv1(h, sec_row(r) + 16u * chain_config.row[r].repeat), r);
    h = fnv1(fnv1(fnv1(h, n), cur + 256u * play), ux.gen * 977u + ux.pal * 31u + trk_n());
    if (!ui.force && h == scs.song) return;
    scs.song = h;
    first = cur > 2u ? cur - 2u : 0u;
    if (slots > 6u && first > slots - 6u) first = slots - 6u;
    if (slots <= 6u) first = 0;
    cv_begin(240, 62, T_BG);
    cv_text_on(8, ty_y(9, TY_LB), TY_LB, "SONG", T_MID, T_BG);
    if (n) {
        char t[12];
        fmt_int(t, (int32_t)n);
        str_cpy(t + str_len(t), n == 1u ? " entry" : " entries", 9);
        cv_text_r(232, ty_y(9, TY_LB), TY_LB, t, T_DIM, T_BG);
    }
    for (i = first; i < first + 6u && i < slots; i++) {
        int32_t x = 8 + 38 * (int32_t)(i - first), y = 20;
        uint32_t sel = i == cur, b = sec_row(i);
        uint16_t fill = sel ? T_TEXT : i < n ? T_SURF : T_BG, ink = sel ? T_BG : T_TEXT;
        cv_rrect(x, y, 34, 40, 6, fill == T_BG ? T_LINE : fill, T_BG);
        if (fill == T_BG) cv_rrect(x + 1, y + 1, 32, 38, 5, T_BG, T_LINE);
        if (i == play) cv_rrect(x + 4, y + 36, 26, 3, 1, sel ? T_BG : T_THEME, fill);
        if (i >= n) {
            cv_text_c(x + 17, ty_y(y + 20, TY_TX), TY_TX, "+", sel ? T_BG : T_DIM, fill);
            continue;
        }
        char l[2] = {b < SEC_N ? (char)('A' + b) : '*', 0}, rp[4] = {'x', 0, 0, 0};
        fmt_int(rp + 1, chain_config.row[i].repeat);
        cv_text_c(x + 17, ty_y(y + 13, TY_TX), TY_TX, l, ink, fill);
        cv_text_c(x + 17, ty_y(y + 30, TY_LB), TY_LB, rp, sel ? T_BG : T_MID, fill);
    }
    cv_blit(0, SC_SONG_Y);
}
static void sc_foot(void)
{
    uint32_t h = fnv1(fnv1(2166136261u, secs.rec + 2u * (jam.n + 1u)), ux.gen * 977u + ux.pal * 31u + 7u * !chain_config.count);
    char t[32];
    if (!ui.force && h == scs.foot) return;
    scs.foot = h;
    if (secs.rec) {
        str_cpy(t, "Recording \xB7 ", 32);
        fmt_int(t + str_len(t), jam.n);
        str_cpy(t + str_len(t), jam.n == 1u ? " entry" : " entries", 9);
    } else {
        str_cpy(t, "Hold: copy  REC: song", 32);
    }
    cv_begin(240, 28, T_BG);
    cv_rect(8, 0, 224, 1, T_LINE);
    cv_text_c(120, ty_y(15, TY_LB), TY_LB, t, secs.rec ? T_REC : T_MID, T_BG);
    cv_blit(0, SC_FOOT_Y);
}
static void sections_draw(void)
{
    uint32_t b, p = sec_playing(), q = sec_queued();
    sec_sync();
    if (ui.force) lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    for (b = 0; b < SEC_N; b++) sc_tile(b, p, q);
    sc_song();
    sc_foot();
}
