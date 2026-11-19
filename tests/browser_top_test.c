/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, beta73: after Skiff's PRESETS; beta52's category line before it) SAVE > PRESETS, the patch browser:
 * KNOB 1 steps the list and stops at the first sound (no wrap to the last); KNOB 2 ENGINE jumps to the next engine's
 * first sound in the list and back to the first sound of the engine before; KNOB 3 FAV marks; KNOB 4 LIST changes the
 * list (no wrap). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
static uint32_t shown_engine(void)
{
    uint32_t total, k, cur = preset_pos(&total);
    return src_engine(preset_at(cur < total ? cur : 0u, &k), k);
}
int main(void)
{
    uint32_t total, cur, k, e0;
    ui_power_on();
    select_engine(ENGI_PROPHET);frames(200);list_set(LM_CAT + 5u);go_page(GR_BROWSE);frame();
    turn(EN_K1, 1);frames(400);                         /* (onto the list: its first) */
    while ((cur = preset_pos(&total)) != 0u) { turn(EN_K1, -1); frames(400); }
    assert(total > 3u);
    turn(EN_K1, -1);frames(100);
    assert(preset_pos(&total) == 0u);                   /* (still the first: no wrap to the last) */
    turn(EN_K1, 1);frames(400);
    assert(preset_pos(&total) == 1u);
    /* ENGINE: on ALL, from the first sound the next engine's first, and back */
    list_set(LM_ALL);ui.force = 1;frame();
    while ((cur = preset_pos(&total)) != 0u) { turn(EN_K1, -1); frames(400); }
    e0 = shown_engine();
    turn(EN_K2, 1);frames(100);
    cur = preset_pos(&total);
    assert(cur > 0u && cur < total && shown_engine() != e0);
    assert(src_engine(preset_at(cur - 1u, &k), k) == e0);   /* (its first: the one before is the engine before's) */
    turn(EN_K2, -1);frames(100);
    assert(preset_pos(&total) == 0u && shown_engine() == e0);
    turn(EN_K2, -1);frames(100);
    assert(preset_pos(&total) == 0u);                   /* (no engine before the first) */
    /* LIST: KNOB 4, no wrap */
    for (uint32_t i = 0; i < LM_N + 2u; i++) turn(EN_K4, 1);
    assert(list_mode() == LM_N - 1u);
    for (uint32_t i = 0; i < LM_N + 2u; i++) turn(EN_K4, -1);
    assert(list_mode() == LM_ALL);
    ui.force = 1;frame();
    puts("PRESETS: KNOB 1 stops at the first patch; KNOB 2 the next engine's first and back; KNOB 4 the list: OK");
    return 0;
}
