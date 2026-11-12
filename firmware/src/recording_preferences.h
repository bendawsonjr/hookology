/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#ifndef MELODEE_RECORDING_PREFERENCES_H
#define MELODEE_RECORDING_PREFERENCES_H
#define CLICK_LEVEL_DEFAULT 35u   /* CLICK LEVEL in %, on a loudness curve (click.c: the gain is its square): 35 % about
                                   * the old default 12 % of DAC full scale (Hookology) */
static volatile uint8_t settings_click, settings_click_level = CLICK_LEVEL_DEFAULT, settings_countin;
static volatile uint8_t settings_click_soft;   /* CLICK SOUND: 0 BRIGHT (1760 / 1320 Hz), 1 SOFT (1000 / 800 Hz) */
static uint8_t settings_preview, settings_chord_add, settings_screen;
static uint8_t settings_autosave = 1;
#endif
