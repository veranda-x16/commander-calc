/* ods_rec.h — the record stream between the two halves of the .ods reader.
 *
 * ods_scan() writes these into a blob in banked RAM and ods_commit() reads
 * them back. Everything is little-endian and unaligned; both halves work a
 * byte at a time.
 */
#ifndef X16S_ODS_REC_H
#define X16S_ODS_REC_H

#include "blob.h"

#define ODS_R_END    0      /* no more                                    */
#define ODS_R_SHEET  1      /* u8 len, name[len]                          */
#define ODS_R_CELL   2      /* see below                                  */
#define ODS_R_ROWREP 3      /* u16 row, u16 count: repeat a filled row    */
#define ODS_R_COLSTY 4      /* u8 col, u16 count, u8 bits: column default */

/* A cell:
 *      u8   ODS_R_CELL
 *      u16  row
 *      u8   column
 *      u16  repeat           columns, already counted by the scanner
 *      u8   value type       ODS_VT_*
 *      u8   len, bytes       office:value, or the date/time/boolean one
 *      u8   len, bytes       the text of <text:p>
 *      u8   len, bytes       table:formula, in ODF's syntax
 *      u8   borders          ODS_B_*
 */

/* Border edges, in the order STY_BORD_* has them shifted down by three, so
 * that ods_commit() can put them straight into a style's flags. */
#define ODS_B_L 0x01
#define ODS_B_R 0x02
#define ODS_B_T 0x04
#define ODS_B_B 0x08
/* The cell named a style of its own, which overrides its column's default
 * even when that style draws nothing. */
#define ODS_B_OWN 0x80

#define ODS_VT_NONE   0
#define ODS_VT_FLOAT  1
#define ODS_VT_PCT    2
#define ODS_VT_CUR    3
#define ODS_VT_DATE   4
#define ODS_VT_TIME   5
#define ODS_VT_BOOL   6
#define ODS_VT_STRING 7

/* The longest field carried across, and the longest style name. */
#define ODS_FIELD_MAX 96
#define ODS_NAME_MAX 12

#endif /* X16S_ODS_REC_H */
