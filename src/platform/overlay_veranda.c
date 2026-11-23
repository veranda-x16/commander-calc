/* overlay_veranda.c -- overlays on Veranda: each one runs in place.
 *
 * The application file is packed with `overlays = banked` (Veranda API
 * level 2): the loader puts overlay record n at $A000 of a RAM bank of its
 * own, and LoadOverlay(n, 0) names the bank. Loading an overlay is then one
 * store to the bank register -- no disk, no copy. The window at $8000 the
 * X16 build loads into does not exist here; that is what lets the
 * resident image have $0801-$85FF.
 *
 * Three things keep the mapping right:
 *   - bank_home: the heap accessors (bankmem_veranda.c) map the heap's
 *     banks and put this one back, not whatever was mapped at their entry.
 *   - appBank: Veranda maps it before it calls the program (keyVector,
 *     appMain ...), so a callback runs with the current overlay mapped.
 *   - the bank gate gives the caller's bank back after every kernel call.
 */
#include "overlay.h"
#include <veranda.h>

#define RAM_BANK (*(volatile uint8_t *)0x0000)

uint8_t bank_home;                      /* the bank the program runs with */
uint8_t ovl_bank[OVL_LAST + 1];        /* 0: not in the file */
uint8_t ovl_status;
static uint8_t resident;

/* Ask the loader where each overlay is: at start-up, once. */
uint8_t ovl_init(void)
{
    uint8_t i, n = 0;

    for (i = 1; i <= OVL_LAST; ++i) {      /* (OVL_PLAY: test builds only) */
        if (LoadOverlay(i, 0) == 0 && r0 == 0xA000) {
            ovl_bank[i] = r1L;
            ++n;
        }
    }
    bank_home = RAM_BANK;
    return n;
}

err_t ovl_require(uint8_t id)
{
    uint8_t b;

    if (id == 0 || id > OVL_LAST || (b = ovl_bank[id]) == 0) {
        ovl_status = 0x0E;
        return ERR_NOTFOUND;
    }
    RAM_BANK = b;
    bank_home = b;
    appBank = b;
    resident = id;
    return ERR_OK;
}

uint8_t ovl_current(void)
{
    return resident;
}

/* Nothing to throw away: an overlay's bank is never written by another. */
void ovl_invalidate(void)
{
}

void ovl_set_device(uint8_t d)
{
    (void)d;
}
