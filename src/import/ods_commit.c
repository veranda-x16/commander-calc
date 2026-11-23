/* ods_commit.c — records into cells. The second half of the .ods reader:
 * value types, dates, number formats, borders, the string pool, the sheets
 * and the formulas. It sees no XML.
 *
 * Positions arrive resolved; see ods_rec.h for the record format.
 */
#define BLOB_OWNER_ODSC
#include "blob_ovl.h"

#include "ods.h"
#include "ods_rec.h"
#include "xlsx_ctx.h"                   /* xlsx_note_formula()            */
#include "../workbook/workbook.h"
#include "../workbook/workbook_priv.h"
#include "../workbook/cells.h"
#include "../workbook/strings.h"
#include "../workbook/styles.h"
#include "../platform/banked_ram.h"
#include "../util/number.h"
#include <string.h>

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY19")
#  pragma rodata-name (push, "OVL19RO")
#  pragma bss-name (push, "OVL19BSS")
#endif

/* wb_sheet_add() is in another overlay, so the shared source is compiled
 * in here instead. See the note at the top of sheet_new.c. */
#include "../workbook/sheet_new.c"

/* How far a run of identical cells or rows is expanded. Empty runs are
 * dropped by the scanner; beyond this bound the result is marked
 * truncated rather than filling the cell store with one repeated value. */
#define REPEAT_MAX 256

static char value[ODS_FIELD_MAX];
static char text[ODS_FIELD_MAX];
static char formula[ODS_FIELD_MAX];
static char work[ODS_FIELD_MAX];        /* the formula, converted          */
/* What a column's cells get when they name no style of their own. */
static uint8_t col_bits[X16S_MAX_COLS];

static blob_t *in;
static ods_result_t *rep;
static uint8_t sheet_seen;
/* Which row a repeat applies to and how many rows it covers. It arrives
 * before that row's cells, so each cell is written to all of them. */
static uint16_t rep_row, rep_rows;

static uint8_t get(void)
{
    uint8_t b = 0;

    blob_read(in, &b, 1);
    return b;
}

static uint16_t get16(void)
{
    uint16_t lo = get();

    return (uint16_t)(lo | ((uint16_t)get() << 8));
}

static uint8_t get_field(char *dst)
{
    uint8_t n = get();

    if (n)
        blob_read(in, dst, n);
    dst[n] = '\0';
    return n;
}

/* --- values ----------------------------------------------------------- */

/* "2024-03-15", with or without a time after a T, to a serial day number.
 * Returns 0 for anything that is not a date.
 *
 * Kept here rather than in util/date.c, where date_to_serial() is built
 * for the host only. It must invert date_from_serial() exactly, phantom
 * leap day included. Sixteen bits carry a serial until 2079.
 */
