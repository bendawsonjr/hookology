/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME's TRACKS page, after SLOOP's home screen (isod89, sloop-fm1), shown as Stage is: under the header, a lane for each
 * of the selected group's four tracks (its numeral, its sound, every step as Stage shows them, a line of 16 a bar,
 * stacked past 16 (ui_stage.c st_layout); a length waiting for the
 * loop end, NEXT and it in the accent colour). The first turn of a knob brings the selected track's four in as Stage's
 * strip, LEVEL, PAN, SWING, STEPS (ui_input.c tracks_knob), and under them that setting for every track of the group
 * side by side; TR_KNOB_MS after the last turn they go and the lanes are whole again. Under it all Stage's band. Each part redraws when what it
 * shows changes; the playhead moves the steps alone. Included by ui_draw.c */
#define TR_ROW_Y 32
#define TR_ROW_H 42                                     /* Stage's: four in 32 .. 205, its band under them */
#define TR_ROW_GAP 2
#define TR_TX 42                                        /* the words and the steps start here in a lane */
#define TR_STEP_Y 25                                    /* the steps: every bar, Stage's layout (st_layout), in */
#define TR_STEP_W (BOX_W - TR_TX - 8)                   /* .. TR_STEP_W x TR_STEP_H */
#define TR_STEP_H (TR_ROW_H - TR_STEP_Y - 3)
#define TR_KNOB_MS 1500u                                /* the knobs stay this long after the last turn (Stage's) */
#define TR_CMP_Y (Y_GRAPH + 4)                          /* under the strip: the setting for every track, to 239 */
#define TR_CMP_H (ST_BAND_Y - 4 - TR_CMP_Y)

static struct {
    uint32_t col[NTRK], grid[NTRK], cmp;                /* what each part drew */
    uint32_t knob_ms;                                   /* the last knob turn on TRACKS (| 1), 0 none */
    uint8_t out, knob;                                  /* the knobs out; the knob turned last */
    uint16_t clr;                                       /* OCT- held to clear the selected track: 1..1000 of the way */
    uint8_t lane, sound;                                /* a drum key held while a knob turned: its lane + 1 and its drum
                                                         * (the strip is that drum's: LEVEL P_LN0.., TUNE DECAY TONE as
                                                         * EDIT > DRUM SOUND; lane 0: the track's) */
} tracks;

static uint32_t fnv1(uint32_t h, uint32_t v);       /* ui_patterns.c */
static void tracks_knob_touch(uint32_t k) { tracks.knob_ms = fm1_ms | 1u; tracks.knob = (uint8_t)(k & 3u); }   /* (ui_input.c) */
static int tracks_knobs_out(void) { return tracks.knob_ms && fm1_ms - (tracks.knob_ms & ~1u) < TR_KNOB_MS; }

