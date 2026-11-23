/* xlsx_styles.c — the number formats a workbook declares.
 *
 * Its own overlay, apart from the worksheet parser. They run in separate
 * phases — styles once, then a worksheet at a time — so they never need to
 * be in memory together, and together they do not fit. Each carries its own
 * copy of the XML tokenizer, which is what an overlay that cannot call
 * another overlay costs.
 */
#define BLOB_OWNER_STYLE
#include "blob_ovl.h"

#define XML_OWNER_STYLE
#include "xml_ovl.h"

#include "xlsx_sheet.h"
#include "xml.h"
#include "../workbook/styles.h"
#include "../platform/banked_ram.h"
#include <string.h>

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY9")
#  pragma rodata-name (push, "OVL9RO")
#  pragma bss-name (push, "OVL9BSS")
#endif

/* Named arrays, not literals: cc65 routes a string literal to
 * resident RODATA however rodata-name is set, and these are element
 * names only this overlay ever compares against. */
static const char Y0[] = "numFmts";
static const char Y1[] = "cellXfs";
static const char Y2[] = "numFmt";
static const char Y3[] = "xf";
static const char Y4[] = "numFmtId";
static const char Y5[] = "formatCode";
static const char Y6[] = "fonts";
static const char Y7[] = "font";
static const char Y8[] = "b";
static const char Y9[] = "fontId";
/* Borders. Each <border> in <borders> is a numbered set of edges, and an
 * <xf> names one by index -- the same shape as fonts and bold. */
static const char YA[] = "borders";
static const char YB[] = "border";
static const char YC[] = "left";
static const char YD[] = "right";
static const char YE[] = "top";
static const char YF[] = "bottom";
static const char YG[] = "borderId";
static const char YH[] = "style";

/* Too large for cc65's stack. */
static xml_t X;
static char  text[256];

/* One byte of edges per borderId. Excel writes a handful of these however
 * many cells use them; past BORD_MAX the rest come in unbordered. */
#define BORD_MAX 32

static uint8_t bord_bits[BORD_MAX];
static uint8_t bord_count;

/* --- styles ----------------------------------------------------------- */

/* Excel's built-in number format ids. Anything not listed reads as General,
 * which is the right failure: a format we do not model should show the
 * value plainly rather than guess at it.
 *
 * The ranges are from the spec's "implied number formats". Only the ones a
 * spreadsheet full of money, percentages and dates actually uses are worth
 * naming here. */
static uint8_t builtin_format(uint16_t id, uint8_t *places)
{
    *places = 2;

    if (id == 0)                     return NF_GENERAL;
    if (id == 1)                   { *places = 0; return NF_INTEGER; }
    if (id == 2)                     return NF_DECIMAL;
    if (id == 3)                   { *places = 0; return NF_INTEGER; }
    if (id == 4)                     return NF_DECIMAL;
    if (id >= 5 && id <= 8)          return NF_CURRENCY;
    if (id == 9)                   { *places = 0; return NF_PERCENT; }
    if (id == 10)                    return NF_PERCENT;
    if (id >= 14 && id <= 17)         return NF_DATE;
    if (id >= 18 && id <= 21)         return NF_TIME;
    if (id == 22)                     return NF_DATE;   /* date and time */
    if (id >= 37 && id <= 40)         return NF_DECIMAL;
    if (id >= 41 && id <= 44)         return NF_CURRENCY;
    if (id >= 45 && id <= 47)         return NF_TIME;
    if (id == 49)                   { *places = 0; return NF_TEXT; }
    return NF_GENERAL;
}

/* Classify a custom format code, e.g. "$#,##0.00" or "yyyy\-mm\-dd".
 *
 * Reading the code rather than matching whole strings, because writers spell
 * the same intent many ways and the interesting question is only which of
 * our seven formats it is closest to. Quoted literals are skipped so that a
 * currency symbol spelled "\"kr\"" does not read as a date because of the
 * letters inside it. */
