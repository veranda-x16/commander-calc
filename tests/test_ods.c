/* test_ods.c — the OpenDocument reader.
 *
 * The first half of the file parses content.xml held in a buffer, one
 * detail per test; the second runs a real .ods through the phases the
 * machine uses -- stage the entry out of the ZIP, inflate it, parse it.
 *
 * Fixtures: tools/make_ods_fixtures.py.
 */
#include "test.h"
#include "../src/platform/bankmem.h"
#include "../src/platform/banked_ram.h"
#include "../src/platform/file_io.h"
#include "../src/workbook/workbook.h"
#include "../src/workbook/strings.h"
#include "../src/workbook/styles.h"
#include "../src/formula/formula.h"
#include "../src/import/blob.h"
#include "../src/import/inflate.h"
#include "../src/import/ods.h"
#include "../src/import/xlsx_stage.h"
#include "../src/import/xlsx_ctx.h"
#include <string.h>
#include <stdio.h>

static blob_t part;

static void setup(void)
{
    CHECK(bankmem_host_init(96));
    CHECK_EQ(bank_heap_init(1, 0), ERR_OK);
    CHECK_EQ(wb_init(), ERR_OK);
    file_host_set_root("build/host/sd");
    memset(&part, 0, sizeof part);
    /* The driver allocates this, and the parser queues formulas into it.
     * Without it every formula is dropped without a word. */
    formula_pending_n = 0;
    formula_pending = bank_alloc(FPEND_MAX * FPEND_FIELD);
    CHECK(formula_pending != H_NULL);
}

static void teardown(void)
{
    blob_free(&part);
    bankmem_host_free();
}

/* Parse a piece of content.xml held in memory. */
static err_t parse(const char *xml, ods_result_t *res)
{
    uint16_t len = (uint16_t)strlen(xml);

    blob_free(&part);
    memset(&part, 0, sizeof part);
    CHECK_EQ(blob_alloc(&part, len), ERR_OK);
    CHECK(blob_write(&part, xml, len));
    return ods_parse(&part, res);
}

static const char HEAD[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<office:document-content "
    "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
    "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
    "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\">"
    "<office:body><office:spreadsheet>";
static const char TAIL[] =
    "</office:spreadsheet></office:body></office:document-content>";

/* Wrap a table body in the document around it. */
static err_t parse_table(const char *body, ods_result_t *res)
{
    static char buf[4096];

    CHECK(strlen(HEAD) + strlen(body) + strlen(TAIL) < sizeof buf);
    strcpy(buf, HEAD);
    strcat(buf, body);
    strcat(buf, TAIL);
    return parse(buf, res);
}

/* What the driver does once the parse is over: the parser cannot load the
 * compiler from inside its own overlay, so the formulas it queued are
 * compiled afterwards and the sheet recalculated. */
static void compile_all(void)
{
    uint16_t n = 0;

    formula_compile_pending(&n);
    wb_after_change();
}

static void disp(uint16_t row, uint16_t col, char *out)
{
    wb_display_text(row, col, out, WB_TEXT_MAX);
}

/* --- values ----------------------------------------------------------- */

/* Every office:value-type, and where the value actually comes from: the
 * attribute, never the <text:p>, which holds the formatted version. */
static void test_value_types(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];
    cell_record_t rec;

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\"><table:table-row>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>hello</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"float\" office:value=\"8.5\">"
        "<text:p>8.50</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"percentage\" "
        "office:value=\"0.175\"><text:p>17.50%</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"currency\" "
        "office:currency=\"USD\" office:value=\"1234.5\">"
        "<text:p>$1,234.50</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"date\" "
        "office:date-value=\"2024-03-15\">"
        "<text:p>2024-03-15</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"boolean\" "
        "office:boolean-value=\"true\"><text:p>TRUE</text:p>"
        "</table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "hello");
    /* The number, not the string beside it: office:value is the truth and
     * <text:p> is only how Calc displayed it. */
    disp(0, 1, b); CHECK_STR(b, "8.5");
    /* A percentage is stored as its fraction and shown to the two places
     * the format carries. */
    disp(0, 2, b); CHECK_STR(b, "17.50%");
    disp(0, 3, b); CHECK_STR(b, "$1234.50");
    /* An ISO date becomes a serial. */
    disp(0, 4, b); CHECK_STR(b, "2024-03-15");
    CHECK(wb_get(0, 4, &rec));
    CHECK_EQ(rec.type, CELL_DATE);
    disp(0, 5, b); CHECK_STR(b, "TRUE");
    CHECK(wb_get(0, 5, &rec));
    CHECK_EQ(rec.type, CELL_BOOLEAN);

    CHECK_EQ(r.cells, 6);
    CHECK_EQ(r.dates, 1);
    CHECK_EQ(r.sheets, 1);
    teardown();
}

