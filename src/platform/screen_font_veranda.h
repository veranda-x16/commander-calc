/* screen_font_veranda.h -- the grid's glyphs from the ROM (Veranda Calc).
 *
 * The X16 ROM's ISO charset (ROM bank 6, $C800), $20-$7F, into
 * screen_font: #included by the two overlays that do it, each a static
 * copy in its own bank -- the start-up's (platform/screen_veranda.c's
 * screen_init) and the file dialog's (ui/filedlg.c: after the Open and
 * Save dialog, whose BSS is the font's meanwhile, cfg/veranda.cfg). Bank 6
 * is data only, so no interrupt while it is mapped. */
#ifndef SCREEN_FONT_VERANDA_H
#define SCREEN_FONT_VERANDA_H
#include <string.h>

extern uint8_t screen_font[96 * 8];

static void screen_font_load(void)
{
    uint8_t b;

    __asm__("sei");
    b = *(volatile uint8_t *)0x0001;
    *(volatile uint8_t *)0x0001 = 6;
    memcpy(screen_font, (void *)(0xC800 + 0x100), 96 * 8);
    *(volatile uint8_t *)0x0001 = b;
    __asm__("cli");
}

#endif
