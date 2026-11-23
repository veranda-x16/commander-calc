/* mouse_veranda.c -- mouse.h on Veranda: its own pointer and buttons.
 *
 * Veranda moves the pointer (a sprite) in its interrupt; this reads where
 * it is. mouseXPos and mouseYPosW are two variables the interrupt writes,
 * so they are read until two readings agree (Veranda's DECISIONS V20). The
 * wheel is GetMouse's x. In pixels, as the X16 build's mouse_get, and in
 * the grid's: its row y is card row y + 3 (screen_veranda.c), so y is
 * 24 less (0 on Veranda's menu bar and the title band). */
#include "mouse.h"
#include <veranda.h>

#ifdef __CC65__
#  pragma bss-name (push, "ZPBSS2", "zp")
#endif
uint16_t mouse_px, mouse_py;
#ifdef __CC65__
#  pragma bss-name (pop)
#endif
uint8_t mouse_btn;
int8_t  mouse_whl;
uint8_t chart_key;                      /* chart_wait's */

void mouse_begin(void) { }
void mouse_end(void) { }

/* In OVL_CLIP's bank (overlay 21; the resident image's bytes went to
 * Veranda Look 2's dialog): only the chart's wait reads the pointer so
 * (chart_wait, below, through ovl_far21). */
#pragma code-name (push, "OVERLAY21")

void mouse_poll(void)
{
    uint16_t x, y;

    do {
        x = mouseXPos;
        y = mouseYPosW;
    } while (x != mouseXPos || y != mouseYPosW);
    mouse_px = x;
    mouse_py = y < 24 ? 0 : y - 24;
    mouse_btn = GetMouse() & (MOUSE_LEFT | MOUSE_RIGHT);
    __asm__("stx %v", mouse_whl);
}

/* the chart's wait (ui/chart.c): a key, or a click -- the press that
 * chose the menu item may still be held, so a click is a press after a
 * release; the key (0: a click) in chart_key */
uint8_t kbd_get(void);
void chart_wait(void)
{
    uint8_t was = 1, k;

    for (;;) {
        k = kbd_get();
        if (k)
            break;
        mouse_poll();
        if ((mouse_btn & MOUSE_LEFT) && !was)
            break;
        was = (uint8_t)(mouse_btn & MOUSE_LEFT);
    }
    chart_key = k;
}

#pragma code-name (pop)