/* A cell with no type and no text is not a cell. ODF writes runs of them
 * to say "nothing here", and materialising them would fill the store. */
static void test_empty_cells_are_not_stored(void)
{
    ods_result_t r;
    cell_record_t rec;

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\"><table:table-row>"
        "<table:table-cell/>"
        "<table:table-cell table:number-columns-repeated=\"16384\"/>"
        "</table:table-row></table:table>", &r), ERR_OK);

    CHECK_EQ(r.cells, 0);
    CHECK_EQ(wb_cells()->cell_count, 0);
    CHECK_EQ(wb_get(0, 0, &rec), 0);
    teardown();
}

/* --- position --------------------------------------------------------- */

/* There are no cell references in ODF: a cell's column is how many came
 * before it, and an empty run counts. Losing this loses the sheet. */
static void test_repeats_place_the_cells(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\"><table:table-row>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>A</text:p></table:table-cell>"
        "<table:table-cell table:number-columns-repeated=\"3\"/>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>E</text:p></table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    /* One cell, three skipped, so the second lands in column E. */
    disp(0, 0, b); CHECK_STR(b, "A");
    disp(0, 4, b); CHECK_STR(b, "E");
    CHECK_EQ(r.cells, 2);
    teardown();
}

/* A repeat on a cell that HAS a value means that value several times. */
static void test_repeated_value_is_copied(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\"><table:table-row>"
        "<table:table-cell office:value-type=\"float\" office:value=\"7\" "
        "table:number-columns-repeated=\"3\"><text:p>7</text:p>"
        "</table:table-cell>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>after</text:p></table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "7");
    disp(0, 1, b); CHECK_STR(b, "7");
    disp(0, 2, b); CHECK_STR(b, "7");
    disp(0, 3, b); CHECK_STR(b, "after");
    CHECK_EQ(r.cells, 4);
    teardown();
}

/* Rows repeat too, and a blank one repeated a thousand times is how every
 * file ends. What follows it has to land on the right row. */
static void test_row_repeats(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\">"
        "<table:table-row><table:table-cell office:value-type=\"string\">"
        "<text:p>top</text:p></table:table-cell></table:table-row>"
        "<table:table-row table:number-rows-repeated=\"10\">"
        "<table:table-cell/></table:table-row>"
        "<table:table-row><table:table-cell office:value-type=\"string\">"
        "<text:p>below</text:p></table:table-cell></table:table-row>"
        "</table:table>", &r), ERR_OK);

    disp(0, 0, b);  CHECK_STR(b, "top");
    disp(11, 0, b); CHECK_STR(b, "below");
    CHECK_EQ(wb_cells()->cell_count, 2);
    teardown();
}

/* A repeated row WITH content is three identical rows, not one. */
static void test_repeated_row_with_content(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\">"
        "<table:table-row table:number-rows-repeated=\"3\">"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>same</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"float\" office:value=\"2\">"
        "<text:p>2</text:p></table:table-cell></table:table-row>"
        "<table:table-row><table:table-cell office:value-type=\"string\">"
        "<text:p>last</text:p></table:table-cell></table:table-row>"
        "</table:table>", &r), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "same");
    disp(1, 0, b); CHECK_STR(b, "same");
    disp(2, 0, b); CHECK_STR(b, "same");
    disp(2, 1, b); CHECK_STR(b, "2");
    disp(3, 0, b); CHECK_STR(b, "last");
    teardown();
}

/* --- formulas --------------------------------------------------------- */

/* ODF references are bracketed and dotted, and the separator is a
 * semicolon. All of that has to come out as ours or the compiler sees a
 * different formula. */
