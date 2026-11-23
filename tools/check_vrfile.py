#!/usr/bin/env python3
"""The SDK's Open and Save dialog (vrchoose.c; Veranda's DECISIONS V79) in
a VRAM dump of Veranda Calc (tools/gate_veranda3.sh, gate_veranda5.sh).

The box is 38 cards by 27 card rows at 640 x 480 (7 list rows), centred
until moved: cards 21-58, card rows 16-42. Its title band (rows 0-1) on
the theme's accent (the default preset, Sunroom: the mist, 3); the list's
row r on card rows 25 + 2 r, 26 + 2 r (cards 23-55; the kind badge cards
24-25, the name from x 212); the selection on the selection colour (5);
Cancel cards 42-48 and the default button 50-56 on rows 40-41, the
default one on the selection colour. Colours are the colour map's (VRAM
$1AC00).

    check_vrfile.py <dump.bin> [sel=<r>] [rows=<n>]

checks the box is open (its title band, its frame, the default button),
and, given, that row r is the selected one (and no other), that rows
0 .. n-1 show a name and the others none (rows=7: all seven). Exit 1 if
not.
"""
import sys

MAP = 0x1AC00
BW, NR = 38, 7
BX, BY = (80 - BW) // 2, (60 - (13 + 2 * NR)) // 2
# the default theme (no settings file): Sunroom (Veranda's V132): its accent
# the mist (SIDEBAR, 3), its highlight the sky (TITLE, 5); Look 2's Veranda preset
# was 5 and 4
TITLE, SELECT = 3, 5


def colour(v, x, y):
    return v[MAP + 2 * (y * 128 + x) + 1] >> 4


def px(v, x, y):
    return (v[y * 80 + (x >> 3)] >> (7 - (x & 7))) & 1


def is_open(v):
    band = all(colour(v, BX + x, BY + y) == TITLE for x in range(BW) for y in range(2))
    top = all(px(v, x, BY * 8 + 1) for x in range(BX * 8 + 1, (BX + BW) * 8 - 1))
    dflt = all(colour(v, BX + x, BY + 24) == SELECT for x in range(29, 36))
    return band and top and dflt


def row_y(r):
    return BY + 9 + 2 * r


def selected(v, r):
    return colour(v, BX + 10, row_y(r)) == SELECT


def ink(v, r):
    y0 = row_y(r) * 8
    return sum(px(v, x, y) for y in range(y0, y0 + 16) for x in range((BX + 4) * 8 + 4, (BX + 34) * 8))


def check(v, sel=None, rows=None):
    """[(what, ok)]"""
    out = [('the Open and Save dialog is open (cards %d-%d, rows %d-%d): its title band, its frame, '
            'the default button' % (BX, BX + BW - 1, BY, BY + 26), is_open(v))]
    if sel is not None:
        s = [r for r in range(NR) if selected(v, r)]
        out.append(('row %d selected (selected: %s)' % (sel, s), s == [sel]))
    if rows is not None:
        shown = [r for r in range(NR) if ink(v, r)]
        out.append(('rows 0-%d show names (shown: %s)' % (rows - 1, shown), shown == list(range(rows))))
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    v = open(sys.argv[1], 'rb').read()
    kw = dict(a.split('=') for a in sys.argv[2:] if '=' in a)
    res = check(v, int(kw['sel']) if 'sel' in kw else None, int(kw['rows']) if 'rows' in kw else None)
    for t, good in res:
        print(('ok   ' if good else 'FAIL ') + '  ' + t)
    sys.exit(0 if all(g for _, g in res) else 1)


if __name__ == '__main__':
    main()
