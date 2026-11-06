/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME's TRACKS page, after SLOOP's home screen (isod89, sloop-fm1): under the header, a lane for each of the selected
 * group's four tracks (its sound, its swing and steps, the steps of the bar playing, its level and pan), and four dials
 * for the selected track: SWING, LEVEL, STEPS, PAN (ui_input.c tracks_knob). STEPS shows a length waiting for the
 * loop end in the accent colour. Each part redraws when what it shows changes; the playhead moves the steps alone.
 * Included by ui_draw.c */
#define TR_ROW_Y 32
#define TR_ROW_H 36
#define TR_ROW_GAP 1
#define TR_TX 34                                        /* the words and the steps start here in a lane */
#define TR_STEP_P 8                                     /* a step's pitch; sixteen in a line */
#define TR_STEP_H 8
#define TR_STEP_Y (TR_ROW_H - 12)
#define TR_LVL_X 170                                    /* the level's bar */
#define TR_LVL_W 34
#define TR_PAN_X 210                                    /* the pan's line */
#define TR_PAN_W 20
#define TR_DIAL_Y 181                                   /* the dials: rows 181 .. 239 */
#define TR_DIAL_H 59
#define TR_ARC_N 25u

static struct {
    uint32_t col[NTRK], grid[NTRK], dial;               /* what each part drew */
} tracks;

static uint32_t tr_steps(const track_t *t) { return t->slen_next ? t->slen_next : (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }

static void tr_cells(const track_t *t, uint32_t ph, uint16_t tc, uint16_t bg, int32_t x, int32_t y)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), first = (ph < len ? ph : 0u) / 16u * 16u, i;
    for (i = first; i < first + 16u && i < len; i++) {
        const step_t *st = &seq_steps(t)[i];
        uint16_t c = i == ph ? T_TEXT : step_on(st) ? tc : st->time == ST_TIE ? ux_mix(bg, tc, 55) : ux_mix(bg, tc, 22);
        cv_rect(x + (int32_t)(i - first) * TR_STEP_P, y, TR_STEP_P - 1, TR_STEP_H, c);
    }
}

static void tr_row(uint32_t k)
{
    const track_t *t = &trk[k];
    uint32_t sel = k == song.sel, mute = t->p[P_MUTE] != 0u, arm = (song.rec >> k) & 1u, len = (uint32_t)t->p[P_SLEN];
    uint32_t ph = song.playing && t->seq_idx < len ? t->seq_idx : 0xFFFFu, lv = stage_rec_lv(k), sig, g;
    uint32_t level = (uint32_t)clamp(t->p[P_LEVEL], 0, 127), sw = (uint32_t)track_swing(t), steps = tr_steps(t);
    int32_t pan = clamp(t->p[P_PAN], -64, 63), y = TR_ROW_Y + (int32_t)(k % NPART) * (TR_ROW_H + TR_ROW_GAP), h = TR_ROW_H;
    uint16_t tc = mute ? T_DIM : T_TRK(k), bg = ux_mix(T_BG, tc, sel ? 24 : 11);
    char nm[24], vals[16], d[2] = {(char)('1' + k), 0};
    sound_name(t, nm);
    if (!nm[0]) str_cpy(nm, ENGINES[eng_idx(t->eng_req)]->name, sizeof nm);
    str_cpy(vals, "SW", sizeof vals);
    fmt_int(vals + 2, (int32_t)sw);
    str_cpy(vals + str_len(vals), " ST", 4);
    fmt_int(vals + str_len(vals), (int32_t)steps);
    sig = str_hash(1u + sel * 2u + mute * 4u + arm * 8u + (t->slen_next ? 16u : 0u), nm) + level * 131u + (uint32_t)(pan + 64) * 7919u +
          sw * 613u + steps * 13u + ux.gen * 977u + ux.pal * 31u + lv * 50021u;
    g = steps_hash(t) + len * 613u + ph * 104729u;
    if (!ui.force && sig == tracks.col[k]) {
        if (g != tracks.grid[k]) {                      /* the steps or the playhead: the steps alone */
            tracks.grid[k] = g;
            for (uint32_t band = 0; band < TR_STEP_H; band += CV_MAX / (16u * TR_STEP_P)) {
                cv_begin_band(16u * TR_STEP_P, TR_STEP_H, bg, band);
                tr_cells(t, ph, tc, bg, 0, 0);
                cv_blit_band((uint32_t)(BOX_X + TR_TX), (uint32_t)(y + TR_STEP_Y));
            }
        }
        return;
    }
    tracks.col[k] = sig;
    tracks.grid[k] = g;
    for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / BOX_W) {
        int32_t lw = (int32_t)level * TR_LVL_W / 127, mid = TR_PAN_X + TR_PAN_W / 2, px = mid + pan * (TR_PAN_W / 2) / 64;
        cv_begin_band(BOX_W, (uint32_t)h, T_BG, band);
        if (sel) {
            cv_rrect(0, 0, BOX_W, h, 6, tc, T_BG);
            cv_rrect(2, 2, BOX_W - 4, h - 4, 4, bg, tc);
        } else {
            cv_rrect(0, 0, BOX_W, h, 6, bg, T_BG);
        }
        stage_rec_paint(0, 0, BOX_W, h, 6, lv);
        cv_rrect(5, 5, 24, 26, 5, tc, bg);
        cv_text_c(17, ty_y(18, TY_TX), TY_TX, d, T_INK, tc);
        if (arm) cv_disc(27, 7, 9, T_REC, 0);
        cv_free_text(TR_TX, ty_y(12, TY_LB), TY_LB, nm, mute ? T_DIM : T_TEXT, bg, BOX_W - TR_TX - 6 - text_w(TY_LB, vals) - 6);
        cv_text_r(BOX_W - 6, ty_y(12, TY_LB), TY_LB, vals, t->slen_next ? T_ACCENT : tc, bg);
        tr_cells(t, ph, tc, bg, TR_TX, TR_STEP_Y);
        cv_rect(TR_LVL_X, TR_STEP_Y + 1, TR_LVL_W, 6, ux_mix(bg, tc, 22));
        cv_rect(TR_LVL_X, TR_STEP_Y + 1, lw, 6, tc);
        cv_rect(TR_PAN_X, TR_STEP_Y + 3, TR_PAN_W, 2, ux_mix(bg, tc, 22));
        cv_rect(mid, TR_STEP_Y + 1, 1, 6, ux_mix(bg, tc, 45));
        cv_rect(px - 1, TR_STEP_Y, 3, 8, tc);
        cv_blit_band(BOX_X, (uint32_t)y);
    }
}

