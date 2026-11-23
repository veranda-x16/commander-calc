/* keymap_veranda.c -- Veranda Calc's keys as the grid's (make veranda).
 *
 * keyVector's routine (main_veranda.c, on_key) has each key translated
 * here, in OVL_CHARTOUT's bank, through ovl_far: keyData and keyDataMods
 * in ck_k and ck_m, the grid's code (the X16 ROM's ISO codes, which grid.c
 * has always had) back in ck_k, 0: none. Not resident: those bytes went
 * to Veranda Look 2's Open and Save dialog. */
#include "../x16sheet.h"
#include <veranda.h>

extern uint8_t ck_k, ck_m;              /* main_veranda.c */


#pragma code-name (push, "OVERLAY16")
#pragma rodata-name (push, "OVL16RO")

/* GEOS key codes (keyData) and grid.c's (the X16 ROM's ISO codes), in
 * pairs: what is not here and not printable is no key to the grid. */
static const uint8_t keymap[] = {
    CR, 0x0D,           KEY_DELETE, 0x19,   KEY_RIGHT, 0x1D,
    KEY_UP, 0x91,       KEY_DOWN, 0x11,     KEY_HOME, 0x13,
    KEY_CLEAR, 0x93,    KEY_TAB, 0x09,      0x18, 0x18,     /* shift+tab */
    KEY_ESC, 0x1B,      KEY_STOP, 0x03,     KEY_INSERT, 0x94,
    KEY_F1, 0x85,       KEY_F2, 0x89,       KEY_F3, 0x86,
    KEY_F4, 0x8A,       KEY_F5, 0x87,       KEY_F6, 0x8B,
    KEY_F7, 0x88,       KEY_F8, 0x8C,       KEY_F9, 0x10,
    KEY_F10, 0x15,      KEY_F11, 0x16,      KEY_F12, 0x17,
    KEY_PGUP, 0x82,     KEY_PGDN, 0x02,     KEY_END, 0x04,
    0
};

/* A GEOS key code (keyData, keyDataMods) as grid.c's code; 0: none. */
static uint8_t calc_key(uint8_t k, uint8_t m)
{
    const uint8_t *p;

    if ((m & 2) && k >= 1 && k <= 26)   /* Ctrl + a letter */
        return k == 2 ? 0x9C : k;       /* Ctrl+B: bold (keyboard_x16.c) */
    if (k >= 0x20 && k < 0x7F)
        return k;
    if (k == BACKSPACE)
        return (m & KEYM_ARROW) ? 0x9D : 0x14;  /* KEY_LEFT is BACKSPACE */
    for (p = keymap; *p; p += 2)
        if (*p == k)
            return p[1];
    return 0;
}

void calc_key_far(void)
{
    ck_k = calc_key(ck_k, ck_m);
}

#pragma rodata-name (pop)
#pragma code-name (pop)
