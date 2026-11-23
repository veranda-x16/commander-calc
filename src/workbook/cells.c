#include "cells.h"
#include "cells_priv.h"
#include "../platform/banked_ram.h"
#include "../platform/bankmem.h"
#include <string.h>

#define BAND_COUNT   (65536UL / BAND_ROWS)      /* 256 bands          */
#define DIR_BYTES    (BAND_COUNT * 4)           /* 1024               */

/* row_record, inline in the band array */
#define BAND_BYTES   (BAND_ROWS * RR_SIZE)      /* 2048 */


#define ROW_SLOT(r)  ((uint16_t)((r) & (BAND_ROWS - 1)))

/* Start capacity for a row's cell array. Eight records is 64 bytes, exactly
 * one size class, and covers the common case of a handful of columns. */
#if !defined(__CC65__) || defined(X16S_VERANDA)
/* The last row looked up, its records copied: the grid draws a row a cell
 * at a time, and finding each one in banked RAM again was most of what a
 * cell cost to draw (Veranda Calc's phase C6). Any change to any row, and a
 * new or reset store, forgets it (rc_ok); a row of more than RC_MAX cells
 * is not kept. Not in the X16 build: no room. */
cell_record_t rc_rec[RC_MAX];
handle_t rc_dir;                        /* (Veranda: cellrow_veranda.s reads */
uint16_t rc_row;                        /* and writes these) */
uint8_t rc_n, rc_ok;
#  define RC_FORGET() (rc_ok = 0)

void cells_forget(void)
{
    RC_FORGET();
}
#else
#  define RC_FORGET() ((void)0)
#endif

#define ROW_START_CAP 8

err_t cells_init(cellstore_t *cs)
{
    RC_FORGET();
    memset(cs, 0, sizeof *cs);
    return ERR_OK;
}

/* cells_reset() is in sheet_admin.c: deleting a sheet is the only thing
 * that calls it, and that lives in OVL_FILEDLG. See cells.h. */

/* --- locating a row ------------------------------------------------ */

/* Handle of the band containing `row`, or H_NULL. */
static handle_t band_of(const cellstore_t *cs, uint16_t row)
{
    if (cs->row_dir == H_NULL)
        return H_NULL;
    return bank_peek32(cs->row_dir, (uint16_t)(ROW_BAND(row) * 4));
}

/* Same, creating the directory and the band if they are missing. */
static handle_t band_make(cellstore_t *cs, uint16_t row)
{
    handle_t band;
    uint16_t off = (uint16_t)(ROW_BAND(row) * 4);

    if (cs->row_dir == H_NULL) {
        cs->row_dir = bank_calloc(DIR_BYTES);
        if (cs->row_dir == H_NULL)
            return H_NULL;
    }
    band = bank_peek32(cs->row_dir, off);
    if (band == H_NULL) {
        band = bank_calloc(BAND_BYTES);
        if (band == H_NULL)
            return H_NULL;
        bank_poke32(cs->row_dir, off, band);
    }
    return band;
}

/* --- searching within a row ---------------------------------------- */

/* Binary search for `col`. Returns 1 on a hit; either way *pos ends up as
 * the index where the column is, or where it would be inserted. */
static uint8_t row_find(handle_t cells, uint16_t count, uint8_t col,
                        uint16_t *pos)
{
    uint16_t lo = 0, hi = count;

    while (lo < hi) {
        uint16_t mid = (uint16_t)((lo + hi) >> 1);
        uint8_t  c   = bank_peek(cells, (uint16_t)(mid * CELL_SIZE));
        if (c == col) {
            *pos = mid;
            return 1;
        }
        if (c < col)
            lo = (uint16_t)(mid + 1);
        else
            hi = mid;
    }
    *pos = lo;
    return 0;
}

/* --- reading -------------------------------------------------------- */

