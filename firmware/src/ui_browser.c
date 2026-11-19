/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* SAVE > PRESETS, the sound browser (Hookology beta73: after Skiff's PRESETS page). At the top the track's numeral and
 * Patches (a message in their place while there is one); under it the four knobs' cards: KNOB 1 No. (the sound's place
 * in the list, as PRESETS), KNOB 2 ENG (the next or previous engine's first sound in the list), KNOB 3 FAV (right: a
 * favourite, left: not), KNOB 4 LIST (All, Faves, Recent, the categories); then the list, BR_SHOW compact rows, each
 * the engine's short name (a user sound's with a dot) and the sound's name, the one shown highlighted in its engine's
 * shade with its star; the place a bar at the right edge. The bottom line: OCT+ keep, OCT- back (the sound from before
 * the browser, when it loaded one); both go back home. Each part remembers what it drew and redraws when that changes.
 * Included by ui_draw.c */
#define BR_TOP_H H_HEAD                                 /* the top: rows 0 .. 29; the cards under it (draw_column) */
#define BR_BODY_Y Y_GRAPH                               /* the sounds: rows 84 .. 211 */
#define BR_BODY_H (Y_FOOT - Y_GRAPH)
#define BR_ROW 21
#define BR_SHOW 6                                       /* (6 rows of 21: 126 of the 128) */
static struct { uint32_t top, side, list, foot, cat; } brv;  /* what each part drew */

/* LIST's names on the screen; the categories by CAT_* */
static const char *const BR_CAT[CAT_N] = {"-", "Bass", "Lead", "Pad", "Pluck", "Keys", "Drums", "FX", "Other",
                                          "Organ", "Strings", "Brass", "Wind", "Bells"};
static const char *browser_list_name(uint32_t m)
{
    return m == LM_ALL ? "All" : m == LM_FAV ? "Faves" : m == LM_RECENT ? "Recent" :
           BR_CAT[m - LM_CAT < NELEM(CAT_ORDER) ? CAT_ORDER[m - LM_CAT] : CAT_OTHER];
}
static int browser_shown_fav(void)                      /* the sound shown is a favourite */
{
    uint32_t k, src;
    if (!browse_pending())
        return preset_favorite();
    src = preset_at(brw.n, &k);
    return favorite_has(src, k);
}
static int browser_can_back(void) { return browse_loads != browse_mark || browse_pending(); }

/* the top: the track's numeral (filled in its colour), PATCHES, the place at the right (or the message) */
static void browser_top(uint32_t cur, uint32_t total)
{
    if (head_shared_status() || song_picker.active) { draw_head(); return; }
    char d[2] = {(char)('1' + song.sel % NTRK), 0}, r[20];
    const aafont_t *tf = TY_TX, *lf = TY_LB;
    int pending = browse_pending(), badge = pending && brw.x > 1u;
    int32_t x, cy = BR_TOP_H / 2;
    (void)r;
    uint32_t sig = song.sel * 7u + list_mode() * 131u + total * 1031u + cur * 3001u + ux.gen * 977u + ux.pal * 31u +
                   (ui.msg_t ? str_hash(3u, ui.msg) : 0u) + (badge ? brw.x : 0u) * 40503u;
    if (!ui.force && sig == brv.top)
        return;
    brv.top = sig;
    cv_begin(240, BR_TOP_H, T_BG);
    cv_rrect(4, cy - 11, 22, 22, 6, T_THEME, T_BG);
    cv_text_c(15, ty_y(cy, lf), lf, d, T_INK, T_THEME);
    x = cv_text_on(34, ty_y(cy, tf), tf, "Patches", T_TEXT, T_BG) + 10;
    if (ui.msg_t) {
        cv_free_text(x, ty_y(cy, lf), lf, ui.msg, T_ACCENT, T_BG, 236 - x);
    } else if (badge) {                                /* a fast turn: its acceleration */
        char b[4] = {'x', 0, 0, 0};
        fmt_int(b + 1, brw.x);
        cv_text_r(236, ty_y(cy, lf), lf, b, T_THEME, T_BG);
    }
    cv_blit(0, 0);
}

/* the four knobs' cards (draw_column, as every page's): No., ENGINE, FAV, LIST */
static void browser_cards(uint32_t cur, uint32_t total)
{
    char no[8], of[8], en[12];
    uint32_t at = brw.on && brw.n < total ? brw.n : cur, k, src;
    int fav = browser_shown_fav();
    if (total && at < total) {
        fmt_int(no, (int32_t)at + 1);
        of[0] = '/'; fmt_int(of + 1, (int32_t)total);
        src = preset_at(at, &k);
        k = src_engine(src, k);
    } else {
        str_cpy(no, "--", sizeof no); of[0] = 0;
        k = TSEL->eng_req;
    }
    k = eng_idx(k);                                     /* (the Prophet's name is wider than a card: P5) */
    if (k == ENGI_PROPHET || k == ENGI_MIDI) str_cpy(en, k == ENGI_MIDI ? "MIDI" : "P5", sizeof en);   /* (MIDI Track too) */
    else list_words(en, ENGINES[k]->name, sizeof en);
    draw_column(0, "No.", no, of, VAL(0u), -1, ICON_NONE);
    draw_column(1, "ENG", en, "", VAL(1u), -1, ICON_NONE);
    draw_column(2, "FAV", fav ? "ON" : "OFF", "", fav ? VAL(2u) : T_DIM, -1, ICON_NONE);
    draw_column(3, "LIST", browser_list_name(list_mode()), "", VAL(3u), -1, ICON_NONE);
}

