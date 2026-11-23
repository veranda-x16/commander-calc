#!/usr/bin/env python3
"""Veranda Calc's screen against the host build's (tools/gate_veranda.sh).

usage: check_veranda.py <dump.bin> <golden.txt> <rom.bin>

dump.bin: x16emu's dump (-dump V) after the session; golden.txt: the host
build's text grid after the same keys (tools/veranda_golden.c, built with
-DX16S_LOOK2: per line 80 characters, a tab, 80 attribute bytes in hex, a
tab, 80 digits: 1 bold; optionally a tab and 80 hex digits: the borders,
SCREEN_B_* -- L 1, R 2, T 4, B 8 -- drawn over the glyph on the card's
edges; 57 lines). The grid's row y is card row y + 3 (rows 0-1 are
Veranda's menu bar, 2-3 the title band), and its row 0 (the X16 build's
own menu bar) is not drawn. Each of the 80 x 56 cells is drawn as Veranda
Calc draws it (src/platform/screen_veranda.c): the glyph from the X16
ROM's ISO font (ROM bank 6, $C800; $20-$7F, others a space) on the
640x480 1 bpp bitmap at VRAM $00000, and the paper colour of its
attribute, as Veranda Look 2's palette entry (shadow_veranda.s's
scr_paper), in Veranda's colour map (VRAM $1AC00: 128 x 64 entries of 2
bytes, the colour in the attribute's high nibble).

Veranda Look 2 (Veranda's DECISIONS V74, V77, V79; check_look):
  - the palette: entries 0-15 Look 2's light ones (no settings file), 14
    Look 3's sage;
  - the menu bar: its cards the chrome (2), the Veranda mark's two cyan
    (11), none covered; the line under it, y 15;
  - the title band (card rows 2-3): the theme's accent (the default preset,
    Sunroom: the mist, 3), a line under it (y 31), a title in it; the grid's
    cursor in the theme's highlight (Sunroom's sky, 5);
  - the status bar's second card row (59): the chrome.
Exit 1 on a difference.
"""
import sys

COL_BLACK, COL_WHITE = 1, 0
MAP = 0x1AC00
TOPROW = 3                      # the grid's row y is card row y + 3
VRAM, COMPOSER = 131072, 256    # the dump: VRAM, then the composer, the palette
# Veranda Look 2 (the SDK's theme.h, theme.c), Look 3's sage in entry 14
LIGHT = [0xFFF, 0x223, 0xDDE, 0xEEF, 0x9CF, 0xACE, 0xFD5, 0xF87, 0x8D9, 0xB9F, 0xFA5, 0x6CE, 0x36A, 0x8BE,
         0x7A8, 0xDAC]                  # (Look 3, V132: entry 14 the sage)
PAPER, CHROME, SIDEBAR, SELECT, TITLE, CYAN = 0, 2, 3, 4, 5, 11
# the default theme (no settings file): Sunroom (Veranda's V132): mist title
# bands (SIDEBAR), sky selections (TITLE: the grid's cursor and marks,
# scr_paper: look_start makes the papers' SELECT the theme's highlight)
ACCENT, HILITE = SIDEBAR, TITLE
# the X16's colours as Look 2's (shadow_veranda.s's papers)
PAPERS = [HILITE if p == SELECT else p for p in [0, 1, 7, 11, 9, 8, 4, 6, 10, 10, 7, 2, 3, 8, 4, 2]]


def paper_of(c):
    """shadow_veranda.s's scr_paper(): the background as Look 2's entry;
    on black the ink's (white: the selection colour, the cursor's)"""
    bg, fg = c >> 4, c & 15
    if bg == COL_BLACK:
        return PAPERS[6] if fg == COL_WHITE else PAPERS[fg]
    return PAPERS[bg]


def card(v, x, y):
    """(attribute colour, tile) of card x, y in the colour map"""
    a = MAP + 2 * (y * 128 + x)
    return v[a + 1] >> 4, v[a]


def row_ink(v, y, x0=0, x1=640):
    return sum(bin(v[y * 80 + x]).count('1') for x in range(x0 >> 3, x1 >> 3))


