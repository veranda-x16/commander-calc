/* keyboard_veranda.c -- keyboard.h for the modal loops on Veranda.
 *
 * The grid's keys come through Veranda's keyVector (main_veranda.c), the
 * questions' through Veranda's dialog boxes. What modal loops are left in
 * the overlays (the file chooser, until phase C3) read the ROM's queue
 * themselves, as keyboard_x16.c does: Veranda's own
 * loop, which would otherwise take the keys, is not running then. Veranda
 * keeps the ROM in ISO mode, so the codes are the X16 build's. */
#include "keyboard.h"

static uint8_t k, held;
uint8_t calc_idle;                      /* 1: waiting for a key (main_veranda.c) */

static uint8_t getin(void)
{
    __asm__("jsr $FFE4");               /* GETIN */
    __asm__("sta %v", k);
    return k;
}

/* PAGE DOWN, or Ctrl+B? (keyboard_x16.c) */
static uint8_t disambiguate(uint8_t c)
{
    if (c == 0x02) {
        __asm__("jsr $FEC0");           /* kbdbuf_get_modifiers */
        __asm__("sta %v", held);
        if (held & 0x04)
            return 0x9C;
    }
    return c;
}

uint8_t kbd_get(void)
{
    if (!getin()) {
        calc_idle = 1;                  /* nothing to do: a test's script may go on */
        return 0;
    }
    return disambiguate(k);
}

uint8_t kbd_wait(void)
{
    uint8_t c;

    do {
        c = getin();
    } while (c == 0);
    return disambiguate(c);
}
