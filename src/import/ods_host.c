/* ods_host.c — the two halves of the .ods reader, run back to back.
 *
 * The machine runs them as separate overlays from the import driver. Off
 * it there are none, so this is the single call the tests use, with the
 * record stream allocated here.
 */
#ifndef __CC65__

#include "ods.h"
#include "ods_rec.h"
#include <string.h>

err_t ods_parse(blob_t *xml, ods_result_t *res)
{
    blob_t recs;
    err_t e;

    memset(&recs, 0, sizeof recs);
    e = blob_alloc(&recs, ODS_REC_BYTES(xml->len));
    if (e != ERR_OK)
        return e;

    e = ods_scan(xml, &recs, res);
    if (e == ERR_OK)
        e = ods_commit(&recs, res);

    blob_free(&recs);
    return e;
}

#endif /* !__CC65__ */
