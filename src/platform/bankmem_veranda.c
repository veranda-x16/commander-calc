/* bankmem_veranda.c -- the heap's banks on Veranda (bankmem.h).
 *
 * As bankmem_x16.c, with two differences, both because overlays run in
 * place in the bank window (overlay_veranda.c):
 *   - every accessor maps bank_home again before it returns, so code in an
 *     overlay gets its own bank back;
 *   - a buffer at $A000 or above is in the caller's (an overlay's) bank,
 *     which is not mapped while the heap's is: such copies go through a
 *     small buffer below $A000, a chunk at a time.
 * The single-value accessors are in bankmem_acc_veranda.s. */
#include "bankmem.h"
#include <string.h>

#define RAM_BANK   (*(volatile uint8_t *)0x0000)
#define WINDOW     ((uint8_t *)0xA000)
#define BOUNCE     64

extern uint8_t bank_home;               /* overlay_veranda.c */
uint16_t bankmem_top;                   /* main_veranda.c: first + count */

uint16_t bankmem_bank_count(void)
{
    return bankmem_top;
}

/* bankmem_read, bankmem_write, bankmem_set: bankmem_acc_veranda.s */
