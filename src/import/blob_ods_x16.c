/* blob_ods_x16.c — the blob layer, compiled again into OVL_ODS.
 * The scan half reads content.xml out of one blob and writes its records
 * into another, so this copy needs the writer as well. See blob_ovl.h.
 */
#define BLOB_OWNER_ODS
#include "blob_ovl.h"

#define BLOB_NO_SEGMENT

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY18")
#  pragma rodata-name (push, "OVL18RO")
#endif

#include "blob.c"

#ifdef __CC65__
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