static uint32_t tr_steps(const track_t *t) { return t->slen_next ? t->slen_next : (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }

static void tr_row(uint32_t k)
{
    const track_t *t = &trk[k];
    uint32_t sel = k == song.sel, mute = t->p[P_MUTE] != 0u, arm = (song.rec >> k) & 1u, len = (uint32_t)t->p[P_SLEN];
    uint32_t ph = song.playing && t->seq_idx < len ? t->seq_idx : 0xFFFFu, lv = stage_rec_lv(k), sig, g;
    int32_t y = TR_ROW_Y + (int32_t)(k % NROWS) * (TR_ROW_H + TR_ROW_GAP), h = TR_ROW_H, nw;
    uint16_t tc = mute ? T_DIM : T_TRK(k), bg = ux_mix(T_BG, tc, sel ? 24 : 11);
    char nm[24], nx[12] = "", d[2] = {(char)('1' + k), 0};
    sound_name(t, nm);
    if (!nm[0]) str_cpy(nm, ENGINES[eng_idx(t->eng_req)]->name, sizeof nm);
    if (t->slen_next) { str_cpy(nx, "NEXT ", sizeof nx); fmt_int(nx + 5, t->slen_next); }   /* (a length waiting) */
    uint32_t clr = sel ? tracks.clr : 0u;               /* (OCT- held: the lane fills red, HOLD TO CLEAR) */
    if (clr) str_cpy(nm, "HOLD TO CLEAR", sizeof nm);
    sig = str_hash(1u + sel * 2u + mute * 4u + arm * 8u + t->slen_next * 16u, nm) + ux.gen * 977u + ux.pal * 31u + lv * 50021u + clr * 7919u;
    st_lay_t l = st_layout(st_bars(t), TR_STEP_W, TR_STEP_H);
    int32_t sy = TR_STEP_Y + (TR_STEP_H - l.h) / 2;     /* (the steps centred in their place) */
    g = steps_hash(t) + len * 613u + ph * 104729u;
    sig += len * 7u;                                    /* (another length: another layout, the lane again) */
    if (!ui.force && sig == tracks.col[k]) {
        if (g != tracks.grid[k] && !clr) {             /* the steps or the playhead: the steps alone */
            tracks.grid[k] = g;
            for (uint32_t band = 0; band < (uint32_t)l.h; band += CV_MAX / (uint32_t)l.w) {
                cv_begin_band((uint32_t)l.w, (uint32_t)l.h, bg, band);
                stage_steps(t, ph, tc, bg, 0, 0, &l);
                cv_blit_band((uint32_t)(BOX_X + TR_TX), (uint32_t)(y + sy));
            }
        }
        return;
    }
    tracks.col[k] = sig;
    tracks.grid[k] = g;
    nw = nx[0] ? text_w(TY_LB, nx) + 8 : 0;
    for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / BOX_W) {
        cv_begin_band(BOX_W, (uint32_t)h, T_BG, band);
        if (sel) {
            cv_rrect(0, 0, BOX_W, h, 6, tc, T_BG);
            cv_rrect(2, 2, BOX_W - 4, h - 4, 4, bg, tc);
        } else {
            cv_rrect(0, 0, BOX_W, h, 6, bg, T_BG);
        }
        stage_rec_paint(0, 0, BOX_W, h, 6, lv);
        cv_rrect(6, 6, 28, h - 12, 6, tc, bg);
        cv_text_c(20, ty_y(h / 2, TY_TX), TY_TX, d, T_INK, tc);
        if (arm) cv_disc(32, 8, 9, T_REC, 0);
        cv_free_text(TR_TX, ty_y(14, TY_TX), TY_TX, nm, clr ? T_REC : mute ? T_DIM : T_TEXT, bg, BOX_W - TR_TX - 8 - nw);
        if (nx[0]) cv_text_r(BOX_W - 8, ty_y(14, TY_LB), TY_LB, nx, T_ACCENT, bg);
        if (clr) {                                      /* OCT- held: a red bar fills where the steps are */
            cv_rrect(TR_TX, TR_STEP_Y + TR_STEP_H / 2 - 4, TR_STEP_W, 8, 4, ux_mix(bg, T_REC, 25), bg);
            cv_rrect(TR_TX, TR_STEP_Y + TR_STEP_H / 2 - 4, 8 + (int32_t)clr * (TR_STEP_W - 8) / 1000, 8, 4, T_REC, bg);
        } else {
            stage_steps(t, ph, tc, bg, TR_TX, sy, &l);
        }
        cv_blit_band(BOX_X, (uint32_t)y);
    }
}

/* ------------------------------------------------------- the knobs --- */
/* the strip: the selected track's LEVEL, PAN, SWING, STEPS as Stage's cards */
static void tr_columns(void)
{
    static const char *const LABEL[4] = {"LEVEL", "PAN", "SWING", "STEPS"};
    const track_t *t = TSEL;
    int32_t level = clamp(t->p[P_LEVEL], 0, 127), pan = clamp(t->p[P_PAN], -64, 63), sw = track_swing(t), steps = (int32_t)tr_steps(t);
    uint32_t c, midi = midi_track_only(t);
    if (tracks.lane) {                                  /* a drum key held: that drum's LEVEL (EDIT > LANES), TUNE DECAY
                                                         * TONE (EDIT > DRUM SOUND) */
        const int8_t *dc = drum_patch_of(song.sel)->c[tracks.sound % drum_sound_count(t)];
        int32_t lv = clamp(t->p[P_LN0 + tracks.lane - 1u], 0, 127);
        char v[12], lb[12];
        const char *unit = "";
        str_cpy(lb, drum_lane_name(t, tracks.lane - 1u), sizeof lb);
        param_format(&TP[P_LN0 + tracks.lane - 1u], lv, v, &unit);
        draw_column(0, lb, v, unit, T_ACCENT, lv * 1000 / 127, ICON_NONE);
        fmt_int(v, dc[0]); draw_column(1, "TUNE", v, "ST", VAL(1u), (dc[0] + 24) * 1000 / 48, ICON_NONE);
        fmt_int(v, dc[1] + 64); draw_column(2, "DECAY", v, "", VAL(2u), (dc[1] + 64) * 1000 / 127, ICON_NONE);
        fmt_int(v, dc[2] + 64);
        draw_column(3, drum_is909(t) ? drum_character(tracks.sound) : drum_issid(t) ? "SNAP" : tracks.sound == D9_SD ? "SNAPPY" : "TONE",
                    v, "", VAL(3u), (dc[2] + 64) * 1000 / 127, ICON_NONE);
        return;
    }
    for (c = 0; c < 4u; c++) {
        char v[12];
        const char *unit = "";
        int32_t ratio;
        if (c == 0u) { param_format(&TP[P_LEVEL], level, v, &unit); ratio = level * 1000 / 127; }
        else if (c == 1u) { param_format(&TP[P_PAN], pan, v, &unit); ratio = (pan + 64) * 1000 / 127; }
        else if (c == 2u) { fmt_int(v, sw); ratio = sw * 10; }
        else { fmt_int(v, steps); ratio = (steps - 1) * 1000 / 63; }
        draw_column(c, LABEL[c], v, unit, midi && c < 2u ? T_DIM : c == 3u && t->slen_next ? T_ACCENT : VAL(c), ratio, ICON_NONE);
    }
}
/* under the strip: the setting of the knob turned for every track of the group, a bar each (PAN a mark from the
 * middle), its value at the right; the selected track outlined */