static uint8_t classify_code(const char *code, uint8_t *places)
{
    uint8_t date = 0, time_ = 0, pct = 0, cur = 0, dec = 0;
    uint8_t after_point = 0, seen_point = 0;
    uint8_t q = 0;
    const char *p;

    *places = 0;

    for (p = code; *p; ++p) {
        char c = *p;

        if (q) {
            /* Inside a quoted literal. Its letters are text to print, not
             * format codes — but a currency symbol is normally written
             * exactly this way, as "$"#,##0.00, so those still count. */
            if (c == '"')
                q = 0;
            else if (c == '$' || c == '\xA3' || c == '\xA4')
                cur = 1;
            continue;
        }
        switch (c) {
        case '"':  q = 1; continue;
        case '\\': if (p[1]) ++p; continue; /* escaped: not a code letter */
        case '[':                            /* [$-409], [Red] and friends */
            while (p[1] && p[1] != ']')
                ++p;
            continue;
        case 'y': case 'Y': case 'd': case 'D': date = 1; continue;
        case 'h': case 'H': case 's': case 'S': time_ = 1; continue;
        case 'm': case 'M':
            /* m is minutes next to h or s, months otherwise. */
            date = 1;
            continue;
        case '%': pct = 1; continue;
        case '$': case '\xA3': case '\xA4': cur = 1; continue;
        case '.': seen_point = 1; dec = 1; continue;
        case '0': case '#': case '?':
            if (seen_point && after_point < 9)
                ++after_point;
            continue;
        default: continue;
        }
    }

    *places = after_point;

    /* Order matters where a code says two things: "h:mm" is a time even
     * though m means months elsewhere, and a percentage with decimals is a
     * percentage. */
    if (time_) return NF_TIME;
    if (date)  return NF_DATE;
    if (pct)   return NF_PERCENT;
    if (cur)   return NF_CURRENCY;
    if (dec)   return NF_DECIMAL;
    return NF_GENERAL;
}

/* Custom ids start at 164; below that the id itself is the meaning. */
#define CUSTOM_FIRST 164
#define CUSTOM_MAX   32

/* Which fonts are bold, one bit each.
 *
 * styles.xml lists the fonts once and every xf record names one by index,
 * so the bold-ness of a cell is two lookups away: xf -> fontId -> the bit.
 * 64 is well past what a spreadsheet uses; anything beyond it reads as not
 * bold, which is the safe way to be wrong.
 *
 * A <font/> with nothing in it still opens and closes, so the count stays
 * in step with Excel's numbering -- the tokenizer reports a self-closing
 * element as a start and an end both. */
#define FONT_MAX 64

