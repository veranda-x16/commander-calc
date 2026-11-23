/* blob_odsc_x16.c — the blob layer, compiled again into OVL_ODSC.
 * Reader-only: the commit half consumes the record stream the scan half
 * wrote and never builds one. See blob_ovl.h.
 */
#define BLOB_OWNER_ODSC
#include "blob_ovl.h"

#define BLOB_NO_SEGMENT
#define BLOB_READER_ONLY
#define BLOB_NEED_FREE          /* it releases the records when done */

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY19")
#  pragma rodata-name (push, "OVL19RO")
#endif

#include "blob.c"

#ifdef __CC65__
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
