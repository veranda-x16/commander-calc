/* grid_rows.c -- the grid's rows, a row at a time (grid_priv.h).
 *
 * The visible columns laid out once a redraw (layout), each row's records
 * read once and walked in column order (cells_row), a style read once a
 * redraw (sc_*), a run of empty cells drawn as one, and whole numbers
 * written without the ROM's floating point (num_int_text). What is drawn is
 * what grid.c's own cell by cell drawing draws (the X16 build's): the host
 * build's tests and the Veranda gates' screens compare it.
 *
 * On Veranda all of it is in OVL_FAR's bank, called through ovl_far(): the
 * resident image has no room, and that overlay's bank had (Veranda Calc's
 * redraw work). Everything it calls is resident.
 */
#include "grid_priv.h"

#ifdef GRID_FAST
#include "../workbook/workbook.h"
#include "../workbook/styles_priv.h"   /* sty_bord */
#include "../workbook/strings.h"
#include "../formula/formula.h"         /* FR_KIND */
#include "../platform/banked_ram.h"
#include <string.h>

#ifdef X16S_VERANDA
#  pragma code-name (push, "OVERLAY16")
#  pragma rodata-name (push, "OVL16RO")
#  pragma bss-name (push, "OVL16BSS")
uint8_t __fastcall__ num_int_text(const uint8_t *v, char *out);  /* platform/numint_veranda.s */
#else
/* A whole number below 2^29 as its digits (what the ROM's FOUT writes for
 * it, less the sign column), or 0 if it is not one: anything else goes the
 * long way, through the floating point. */
static uint8_t num_int_text(const uint8_t *b, char *out)
{
    int8_t e;
    uint32_t m, v;
    char tmp[12];
    uint8_t n = 0, k = 0;

    if (b[0] == 0) {
        out[0] = '0';
        out[1] = '\0';
        return 1;
    }
    e = (int8_t)(b[0] - 128);
    if (e < 1 || e > 29)
        return 0;
    m = ((uint32_t)(b[1] | 0x80) << 24) | ((uint32_t)b[2] << 16)
      | ((uint32_t)b[3] << 8) | b[4];
    if ((uint32_t)(m << e) != 0)
        return 0;                       /* a fraction */
    v = m >> (32 - e);
    if (b[1] & 0x80)
        out[k++] = '-';
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        out[k++] = tmp[--n];
    out[k] = '\0';
    return k;
}
#endif

#define LAY_MAX 80
static uint8_t lay_n;                   /* the visible columns: */
static uint8_t lay_col[LAY_MAX];        /* sheet column (< 256), */
static uint8_t lay_x[LAY_MAX];          /* screen x, */
static uint8_t lay_w[LAY_MAX];          /* the width drawn */
static cell_style_t sc_st[X16S_MAX_STYLES];
static uint8_t sc_ok[X16S_MAX_STYLES];  /* sc_st[i] read this redraw */

static void layout(void)
{
    uint16_t c, i;
    uint8_t x = GRID_ROWHDR_W, n = 0;

    for (i = 0; (c = col_nth(i)) < X16S_MAX_COLS && x < screen_cols()
                && n < LAY_MAX; ++i) {
        uint8_t cw = col_w(c);

        lay_col[n] = (uint8_t)c;
        lay_x[n] = x;
        lay_w[n] = clip_w(x, cw);
        ++n;
        x = (uint8_t)(x + cw);
    }
    lay_n = n;
    memset(sc_ok, 0, sizeof sc_ok);
}

static const cell_style_t *style_of(uint8_t id)
{
    cell_style_t st;                    /* (the C stack: below the bank window) */

    if (id >= X16S_MAX_STYLES)
        id = X16S_MAX_STYLES - 1;       /* (styles_get's default; not in use) */
    if (!sc_ok[id]) {
        styles_get(id, &st);
        sc_st[id] = st;
        sc_ok[id] = 1;
    }
    return &sc_st[id];
}

/* The text a number shows in its style, if it is a whole number of a style
 * num_int_text can write: General, Integer, Decimal, Currency. 0: not. */