static void test_formula_conversion(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\">"
        "<table:table-row>"
        "<table:table-cell office:value-type=\"float\" office:value=\"2\">"
        "<text:p>2</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"float\" office:value=\"3\">"
        "<text:p>3</text:p></table:table-cell>"
        "<table:table-cell table:formula=\"of:=[.A1]*[.B1]\" "
        "office:value-type=\"float\" office:value=\"6\"><text:p>6</text:p>"
        "</table:table-cell>"
        "<table:table-cell table:formula=\"of:=SUM([.A1:.B1])\" "
        "office:value-type=\"float\" office:value=\"5\"><text:p>5</text:p>"
        "</table:table-cell>"
        "<table:table-cell table:formula=\"of:=ROUND([.A1]/[.B1];2)\" "
        "office:value-type=\"float\" office:value=\"0.67\">"
        "<text:p>0.67</text:p></table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    CHECK_EQ(r.formulas, 3);

    /* Until they are compiled the cells hold the cached values. */
    disp(0, 2, b); CHECK_STR(b, "6");

    /* Compile them, as the driver does, and they must agree. */
    compile_all();
    disp(0, 2, b); CHECK_STR(b, "6");
    disp(0, 3, b); CHECK_STR(b, "5");
    /* ".67": how this program has always written a number below one. */
    disp(0, 4, b); CHECK_STR(b, ".67");

    /* And the text really is ours, not ODF's. */
    wb_edit_text(0, 3, b, sizeof b);
    CHECK_STR(b, "=SUM(A1:B1)");
    wb_edit_text(0, 4, b, sizeof b);
    CHECK_STR(b, "=ROUND(A1/B1,2)");
    teardown();
}

/* A sheet-qualified reference, with and without the dollars ODF puts on the
 * sheet name -- which mean nothing to us and must not survive. */
static void test_cross_sheet_formula(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"First\"><table:table-row>"
        "<table:table-cell office:value-type=\"float\" office:value=\"4\">"
        "<text:p>4</text:p></table:table-cell></table:table-row>"
        "</table:table>"
        "<table:table table:name=\"Second\"><table:table-row>"
        "<table:table-cell table:formula=\"of:=[First.A1]+1\" "
        "office:value-type=\"float\" office:value=\"5\"><text:p>5</text:p>"
        "</table:table-cell>"
        "<table:table-cell table:formula=\"of:=[$First.$A$1]*2\" "
        "office:value-type=\"float\" office:value=\"8\"><text:p>8</text:p>"
        "</table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    CHECK_EQ(r.sheets, 2);
    CHECK_EQ(wb_sheet_n, 2);

    compile_all();

    CHECK_EQ(wb_sheet_switch(1), ERR_OK);
    wb_edit_text(0, 0, b, sizeof b);
    CHECK_STR(b, "=First!A1+1");
    /* The dollars on the sheet name go; the ones on the cell stay. */
    wb_edit_text(0, 1, b, sizeof b);
    CHECK_STR(b, "=First!$A$1*2");
    disp(0, 0, b); CHECK_STR(b, "5");
    disp(0, 1, b); CHECK_STR(b, "8");
    teardown();
}

/* A formula too long for the tokenizer's attribute buffer would compile
 * into a DIFFERENT formula. It is dropped, the cached value stays, and the
 * import says it lost something. */
static void test_overlong_formula_is_dropped_not_truncated(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];
    static char body[1024];
    char longref[600];
    uint16_t i;

    setup();
    /* Comfortably past XML_VALUE_MAX. */
    strcpy(longref, "of:=");
    for (i = 0; i < 40; ++i)
        strcat(longref, "[.A1]+");
    strcat(longref, "1");

    sprintf(body,
            "<table:table table:name=\"T\"><table:table-row>"
            "<table:table-cell table:formula=\"%s\" "
            "office:value-type=\"float\" office:value=\"41\">"
            "<text:p>41</text:p></table:table-cell>"
            "</table:table-row></table:table>", longref);
    CHECK_EQ(parse_table(body, &r), ERR_OK);

    CHECK_EQ(r.formulas, 0);
    CHECK_EQ(r.truncated, 1);
    /* The value the file cached is what is left, not a broken formula. */
    disp(0, 0, b); CHECK_STR(b, "41");
    teardown();
}

/* --- text ------------------------------------------------------------- */

