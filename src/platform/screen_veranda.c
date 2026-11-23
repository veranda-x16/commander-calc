/* screen_veranda.c -- screen.h on Veranda's 640x480 bitmap.
 *
 * The grid stays 80 x 60 character cells: one cell is one 8 x 8 card of
 * the bitmap, so grid.c's arithmetic and the host tests hold unchanged.
 * Characters go through PutCards (Veranda API 2) with an 8 x 8 font copied
 * at start-up from the X16 ROM's ISO charset (ROM bank 6, $C800: the font
 * the X16 build shows), glyphs $20-$7F; a control code shows as a space.
 *
 * Colour: Veranda's bitmap has one ink colour and a paper colour per card
 * (its colour map, ColorRect). A cell's background becomes its card's
 * paper; the ink is always palette entry 1 (black: entry 0 and 1 are
 * swapped on Veranda as in this program's own palette). A light text on
 * black (the cursor, the edit line) becomes black text on the light
 * colour: COL_WHITE on black is the cursor colour, COL_LBLUE. Dark
 * backgrounds under black text become their light twins. tools/veranda_
 * golden.py makes the same choice (scr_paper, shadow_veranda.s).
 *
 * Bold is PutCards' bold (v OR v >> 1).
 *
 * Borders: lines on the cards' edges, drawn after the text (PutCards is
 * opaque). grid.c gives a cell's borders (screen_border) just before its
 * text, at the same place: they are kept until that text is drawn (out),
 * then drawn over it -- the left edge on the first card's left column, the
 * right on the last's right column, top and bottom along every card's top
 * and bottom row. The X16 build draws them on a layer behind the text;
 * nothing else changes. tools/check_veranda.py checks the same pixels.
 *
 * Only what changed is drawn: a shadow of every card's character, bold,
 * paper and borders in two RAM banks of the program's own
 * (shadow_veranda.s) is compared with each run of text first, and only
 * the cards between the first and the last that differ are drawn, the
 * paper set only if it changed. screen_clear() draws nothing: a redraw
 * writes every card again, and those that already show what they should
 * cost a comparison. What draws past this file (a chart, ui/chart.c) clears
 * the screen and the shadow itself (screen_vinvalidate there).
 *
 * The window (Veranda Look 2, platform/look_veranda.c): Veranda's menu
 * bar (main_veranda.c, DoMenu) takes card rows 0 and 1, the title band
 * (the document's name) rows 2 and 3. The grid is 80 x 57: its row y is
 * card row y + 3, and its row 0 -- the X16 build's own menu bar -- is not
 * drawn at all (it lies under the title band's second row). Its row 1,
 * the formula bar, is the window's toolbar; its last two rows the sheets'
 * tabs and the status bar, whose second card row (59) is the grid's last
 * row, blank but for a message.
 *
 * Colours: Veranda Look 2's palette (look_veranda.c sets it), an X16
 * colour as Look 2's nearest (shadow_veranda.s's scr_paper): white the
 * paper, the greys the chrome, the blues the selection colour. */
#include "../ui/screen.h"
#include <veranda.h>
#include <string.h>

#define ROM_BANK (*(volatile uint8_t *)0x0001)
#define COLS 80
#define ROWS 57                         /* the grid's; the screen has 60 */
#define TOP 3                           /* the grid's row y: card row y + TOP
                                           (shadow_veranda.s's TOPROW) */

/* $20-$7F: glyph c at font + 8 (c - $20). Before screen_init() fills it,
 * util/number_x16.s's zero page probe borrows it (Veranda). */
#pragma bss-name (push, "FONTBSS")    /* (cfg/veranda.cfg: shared with the */
uint8_t screen_font[96 * 8];            /* Open and Save dialog's BSS) */
#pragma bss-name (pop)
#define font screen_font
#define FONT_AT (font - 0x100)
#pragma bss-name (push, "LINEBSS")    /* (shared with vrwin.c's BSS) */
char scr_line[COLS];                    /* the run being drawn */
#pragma bss-name (pop)
#define line scr_line
#define bold scr_bold
static uint8_t bord_on;
#define by scr_by
uint8_t scr_bx, scr_blen, scr_bbits;    /* a cell's borders, until its text */
#define bx    scr_bx
#define blen  scr_blen
#define bbits scr_bbits

/* shadow_veranda.s */
void scr_out(void);
void scr_reset(void);
uint8_t __fastcall__ scr_paper(color_t c);
extern uint8_t scr_sbank, scr_ox, scr_oy, scr_on, scr_op, scr_by, scr_bold;
#define RAM_BANK (*(volatile uint8_t *)0x0000)


/* The ROM's ISO font into screen_font: at the start (screen_init), and
 * after the Open and Save dialog, whose BSS it holds meanwhile (in the
 * file dialog's overlay: ui/filedlg.c; screen_font_veranda.h). */
/* Start-up, once: in the start-up overlay (OVL_CHARTOUT, which Veranda's
 * chart does without; main_veranda.c maps it first) */
#pragma code-name (push, "OVERLAY16")
#include "screen_font_veranda.h"
err_t screen_init(void)
{
    screen_font_load();
    dispBufferOn = ST_WR_FORE;
    scr_sbank = BankAlloc(2);           /* the shadow (0: none, draw it all) */
    SetPattern(0);
    WRectangle(0, (TOP + 1) * 8, 639, 479);
    ColorRect(0, COLS, TOP + 1, ROWS - 1, 0);
    scr_reset();                        /* blank cards on paper 0 */
    return ERR_OK;
}
#pragma code-name (pop)

void screen_reset_charset(void) { }
void screen_paper(uint8_t on) { (void)on; }
void screen_shutdown(void) { }
void screen_bold(uint8_t on) { bold = on ? 2 : 0; }
void screen_borders(uint8_t on) { bord_on = on; }

void screen_border(uint8_t x, uint8_t y, uint8_t len, uint8_t bits)
{
    bbits = bord_on ? bits : 0;
    bx = x;
    by = y;
    blen = len;
}

void __fastcall__ screen_vborders(uint8_t y);    /* border_veranda.s */


uint8_t screen_cols(void) { return COLS; }
uint8_t screen_rows(void) { return ROWS; }

/* Nothing with the shadow: what follows draws every card
 * (grid_render_all), and the shadow knows what each one shows. Without one
 * (no banks for it), the grid's area cleared. */
void screen_clear(color_t c)
{
    if (scr_sbank)
        return;
    SetPattern(0);
    WRectangle(0, (TOP + 1) * 8, 639, 479);  /* not the menu bar */
    ColorRect(0, COLS, TOP + 1, ROWS - 1, scr_paper(c));
}

/* screen_put, screen_fill, screen_text, screen_text_right, screen_recolor:
 * shadow_veranda.s */