static uint8_t num_fast(const uint8_t *v, const cell_style_t *st, char *out)
{
    uint8_t places, n, d;

    switch (st->number_format) {
    case NF_PERCENT:
    case NF_DATE:
        return 0;
    case NF_INTEGER:
        places = 0;
        break;
    case NF_CURRENCY:
        *out++ = '$';
        /* fall through */
    case NF_DECIMAL:
        places = st->decimal_places;
        if (places > 9)
            places = 9;
        break;
    default:                            /* General, and as General */
        return num_int_text(v, out);
    }
    n = num_int_text(v, out);
    d = (uint8_t)(n - (out[0] == '-'));
    if (n == 0 || d + places > 9)       /* (snum_to_fixed's 32 bits) */
        return 0;
    if (places) {
        out[n++] = '.';
        while (places--)
            out[n++] = '0';
        out[n] = '\0';
    }
    return (uint8_t)(n + (st->number_format == NF_CURRENCY));
}

/* The cell being drawn (render_cell_rec's arguments: statics are cheaper
 * than cc65's C stack): where, how wide, its colour, its record (0: none). */
static uint8_t cl_x, cl_y, cl_w;
static color_t cl_c;
static const cell_record_t *cl_rec;

/* The colour of the cell at (row, col): the cursor's, the block's, the
 * paper's. */
static color_t cell_color(uint16_t row, uint16_t col)
{
    if (row == grid.cur_row && col == grid.cur_col)
        return C_CURSOR;
    return in_sel(row, col) ? C_SEL : C_CELL;
}

/* Draw one cell (cl_*). Values are clipped to the column rather than
 * spilling into the neighbour. */
static void render_cell_rec(void)
{
    char text[WB_TEXT_MAX];             /* (the C stack: below the bank window) */
    const cell_record_t *rec = cl_rec;
    const cell_style_t *st;
    uint8_t b, a, right;

    if (!rec) {
        screen_text(cl_x, cl_y, "", cl_w, cl_c);
        if (sty_bord)
            screen_border(cl_x, cl_y, cl_w, 0); /* clears the last repaint's lines */
        return;
    }
    st = style_of(rec->style);
    b = st->flags;
    /* Borders are on the layer behind the text: the style bits ARE the
     * tile, three places up (styles.h). With none in the workbook the
     * layer is off and screen_border() would draw nothing. */
    if (sty_bord)
        screen_border(cl_x, cl_y, cl_w, (uint8_t)(b >> 3));

    right = rec->type == CELL_NUMBER || rec->type == CELL_DATE;
    switch (rec->type) {
    case CELL_TEXT:                     /* no more than fits: it is clipped */
        strpool_get((uint16_t)(rec->val[0] | ((uint16_t)rec->val[1] << 8)),
                    text, (uint16_t)(cl_w + 1));
        break;
    case CELL_NUMBER:
    case CELL_DATE:
        if (num_fast(rec->val, st, text))
            break;
        /* fall through */
    default:
        wb_display_rec(rec, text, sizeof text);
        if (rec->type == CELL_FORMULA) {
            handle_t fr = (handle_t)rec->val[0]
                        | ((handle_t)rec->val[1] << 8)
                        | ((handle_t)rec->val[2] << 16)
                        | ((handle_t)rec->val[3] << 24);
            a = bank_peek(fr, FR_KIND);
            right = a == CELL_NUMBER || a == CELL_DATE;
        }
    }
    a = b & STY_ALIGN_MASK;             /* wb_align_right's rule */
    if (a == STY_ALIGN_LEFT)
        right = 0;
    else if (a == STY_ALIGN_RIGHT)
        right = 1;

    /* Off again before returning: screen_bold() is a mode, and the row
     * header and everything else on the row would inherit it. */
    if (b & STY_BOLD)
        screen_bold(1);
    if (right)
        screen_text_right(cl_x, cl_y, text, cl_w, cl_c);
    else
        screen_text(cl_x, cl_y, text, cl_w, cl_c);
    if (b & STY_BOLD)
        screen_bold(0);
}

