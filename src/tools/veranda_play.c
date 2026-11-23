/* veranda_play.c -- the scripted session of tools/gate_veranda.sh.
 *
 * Linked into the test build only (make veranda-test; -DCALC_PLAY). A
 * Veranda process puts the script's keys, one at a time, into the ROM's
 * key queue (KBDBUF_PUT), each when the last has been taken: they reach
 * the program as typed keys do -- GETIN, Veranda's translation, keyVector,
 * calc_key(), grid_key(). The host build plays the same keys
 * (tools/veranda_golden.c, from tools/veranda_keys.h) for the golden.
 *
 * Console: "TIME render <cycles, hex>" (a full grid_render_all, 8 MHz),
 * "CALC A3 <text>", "CALC DONE"; then the emulator is stopped with its
 * dump (EMU_SAVE_ON_EXIT). */
#include "../x16sheet.h"
#include "../ui/grid.h"
#include "../workbook/workbook.h"
#include <veranda.h>
#include <vr.h>
#include <string.h>

#pragma rodata-name (push, "OVL20RO")  /* (the resident image is short) */
#include "../../tools/veranda_keys.h"   /* play_keys[], PLAY_N */
#pragma rodata-name (pop)
#include "../platform/overlay.h"
extern uint8_t ovl_bank[];              /* platform/overlay_veranda.c */

/* All but the process in overlay 20 (OVL_PLAY, test builds only): the resident image has no room for a test harness. */
static void finish(void);
static void step(void);
static uint8_t at, kk;
static uint16_t procs[2];
static char vstatus[] = "VERANDA CALC";  /* resident (DATA): the grid keeps the pointer */

/* the process: the next key once the last has been taken (resident) */
static void step(void)
{
    uint8_t b;

    if (pressFlag & SET_KEYPRESS)
        return;
    __asm__("jsr $FEBD");               /* KBDBUF_PEEK: x = how many */
    __asm__("stx %v", kk);
    if (kk)
        return;
    if (at < PLAY_N) {
        b = *(volatile uint8_t *)0;     /* the keys are in overlay 20's bank */
        *(volatile uint8_t *)0 = ovl_bank[OVL_PLAY];
        kk = play_keys[at++];
        *(volatile uint8_t *)0 = b;
        __asm__("lda %v", kk);
        __asm__("jsr $FEC3");           /* KBDBUF_PUT */
    } else {
        ovl_require(OVL_PLAY);
        finish();
    }
}

#pragma code-name (push, "OVERLAY20")
#pragma rodata-name (push, "OVL20RO")

#define EMU_CLOCK   ((volatile uint8_t *)0x9FB8)
#define EMU_SAVE    (*(volatile uint8_t *)0x9FB4)   /* EMU_SAVE_ON_EXIT */

extern char scr_line[];                 /* screen_veranda.c's: free after */
#define out scr_line                    /* the render (resident, 80 bytes) */
static const char hexd[] = "0123456789ABCDEF";
/* named, so that they are in OVL20RO (a literal is resident RODATA) */
static const char S_time[] = "TIME ", S_render[] = "render", S_render0[] = "render0",
    S_a3[] = "CALC A3 ", S_done[] = "CALC DONE";

static uint32_t clock(void)
{
    uint32_t t;

    t = EMU_CLOCK[0];                   /* (reading byte 0 latches the rest) */
    t |= (uint32_t)EMU_CLOCK[1] << 8;
    t |= (uint32_t)EMU_CLOCK[2] << 16;
    t |= (uint32_t)EMU_CLOCK[3] << 24;
    return t;
}

static void log_time(const char *name, uint32_t t)
{
    uint8_t i, n;

    strcpy(out, S_time);
    strcat(out, name);
    n = (uint8_t)strlen(out);
    out[n++] = ' ';
    for (i = 0; i < 8; ++i)
        out[n++] = hexd[(uint8_t)(t >> (28 - 4 * i)) & 15];
    out[n] = 0;
    LogText(out);
}

static void finish(void)
{
    uint32_t t;

    t = clock();
    grid_render_all();
    log_time(S_render, clock() - t);
    strcpy(out, S_a3);
    wb_display_text(2, 0, out + 8, 30);
    LogText(out);
    strcpy(out, S_done);
    LogText(out);
    EMU_SAVE = 1;
    __asm__("jmp $FFFF");
}

void play_start(void)
{
    uint32_t t;

    grid_set_status(vstatus);
    t = clock();
    grid_render_all();
    log_time(S_render0, clock() - t);
    procs[0] = VR_FN(step);
    procs[1] = 3;                       /* every 3 frames */
    InitProcesses(procs, 1);
    RestartProcess(0);
}

#pragma rodata-name (pop)
#pragma code-name (pop)
