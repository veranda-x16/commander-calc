/* vcall_veranda.h -- Veranda's drawing calls made in place, for overlays.
 *
 * veranda.lib's C bindings are linked into the resident image, which has
 * a few hundred bytes to spare; these set the registers the bindings would
 * and jump through the kernel's table themselves, so the code stays in
 * the overlay that draws. VA_<name> is the call's address
 * (build/veranda/gen/vaddr.h, made from the SDK's veranda.inc).
 * Coordinates are pixels; every pointer passed must be below $A000. */
#ifndef VCALL_VERANDA_H
#define VCALL_VERANDA_H
#include <veranda.h>
#include "vaddr.h"

#define VJSR(a) __asm__("jsr %w", (unsigned)(a))

#ifndef VCALL_NO_VARS
static unsigned char vc_a;              /* a value for the a register */
#endif

#define vPattern(p)          do { vc_a = (p); __asm__("lda %v", vc_a); VJSR(VA_SetPattern); } while (0)
#define vRect(l, t, r, b)    do { r3 = (l); r11 = (t); r4 = (r); r12 = (b); VJSR(VA_WRectangle); } while (0)
#define vInvert(l, t, r, b)  do { r3 = (l); r11 = (t); r4 = (r); r12 = (b); VJSR(VA_WInvertRectangle); } while (0)
#define vFrame(l, t, r, b)   do { r3 = (l); r11 = (t); r4 = (r); r12 = (b); __asm__("lda #$FF"); VJSR(VA_WFrameRectangle); } while (0)
#define vVLine(x, t, b)      do { r4 = (x); r11 = (t); r12 = (b); __asm__("lda #$FF"); VJSR(VA_WVerticalLine); } while (0)
#define vHLine(l, r, y)      do { r3 = (l); r4 = (r); r11 = (y); __asm__("lda #$FF"); VJSR(VA_WHorizontalLine); } while (0)
#define vString(s, x, y)     do { r0 = (unsigned)(s); r11 = (x); r12 = (y); VJSR(VA_WPutString); } while (0)
#define vRstrDlg()           VJSR(VA_RstrFrmDialogue)

#endif
