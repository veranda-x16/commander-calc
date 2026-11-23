#!/usr/bin/env python3
"""The stop points of tools/gate_veranda2.sh: is the menu or the dialog
box open in the dump?

usage: check_veranda2.py menu|dialog <dump.bin>

dump.bin: x16emu's VRAM dump; the 640x480 1 bpp bitmap at $00000, 80
bytes a row, the leftmost pixel in bit 7.
  menu    the Layout menu (src/ui/vmenu_veranda.c) in the kernel's
          Veranda look (theme_menus; Veranda's DECISIONS V74): nine items
          of 16 lines under the bar from y 16, so a frame line down its
          left edge, y 17-158, and along its bottom, y 160; its title's
          cards on the bar in the selection colour (the default theme's,
          Sunroom's sky: 5; Veranda's V132; the colour map at
          VRAM $1AC00), the mark's cyan, the rest the chrome
  dialog  the dialog box (src/ui/vdlg_veranda.h): a frame at x 160-479,
          y 192-287
Exit 1 if it is not there.
"""
import sys

HILITE = 5                      # Sunroom's highlight (the default theme)


def px(v, x, y):
    return (v[y * 80 + (x >> 3)] >> (7 - (x & 7))) & 1


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ('menu', 'dialog'):
        sys.exit(__doc__)
    v = open(sys.argv[2], 'rb').read()
    if sys.argv[1] == 'menu':
        left = [x for x in range(16, 400) if all(px(v, x, y) for y in range(17, 159))]
        # (the list's last line: 16 lines an item from y 16, the frame on the last item's
        # last line, y 159, since Veranda's Sunroom kernel; 160 before)
        bottom = left and any(sum(px(v, x, y) for x in range(left[0], left[0] + 60)) == 60 for y in (159, 160))
        cols = [v[0x1AC00 + 2 * x + 1] >> 4 for x in range(80)]
        hi = [x for x in range(80) if cols[x] == HILITE]
        bar = (bool(hi) and hi == list(range(hi[0], hi[-1] + 1)) and left and hi[0] * 8 == left[0]
               and cols[:2] == [11, 11] and all(c == 2 for x, c in enumerate(cols) if x >= 2 and x not in hi))
        ok = bool(left) and bottom and bar
        print('%s  the Layout menu is open: its frame down x %s, along y 159; its title\'s cards %s in the '
              'selection colour' % ('ok   ' if ok else 'FAIL ', left[0] if left else '?',
                                    '%d-%d' % (hi[0], hi[-1]) if hi else '?'))
    else:
        top = all(px(v, x, 192) for x in range(160, 480))
        bot = all(px(v, x, 287) for x in range(160, 480))
        sides = all(px(v, 160, y) and px(v, 479, y) for y in range(192, 288))
        ok = top and bot and sides
        print('%s  the dialog box is open: its frame at (160, 192)-(479, 287)' % ('ok   ' if ok else 'FAIL '))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