/* the sounds: BR_SHOW rows from the top, scrolling once the one shown reaches the second; each its engine's short
 * name (a user sound's with a dot after it) and its name; the one shown highlighted, its star at the right. The place
 * as a bar at the right edge. The loaded sound out of the list: the list's first, unhighlighted */
static void browser_list(uint32_t cur, uint32_t total)
{
    uint32_t m = list_mode(), at0 = brw.on && brw.n < total ? brw.n : cur, out = at0 >= total, at = out ? 0u : at0, k, src, sig;
    int fav = browser_shown_fav();
    const aafont_t *lf = TY_LB;
    int32_t h = BR_BODY_H;
    char tag[6], nm[16];
    sig = at * 7u + total * 131u + m * 1031u + (uint32_t)browse_pending() * 3u + out * 17u + (uint32_t)fav * 5u +
          song.sel * 11u + up_gen * 7919u + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == brv.list)
        return;
    brv.list = sig;
    for (uint32_t band = 0; band < (uint32_t)h; band += CV_MAX / 240u) {
        cv_begin_band(240, (uint32_t)h, T_BG, band);
        if (!total) {
            cv_text_c(120, ty_y(h / 2, TY_TX), TY_TX, m == LM_FAV ? "No favourites" : m == LM_RECENT ? "Nothing loaded yet" :
                      "No sounds", T_MID, T_BG);
        } else {
            int32_t first = clamp((int32_t)at - 1, 0, total > BR_SHOW ? (int32_t)total - BR_SHOW : 0);
            for (int32_t j = 0; j < BR_SHOW && first + j < (int32_t)total; j++) {
                int32_t n = first + j, y = j * BR_ROW + 1, sel = n == (int32_t)at && !out, tx = 60, cy = y + BR_ROW / 2 - 1;
                uint32_t e;
                src = preset_at((uint32_t)n, &k);
                e = src_engine(src, k);
                uint16_t hc = T_KNOB(eng_rank(e)), bg = sel ? ux_mix(T_BG, hc, 22) : T_BG;
                entry_label(src, k, tag, nm);
                if (src == ENGI_PROPHET)                /* (the Prophet's own name, whole) */
                    str_cpy(nm, eng_preset(ENGINES[ENGI_PROPHET], k)->name, sizeof nm);
                if (sel) cv_rrect(BOX_X, y, BOX_W - 4, BR_ROW - 1, 5, bg, T_BG);
                int32_t x = cv_text_on(10, ty_y(cy, lf), lf, eng_idx(e) == ENGI_MIDI ? "MIDI" : eng_abbr(ENGINES[eng_idx(e)]->name),
                                       sel ? hc : ux_mix(T_BG, hc, 70), bg);
                if (src_slot(src)) cv_rect(x + 2, cy + 3, 3, 3, sel ? hc : ux_mix(T_BG, hc, 70));   /* (yours) */
                cv_free_text(tx, ty_y(cy, lf), lf, nm[0] ? nm : tag, sel ? T_TEXT : T_SEC, bg, sel ? 150 : 172);
                if (sel) cv_icon_mid(214, cy, 16, fav ? ICON_X_STAR : ICON_X_STAR_O, fav ? hc : T_DIM, bg);
            }
            cv_rect(238, 0, 2, h, T_LINE);              /* the place */
            {
                int32_t th = 18, ty = total > 1u ? (int32_t)at * (h - th) / (int32_t)(total - 1u) : 0;
                cv_rect(238, ty, 2, th, out ? T_MID : T_THEME);
            }
        }
        cv_blit_band(0, BR_BODY_Y);
    }
}

/* the bottom line: what OCT+ and OCT- do */
static void browser_foot(void)
{
    uint32_t sig = ux.gen * 977u + ux.pal * 31u + 5u;
    if (!ui.force && sig == brv.foot)
        return;
    brv.foot = sig;
    khint_t kh[2] = {{KC_OCTUP, "KEEP"}, {KC_OCTDN, "BACK"}};
    cv_begin(240, H_FOOT, T_BG);
    cv_key_row(8, 232, (H_FOOT - KC_PH) / 2, kh, 2, 3u, T_BG);
    cv_blit(0, Y_FOOT);
}

static void browser_draw(void)
{
    uint32_t total, cur = preset_pos(&total);
    if (ui.force) {                                     /* (the canvases cover the rest) */
        lcd_fill(0, 238, 240, 2, T_BG);
        lcd_fill(0, H_HEAD, 240, Y_GRAPH - H_HEAD, T_BG);   /* (the cards' band: the gaps between and around them,
                                                         * which no card draws; the page before's would show there) */
    }
    browser_top(cur, total);
    browser_cards(cur, total);
    browser_foot();
    browser_list(cur, total);                           /* (the busiest last: the SPI sends it during the next frame) */
}

/* OCT+ keeps the sound shown (a pending one loads now), OCT- goes back to the one from before the browser (the loads
 * since are one undo level: ui.c load_begin); either way back to Stage */
static void browser_key(int keep)
{
    if (song_picker.active) { song_preset_finish(keep); return; }
    if (keep) {
        browse_commit();
    } else if (browser_can_back()) {
        brw.on = 0;
        if (browse_loads != browse_mark)
            undo_step(0);
    }
    go_home();
}
