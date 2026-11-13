/* SPDX-License-Identifier: GPL-3.0-only */
/* (Hookology, beta54) the boot splash (ui_draw.c splash_draw) in STAGE and DAYLIGHT: DIR/splash_<PALETTE>.ppm; checks the
 * mark is drawn in track 1's colour and the wordmark in TEXT */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    ui_power_on();
    for (uint32_t p = 0; p < 2u; p++) {
        palette_set(p);
        splash_draw();
        uint32_t mark = 0, word = 0;
        for (uint32_t i = 0; i < 240u * 240u; i++) {
            uint16_t c = swap16(host_screen[i]);
            if (c == UI_PALETTES[p].trk[0]) mark++;
            if (c == UI_PALETTES[p].text) word++;
        }
        assert(mark > 1500u && word > 1000u);
        char path[512];
        snprintf(path, sizeof path, "%s/splash_%s.ppm", dir, UI_PALETTES[p].name);
        FILE *f = fopen(path, "wb");
        if (f) {
            fprintf(f, "P6\n240 240\n255\n");
            for (uint32_t i = 0; i < 240u * 240u; i++) {
                uint16_t c = swap16(host_screen[i]);
                uint8_t b[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
                fwrite(b, 1, 3, f);
            }
            fclose(f);
        }
        printf("splash %s: mark %u px in track 1's colour, wordmark %u px in TEXT\n", UI_PALETTES[p].name, mark, word);
    }
    return 0;
}