#ifndef __CC65__                         /* Veranda: platform/cellrow_veranda.s */
/* The row's records in rc_rec, in column order: how many (an empty row is
 * kept too: a redraw asks about every cell of it), or RC_BIG when the row
 * has more than RC_MAX (not kept: cells_get finds its cells one by one). */
uint8_t cells_row(const cellstore_t *cs, uint16_t row)
{
    handle_t band;
    uint16_t count = 0;

    if (rc_ok && rc_row == row && rc_dir == cs->row_dir)
        return rc_n;
    band = band_of(cs, row);
    if (band != H_NULL)
        count = bank_peek16(band, (uint16_t)(RR_OFF(row) + RR_COUNT));
    if (count > RC_MAX)
        return RC_BIG;
    if (count)
        bank_read(bank_peek32(band, (uint16_t)(RR_OFF(row) + RR_CELLS)), 0,
                  rc_rec, (uint16_t)(count * CELL_SIZE));
    rc_n = (uint8_t)count;
    rc_row = row;
    rc_dir = cs->row_dir;
    rc_ok = 1;
    return rc_n;
}
#endif

uint8_t cells_get(const cellstore_t *cs, uint16_t row, uint16_t col,
                  cell_record_t *out)
{
#if defined(__CC65__) && !defined(X16S_VERANDA)
    handle_t band = band_of(cs, row);   /* (as it was: the X16's image is full) */
#else
    handle_t band;
    uint8_t i, n;
#endif
    uint16_t count, pos;
    handle_t cells;

#if !defined(__CC65__) || defined(X16S_VERANDA)
    if (col >= X16S_MAX_COLS)
        return 0;
    n = cells_row(cs, row);
    if (n != RC_BIG) {
        for (i = 0; i < n; ++i)
            if (rc_rec[i].col == (uint8_t)col) {
                *out = rc_rec[i];
                return 1;
            }
        return 0;
    }
    band = band_of(cs, row);            /* a long row: found by halves */
#endif
    if (band == H_NULL || col >= X16S_MAX_COLS)
        return 0;

    count = bank_peek16(band, (uint16_t)(RR_OFF(row) + RR_COUNT));
    if (count == 0)
        return 0;
    cells = bank_peek32(band, (uint16_t)(RR_OFF(row) + RR_CELLS));
    if (!row_find(cells, count, (uint8_t)col, &pos))
        return 0;
    bank_read(cells, (uint16_t)(pos * CELL_SIZE), out, CELL_SIZE);
    return 1;
}

uint16_t cells_next_row(const cellstore_t *cs, uint16_t from)
{
    uint32_t r;

    if (cs->row_dir == H_NULL || cs->cell_count == 0)
        return X16S_MAX_ROWS;

    for (r = from; r < 65536UL; ) {
        handle_t band = bank_peek32(cs->row_dir,
                                    (uint16_t)(ROW_BAND((uint16_t)r) * 4));
        if (band == H_NULL) {
            /* Skip the whole band rather than 256 individual probes. */
            r = (r | (BAND_ROWS - 1)) + 1;
            continue;
        }
        if (bank_peek16(band, (uint16_t)(RR_OFF((uint16_t)r) + RR_COUNT)))
            return (uint16_t)r;
        ++r;
    }
    return X16S_MAX_ROWS;
}

/* --- writing -------------------------------------------------------- */

