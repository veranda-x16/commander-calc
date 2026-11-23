/* ods.h — reading an OpenDocument spreadsheet.
 *
 * An .ods is a ZIP, so the first two phases are the ones the .xlsx reader
 * uses: the ZIP overlay stages content.xml and the inflate overlay expands
 * it. Only the parse differs, and it is in two halves across two overlays
 * -- ods_scan() turns XML into records, ods_commit() turns records into
 * cells. docs/design/ods.md has the reasoning.
 */
#ifndef X16S_ODS_H
#define X16S_ODS_H

#include "blob.h"

typedef struct {
    uint16_t cells;         /* stored                                     */
    uint16_t formulas;      /* cells that carried one                     */
    uint16_t dates;         /* dates and times                            */
    uint8_t  sheets_found;  /* <table:table> elements seen (scan)         */
    uint8_t  sheets;        /* worksheets created (commit)                */
    uint8_t  sheets_skipped;/* more than the workbook holds               */
    uint8_t  truncated;     /* something was dropped                      */
    uint8_t  lossy;         /* text had characters ISO-8859-15 lacks      */
    uint16_t rows;          /* up to the last row a cell landed in (commit) */
} ods_result_t;

/* content.xml into records. In OVL_ODS. */
err_t ods_scan(blob_t *xml, blob_t *recs, ods_result_t *res);

/* Records into cells, adding its counts to the same report. The workbook
 * must already be empty; the caller resets it. Formulas are queued with
 * xlsx_note_formula() for the driver to compile. In OVL_ODSC. */
err_t ods_commit(blob_t *recs, ods_result_t *res);

/* Room to allocate for the records. They are smaller than the XML they
 * came from; running short is reported as truncated, not fatal. */
#define ODS_REC_BYTES(xml_bytes) ((xml_bytes) / 2 + 4096)

#ifndef __CC65__
/* Both halves back to back. Host only: on the machine they are separate
 * overlays and the driver runs them. */
err_t ods_parse(blob_t *xml, ods_result_t *res);
#endif

#endif /* X16S_ODS_H */