/* An annotation is a comment. Its text:p is not the cell's. */
static void test_annotation_is_not_cell_text(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\"><table:table-row>"
        "<table:table-cell office:value-type=\"string\">"
        "<office:annotation><text:p>a note</text:p></office:annotation>"
        "<text:p>kept</text:p></table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "kept");
    teardown();
}

/* Rich text is spans inside the paragraph; the cell takes the words. */
static void test_spans_are_joined(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"T\"><table:table-row>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>a<text:span>b</text:span>c</text:p></table:table-cell>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>Cable &amp; Plug</text:p></table:table-cell>"
        "</table:table-row></table:table>", &r), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "abc");
    disp(0, 1, b); CHECK_STR(b, "Cable & Plug");
    teardown();
}

/* --- sheets ----------------------------------------------------------- */

/* The first table renames the sheet the reset left; the rest are created,
 * which this overlay has to do for itself. */
static void test_sheets_are_named_and_created(void)
{
    ods_result_t r;
    char name[X16S_MAX_SHEET_NAME + 1];

    setup();
    CHECK_EQ(parse_table(
        "<table:table table:name=\"Budget\"><table:table-row>"
        "<table:table-cell office:value-type=\"float\" office:value=\"1\">"
        "<text:p>1</text:p></table:table-cell></table:table-row>"
        "</table:table>"
        "<table:table table:name=\"Notes\"><table:table-row>"
        "<table:table-cell office:value-type=\"string\">"
        "<text:p>hi</text:p></table:table-cell></table:table-row>"
        "</table:table>", &r), ERR_OK);

    CHECK_EQ(wb_sheet_n, 2);
    CHECK_EQ(r.sheets, 2);
    wb_sheet_name(0, name); CHECK_STR(name, "Budget");
    wb_sheet_name(1, name); CHECK_STR(name, "Notes");
    teardown();
}

/* --- the package ------------------------------------------------------ */

/* A real .ods, through the phases the machine runs: stage the entry out of
 * the ZIP, inflate it, parse it. */
static err_t open_ods(const char *file)
{
    stage_info_t info;
    static blob_t raw;
    err_t e;

    memset(&raw, 0, sizeof raw);
    e = xlsx_stage(file, "content.xml", &raw, &info);
    if (e != ERR_OK)
        return e;
    if (!info.compressed) {
        part = raw;
        memset(&raw, 0, sizeof raw);
        return ERR_OK;
    }
    return inflate_blob(&raw, &part, info.raw_size);
}

static void test_real_package(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];
    char name[X16S_MAX_SHEET_NAME + 1];

    setup();
    CHECK_EQ(open_ods("RICH.ODS"), ERR_OK);
    CHECK_EQ(ods_parse(&part, &r), ERR_OK);

    CHECK_EQ(r.sheets, 2);
    wb_sheet_name(0, name); CHECK_STR(name, "First");
    wb_sheet_name(1, name); CHECK_STR(name, "Second");

    /* The parse leaves the LAST sheet active, because that is the one it
     * was filling when the document ended. */
    CHECK_EQ(wb_sheet_i, 1);
    CHECK_EQ(wb_sheet_switch(0), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "Name");
    disp(1, 0, b); CHECK_STR(b, "Cable & Plug");
    disp(1, 1, b); CHECK_STR(b, "4");
    disp(1, 2, b); CHECK_STR(b, "8.5");
    /* Column D of the total row, past the two-cell gap. */
    disp(3, 3, b); CHECK_STR(b, "64");
    /* The typed row. */
    disp(4, 0, b); CHECK_STR(b, "17.50%");
    disp(4, 1, b); CHECK_STR(b, "$1234.50");
    disp(4, 2, b); CHECK_STR(b, "2024-03-15");
    disp(4, 3, b); CHECK_STR(b, "TRUE");
    /* Five rows, then a blank one standing for a thousand, so what follows
     * is row 1005 -- the repeat counts the row it is written on. */
    disp(1005, 0, b); CHECK_STR(b, "After the gap");

    /* Compiling walks the queue sheet by sheet and leaves the last one it
     * touched active, so the sheet has to be chosen again afterwards. */
    compile_all();
    CHECK_EQ(wb_sheet_switch(0), ERR_OK);
    disp(1, 3, b); CHECK_STR(b, "34");
    disp(3, 3, b); CHECK_STR(b, "64");

    CHECK_EQ(wb_sheet_switch(1), ERR_OK);
    disp(0, 0, b); CHECK_STR(b, "Second sheet");
    disp(0, 1, b); CHECK_STR(b, "5");
    teardown();
}