static uint8_t leap(uint16_t y)
{
    return (uint8_t)((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
}

static uint16_t iso_date(const char *s)
{
    static const uint8_t mdays[12] = { 31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31 };
    uint16_t days = 0;
    uint16_t y = 0, yy;
    uint8_t  m, d, i;

    for (i = 0; i < 4; ++i) {
        if (s[i] < '0' || s[i] > '9')
            return 0;
        y = (uint16_t)(y * 10 + (s[i] - '0'));
    }
    if (s[4] != '-' || s[7] != '-' || y < 1900 || y > 2078)
        return 0;
    m = (uint8_t)((s[5] - '0') * 10 + (s[6] - '0'));
    d = (uint8_t)((s[8] - '0') * 10 + (s[9] - '0'));
    if (m < 1 || m > 12 || d < 1 || d > 31)
        return 0;

    for (yy = 1900; yy < y; ++yy)
        days = (uint16_t)(days + (leap(yy) ? 366 : 365));
    for (i = 1; i < m; ++i)
        days = (uint16_t)(days + mdays[i - 1]
                          + ((i == 2 && leap(y)) ? 1 : 0));
    days = (uint16_t)(days + d);        /* 1900-01-01 is serial 1 */

    /* 1900 was not a leap year and every spreadsheet says it was. */
    if (days >= 60)
        ++days;
    return days;
}

/* "PT10H30M00S" to a fraction of a day in thousandths, which is how a time
 * is held here. Minutes are enough: a thousandth of a day is 86 seconds.
 * mins * 25 / 36 is mins / 1440 * 1000 without the 32-bit multiply. */
static uint16_t iso_time(const char *s)
{
    uint16_t mins = 0, n = 0;

    while (*s) {
        if (*s >= '0' && *s <= '9')
            n = (uint16_t)(n * 10 + (*s - '0'));
        else if (*s == 'H') {
            mins = (uint16_t)(mins + n * 60);
            n = 0;
        } else if (*s == 'M') {
            mins = (uint16_t)(mins + n);
            n = 0;
        } else {
            n = 0;
        }
        ++s;
    }
    mins %= 1440;
    return (uint16_t)((mins * 25U + 18U) / 36U);
}

/* A number, written as text and parsed by the one parser the program has.
 * snum_from_i32() is another host-only function. */
static err_t serial_num(uint16_t serial, snum_t *v)
{
    char n[8];
    uint8_t i = 0, j;

    do {
        n[i++] = (char)('0' + (serial % 10));
        serial = (uint16_t)(serial / 10);
    } while (serial && i < sizeof n - 1);
    n[i] = '\0';
    for (j = 0; j < i / 2; ++j) {
        char t = n[j];
        n[j] = n[i - 1 - j];
        n[i - 1 - j] = t;
    }
    return snum_parse(n, v);
}

/* The style a typed cell needs. Styles are interned, so asking for the same
 * one on every cell of a column costs one slot in the table. */
static uint8_t style_for(uint8_t nf, uint8_t bord)
{
    cell_style_t st;
    uint8_t id;

    memset(&st, 0, sizeof st);
    st.number_format = nf;
    st.decimal_places = 2;
    /* The border bits are the style's flags three places up -- the record
     * format is built to make this the whole conversion. See ods_rec.h. */
    st.flags = (uint8_t)(bord << 3);
    if (styles_add(&st, &id) != ERR_OK)
        return 0;                       /* the table is full: General */
    return id;
}

/* --- formulas --------------------------------------------------------- */

/* An ODF formula into ours:
 *
 *   of:=[.B2]*[.C2]        =B2*C2
 *   of:=SUM([.D2:.D4];2)   =SUM(D2:D4,2)
 *   of:=[Sales.B2]         =Sales!B2
 *   of:=[$Sales.$B$2]      =Sales!$B$2
 *
 * References are bracketed and dotted, the argument separator is a
 * semicolon, and a $ on the sheet name is dropped while those inside the
 * reference are kept.
 *
 * Returns the length, or 0 if it would not fit -- the caller then keeps
 * the cached value rather than compiling a truncated formula.
 */
static uint8_t convert_formula(const char *s, char *o, uint8_t max)
{
    uint8_t n = 0, quoted = 0, half;
    const char *dot, *end;

    /* "of:=..." -- the namespace the formula language is named by. */
    {
        const char *p = s;

        while (*p && *p != ':' && *p != '=' && *p != '"' && *p != '[')
            ++p;
        if (*p == ':')
            s = p + 1;
    }

    while (*s) {
        if (n + 2 >= max)
            return 0;

        if (quoted) {
            if (*s == '"')
                quoted = 0;
            o[n++] = *s++;
            continue;
        }
        if (*s == '"') {
            quoted = 1;
            o[n++] = *s++;
            continue;
        }
        if (*s == ';') {
            o[n++] = ',';
            ++s;
            continue;
        }
        if (*s != '[') {
            o[n++] = *s++;
            continue;
        }

        /* A reference, and possibly a range -- in which case each half is
         * qualified separately and only the first names the sheet. */
        ++s;
        for (half = 0; half < 2 && *s && *s != ']'; ++half) {
            end = s;
            while (*end && *end != ']' && *end != ':')
                ++end;
            dot = s;
            while (dot < end && *dot != '.')
                ++dot;

            if (dot < end && dot != s && half == 0) {
                const char *p = s;

                if (*p == '$')          /* means nothing to us */
                    ++p;
                while (p < dot) {
                    if (n + 2 >= max)
                        return 0;
                    o[n++] = *p++;
                }
                o[n++] = '!';
            }
            s = (dot < end) ? dot + 1 : s;
            while (s < end) {
                if (n + 2 >= max)
                    return 0;
                o[n++] = *s++;
            }
            if (*s != ':')
                break;
            o[n++] = *s++;
        }
        while (*s && *s != ']')
            ++s;
        if (*s == ']')
            ++s;
    }
    o[n] = '\0';
    return n;
}

/* --- cells ------------------------------------------------------------ */

static err_t put_cell(uint16_t r, uint16_t c, uint8_t vtype,
                      uint8_t tlen, uint8_t flen, uint8_t bord)
{
    cell_record_t rec;
    snum_t v;
    err_t e;

    memset(&rec, 0, sizeof rec);
    rec.col = (uint8_t)c;

    switch (vtype) {
    case ODS_VT_FLOAT:
    case ODS_VT_PCT:
    case ODS_VT_CUR:
        if (snum_parse(value, &v) != ERR_OK) {
            rep->truncated = 1;
            return ERR_OK;
        }
        memcpy(rec.val, v.b, 5);
        rec.type = CELL_NUMBER;
        if (vtype == ODS_VT_PCT)
            rec.style = style_for(NF_PERCENT, bord);
        else if (vtype == ODS_VT_CUR)
            rec.style = style_for(NF_CURRENCY, bord);
        else if (bord)
            rec.style = style_for(NF_GENERAL, bord);
        break;

    case ODS_VT_DATE: {
        uint16_t serial = iso_date(value);

        if (serial == 0 || serial_num(serial, &v) != ERR_OK) {
            rep->truncated = 1;
            return ERR_OK;
        }
        memcpy(rec.val, v.b, 5);
        rec.type = CELL_DATE;
        rec.style = style_for(NF_DATE, bord);
        ++rep->dates;
        break;
    }

    case ODS_VT_TIME:
        /* A fraction of a day. NF_TIME shows the number until the
         * clock formatter is wired in; see wb_display_text(). */
        if (serial_num(iso_time(value), &v) != ERR_OK) {
            rep->truncated = 1;
            return ERR_OK;
        }
        memcpy(rec.val, v.b, 5);
        rec.type = CELL_NUMBER;
        rec.style = style_for(NF_TIME, bord);
        ++rep->dates;
        break;

    case ODS_VT_BOOL:
        rec.type = CELL_BOOLEAN;
        rec.val[0] = (uint8_t)(value[0] == 't' || value[0] == 'T'
                               || value[0] == '1');
        if (bord)
            rec.style = style_for(NF_GENERAL, bord);
        break;

    default: {
        uint16_t id;

        if (tlen == 0) {
            /* Nothing in it. A border still needs a record to hang
             * from, as the Border command's own empty cells do. */
            if (!bord)
                return ERR_OK;
            rec.type = CELL_EMPTY;
            rec.style = style_for(NF_GENERAL, bord);
            break;
        }
        e = strpool_add(text, tlen, &id);
        if (e == ERR_NOMEM)
            return e;                   /* the heap is full: said, not skipped */
        if (e != ERR_OK) {
            rep->truncated = 1;
            return ERR_OK;
        }
        rec.type = CELL_TEXT;
        rec.val[0] = (uint8_t)id;
        rec.val[1] = (uint8_t)(id >> 8);
        if (bord)
            rec.style = style_for(NF_GENERAL, bord);
        break;
    }
    }

    e = wb_set(r, c, &rec);
    if (e == ERR_LIMIT) {
        rep->truncated = 1;
        return ERR_OK;
    }
    if (e != ERR_OK)
        return e;
    ++rep->cells;
    if (r >= rep->rows)
        rep->rows = (uint16_t)(r + 1);

    if (flen) {
        uint8_t n = convert_formula(formula, work, sizeof work);

        /* The cached value stays in the cell meanwhile: the driver compiles
         * the queue after the import and the result replaces it. */
        if (n) {
            xlsx_note_formula(r, c, work, n);
            ++rep->formulas;
        } else {
            rep->truncated = 1;
        }
    }
    return ERR_OK;
}

/* A new worksheet. The first names the one wb_reset() left behind; the rest
 * are created here. */
static void open_sheet(const char *name)
{
    if (!sheet_seen) {
        sheet_seen = 1;
        if (name[0])
            sheet_set_name(0, name);
        ++rep->sheets;
        wb_sheet_switch(0);
        return;
    }
    if (wb_sheet_n >= X16S_MAX_SHEETS || sheet_new(name) != ERR_OK) {
        ++rep->sheets_skipped;
        return;                         /* its cells land in the last one */
    }
    ++rep->sheets;
    wb_sheet_switch((uint8_t)(wb_sheet_n - 1));
}

static err_t commit_records(blob_t *recs, ods_result_t *res)
{
    uint8_t kind, vtype, tlen, flen, bord;
    uint16_t row, col, repeat, n;
    err_t e;

    in = recs;
    rep = res;
    sheet_seen = 0;
    rep_row = 0;
    rep_rows = 1;

    blob_reset_read(recs);
    for (;;) {
        kind = get();
        if (kind == ODS_R_END)
            break;

        if (kind == ODS_R_SHEET) {
            get_field(text);
            open_sheet(text);
            memset(col_bits, 0, sizeof col_bits);   /* per sheet */
            continue;
        }
        if (kind == ODS_R_COLSTY) {
            uint16_t c = get(), k = get16();
            uint8_t b = get();

            while (k-- && c < X16S_MAX_COLS)
                col_bits[c++] = b;
            continue;
        }
        if (kind == ODS_R_ROWREP) {
            rep_row = get16();
            rep_rows = get16();
            if (rep_rows > REPEAT_MAX) {
                rep->truncated = 1;
                rep_rows = REPEAT_MAX;
            }
            continue;
        }
        if (kind != ODS_R_CELL)
            return ERR_BADFORMAT;       /* the stream is not ours */

        row = get16();
        col = get();
        repeat = get16();
        vtype = get();
        get_field(value);
        tlen = get_field(text);
        flen = get_field(formula);
        bord = get();

        if (repeat > REPEAT_MAX) {
            rep->truncated = 1;
            repeat = REPEAT_MAX;
        }
        /* Every column of the run, on every row the run covers. */
        n = (row == rep_row) ? rep_rows : 1;
        while (repeat--) {
            uint16_t k;

            if (col >= X16S_MAX_COLS) {
                rep->truncated = 1;
                break;
            }
            for (k = 0; k < n; ++k) {
                if (row + k >= X16S_MAX_ROWS) {
                    rep->truncated = 1;
                    break;
                }
                /* Its own style if it named one, its column's default
                 * otherwise. */
                e = put_cell((uint16_t)(row + k), col, vtype, tlen, flen,
                             (uint8_t)((bord & ODS_B_OWN)
                                       ? (bord & 0x0F) : col_bits[col]));
                if (e != ERR_OK)
                    return e;
            }
            ++col;
        }
    }
    return ERR_OK;
}


/* Releases the records when it is done with them, the way ods_scan()
 * releases content.xml: they are cells now, and the banks are wanted for
 * the workbook. */
err_t ods_commit(blob_t *recs, ods_result_t *res)
{
    err_t e = commit_records(recs, res);

    blob_free(recs);
    return e;
}

#ifdef __CC65__
#  pragma bss-name (pop)
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
