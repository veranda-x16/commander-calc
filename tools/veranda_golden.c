/* veranda_golden.c -- the golden of tools/gate_veranda.sh's session.
 *
 * The host build (screen_host.c's text grid) plays tools/veranda_keys.h's
 * keys straight into grid_key(), as Veranda Calc does through its key
 * queue, then repaints the grid once (as src/tools/veranda_play.c does
 * before its dump) and writes the screen: 57 lines (the Veranda grid's 80 x
 * 57, built with -DX16S_LOOK2: Veranda Look 2's colours) of "cc..." -- 80
 * characters, then a tab and 80 attribute bytes as hex. tools/
 * check_veranda.py draws it with the X16 ROM's font and compares it with
 * Veranda's VRAM card for card.
 *
 *   make build/host/veranda_golden; build/host/veranda_golden > golden.txt
 */
#include "../src/x16sheet.h"
#include "../src/ui/grid.h"
#include "../src/ui/screen.h"
#include "../src/platform/bankmem.h"
#include "../src/platform/banked_ram.h"
#include "../src/workbook/workbook.h"
#include <stdio.h>

#include "veranda_keys.h"

int main(void)
{
    uint8_t i, x, y;
    char a3[32];

    if (!bankmem_host_init(64) || bank_heap_init(1, 0) != ERR_OK || wb_init() != ERR_OK)
        return 1;
    screen_host_init(80, 57);     /* Veranda: card rows 0-3 are its menu bar
                                     and the title band */
    if (grid_init() != ERR_OK)
        return 1;
    grid_set_status("VERANDA CALC");
    grid_render_all();
    for (i = 0; i < PLAY_N; ++i)
        grid_key(play_keys[i]);
    grid_render_all();
    for (y = 0; y < 57; ++y) {
        printf("%s\t", screen_host_row(y));
        for (x = 0; x < 80; ++x)
            printf("%02X", screen_host_color(x, y));
        printf("\t");
        for (x = 0; x < 80; ++x)
            putchar('0' + screen_host_bold(x, y));
        printf("\n");
    }
    wb_display_text(2, 0, a3, 30);
    fprintf(stderr, "A3 %s\n", a3);
    return 0;
}