static void test_real_package_oddities(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(open_ods("ODD.ODS"), ERR_OK);
    CHECK_EQ(ods_parse(&part, &r), ERR_OK);

    disp(0, 0, b); CHECK_STR(b, "plain");
    disp(0, 1, b); CHECK_STR(b, "kept");     /* not "a notekept" */
    disp(0, 2, b); CHECK_STR(b, "abc");
    /* The merged anchor keeps its column, and the covered cell holds the
     * place so what follows is not shifted left. */
    disp(1, 0, b); CHECK_STR(b, "merged");
    disp(1, 2, b); CHECK_STR(b, "7");
    /* Three identical rows written once. */
    disp(2, 0, b); CHECK_STR(b, "three");
    disp(4, 0, b); CHECK_STR(b, "three");
    disp(5, 0, b); CHECK_STR(b, "end");
    teardown();
}

/* --- borders ----------------------------------------------------------
 *
 * A cell's borders come from an automatic style, named either on the cell
 * or on its column. Both paths matter: Calc writes a heading row's
 * underline as the COLUMN's default cell style and then opts the cells
 * below out by name. */
static void test_borders(void)
{
    ods_result_t r;
    cell_record_t rec;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(open_ods("BORDER.ODS"), ERR_OK);
    CHECK_EQ(ods_parse(&part, &r), ERR_OK);

    /* Column A defaults to an underline, so A1 has one without naming a
     * style at all. B1 and C1 are in columns that default to nothing. */
    CHECK(wb_get(0, 0, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, STY_BORD_B);
    CHECK(wb_get(0, 1, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, 0);

    /* A2 names "Default", which is how a cell opts OUT of its column. */
    disp(1, 0, b); CHECK_STR(b, "opted out");
    CHECK(wb_get(1, 0, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, 0);

    /* Two corners of a box, named on the cells themselves. */
    CHECK(wb_get(1, 1, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL,
             (uint8_t)(STY_BORD_T | STY_BORD_L));
    CHECK(wb_get(1, 2, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL,
             (uint8_t)(STY_BORD_T | STY_BORD_R));

    /* fo:border, the shorthand, is all four at once. */
    CHECK(wb_get(2, 0, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, STY_BORD_ALL);

    /* An EMPTY cell that is nothing but a border still has to exist: the
     * renderer reads the style off the record, so without one the line
     * cannot be drawn. */
    CHECK(wb_get(2, 1, &rec));
    CHECK_EQ(rec.type, CELL_EMPTY);
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, STY_BORD_ALL);

    /* And a border does not disturb the value beside it. */
    disp(2, 2, b); CHECK_STR(b, "9");
    teardown();
}

/* The same, in a file LibreOffice wrote rather than one I did. */
static void test_borders_libreoffice(void)
{
    ods_result_t r;
    cell_record_t rec;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(open_ods("LOBOX.ODS"), ERR_OK);
    CHECK_EQ(ods_parse(&part, &r), ERR_OK);

    /* The heading row, underlined -- and Calc put that underline on the
     * columns rather than the cells. */
    disp(0, 0, b); CHECK_STR(b, "Item");
    CHECK(wb_get(0, 0, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, STY_BORD_B);
    CHECK(wb_get(0, 2, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, STY_BORD_B);

    /* The cells below say table:style-name="Default" to opt out. */
    disp(1, 0, b); CHECK_STR(b, "plain");
    CHECK(wb_get(1, 0, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, 0);

    /* Two top corners of a block, and a fully boxed cell. */
    CHECK(wb_get(1, 1, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL,
             (uint8_t)(STY_BORD_T | STY_BORD_L));
    CHECK(wb_get(1, 2, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL,
             (uint8_t)(STY_BORD_T | STY_BORD_R));
    CHECK(wb_get(2, 0, &rec));
    CHECK_EQ(wb_flags(&rec) & STY_BORD_ALL, STY_BORD_ALL);
    teardown();
}

/* A name the card does not have is reported as missing, not as damaged.
 *
 * Both come back ERR_NOTFOUND from the staging layer -- the archive would
 * not open, or it opened and had no content.xml -- and telling the user a
 * file is "damaged or unrecognised" when the real problem is the path
 * sends them looking in the wrong place. */
static void test_missing_file_is_not_damaged(void)
{
    setup();
    CHECK_EQ(ods_import("NOSUCH.ODS"), ERR_NOTFOUND);

    /* A real file that is not an archive at all is the damaged case. */
    {
        FILE *f = fopen("build/host/sd/JUNK.ODS", "wb");
        CHECK(f != NULL);
        if (f) {
            fputs("this is not a zip", f);
            fclose(f);
        }
    }
    CHECK_EQ(ods_import("JUNK.ODS"), ERR_BADFORMAT);
    remove("build/host/sd/JUNK.ODS");
    teardown();
}

/* A file that is not an .ods at all fails rather than importing rubbish. */
static void test_not_a_spreadsheet(void)
{
    ods_result_t r;

    setup();
    CHECK_EQ(parse("<html><body>no</body></html>", &r), ERR_OK);
    CHECK_EQ(r.cells, 0);
    CHECK_EQ(r.sheets, 0);
    teardown();
}

/* The fixtures above are built by a script; these two came out of
 * LibreOffice itself, and check the script against it. */
static void test_libreoffice_file(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];

    setup();
    CHECK_EQ(open_ods("LOSHEET.ODS"), ERR_OK);
    CHECK_EQ(ods_parse(&part, &r), ERR_OK);
    disp(0, 0, b); CHECK_STR(b, "Name");
    disp(1, 0, b); CHECK_STR(b, "Cable");
    disp(1, 1, b); CHECK_STR(b, "4");
    disp(1, 2, b); CHECK_STR(b, "8.5");
    disp(4, 0, b); CHECK_STR(b, "Total");
    compile_all();
    disp(1, 3, b); CHECK_STR(b, "34");
    disp(4, 3, b); CHECK_STR(b, "93");   /* =SUM(D2:D4) over 34+30+29 */
    teardown();

    setup();
    CHECK_EQ(open_ods("LOTYPES.ODS"), ERR_OK);
    CHECK_EQ(ods_parse(&part, &r), ERR_OK);
    disp(0, 0, b); CHECK_STR(b, "When");
    disp(1, 0, b); CHECK_STR(b, "2024-03-15");
    disp(1, 1, b); CHECK_STR(b, "17.50%");
    disp(1, 3, b); CHECK_STR(b, "$1234.50");
    disp(1, 4, b); CHECK_STR(b, "plain");
    disp(2, 0, b); CHECK_STR(b, "2025-12-01");
    disp(2, 4, b); CHECK_STR(b, "a, b");
    teardown();
}

static void probe_user_file(void)
{
    ods_result_t r;
    char b[WB_TEXT_MAX];
    uint16_t rr;

    {
        err_t oe, pe;

        setup();
        oe = open_ods("TESTODS.ODS");
        pe = ods_parse(&part, &r);
        fprintf(stderr, "PROBE open=%d parse=%d cells=%u sheets=%u "
                "found=%u trunc=%u sheet_n=%u\n",
                (int)oe, (int)pe, (unsigned)r.cells, (unsigned)r.sheets,
                (unsigned)r.sheets_found, (unsigned)r.truncated,
                (unsigned)wb_sheet_n);
    }
    for (rr = 0; rr < 4; ++rr) {
        wb_display_text(rr, 0, b, sizeof b);
        fprintf(stderr, "PROBE r%u c0 = '%s'\n", (unsigned)rr, b);
    }
    teardown();
}

void test_ods(void)
{
    probe_user_file();
    test_libreoffice_file();
    test_value_types();
    test_empty_cells_are_not_stored();
    test_repeats_place_the_cells();
    test_repeated_value_is_copied();
    test_row_repeats();
    test_repeated_row_with_content();
    test_formula_conversion();
    test_cross_sheet_formula();
    test_overlong_formula_is_dropped_not_truncated();
    test_annotation_is_not_cell_text();
    test_spans_are_joined();
    test_sheets_are_named_and_created();
    test_real_package();
    test_real_package_oddities();
    test_borders();
    test_borders_libreoffice();
    test_missing_file_is_not_damaged();
    test_not_a_spreadsheet();
}
