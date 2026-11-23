/* veranda_play2.c -- the scripted sessions of tools/gate_veranda2.sh (C2)
 * and tools/gate_veranda3.sh (C3).
 *
 * Linked into make veranda-test2's build only (-DCALC_PLAY2), with
 * src/tools/veranda_play2.s, which plays the script (the pointer, the
 * wheel, the keys) from Veranda's interrupt as its mouseSource. The
 * script (tools/veranda_session2.h) and everything here are in overlay
 * 20 (OVL_PLAY, test builds only): the resident image has no room for a
 * test harness. The host build plays the same session
 * (tools/veranda_golden2.c).
 *
 * At the script's 'L' records and at its end (play_log, seen by
 * main_veranda.c's appMain) the console gets "LOG", then "CELL <name>
 * <text>" for the session's cells, "CURSOR", "SHEETS" and "WIDTH B" -- the
 * lines the host build writes too; at the end the grid is drawn once more
 * first, then "CALC DONE", and the emulator stops with its dump
 * (EMU_SAVE_ON_EXIT). PLAY_SESSION 3 plays tools/veranda_session3.h (the
 * files: gate veranda3), else tools/veranda_session2.h. PLAY_STOP n stops at the script's stop
 * point n instead (a picture of an open menu, an open dialog box). */
#include "../x16sheet.h"
#include "../ui/grid.h"
#include "../ui/vmenu_veranda.h"
#include "../workbook/workbook.h"
#include <veranda.h>
#include <string.h>

extern uint8_t play_stop, play_log;     /* veranda_play2.s */
extern uint8_t play_hpeak;              /* (its bank: overlay 20's) */
extern uint8_t calc_heap_first, calc_heap_banks;
#include "../platform/banked_ram.h"
#include "../platform/bankmem.h"
void play_src(void);

typedef char vm_x_is_103[(VM_X == 103) ? 1 : -1]; /* veranda_play2.s's VM_X */

static char vstatus[] = "VERANDA CALC";  /* resident: the grid keeps the pointer */

#pragma code-name (push, "OVERLAY20")
#pragma rodata-name (push, "OVL20RO")
#pragma bss-name (push, "OVL20BSS")

#define PLAY2_VERANDA
#if PLAY_SESSION == 3
#  include "../../tools/veranda_session3.h"
#elif PLAY_SESSION == 4
#  include "../../tools/veranda_session4.h"
#elif PLAY_SESSION == 5
#  include "../../tools/veranda_session5.h"
#elif PLAY_SESSION == 6
#  include "../../tools/veranda_session6.h"
#elif PLAY_SESSION == 7
#  include "../../tools/veranda_session7.h"
#elif PLAY_SESSION == 8
#  include "../../tools/veranda_session8.h"
#else
#  include "../../tools/veranda_session2.h"
#endif

#define EMU_SAVE    (*(volatile uint8_t *)0x9FB4)   /* EMU_SAVE_ON_EXIT */

#define say() LogText(out)

/* named, so that they are in OVL20RO (a literal is resident RODATA) */
static const char S_log[] = "LOG", S_cell[] = "CELL ", S_cursor[] = "CURSOR ",
    S_sheets[] = "SHEETS", S_dirty[] = "DIRTY 0", S_width[] = "WIDTH B ",
    S_done[] = "CALC DONE", S_heap[] = "HEAP first ", S_hb[] = " banks ",
    S_hp[] = " peak ", S_hn[] = " now ", S_hf[] = " free ";

/* v in decimal at out's end */
static void dec(char *out, uint32_t v)
{
    char t[11];
    uint8_t n = 0, i = (uint8_t)strlen(out);

    do {
        t[n++] = (char)('0' + (uint8_t)(v % 10));
        v /= 10;
    } while (v);
    while (n)
        out[i++] = t[--n];
    out[i] = 0;
}

/* "HEAP first <bank> banks <the heap's> peak <the most in use since the
 * last log> now <in use> free <bytes>": the heap at each log, not in the
 * gates' comparison (the cells' lines only) */
static void heap_log(char *out)
{
    uint8_t now = (uint8_t)(cur_bank - first_bank + 1
                            + (bankmem_bank_count() - first_bank - bank_count));

    strcpy(out, S_heap);
    dec(out, calc_heap_first);
    strcpy(out + strlen(out), S_hb);
    dec(out, calc_heap_banks);
    strcpy(out + strlen(out), S_hp);
    dec(out, play_hpeak > now ? play_hpeak : now);
    strcpy(out + strlen(out), S_hn);
    dec(out, now);
    strcpy(out + strlen(out), S_hf);
    dec(out, bank_bytes_free());
    LogText(out);
    play_hpeak = 0;
}

/* the script's 'L' n (play_log = n) and its end ('E': $FF) */
void play2_log(void)
{
    char out[64];                       /* the C stack: below $A000, for LogText */
    uint8_t i, n, end = (uint8_t)(play_log == 0xFF);

    play_log = 0;
    if (end)
        grid_render_all();
    heap_log(out);
    strcpy(out, S_log);
    say();
    for (i = 0; i < sizeof play2_cells; i += 2) {
        strcpy(out, S_cell);
        grid_cell_name(play2_cells[i], play2_cells[i + 1], out + 5);
        n = (uint8_t)strlen(out);
        out[n++] = ' ';
        wb_display_text(play2_cells[i], play2_cells[i + 1], out + n, 40);
        say();
    }
    strcpy(out, S_cursor);
    grid_cell_name(grid.cur_row, grid.cur_col, out + 7);
    say();
    strcpy(out, S_sheets);
    for (i = 0; i < wb_sheet_n; ++i) {
        n = (uint8_t)strlen(out);
        out[n++] = ' ';
        wb_tab_name(i, out + n);        /* resident (wb_sheet_name: OVL1) */
    }
    say();
    strcpy(out, S_dirty);
    out[6] = (char)('0' + wb_dirty);
    say();
    strcpy(out, S_width);
    n = wb_col_width(1);
    out[8] = (char)('0' + n / 10);
    out[9] = (char)('0' + n % 10);
    out[10] = 0;
    say();
    if (!end)
        return;
    strcpy(out, S_done);
    say();
    EMU_SAVE = 1;
    __asm__("jmp $FFFF");
}

void play2_start(void)
{
    grid_set_status(vstatus);
    grid_render_all();
#ifdef PLAY_STOP
    play_stop = PLAY_STOP;
#endif
    mouseSource = (uint16_t)play_src;
}

#pragma bss-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)