/* .. a drum key held: the kit's eight drum levels in two columns, the held one outlined */
static void tr_lanes(void)
{
    const track_t *t = TSEL;
    uint32_t l, sig = fnv1(2166136261u, tracks.lane + 16u * song.sel + 4096u * (uint32_t)t->p[P_E0]);
    int32_t rh = TR_CMP_H / 4;
    uint16_t tc = T_TRK(song.sel);
    for (l = 0; l < 8u; l++) sig = fnv1(sig, (uint32_t)t->p[P_LN0 + l]);
    sig = fnv1(sig, ux.gen * 977u + ux.pal * 31u + 1u);
    if (!ui.force && sig == tracks.cmp) return;
    tracks.cmp = sig;
    for (uint32_t band = 0; band < (uint32_t)TR_CMP_H; band += CV_MAX / 240u) {
        cv_begin_band(240u, (uint32_t)TR_CMP_H, T_BG, band);
        for (l = 0; l < 8u; l++) {
            int32_t x = BOX_X + (int32_t)(l / 4u) * (BOX_W / 2), y = (int32_t)(l % 4u) * rh, w = BOX_W / 2 - 2;
            int32_t v = clamp(t->p[P_LN0 + l], 0, 127), bx = x + 34, bw = w - 34 - 54;
            uint16_t bg = ux_mix(T_BG, tc, l + 1u == tracks.lane ? 24 : 11);
            char val[12];
            const char *unit = "";
            param_format(&TP[P_LN0 + l], v, val, &unit);      /* (as the card above and EDIT > LANES: %) */
            if (unit[0] && str_len(val) + str_len(unit) < sizeof val) str_cpy(val + str_len(val), unit, sizeof val - str_len(val));
            if (l + 1u == tracks.lane) { cv_rrect(x, y + 1, w, rh - 2, 5, tc, T_BG); cv_rrect(x + 2, y + 3, w - 4, rh - 6, 3, bg, tc); }
            else cv_rrect(x, y + 1, w, rh - 2, 5, bg, T_BG);
            cv_text(x + 6, ty_y(y + rh / 2, TY_LB), TY_LB, drum_lane_abbr(t, l), T_TEXT);
            cv_rect(bx, y + rh / 2 - 3, bw, 6, ux_mix(bg, tc, 22));
            cv_rect(bx, y + rh / 2 - 3, v * bw / 127, 6, tc);
            cv_text_r(x + w - 6, ty_y(y + rh / 2, TY_LB), TY_LB, val, T_TEXT, bg);
        }
        cv_blit_band(0, TR_CMP_Y);
    }
}
static void tr_compare(void)
{
    uint32_t k, c = tracks.knob, first = trk_group(), sig = 2166136261u;
    int32_t rh = TR_CMP_H / NROWS;
    if (tracks.lane) { tr_lanes(); return; }
    for (k = first; k < first + NROWS && k < trk_n(); k++) {
        const track_t *t = &trk[k];
        sig = fnv1(fnv1(sig, (uint32_t)(t->p[P_LEVEL] + 256 * (t->p[P_PAN] + 64))), (uint32_t)track_swing(t) + 256u * tr_steps(t) + 65536u * t->p[P_MUTE]);
    }
    sig = fnv1(fnv1(sig, c + 8u * song.sel + 256u * first), ux.gen * 977u + ux.pal * 31u + trk_n());
    if (!ui.force && sig == tracks.cmp) return;
    tracks.cmp = sig;
    for (uint32_t band = 0; band < (uint32_t)TR_CMP_H; band += CV_MAX / 240u) {
        cv_begin_band(240u, (uint32_t)TR_CMP_H, T_BG, band);
        for (k = first; k < first + NROWS && k < trk_n(); k++) {
            const track_t *t = &trk[k];
            int32_t y = (int32_t)(k - first) * rh, bx = 40, bw = 140, v, ratio;
            uint16_t tc = t->p[P_MUTE] ? T_DIM : T_TRK(k), bg = ux_mix(T_BG, tc, k == song.sel ? 24 : 11);
            char d[2] = {(char)('1' + k), 0}, val[12];
            const char *unit = "";
            if (k == song.sel) { cv_rrect(BOX_X, y + 1, BOX_W, rh - 2, 5, tc, T_BG); cv_rrect(BOX_X + 2, y + 3, BOX_W - 4, rh - 6, 3, bg, tc); }
            else cv_rrect(BOX_X, y + 1, BOX_W, rh - 2, 5, bg, T_BG);
            cv_rrect(8, y + 5, 22, rh - 10, 4, tc, bg);
            cv_text_c(19, ty_y(y + rh / 2, TY_LB), TY_LB, d, T_INK, tc);
            if (c == 0u) { v = clamp(t->p[P_LEVEL], 0, 127); param_format(&TP[P_LEVEL], v, val, &unit); ratio = v * 1000 / 127; }
            else if (c == 1u) { v = clamp(t->p[P_PAN], -64, 63); param_format(&TP[P_PAN], v, val, &unit); ratio = (v + 64) * 1000 / 127; }
            else if (c == 2u) { v = track_swing(t); fmt_int(val, v); ratio = v * 10; }
            else { v = (int32_t)tr_steps(t); fmt_int(val, v); ratio = (v - 1) * 1000 / 63; }
            if (c < 2u && midi_track_only(t)) { str_cpy(val, "MIDI", sizeof val); unit = ""; ratio = -1; }
            if (unit[0] && str_len(val) + str_len(unit) + 1u < sizeof val) { str_cpy(val + str_len(val), unit, sizeof val - str_len(val)); }
            cv_rect(bx, y + rh / 2 - 3, bw, 6, ux_mix(bg, tc, 22));
            if (ratio >= 0 && c == 1u) {                /* PAN: from the middle, a mark */
                int32_t mid = bx + bw / 2, px = bx + ratio * bw / 1000;
                cv_rect(mid, y + rh / 2 - 6, 1, 12, ux_mix(bg, tc, 45));
                cv_rect(px - 2, y + rh / 2 - 7, 5, 14, tc);
            } else if (ratio >= 0) {
                cv_rect(bx, y + rh / 2 - 3, ratio * bw / 1000, 6, tc);
            }
            cv_text_r(BOX_W - 6, ty_y(y + rh / 2, TY_LB), TY_LB, val, c == 3u && t->slen_next ? T_ACCENT : T_TEXT, bg);
        }
        cv_blit_band(0, TR_CMP_Y);
    }
}
/* the knobs: out while one turns (and TR_KNOB_MS after); gone at once, the lanes whole again */
static void tr_knobs(void)
{
    int out = tracks_knobs_out();
    if (out && !tracks.out) {
        tracks.out = 1;
        tracks.cmp = 0;
        memset(ui.col, 0, sizeof ui.col);
        lcd_fill(0, Y_LABEL, 240, ST_BAND_Y - Y_LABEL, T_BG);
    } else if (!out && tracks.out) {
        tracks.out = 0;
        tracks.lane = 0;
        memset(tracks.col, 0, sizeof tracks.col);
        memset(ui.col, 0, sizeof ui.col);
        lcd_fill(0, Y_LABEL, 240, ST_BAND_Y - Y_LABEL, T_BG);
        return;
    }
    if (!tracks.out) return;
    tr_columns();
    tr_compare();
}

/* every frame the knobs; every other frame the lanes (the knobs out: not), then Stage's band last: the notes the
 * selected track played and their chord over its waveform (ui_stage.c stage_band) */
static void tracks_draw(void)
{
    uint32_t k;
    stage_panel_hits();
    tr_knobs();
    if (!ui.force && (ui.frame & 1u))
        return;
    if (!tracks.out)
        for (k = trk_group(); k < trk_group() + NROWS; k++)
            if (k < trk_n()) {
                tr_row(k);
            } else if (ui.force) {                      /* a place not in use: Stage adds a track */
                lcd_fill(BOX_X, (uint32_t)(TR_ROW_Y + (int32_t)(k % NROWS) * (TR_ROW_H + TR_ROW_GAP)), BOX_W, TR_ROW_H, ux.panel);
            }
    stage_band();
}
