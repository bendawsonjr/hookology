/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, beta52) SAVE > PRESETS, the patch browser: KNOB 2 up past the first patch lights the category line (no
 * wrap to the last); further up stays there; down again: the first patch, unchanged; KNOB 1 changes the category and
 * unlights it; coming back to the page unlit. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
int main(void)
{
    ui_power_on();
    select_engine(ENGI_PROPHET);frames(200);list_set(LM_CAT + 5u);go_page(GR_BROWSE);frame();
    uint32_t total, cur;
    turn(EN_K2, 1);frames(400);                         /* (onto the list: its first) */
    while ((cur = preset_pos(&total)) != 0u) { turn(EN_K2, -1); frames(400); }
    assert(total > 3u && !br_at_top);
    turn(EN_K2, -1);frames(100);
    assert(br_at_top && preset_pos(&total) == 0u);      /* (lit, still the first: no wrap to the last) */
    turn(EN_K2, -1);frames(100);
    assert(br_at_top && preset_pos(&total) == 0u);
    turn(EN_K2, 1);frames(100);
    assert(!br_at_top && preset_pos(&total) == 0u);     /* (back on the first, not the second) */
    turn(EN_K2, 1);frames(400);
    assert(preset_pos(&total) == 1u);
    while (preset_pos(&total) != 0u) { turn(EN_K2, -1); frames(400); }
    turn(EN_K2, -1);frames(100);assert(br_at_top);
    turn(EN_K1, 1);frames(100);
    assert(!br_at_top && list_mode() == LM_CAT + 6u);   /* (KNOB 1: the next category, unlit) */
    turn(EN_K2, -1);frames(100);                         /* (lit again, then away and back: unlit) */
    if (preset_pos(&total) == 0u) assert(br_at_top);
    go_home();frames(200);go_page(GR_BROWSE);frames(100);
    assert(!br_at_top);
    puts("PRESETS: KNOB 2 up past the first patch lights the category line, no wrap; KNOB 1 and leaving unlight it: OK");
    return 0;
}