err_t cells_set(cellstore_t *cs, uint16_t row, uint16_t col,
                const cell_record_t *rec)
{
    handle_t band, cells;
    uint16_t count, cap, pos, rr;
    cell_record_t stored;

    RC_FORGET();

    if (row >= X16S_MAX_ROWS || col >= X16S_MAX_COLS)
        return ERR_REF;

    band = band_make(cs, row);
    if (band == H_NULL)
        return ERR_NOMEM;

    rr    = RR_OFF(row);
    count = bank_peek16(band, (uint16_t)(rr + RR_COUNT));
    cap   = bank_peek16(band, (uint16_t)(rr + RR_CAP));
    cells = bank_peek32(band, (uint16_t)(rr + RR_CELLS));

    stored = *rec;
    stored.col = (uint8_t)col;

    /* pos must be set even when the row is empty: row_find is skipped in
     * that case, and everything below uses pos as the insertion point. */
    pos = 0;
    if (count && row_find(cells, count, (uint8_t)col, &pos)) {
        bank_write(cells, (uint16_t)(pos * CELL_SIZE), &stored, CELL_SIZE);
        return ERR_OK;                          /* overwrite: no new cell */
    }

    if (cs->cell_count >= X16S_MAX_CELLS)
        return ERR_LIMIT;

    if (count >= cap) {
        uint16_t new_cap = cap ? (uint16_t)(cap * 2) : ROW_START_CAP;
        handle_t grown;

        if (new_cap > X16S_MAX_COLS)
            new_cap = X16S_MAX_COLS;
        grown = bank_realloc(cells, (uint16_t)(count * CELL_SIZE),
                             (uint16_t)(new_cap * CELL_SIZE));
        if (grown == H_NULL)
            return ERR_NOMEM;
        cells = grown;
        cap = new_cap;
        bank_poke32(band, (uint16_t)(rr + RR_CELLS), cells);
        bank_poke16(band, (uint16_t)(rr + RR_CAP), cap);
        /* row_find ran against the old block; positions are unchanged by a
         * copy, so `pos` is still where this column belongs. */
    }

    if (pos < count) {
        /* Open a gap. The array is contiguous inside one bank, so a single
         * mapped memmove beats shuffling records through a buffer — this is
         * the one place bankmem_map is worth its rules. */
        uint8_t *p = (uint8_t *)bankmem_map(H_BANK(cells), H_OFF(cells),
                                            (uint16_t)((count + 1) * CELL_SIZE));
        memmove(p + (uint16_t)((pos + 1) * CELL_SIZE),
                p + (uint16_t)(pos * CELL_SIZE),
                (size_t)(count - pos) * CELL_SIZE);
    }

    bank_write(cells, (uint16_t)(pos * CELL_SIZE), &stored, CELL_SIZE);
    bank_poke16(band, (uint16_t)(rr + RR_COUNT), (uint16_t)(count + 1));

    ++cs->cell_count;
    if (cs->cell_count == 1 || row > cs->max_row)
        cs->max_row = row;
    if (cs->cell_count == 1 || col > cs->max_col)
        cs->max_col = col;

    return ERR_OK;
}

uint8_t cells_delete(cellstore_t *cs, uint16_t row, uint16_t col)
{
    handle_t band = band_of(cs, row), cells;
    uint16_t count, pos, rr;

    RC_FORGET();

    if (band == H_NULL)
        return 0;

    rr    = RR_OFF(row);
    count = bank_peek16(band, (uint16_t)(rr + RR_COUNT));
    if (count == 0)
        return 0;
    cells = bank_peek32(band, (uint16_t)(rr + RR_CELLS));

    if (!row_find(cells, count, (uint8_t)col, &pos))
        return 0;

    if (pos + 1 < count) {
        uint8_t *p = (uint8_t *)bankmem_map(H_BANK(cells), H_OFF(cells),
                                            (uint16_t)(count * CELL_SIZE));
        memmove(p + (uint16_t)(pos * CELL_SIZE),
                p + (uint16_t)((pos + 1) * CELL_SIZE),
                (size_t)(count - pos - 1) * CELL_SIZE);
    }
    bank_poke16(band, (uint16_t)(rr + RR_COUNT), (uint16_t)(count - 1));

    if (count == 1) {
        bank_free(cells);
        bank_poke32(band, (uint16_t)(rr + RR_CELLS), H_NULL);
        bank_poke16(band, (uint16_t)(rr + RR_CAP), 0);
    }

    --cs->cell_count;
    /* The used range is not recomputed here. Shrinking it would mean a scan
     * of the whole sheet on every delete, and everything that consumes it
     * treats it as an upper bound. */
    return 1;
}