/* a dial, as a share of its range (0..1000); from the middle when bipolar */
static void tr_dial(uint32_t c, uint32_t share, int bipolar, uint16_t col)
{
    /* 270 degrees of a circle of radius 13, the gap at the bottom */
    static const int8_t ARC[TR_ARC_N][2] = {{-9, 9}, {-11, 7}, {-12, 5}, {-13, 3}, {-13, 0}, {-13, -2}, {-12, -5}, {-11, -7}, {-9, -9},
        {-7, -11}, {-5, -12}, {-3, -13}, {0, -13}, {3, -13}, {5, -12}, {7, -11}, {9, -9}, {11, -7}, {12, -5}, {13, -2}, {13, 0}, {13, 3},
        {12, 5}, {11, 7}, {9, 9}};
    int32_t cx = 30 + (int32_t)c * 60, cy = 15;
    uint32_t at = share * (TR_ARC_N - 1u) / 1000u, lo = bipolar && at > 12u ? 12u : at, hi = bipolar && at < 12u ? 12u : at, i;
    if (!bipolar) lo = 0;
    for (i = 0; i + 1u < TR_ARC_N; i++)
        cv_line_t(cx + ARC[i][0], cy + ARC[i][1], cx + ARC[i + 1u][0], cy + ARC[i + 1u][1],
                  i >= lo && i < hi ? col : T_LINE, 2);
    cv_line_t(cx, cy, cx + ARC[at][0] * 2 / 3, cy + ARC[at][1] * 2 / 3, T_TEXT, 2);
}

static void tr_dials(void)
{
    static const char *const LABEL[4] = {"SWING", "LEVEL", "STEPS", "PAN"};
    const track_t *t = TSEL;
    int32_t sw = track_swing(t), level = clamp(t->p[P_LEVEL], 0, 127), pan = clamp(t->p[P_PAN], -64, 63), steps = (int32_t)tr_steps(t);
    uint32_t sig = (uint32_t)sw * 131u + (uint32_t)level * 7919u + (uint32_t)(pan + 64) * 613u + (uint32_t)steps * 13u + song.sel * 977u +
                   ux.gen * 31u + ux.pal * 50021u + (t->slen_next ? 104729u : 0u), c;
    if (!ui.force && sig == tracks.dial) return;
    tracks.dial = sig;
    for (uint32_t band = 0; band < TR_DIAL_H; band += CV_MAX / 240u) {
        cv_begin_band(240u, TR_DIAL_H, T_BG, band);
        for (c = 0; c < 4u; c++) {
            char v[12];
            const char *unit = "";
            uint16_t col = T_KNOB(c);
            if (c == 0u) fmt_int(v, sw);
            else if (c == 1u) param_format(&TP[P_LEVEL], level, v, &unit);
            else if (c == 2u) fmt_int(v, steps);
            else param_format(&TP[P_PAN], pan, v, &unit);
            tr_dial(c, c == 0u ? (uint32_t)sw * 10u : c == 1u ? (uint32_t)level * 1000u / 127u :
                    c == 2u ? (uint32_t)(steps - 1) * 1000u / 63u : (uint32_t)(pan + 64) * 1000u / 127u, c == 3u, col);
            cv_text_c(30 + (int32_t)c * 60, ty_y(35, TY_LB), TY_LB, LABEL[c], col, T_BG);
            cv_text_c(30 + (int32_t)c * 60, ty_y(51, TY_LB), TY_LB, v, c == 2u && t->slen_next ? T_ACCENT : T_TEXT, T_BG);
        }
        cv_blit_band(0, TR_DIAL_Y);
    }
}

static void tracks_draw(void)
{
    uint32_t k;
    tr_dials();
    if (!ui.force && (ui.frame & 1u))
        return;
    for (k = trk_group(); k < trk_group() + NPART; k++)
        if (k < trk_n()) {
            tr_row(k);
        } else if (ui.force) {                          /* a place not in use: Stage adds a track */
            lcd_fill(BOX_X, (uint32_t)(TR_ROW_Y + (int32_t)(k % NPART) * (TR_ROW_H + TR_ROW_GAP)), BOX_W, TR_ROW_H, ux.panel);
        }
}