def check_look(v, accent=ACCENT):
    """Veranda Look 2's palette, the menu bar's, the title band's and the
    status bar's colours (see the module's doc); a list of failures"""
    bad = []
    p = v[VRAM + COMPOSER:VRAM + COMPOSER + 32]
    got = [p[2 * i] | p[2 * i + 1] << 8 for i in range(16)]
    if got != LIGHT:
        bad.append('palette %s, want Look 2\'s light %s' % (' '.join('$%03X' % c for c in got),
                                                           ' '.join('$%03X' % c for c in LIGHT)))
    for y in range(2):
        for x in range(80):
            c, t = card(v, x, y)
            want = CYAN if x < 2 else CHROME
            if c != want or t:
                bad.append('menu bar card %d,%d: colour %d tile $%02X, want %d, uncovered' % (x, y, c, t, want))
                break
    for y in (2, 3):
        cs = set(card(v, x, y)[0] for x in range(80))
        if cs != {accent}:
            bad.append('title band card row %d: colours %s, want %d' % (y, sorted(cs), accent))
    cs = set(card(v, x, 59)[0] for x in range(80))
    if cs != {CHROME}:
        bad.append('status bar card row 59: colours %s, want the chrome %d' % (sorted(cs), CHROME))
    if row_ink(v, 15) != 640:
        bad.append('no line under the menu bar (y 15)')
    if row_ink(v, 31) != 640:
        bad.append('no line under the title band (y 31)')
    if sum(row_ink(v, y, 160, 480) for y in range(17, 30)) < 40:
        bad.append('no title in the title band')
    return bad


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    v = open(sys.argv[1], 'rb').read()
    lines = open(sys.argv[2], encoding='latin-1').read().split('\n')
    rom = open(sys.argv[3], 'rb').read()
    font = rom[6 * 16384 + 0x800:6 * 16384 + 0x1000]
    bad_glyph, bad_paper = [], []
    rows = len([l for l in lines if l])
    for y in range(1, rows):
        f = lines[y].split('\t')
        text, attrs = f[0], f[1]
        bold = f[2] if len(f) > 2 else '0' * 80
        bord = f[3] if len(f) > 3 else '0' * 80
        for x in range(80):
            ch = ord(text[x]) & 0x7F
            if ch < 0x20:
                ch = 0x20
            want = font[ch * 8:ch * 8 + 8]
            if bold[x] == '1':                  # PutCards' bold: v OR v >> 1
                want = bytes(b | (b >> 1) for b in want)
            b = int(bord[x], 16)                # borders over the text: L R T B
            if b:
                w = list(want)
                for r in range(8):
                    w[r] |= (0x80 if b & 1 else 0) | (0x01 if b & 2 else 0)
                if b & 4:
                    w[0] = 0xFF
                if b & 8:
                    w[7] = 0xFF
                want = bytes(w)
            got = bytes(v[((y + TOPROW) * 8 + r) * 80 + x] for r in range(8))
            if got != want:
                bad_glyph.append((x, y, text[x]))
            if y == rows - 1:
                continue                        # card row 59: the status bar's (check_look)
            a = int(attrs[2 * x:2 * x + 2], 16)
            p = v[MAP + ((y + TOPROW) * 128 + x) * 2 + 1] >> 4
            if p != paper_of(a):
                bad_paper.append((x, y, '%02X' % a, p))
    # the menu bar (Veranda's DoMenu): a solid line at its bottom, y 15,
    # under its titles
    bar = v[15 * 80:15 * 80 + 8] == b'\xff' * 8 and any(v[y * 80 + x] != 0 for y in range(2, 13) for x in range(8))
    print('%s  the menu bar: drawn on card rows 0-1 (a frame at y 15)' % ('ok   ' if bar else 'FAIL '))
    look = check_look(v)
    print('%s  Veranda Look 2: the palette, the menu bar\'s chrome and mark, the title band (%d) and its line, '
          'the status bar\'s chrome%s' % ('ok   ' if not look else 'FAIL ', ACCENT,
                                           '' if not look else ' (%s)' % '; '.join(look[:4])))
    ok = bar and not look and not bad_glyph and not bad_paper
    print('%s  the grid: %d cells, glyphs equal the host build\'s text drawn with the ROM\'s font%s'
          % ('ok   ' if not bad_glyph else 'FAIL ', 80 * (rows - 1), '' if not bad_glyph else ' (%d differ: %s)' % (len(bad_glyph), bad_glyph[:8])))
    print('%s  the grid: every cell\'s paper colour is its attribute\'s%s'
          % ('ok   ' if not bad_paper else 'FAIL ', '' if not bad_paper else ' (%d differ: %s)' % (len(bad_paper), bad_paper[:8])))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
