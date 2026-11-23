/* clip_veranda.c -- Veranda's clipboard in Veranda Calc (make veranda).
 *
 * Veranda's system clipboard is GEOS's Text Scrap, a file in its system
 * folder (Veranda's DECISIONS V54; the SDK's scrap.h). Calc keeps its own
 * clipboard (ui/menu.c: the cells' texts and styles, formulas as
 * formulas) and adds this to it, on Veranda only:
 *   - Copy (^C) also puts the selected cells on the Text Scrap as text:
 *     each cell as the grid shows it, a TAB between the cells of a row, a
 *     CR after each row (what other programs paste);
 *   - Paste (^V) takes the Text Scrap when another program wrote it after
 *     Calc's last copy (its stamp is not the one Calc's copy left): its
 *     lines are rows, its TABs separate cells, from the cursor down and
 *     right, each read as typed (numbers, TRUE/FALSE, text; =... a
 *     formula); else Calc's own paste.
 *
 * The scrap library's sources (the SDK's src/scrapio.c, scraptbeg.c) and
 * the code here run in OVL_CLIP, an overlay of their own; what stays
 * resident is the key's hook (clipkey_veranda.s), which also sets a
 * pasted formula (wb_set_text() then loads the formula compiler, so it is
 * not called from the overlay for one) and redraws. */
#include "../x16sheet.h"
#include "../ui/grid.h"
#include "../ui/editor.h"
#include "../workbook/workbook.h"
#include "overlay.h"
#include <scrap.h>
#include <string.h>

#define CELL_MAX 56

/* below $A000: the scrap's name, which the kernel reads, and a formula's
 * text, which the compiler's overlay reads: the file layer's bounce
 * buffer, free between file commands (-DSCRAP_PATH=fio_bounce: its first
 * 32 bytes) */
extern char fio_bounce[];               /* file_buf_veranda.c */
#define cell (fio_bounce + 32)
uint16_t clip_r, clip_c;                /* the cell clip_key sets (clipkey_veranda.s) */

/* clip_key(), the resident part: clipkey_veranda.s */

/* ---- OVL_CLIP ---------------------------------------------------------- */
#pragma code-name (push, "OVERLAY21")
#pragma rodata-name (push, "OVL21RO")
#pragma bss-name (push, "OVL21BSS")

static char buf[CELL_MAX + 2];
static uint8_t in[64], ii, ik, skip, eof, was;
static uint8_t tab, row;                /* the last cell ended in a TAB, a CR */
static uint16_t left, r0, c0, dr, dc;
static uint8_t mine[8];         /* the stamp Calc's last copy left (never
                                   zeroed: before a copy, any stamp is not
                                   Calc's) */
#define cr clip_r
#define cc clip_c

/* the cell as the grid shows it, without the spaces around it; non-ASCII
 * as '?' */
static uint8_t text_of(uint16_t r, uint16_t c)
{
    uint8_t n, i = 0, j;

    n = wb_display_text(r, c, buf, CELL_MAX);
    buf[n > CELL_MAX ? CELL_MAX : n] = 0;
    while (buf[i] == ' ')
        ++i;
    for (n = 0; buf[i]; ++i) {
        j = (uint8_t)buf[i];
        buf[n++] = (char)(j >= 32 && j < 127 ? j : '?');
    }
    while (n && buf[n - 1] == ' ')
        --n;
    return n;
}

uint8_t clip_copy(void)
{
    static const uint8_t cs[4] = { SCRAP_NEWCARDSET, 9, 0, 0 };
    uint16_t r, c, len = 4;
    uint8_t pass, n, e = 0;

    for (pass = 0; pass < 2 && !e; ++pass) {
        if (pass) {
            e = scrap_text_begin(len);
            if (!e)
                e = scrap_write(cs, 4);
        }
        for (r = sel_r1; r <= sel_r2 && !e; ++r)
            for (c = sel_c1; c <= sel_c2 && !e; ++c) {
                n = text_of(r, c);
                buf[n++] = (char)(c == sel_c2 ? 13 : 9);
                if (pass)
                    e = scrap_write(buf, n);
                else
                    len += n;
            }
    }
    if (scrap_close() && !e)
        e = 1;
    scrap_stamp(SCRAP_TEXT, mine);
    return e;
}

/* 1: the Text Scrap is another program's: open, for clip_field() */
static uint8_t clip_fetch(void)
{
    uint8_t st[8];

    if (scrap_stamp(SCRAP_TEXT, st) || !memcmp(st, mine, 8))
        return 0;
    if (!scrap_open(SCRAP_TEXT) || scrap_read(&left, 2) != 2) {
        scrap_close();
        return 0;
    }
    ii = ik = skip = eof = tab = row = 0;   /* (an overlay's BSS is never zeroed) */
    dr = dc = 0;
    return 1;
}

/* the next cell's text into cell, its place in dr, dc (from the
 * cursor); 0 at the end (the scrap closed) */
static uint8_t nextc(void)
{
    if (ii == ik) {
        ik = left ? (uint8_t)scrap_read(in, left > sizeof in ? sizeof in : left) : 0;
        left -= ik;
        ii = 0;
        if (!ik)
            return 0;
    }
    return in[ii++];
}


static uint8_t clip_field(void)
{
    uint8_t c, n = 0;

    if (eof) {
        scrap_close();
        return 0;
    }
    if (tab)
        ++dc;
    if (row) {
        ++dr;
        dc = 0;
    }
    tab = row = 0;
    for (;;) {
        c = nextc();
        if (!c && !ik) {                /* the end */
            eof = 1;
            if (!n) {
                scrap_close();
                return 0;
            }
            break;
        }
        if (skip) {
            --skip;
            continue;
        }
        if (c == SCRAP_NEWCARDSET)
            skip = 3;
        else if (c == SCRAP_ESC_RULER)
            skip = 26;
        else if (c == SCRAP_ESC_GRAPHICS)
            skip = 4;
        else if (c == 9) {
            tab = 1;
            break;
        } else if (c == 13 || c == SCRAP_PAGE_BREAK) {
            row = 1;
            break;
        } else if (c >= 32 && c < 127 && n < CELL_MAX)
            cell[n++] = (char)c;
    }
    cell[n] = 0;
    return 1;
}

/* k 0x03: the copy, then 3 (Calc's own copy follows); 0x16: the paste:
 * 3 if the Text Scrap is Calc's own (Calc's paste then), else 2 for each
 * of its cells (in cell, clip_r, clip_c: the resident part sets it, as
 * typed: wb_set_text() may load the formula compiler; then k 0: go on),
 * 1 at the end */
uint8_t clip_run(uint8_t k)
{
    if (editor_active())                /* (the cell editor's keys) */
        return 3;
    if (k == 0x03) {
        clip_copy();
        return 3;                       /* then Calc's own copy */
    }
    if (k == 0x16) {
        if (!clip_fetch())
            return 3;
        r0 = grid_state()->cur_row;
        c0 = grid_state()->cur_col;
        was = wb_manual;                /* one recalculation, at the end */
        wb_manual = 1;
    }
    while (clip_field()) {
        cr = r0 + dr;
        cc = c0 + dc;
        if (cr < X16S_MAX_ROWS && cc < X16S_MAX_COLS)
            return 2;                   /* set from resident code */
    }
    wb_manual = was;
    return 1;
}

#pragma bss-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)
