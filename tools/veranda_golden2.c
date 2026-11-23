/* veranda_golden2.c -- the golden of tools/gate_veranda2.sh's session (C2)
 * and, built with -DPLAY_SESSION=3, of tools/gate_veranda3.sh's (C3: run
 * it in a folder with the session's files; it writes T1.X16S and T1.CSV).
 *
 * The host build plays host_script[] (tools/veranda_session2.h, written by
 * tools/veranda_script.py with Veranda's play_script[]): the same session
 * as the X16 build's code sees it --
 *   'K' k      grid_key(k): a key typed into the grid, or a menu item's code
 *   'Q' k      a key into the host's key queue: what was typed into one of
 *              Veranda's dialog boxes, read by the X16 build's own prompts
 *   'C' x y    a click at Veranda's pixel (x, y): grid_mouse() with the
 *              button down, then up (the grid's row y is card row y + 3:
 *              the menu bar and the title band above it)
 *   'H' n      the wheel, n notches
 *   'L' n      the lines Veranda Calc's player logs, to stderr
 * (at each 'L' the screen too, to screen<n>.txt), then repaints the grid
 * and writes the screen to stdout (as
 * tools/veranda_golden.c, and a fourth field: each character's borders,
 * SCREEN_B_* in hex) and the log lines (LOG, CELL, CURSOR, SHEETS,
 * WIDTH B) once more to stderr.
 *
 *   make build/host/veranda_golden2; build/host/veranda_golden2 > golden.txt 2> cells.txt
 */
#include "../src/x16sheet.h"
#include "../src/ui/grid.h"
#include "../src/ui/screen.h"
#include "../src/platform/bankmem.h"
#include "../src/platform/banked_ram.h"
#include "../src/platform/mouse.h"
#include "../src/workbook/workbook.h"
#include <stdio.h>

void kbd_host_push(const uint8_t *keys, uint8_t n);   /* keyboard_host.c */

/* Veranda Calc's grid: 80 x 57 (platform/screen_veranda.c; its row y is
 * card row y + 3). Built with -DX16S_LOOK2: Veranda Look 2's colours. */
#define VR_ROWS 57
void kbd_host_reset(void);

#if PLAY_SESSION == 3
#  include "veranda_session3.h"
#elif PLAY_SESSION == 4
#  include "veranda_session4.h"
#elif PLAY_SESSION == 5
#  include "veranda_session5.h"
#elif PLAY_SESSION == 6
#  include "veranda_session6.h"
#else
#  include "veranda_session2.h"
#endif


/* the lines src/tools/veranda_play2.c logs */
static void host_log(void)
{
    unsigned i;
    uint8_t n;
    char t[64];

    fprintf(stderr, "LOG\n");
    for (i = 0; i < sizeof play2_cells; i += 2) {
        grid_cell_name(play2_cells[i], play2_cells[i + 1], t);
        fprintf(stderr, "CELL %s ", t);
        wb_display_text(play2_cells[i], play2_cells[i + 1], t, 40);
        fprintf(stderr, "%s\n", t);
    }
    grid_cell_name(grid.cur_row, grid.cur_col, t);
    fprintf(stderr, "CURSOR %s\nSHEETS", t);
    for (n = 0; n < wb_sheet_n; ++n) {
        wb_tab_name(n, t);
        fprintf(stderr, " %s", t);
    }
    fprintf(stderr, "\nDIRTY %u\nWIDTH B %02u\n", wb_dirty, wb_col_width(1));
}

/* the text grid: per line 80 characters, a tab, 80 attributes in hex, a
 * tab, 80 bold digits, a tab, 80 border digits (tools/check_veranda.py) */
static void put_screen(FILE *f)
{
    uint8_t x, y;

    for (y = 0; y < VR_ROWS; ++y) {
        fprintf(f, "%s\t", screen_host_row(y));
        for (x = 0; x < 80; ++x)
            fprintf(f, "%02X", screen_host_color(x, y));
        fputc('\t', f);
        for (x = 0; x < 80; ++x)
            fputc('0' + screen_host_bold(x, y), f);
        fputc('\t', f);
        for (x = 0; x < 80; ++x)
            fputc("0123456789ABCDEF"[screen_host_border(x, y) & 15], f);
        fputc('\n', f);
    }
}

int main(void)
{
    unsigned i;


    if (!bankmem_host_init(64) || bank_heap_init(1, 0) != ERR_OK || wb_init() != ERR_OK)
        return 1;
    screen_host_init(80, VR_ROWS);      /* Veranda: card rows 0-3 are its menu
                                           bar and the title band */
    if (grid_init() != ERR_OK)
        return 1;
    grid_set_status("VERANDA CALC");
    grid_render_all();
    for (i = 0; i < sizeof host_script; i += 5) {
        const unsigned char *r = host_script + i;
        uint16_t px = (uint16_t)(r[1] | r[2] << 8), py = (uint16_t)(r[3] | r[4] << 8);

        switch (r[0]) {
        case 'K':
            grid_key(r[1]);
            break;
        case 'Q':
            /* the first of a run: the keys before it were all taken by
             * the command they were for (the host's queue never wraps) */
            if (i == 0 || r[-5] != 'Q')
                kbd_host_reset();
            kbd_host_push(&r[1], 1);
            break;
        case 'C':
            grid_mouse(px, (uint16_t)(py - 24), MOUSE_LEFT, 0);
            grid_mouse(px, (uint16_t)(py - 24), 0, 0);
            break;
        case 'H':
            grid_mouse(0, 0, 0, (int8_t)r[1]);
            break;
        case 'L':
            host_log();
            {   /* and the screen as it is: screen<n>.txt */
                static unsigned n;
                char name[16];
                FILE *f;

                sprintf(name, "screen%u.txt", ++n);
                if ((f = fopen(name, "w")) != NULL) {
                    put_screen(f);
                    fclose(f);
                }
            }
            break;
        }
    }
    grid_render_all();
    put_screen(stdout);
    host_log();
    return 0;
}
