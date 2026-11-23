/* ods_step.c — the .ods phases, as steps of the import driver.
 *
 * One per overlay the reader is split across: the scan step names the
 * commit step as its successor, the commit step names nothing, and the
 * driver compiles the queued formulas afterwards.
 */
/* The scan step allocates the record blob, so it uses the scan half's copy
 * of the blob layer -- the commit step only passes the blob along. */
#define BLOB_OWNER_ODS
#include "blob_ovl.h"

#include "xlsx_ctx.h"
#include "ods.h"
#include "ods_rec.h"
#include "../platform/overlay.h"
#include <string.h>

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY18")
#  pragma rodata-name (push, "OVL18RO")
#endif

/* The records reuse the blob the ZIP phase staged content.xml into, which
 * inflate_blob() released on its way out. */
uint8_t ods_step_scan(void)
{
    ods_result_t r;

    xctx.err = blob_alloc(&xctx.staged, ODS_REC_BYTES(xctx.raw_size));
    if (xctx.err != ERR_OK)
        return 0;

    xctx.err = ods_scan(&xctx.part, &xctx.staged, &r);
    if (xctx.err != ERR_OK)
        return 0;

    /* Only the counts the scanner knows about. The other half adds its own
     * to the same report. */
    xctx.rep.sheets_found = r.sheets_found;
    xctx.rep.truncated    = r.truncated;
    xctx.rep.lossy        = r.lossy;
    return OVL_ODSC;
}

#ifdef __CC65__
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#  pragma code-name (push, "OVERLAY19")
#  pragma rodata-name (push, "OVL19RO")
#endif

uint8_t ods_step_commit(void)
{
    ods_result_t r;

    memset(&r, 0, sizeof r);
    xctx.err = ods_commit(&xctx.staged, &r);

    xctx.rep.cells          = r.cells;
    xctx.rep.formulas       = r.formulas;
    xctx.rep.dates          = r.dates;
    xctx.rep.sheets_skipped = r.sheets_skipped;
    xctx.rep.rows           = r.rows;
    if (r.truncated)
        xctx.rep.truncated = 1;
    return 0;
}

#ifdef __CC65__
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
