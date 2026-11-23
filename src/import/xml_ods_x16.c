/* xml_ods_x16.c — the XML tokenizer, compiled again into OVL_ODS.
 *
 * The .ods reader is a third consumer of the tokenizer and, like the two
 * XLSX overlays, cannot call into either of theirs. Renamed through
 * XML_OWNER_ODS so the copies do not collide.
 */
#define XML_OWNER_ODS
#include "xml_ovl.h"

#define XML_NO_SEGMENT

#ifdef __CC65__
#  pragma code-name (push, "OVERLAY18")
#  pragma rodata-name (push, "OVL18RO")
#endif

#include "xml.c"

#ifdef __CC65__
#  pragma rodata-name (pop)
#  pragma code-name (pop)
#endif
