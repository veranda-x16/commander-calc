/* xlsx_step_sheet.c — the cells, and the end of the import.
 *
 * The last step, so it returns 0 whatever happens: there is no next overlay
 * and the driver stops on it.
 */
#define BLOB_OWNER_SHEET                /* (blob_free: blob_sheet_x16.c's) */
#include "blob_ovl.h"
#include "xlsx_ctx.h"
#include "../platform/overlay.h"
#include "../platform/banked_ram.h"
#include <string.h>

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY10")
#  pragma rodata-name (push, "OVL10RO")
#  pragma bss-name (push, "OVL10BSS")
#endif

uint8_t xlsx_step_sheet(void)
{
    xlsx_sheet_result_t res;

    memset(&res, 0, sizeof res);
    xctx.err = xlsx_parse_sheet(&xctx.part, xctx.ids, xctx.rep.strings,
                                xctx.styles, xctx.sinfo.count, &res);

    /* Filled in even when the parse failed part-way: the report is what
     * tells the user how much of their workbook did get through. */
    /* Added, not assigned: the report covers the whole workbook and this
     * runs once per sheet. */
    xctx.rep.cells    += res.cells;
    xctx.rep.formulas += res.formulas;
    xctx.rep.dates    += res.dates;
    if (res.truncated)
        xctx.rep.truncated = 1;
    if (res.rows > xctx.rep.rows)
        xctx.rep.rows = res.rows;
    if (xctx.target.lossy)
        xctx.rep.lossy = 1;
#ifdef HEAP_TOP
    /* The rest of the text back to the heap, the top where it was (the
     * parser gave back each bank it read to its end: import/blob.c), even
     * when the parse stopped part-way */
    blob_free(&xctx.part);
#endif

    return 0;
}

#ifdef __CC65__
#  pragma bss-name (pop)
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