/* A data row's header: its number, highlighted on the cursor's row, as
 * one run (right-aligned in GRID_ROWLBL_W, then a space). 0 past the last
 * row (nothing drawn). The label is kept: the next row's is one more,
 * counted up in its digits; any other worked out by subtraction. */
static char lbl[GRID_ROWHDR_W + 1];
static uint16_t lbl_row = 0xFFFF;       /* (0xFFFF: none; past the last row) */

static uint8_t render_rowhdr(uint8_t sy)
{
    static const uint16_t p10[4] = { 10000, 1000, 100, 10 };
    uint16_t row = row_at(sy), v;
    uint8_t i, d, k;

    if (row >= X16S_MAX_ROWS)
        return 0;
    if (row == (uint16_t)(lbl_row + 1) && lbl_row != 0xFFFF) {
        for (i = GRID_ROWLBL_W - 1; lbl[i] == '9'; --i)
            lbl[i] = '0';               /* (65535 is the last: no carry out) */
        lbl[i] = lbl[i] == ' ' ? '1' : (char)(lbl[i] + 1);
    } else if (row != lbl_row) {
        v = (uint16_t)(row + 1);
        memset(lbl, ' ', GRID_ROWHDR_W);
        lbl[GRID_ROWHDR_W] = '\0';
        k = 0;
        for (i = 0; i < 4; ++i) {       /* (subtraction: no division) */
            for (d = 0; v >= p10[i]; ++d)
                v = (uint16_t)(v - p10[i]);
            if (d || k)
                lbl[k++] = (char)('0' + d);
        }
        lbl[k++] = (char)('0' + v);
        /* right-aligned: the digits to the label's end, a space after */
        memmove(lbl + GRID_ROWLBL_W - k, lbl, k);
        memset(lbl, ' ', (uint8_t)(GRID_ROWLBL_W - k));
    }
    lbl_row = row;
    screen_text(0, (uint8_t)(ROW_GRID0 + sy), lbl, GRID_ROWHDR_W,
                row == grid.cur_row ? C_HDR_SEL : C_HEADER);
    return 1;
}

/* One data row: its header, then every visible column (layout()), the
 * row's records walked in column order. A run of empty cells of one
 * colour is one fill: what screen_text("") and screen_border(0) would have
 * drawn cell by cell. */
static void render_row(uint8_t sy)
{
    uint16_t row = row_at(sy);
    uint8_t i, n, c, last = 0, rx = 0, rw = 0;
    color_t rc = 0, cc;
    const cell_record_t *p = rc_rec, *end;
    cell_record_t one;

    cl_y = (uint8_t)(ROW_GRID0 + sy);
    if (!render_rowhdr(sy)) {
        screen_fill(0, cl_y, screen_cols(), ' ', C_CELL);
        return;
    }
    n = cells_row(wb_cells(), row);
    end = rc_rec + (n == RC_BIG ? 0 : n);
    for (i = 0; i < lay_n; ++i) {
        c = lay_col[i];
        if (n == RC_BIG) {              /* a long row: each cell found */
            cl_rec = wb_get(row, c, &one) ? &one : 0;
        } else {
            if (c < last)               /* (frozen columns: from the start) */
                p = rc_rec;
            last = c;
            while (p < end && p->col < c)
                ++p;
            cl_rec = (p < end && p->col == c) ? p : 0;
        }
        cc = (row == grid.cur_row || grid.sel_on) ? cell_color(row, c) : C_CELL;
        if (!cl_rec) {
            if (rw && cc == rc && lay_x[i] == rx + rw) {
                rw = (uint8_t)(rw + lay_w[i]);
                continue;
            }
        }
        if (rw) {                       /* the run so far */
            if (sty_bord)
                screen_border(rx, cl_y, rw, 0);
            screen_fill(rx, cl_y, rw, ' ', rc);
            rw = 0;
        }
        if (cl_rec) {
            cl_x = lay_x[i];
            cl_w = lay_w[i];
            cl_c = cc;
            render_cell_rec();
        } else {
            rx = lay_x[i];
            rw = lay_w[i];
            rc = cc;
        }
    }
    if (rw) {
        if (sty_bord)
            screen_border(rx, cl_y, rw, 0);
        screen_fill(rx, cl_y, rw, ' ', rc);
    }
}