err_t xlsx_parse_styles(blob_t *xml, handle_t out, xlsx_styles_info_t *info)
{
    /* Custom formats, kept only long enough to resolve the xf records that
     * point at them. */
    static uint16_t custom_id[CUSTOM_MAX];
    static uint8_t  custom_fmt[CUSTOM_MAX];
    static uint8_t  custom_places[CUSTOM_MAX];
    uint8_t custom_count = 0;

    static uint8_t font_bold[FONT_MAX / 8];
    uint8_t font_count = 0, in_fonts = 0, cur_bold = 0;
    uint16_t pending_font = 0;

    xml_token_t t;
    uint8_t in_cellxfs = 0, in_numfmts = 0;
    uint16_t pending_id = 0;
    uint8_t  i;
    /* Borders: which set is being read, which edge within it, and which
     * set the current <xf> names. */
    uint8_t in_borders = 0, cur_bord = 0, cur_edge = 0;
    uint16_t pending_bord = 0;

    memset(info, 0, sizeof *info);
    memset(font_bold, 0, sizeof font_bold);
    memset(bord_bits, 0, sizeof bord_bits);
    bord_count = 0;
    blob_reset_read(xml);
    xml_init(&X, blob_feed, xml);

    while ((t = xml_next(&X)) != XML_EOF && t != XML_ERROR) {
        if (t == XML_START) {
            if (xml_is(&X, Y0))       in_numfmts = 1;
            else if (xml_is(&X, Y1))  in_cellxfs = 1;
            else if (xml_is(&X, Y6))  in_fonts = 1;
            else if (xml_is(&X, Y7) && in_fonts) cur_bold = 0;
            else if (xml_is(&X, Y8) && in_fonts) cur_bold = 1;
            else if (xml_is(&X, YA))  in_borders = 1;
            else if (in_borders) {
                /* An edge is only drawn if it names a line style, and an
                 * empty <left/> is how Excel writes one that is not. The
                 * decision is made on the style attribute below; this just
                 * says which edge the attribute belongs to. */
                if (xml_is(&X, YB))      cur_bord = cur_edge = 0;
                else if (xml_is(&X, YC)) cur_edge = STY_BORD_L >> 3;
                else if (xml_is(&X, YD)) cur_edge = STY_BORD_R >> 3;
                else if (xml_is(&X, YE)) cur_edge = STY_BORD_T >> 3;
                else if (xml_is(&X, YF)) cur_edge = STY_BORD_B >> 3;
                else                     cur_edge = 0;
            }
            else if (xml_is(&X, Y2) && in_numfmts) {
                pending_id = 0;
                text[0] = '\0';
            } else if (xml_is(&X, Y3) && in_cellxfs) {
                pending_id = 0;
                pending_font = 0;
                pending_bord = 0;
            }
        } else if (t == XML_ATTR) {
            if (in_borders && cur_edge && xml_is(&X, YH)) {
                /* style="none" is written out in full, and so is an empty
                 * value; anything else is a line. */
                if (X.value[0] && X.value[0] != 'n')
                    cur_bord |= cur_edge;
            } else if (xml_is(&X, YG) && in_cellxfs) {
                uint16_t v = 0;
                const char *p = X.value;

                while (*p >= '0' && *p <= '9')
                    v = (uint16_t)(v * 10 + (*p++ - '0'));
                pending_bord = v;
            } else if (xml_is(&X, Y4) || xml_is(&X, Y9)) {
                uint16_t v = 0;
                const char *p = X.value;
                while (*p >= '0' && *p <= '9')
                    v = (uint16_t)(v * 10 + (*p++ - '0'));
                if (xml_is(&X, Y9))
                    pending_font = v;
                else
                    pending_id = v;
            } else if (xml_is(&X, Y5)) {
                uint8_t n = 0;
                while (X.value[n] && n < sizeof text - 1) {
                    text[n] = X.value[n];
                    ++n;
                }
                text[n] = '\0';
            }
        } else if (t == XML_END) {
            if (xml_is(&X, Y2) && in_numfmts) {
                if (custom_count < CUSTOM_MAX && pending_id >= CUSTOM_FIRST) {
                    custom_id[custom_count] = pending_id;
                    custom_fmt[custom_count] =
                        classify_code(text, &custom_places[custom_count]);
                    ++custom_count;
                }
            } else if (xml_is(&X, Y3) && in_cellxfs) {
                if (info->count < X16S_MAX_STYLES) {
                    uint8_t fmt, places;

                    if (pending_id >= CUSTOM_FIRST) {
                        fmt = NF_GENERAL;
                        places = 0;
                        for (i = 0; i < custom_count; ++i)
                            if (custom_id[i] == pending_id) {
                                fmt = custom_fmt[i];
                                places = custom_places[i];
                                break;
                            }
                    } else {
                        fmt = builtin_format(pending_id, &places);
                    }
                    /* Bold rides in the top bit of the places byte, which
                     * counts decimals and never needs more than four. A
                     * third byte a style would widen the table and every
                     * offset that indexes it. */
                    if (pending_font < FONT_MAX
                        && (font_bold[pending_font >> 3]
                            & (uint8_t)(1 << (pending_font & 7))))
                        places = (uint8_t)(places | XLSX_PLACES_BOLD);

                    bank_poke(out, XLSX_STYLE_FMT(info->count), fmt);
                    bank_poke(out, XLSX_STYLE_PLACES(info->count), places);
                    bank_poke(out, XLSX_STYLE_BORD(info->count),
                              pending_bord < bord_count
                                  ? bord_bits[pending_bord] : 0);
                    ++info->count;
                } else {
                    info->dropped = 1;
                }
            } else if (xml_is(&X, YB) && in_borders) {
                if (bord_count < BORD_MAX)
                    bord_bits[bord_count++] = cur_bord;
                cur_bord = 0;
            } else if (xml_is(&X, YA)) {
                in_borders = 0;
            } else if (xml_is(&X, Y7) && in_fonts) {
                if (cur_bold && font_count < FONT_MAX)
                    font_bold[font_count >> 3] |=
                        (uint8_t)(1 << (font_count & 7));
                ++font_count;
                cur_bold = 0;
            } else if (xml_is(&X, Y6)) {
                in_fonts = 0;
            } else if (xml_is(&X, Y0)) {
                in_numfmts = 0;
            } else if (xml_is(&X, Y1)) {
                in_cellxfs = 0;
            }
        }
    }
    return ERR_OK;
}


#ifdef __CC65__
#  pragma bss-name (pop)
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
