/* ods_scan.c — content.xml into records. The first half of the .ods
 * reader; see ods_rec.h for the record format.
 *
 * ODF writes no cell references, so this half owns position: a cell's
 * column is how many came before it in the row, and a run of identical
 * cells is written once with a count. The records it writes carry a
 * resolved row and column.
 *
 * Element names carry the prefixes the ODF specification defines and every
 * writer uses. A file binding those namespaces to other prefixes reads as
 * empty rather than wrongly.
 */
#define BLOB_OWNER_ODS
#include "blob_ovl.h"

#define XML_OWNER_ODS
#include "xml_ovl.h"

#include "ods.h"
#include "ods_rec.h"
#include "xml.h"
#include <string.h>

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY18")
#  pragma rodata-name (push, "OVL18RO")
#  pragma bss-name (push, "OVL18BSS")
#endif

/* Named arrays, not literals: cc65 routes a string literal to resident
 * RODATA however rodata-name is set. */
static const char E_TABLE[]   = "table:table";
static const char E_ROW[]     = "table:table-row";
static const char E_CELL[]    = "table:table-cell";
static const char E_COVERED[] = "table:covered-table-cell";
static const char E_ANNOT[]   = "office:annotation";
static const char A_NAME[]    = "table:name";
static const char A_FORMULA[] = "table:formula";
static const char A_CREP[]    = "table:number-columns-repeated";
static const char A_RREP[]    = "table:number-rows-repeated";
static const char A_VTYPE[]   = "office:value-type";
static const char A_VALUE[]   = "office:value";
static const char A_DATE[]    = "office:date-value";
static const char A_TIME[]    = "office:time-value";
static const char A_BOOL[]    = "office:boolean-value";
/* Styles. A cell's borders come from an automatic style declared before the
 * body, named either on the cell or on its column -- see ods_rec.h. */
static const char E_STYLE[]   = "style:style";
static const char E_CELLPROP[]= "style:table-cell-properties";
static const char E_COLUMN[]  = "table:table-column";
static const char A_STYNAME[] = "style:name";
static const char A_FAMILY[]  = "style:family";
static const char A_CELLSTY[] = "table:style-name";
static const char A_COLSTY[]  = "table:default-cell-style-name";
static const char A_BORD[]    = "fo:border";
static const char A_BORD_L[]  = "fo:border-left";
static const char A_BORD_R[]  = "fo:border-right";
static const char A_BORD_T[]  = "fo:border-top";
static const char A_BORD_B[]  = "fo:border-bottom";

/* Too large for cc65's stack, and only one document is read at a time. */
static xml_t X;

/* One cell, held until its end tag: the value is an attribute and the
 * displayed text a child element, so neither is complete before then. */
/* Sized to what each carries: a value is a number, a date or a boolean,
 * and an attribute cannot exceed the tokenizer's own buffer. */
static char    value[32];
static char    text[ODS_FIELD_MAX];
static char    formula[XML_VALUE_MAX + 8];
static char    sname[ODS_NAME_MAX];     /* the cell's style, if it names one */
static uint8_t vlen, tlen, flen, snlen, vtype, cbord;
static uint16_t crepeat, rrepeat;

/* The automatic style being read, and what it says about borders. */
static char    stname[ODS_NAME_MAX];
static uint8_t stlen, stbits, stcell;

/* The automatic styles that draw a border, resolved here so that a cell
 * record carries bits rather than a style name. Past ODS_STYLES the rest
 * import unbordered and the result is marked truncated. */
#define ODS_STYLES 16

static char    sty_name[ODS_STYLES][ODS_NAME_MAX];
static uint8_t sty_bits[ODS_STYLES];
static uint8_t sty_n;

/* The borders a style name draws, or none if it is not in the table. */
static uint8_t bits_of(const char *name)
{
    uint8_t i;

    for (i = 0; i < sty_n; ++i)
        if (strcmp(sty_name[i], name) == 0)
            return sty_bits[i];
    return 0;
}

/* The column declaration being read. Columns come before the rows, in
 * order and with repeat counts, so the cursor gives their positions. */
static char    colname[ODS_NAME_MAX];
static uint8_t in_col, colsty;
static uint16_t colrep, colcur;

static uint16_t row, col;
static uint8_t  row_rep_sent;       /* has this row's repeat been said?   */
static blob_t  *out;
static uint8_t  full;               /* the blob would not take any more   */

static void put(uint8_t b)
{
    if (!full && !blob_write(out, &b, 1))
        full = 1;
}

static void put16(uint16_t v)
{
    put((uint8_t)v);
    put((uint8_t)(v >> 8));
}

static void put_field(const char *s, uint8_t n)
{
    put(n);
    if (!full && n && !blob_write(out, s, n))
        full = 1;
}