/* The column letters, the cursor's highlighted, in at most three runs. */
void far_colhdr(void)
{
    char line[LAY_MAX + 1], name[4];
    uint8_t i, j, x, w, cx = 0, cw = 0, cols = screen_cols();

    if (cols > LAY_MAX)
        cols = LAY_MAX;

    layout();
    memset(line, ' ', cols);
    line[cols] = '\0';
    for (i = 0; i < lay_n; ++i) {
        x = lay_x[i];
        w = lay_w[i];
        if (lay_col[i] == grid.cur_col) {
            cx = x;
            cw = w;
        }
        grid_col_name(lay_col[i], name);
        /* one leading space reads as a column separator */
        for (j = 0; name[j] && j + 1 < w; ++j)
            line[x + 1 + j] = name[j];
    }
    if (!cw) {
        screen_text(0, ROW_COLHDR, line, cols, C_HEADER);
        return;
    }
    screen_text(0, ROW_COLHDR, line, cx, C_HEADER);
    screen_text(cx, ROW_COLHDR, line + cx, cw, C_HDR_SEL);
    screen_text((uint8_t)(cx + cw), ROW_COLHDR, line + cx + cw,
                (uint8_t)(cols - cx - cw), C_HEADER);
}

void far_grid(void)
{
    uint8_t sy;

    /* Once, here, rather than per cell: bringing the layer up clears it,
     * and with it down every screen_border() below returns at once. */
    screen_borders(sty_bord);
    layout();
    for (sy = 0; sy < grid.grid_rows; ++sy)
        render_row(sy);
}

/* Repaint just the cell at (far_row, far_col), if it is on screen. The
 * cursor colour follows from where the cursor is, so this needs no colour
 * argument -- and redrawing the text as well as the attribute means an
 * edit shows up without touching the rest of the row. */
void far_repaint(void)
{
    int16_t x = grid_col_x(far_col);
    uint8_t w;
    cell_record_t rec;

    if (x < 0 || far_row < grid.top_row
        || far_row >= grid.top_row + grid.grid_rows)
        return;
    w = col_visible_w(far_col);
    if (!w)
        return;
    memset(sc_ok, 0, sizeof sc_ok);
    cl_x = (uint8_t)x;
    cl_y = (uint8_t)(ROW_GRID0 + (far_row - grid.top_row));
    cl_w = w;
    cl_c = cell_color(far_row, far_col);
    cl_rec = wb_get(far_row, far_col, &rec) ? &rec : 0;
    render_cell_rec();
}

void far_rowhdr(void)
{
    render_rowhdr(far_sy);
}

/* The grid scrolled a row (grid.c's grid_goto: up if far_sy, the cursor's
 * old cell far_row, far_col; no block, no frozen rows, the same columns):
 * the rows moved on the screen, the row uncovered drawn, then the old
 * cursor cell and the new one, with their rows' headers. What the other
 * rows show has only moved. */
void far_scroll(void)
{
    uint8_t last = (uint8_t)(grid.grid_rows - 1);
    uint16_t r = far_row;

    screen_scroll(ROW_GRID0, (uint8_t)(ROW_GRID0 + last), far_sy);
    layout();
    render_row(far_sy ? last : 0);
    if (r >= grid.top_row && r < grid.top_row + grid.grid_rows) {
        far_repaint();
        render_rowhdr((uint8_t)(r - grid.top_row));
    }
    far_row = grid.cur_row;
    far_col = grid.cur_col;
    far_repaint();
    render_rowhdr((uint8_t)(grid.cur_row - grid.top_row));
}

#endif /* GRID_FAST */
