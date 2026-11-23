/* grid_priv.h -- what grid.c shares with grid_rows.c, the grid's rows.
 *
 * GRID_FAST (the host build and Veranda's): the rows are drawn by
 * grid_rows.c, a row at a time, its columns laid out once a redraw. On
 * Veranda that file is all in OVL_FAR's bank (OVL_CHARTOUT) and grid.c
 * calls it through ovl_far(): the far_* routines, their arguments in
 * far_row, far_col, far_sy (resident). The X16 build keeps grid.c's own
 * (its resident image has no room for either).
 */
#ifndef X16S_GRID_PRIV_H
#define X16S_GRID_PRIV_H

#include "grid.h"
#include "screen.h"

#if !defined(__CC65__) || defined(X16S_VERANDA)
#  define GRID_FAST 1
#endif

/* --- palette (the rest is grid.c's) --- */
#define C_HEADER  COLOR(COL_BLACK, COL_LGREY)   /* row and column labels */
/* White paper, black figures -- Excel 3 rather than a terminal. */
#define C_CELL    COLOR(COL_BLACK, COL_WHITE)   /* the paper            */
#define C_CURSOR  COLOR(COL_WHITE, COL_BLACK)   /* reverse video        */
#ifdef X16S_LOOK2
/* Veranda Look 2 (Veranda Calc, and its goldens on the host): the headers
 * the chrome (light grey), the cursor's header and a selected block the
 * selection colour, as the cursor is -- light blue, which a block needs
 * an attribute of its own for (screen_veranda.c's scr_paper maps the
 * X16's colours onto Look 2's palette) */
#  define C_HDR_SEL COLOR(COL_BLACK, COL_LBLUE)
#  define C_SEL     COLOR(COL_BLACK, COL_LBLUE)
#else
#  define C_HDR_SEL COLOR(COL_BLACK, COL_YELLOW) /* ...where the cursor is */
#  define C_SEL     COLOR(COL_BLACK, COL_LGREY)  /* a selected block     */
#endif

#define ROW_COLHDR   2
#define ROW_GRID0    3

/* The selection, sorted (grid.c's sel_bounds), and whether a cell is in it. */
extern uint16_t sel_r1, sel_c1, sel_r2, sel_c2;
#define in_sel(row, col) (grid.sel_on \
    && (row) >= sel_r1 && (row) <= sel_r2 \
    && (col) >= sel_c1 && (col) <= sel_c2)

/* grid.c's geometry */
uint16_t row_at(uint8_t sy);            /* the sheet row on screen row sy */
uint16_t col_nth(uint16_t i);           /* the sheet column i-th across */
uint8_t  col_w(uint16_t c);
uint8_t  clip_w(uint8_t x, uint8_t cw);
uint8_t  col_visible_w(uint16_t col);

#ifdef GRID_FAST
extern uint16_t far_row, far_col;
extern uint8_t far_sy;
void far_grid(void);                    /* the rows, all of them */
void far_colhdr(void);                  /* the column letters */
void far_repaint(void);                 /* the cell far_row, far_col */
void far_rowhdr(void);                  /* screen row far_sy's header */
void far_scroll(void);                  /* the rows moved one (grid_goto) */
#endif

#endif /* X16S_GRID_PRIV_H */
