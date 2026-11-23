/* vrfile_veranda.c -- the SDK's Open and Save dialog in Veranda Calc
 * (make veranda): its call, in overlay 22 (OVL_VRFILE) with the SDK's
 * src/vrchoose.c, which takes nearly all of that bank (Makefile: compiled
 * there, its BSS in cfg/veranda.cfg's VRFB, which is screen_font's).
 *
 * ui/filedlg.c (overlay 1) sets the dialog up -- its title and type
 * filter copied into vrf_text, below $A000 -- and calls vrf_run through
 * ovl_far22; this runs vr_file with that overlay unmapped. */
#include "../x16sheet.h"
#include <veranda.h>
#include <vrchoose.h>

/* the call's arguments and result (resident: this file's BSS) */
uint8_t vrf_mode, vrf_res;
char *vrf_name;
extern char vrf_text[];                 /* ui/filedlg.c (cfg/veranda.cfg's VRFB) */
extern uint8_t calc_idle;               /* keyboard_veranda.c */

#pragma code-name (push, "OVERLAY22")
#pragma rodata-name (push, "OVL22RO")

/* every pass of the box's own loop: the program is idle (a test's
 * player goes on: src/tools/veranda_play2.s) */
static void idle(void)
{
    calc_idle = 1;
}

void vrf_run(void)
{
    vr_file_hook = idle;                /* (the dialog's BSS is the font's: */
    vrf_res = vr_file(vrf_mode, vrf_text, vrf_name);   /* set each time) */
}

#pragma rodata-name (pop)
#pragma code-name (pop)