/* Copy an attribute into a name buffer, which is shorter than the rest. */
static uint8_t take_name(char *dst)
{
    uint8_t n = X.value_len;

    if (n >= ODS_NAME_MAX)
        n = ODS_NAME_MAX - 1;
    memcpy(dst, X.value, n);
    dst[n] = '\0';
    return n;
}

/* Copy an attribute into one of the three buffers. */
static uint8_t take(char *dst)
{
    uint8_t n = X.value_len;

    if (n >= ODS_FIELD_MAX)
        n = ODS_FIELD_MAX - 1;
    memcpy(dst, X.value, n);
    dst[n] = '\0';
    return n;
}

static uint16_t attr_u16(const char *s)
{
    uint16_t n = 0;

    while (*s >= '0' && *s <= '9') {
        if (n > 6553)
            return 0xFFFF;
        n = (uint16_t)(n * 10 + (*s - '0'));
        ++s;
    }
    return n;
}

err_t ods_scan(blob_t *xml, blob_t *recs, ods_result_t *res)
{
    xml_token_t t;
    uint8_t in_cell = 0, skipping = 0;
    err_t e;

    memset(res, 0, sizeof *res);
    out = recs;
    full = 0;
    row = col = 0;
    colcur = 0;
    rrepeat = 1;
    colrep = 1;
    in_col = colsty = 0;
    sty_n = 0;
    row_rep_sent = 0;

    blob_reset_read(xml);
    blob_reset_write(recs);
    e = xml_init(&X, blob_feed, xml);
    if (e != ERR_OK)
        return e;

    for (;;) {
        t = xml_next(&X);
        if (t == XML_EOF)
            break;
        if (t == XML_ERROR)
            return X.err != ERR_OK ? X.err : ERR_BADFORMAT;

        if (t == XML_START) {
            /* An annotation is a comment and carries text:p elements of its
             * own, which would otherwise be appended to the cell. */
            if (xml_is(&X, E_ANNOT)) {
                skipping = 1;
            } else if (skipping) {
                ;
            } else if (xml_is(&X, E_STYLE)) {
                stlen = stbits = stcell = 0;
            } else if (xml_is(&X, E_COLUMN)) {
                in_col = 1;
                colsty = 0;
                colrep = 1;
            } else if (xml_is(&X, E_TABLE)) {
                row = col = 0;
                colcur = 0;
            } else if (xml_is(&X, E_ROW)) {
                col = 0;
                rrepeat = 1;
                row_rep_sent = 0;
            } else if (xml_is(&X, E_CELL) || xml_is(&X, E_COVERED)) {
                in_cell = 1;
                vtype = ODS_VT_NONE;
                crepeat = 1;
                vlen = tlen = flen = snlen = cbord = 0;
            }
            continue;
        }

        if (t == XML_ATTR) {
            if (skipping)
                continue;
            if (!in_cell) {
                /* A style's own attributes and its border properties.
                 * An edge is drawn unless the value is "none". */
                if (xml_is(&X, A_STYNAME)) {
                    stlen = take_name(stname);
                    continue;
                }
                if (xml_is(&X, A_FAMILY)) {
                    stcell = (uint8_t)(X.value[0] == 't');   /* table-cell */
                    continue;
                }
                if (X.value[0] != 'n') {                    /* not "none" */
                    if (xml_is(&X, A_BORD))
                        stbits |= ODS_B_L | ODS_B_R | ODS_B_T | ODS_B_B;
                    else if (xml_is(&X, A_BORD_L))
                        stbits |= ODS_B_L;
                    else if (xml_is(&X, A_BORD_R))
                        stbits |= ODS_B_R;
                    else if (xml_is(&X, A_BORD_T))
                        stbits |= ODS_B_T;
                    else if (xml_is(&X, A_BORD_B))
                        stbits |= ODS_B_B;
                }
                /* A column's default cell style, which is where a whole
                 * formatted column keeps its borders. */
                if (in_col) {
                    if (xml_is(&X, A_COLSTY))
                        colsty = take_name(colname);
                    else if (xml_is(&X, A_CREP))
                        colrep = attr_u16(X.value);
                    continue;
                }
                /* A table's name, or a row's repeat count. */
                if (xml_is(&X, A_NAME)) {
                    put(ODS_R_SHEET);
                    put_field(X.value, take(text));
                    ++res->sheets_found;
                } else if (xml_is(&X, A_RREP)) {
                    rrepeat = attr_u16(X.value);
                }
                continue;
            }
            if (xml_is(&X, A_CREP)) {
                crepeat = attr_u16(X.value);
            } else if (xml_is(&X, A_CELLSTY)) {
                snlen = take_name(sname);
                /* Named one at all: that is what overrides the column. */
                cbord = (uint8_t)(ODS_B_OWN | bits_of(sname));
            } else if (xml_is(&X, A_VTYPE)) {
                switch (X.value[0]) {
                case 'f': vtype = ODS_VT_FLOAT;  break;
                case 'p': vtype = ODS_VT_PCT;    break;
                case 'c': vtype = ODS_VT_CUR;    break;
                case 'd': vtype = ODS_VT_DATE;   break;
                case 't': vtype = ODS_VT_TIME;   break;
                case 'b': vtype = ODS_VT_BOOL;   break;
                case 's': vtype = ODS_VT_STRING; break;
                default:  vtype = ODS_VT_NONE;   break;
                }
            } else if (xml_is(&X, A_VALUE) || xml_is(&X, A_DATE)
                       || xml_is(&X, A_TIME) || xml_is(&X, A_BOOL)) {
                vlen = take(value);
            } else if (xml_is(&X, A_FORMULA)) {
                /* A truncated attribute would compile into a different
                 * formula, so drop it and keep the cached value. */
                if (X.value_len >= XML_VALUE_MAX - 1)
                    res->truncated = 1;
                else
                    flen = take(formula);
            }
            continue;
        }

        if (t == XML_TEXT) {
            if (skipping || !in_cell)
                continue;
            {
                uint8_t n = (uint8_t)X.value_len;

                if (tlen + n >= ODS_FIELD_MAX) {
                    n = (uint8_t)(ODS_FIELD_MAX - 1 - tlen);
                    res->truncated = 1;
                }
                if (n) {
                    memcpy(text + tlen, X.value, n);
                    tlen = (uint8_t)(tlen + n);
                }
            }
            continue;
        }

        /* XML_END */
        if (xml_is(&X, E_ANNOT)) {
            skipping = 0;
            continue;
        }
        if (skipping)
            continue;

        if (xml_is(&X, E_STYLE)) {
            /* Only the ones that draw something: a name absent from the
             * table resolves to no borders. */
            if (stcell && stlen && stbits) {
                if (sty_n < ODS_STYLES) {
                    memcpy(sty_name[sty_n], stname, ODS_NAME_MAX);
                    sty_bits[sty_n++] = stbits;
                } else {
                    res->truncated = 1;
                }
            }
            continue;
        }
        if (xml_is(&X, E_COLUMN)) {
            uint8_t b = colsty ? bits_of(colname) : 0;

            in_col = 0;
            if (b && colcur < X16S_MAX_COLS) {
                put(ODS_R_COLSTY);
                put((uint8_t)colcur);
                put16(colrep);
                put(b);
            }
            colcur = (uint16_t)(colcur + (colrep ? colrep : 1));
            continue;
        }

        if (xml_is(&X, E_CELL) || xml_is(&X, E_COVERED)) {
            in_cell = 0;
            /* Nothing in it: a run of these is how ODF says "empty to the
             * end of the row", and it only moves the cursor. */
            if (vtype == ODS_VT_NONE && tlen == 0 && flen == 0
                && (cbord & 0x0F) == 0) {
                col = (uint16_t)(col + crepeat);
                continue;
            }
            if (col < X16S_MAX_COLS) {
                /* A repeated row with content is several identical rows.
                 * Emitted before the cells so that ods_commit() can write
                 * each cell to every row as it goes. */
                if (rrepeat > 1 && !row_rep_sent) {
                    put(ODS_R_ROWREP);
                    put16(row);
                    put16(rrepeat);
                    row_rep_sent = 1;
                }
                put(ODS_R_CELL);
                put16(row);
                put((uint8_t)col);
                put16(crepeat);
                put(vtype);
                put_field(value, vlen);
                put_field(text, tlen);
                put_field(formula, flen);
                put(cbord);
            } else {
                res->truncated = 1;
            }
            col = (uint16_t)(col + crepeat);
            continue;
        }

        if (xml_is(&X, E_ROW)) {
            if (rrepeat == 0)
                rrepeat = 1;
            row = (uint16_t)(row + rrepeat);
            if (row >= X16S_MAX_ROWS) {
                res->truncated = 1;
                row = X16S_MAX_ROWS - 1;
            }
            continue;
        }
    }

    put(ODS_R_END);
    if (full)
        res->truncated = 1;
    if (X.lossy)
        res->lossy = 1;

    /* Releases its source, as inflate_blob() does: the commit half reads
     * only the records, and the banks content.xml held are wanted for the
     * workbook being built. */
    blob_free(xml);
    return ERR_OK;
}

#ifdef __CC65__
#  pragma bss-name (pop)
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
