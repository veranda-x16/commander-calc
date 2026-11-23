/* overlay.h — load one of the swappable code overlays into $8000.
 *
 * Only one overlay is resident at a time. Overlays never call each other and
 * keep no state across a swap: results go to banked RAM or to disk. Calling
 * into an overlay that is not currently loaded jumps into whatever the last
 * overlay left behind, so every entry point must go through ovl_require().
 *
 * On the host build every overlay is linked in permanently and these calls
 * are no-ops that always succeed.
 */
#ifndef X16S_OVERLAY_H
#define X16S_OVERLAY_H

#include "../util/errors.h"

/* Load overlay `id` (OVL_CSV / OVL_ZIP / OVL_XLSX) unless it is already the
 * resident one. Returns ERR_OK, ERR_NOTFOUND or ERR_IO. */
err_t ovl_require(uint8_t id);

/* The KERNAL status byte from the last failed load: $80 device not
 * present, $40 end of file, $02 timeout, $10 checksum. Zero until one
 * fails. */
extern uint8_t ovl_status;

/* Which overlay is resident right now, or 0 for none. */
uint8_t ovl_current(void);

/* Forget the resident overlay, forcing the next ovl_require() to reload.
 * Used after anything that may have overwritten $8000..$9EFF. */
void ovl_invalidate(void);

/* Override the drive overlays are read from. Defaults to the device the
 * program was loaded from, or 8. */
void ovl_set_device(uint8_t d);

/* Call fn, a routine of OVL_FAR's, from resident code or any overlay, and
 * return to it (Veranda: overlays run in place, so a call into another one
 * maps its bank and puts the caller's back -- ovlfar_veranda.s). fn takes
 * nothing and returns nothing. Elsewhere a plain call. */
#ifdef X16S_VERANDA
#  define OVL_FAR OVL_CHARTOUT
void __fastcall__ ovl_far(void (*fn)(void));
/* the same for a routine of OVL_CLIP's (overlay 21: the clipboard and
 * Veranda's other own parts: the date, Veranda Look 2's chrome) */
void __fastcall__ ovl_far21(void (*fn)(void));
/* and of OVL_VRFILE's (overlay 22: the SDK's Open and Save dialog) */
void __fastcall__ ovl_far22(void (*fn)(void));
#else
#  define ovl_far(fn) (fn)()
#  define ovl_far21(fn) (fn)()
#  define ovl_far22(fn) (fn)()
#endif

#endif /* X16S_OVERLAY_H */
