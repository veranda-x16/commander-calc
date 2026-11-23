#!/usr/bin/env python3
"""Veranda Calc's charts (tools/gate_veranda4.sh): the pictures at the C4
session's stop points, worked out again from the data, and CHART.BMP.

usage: check_veranda4.py bars|line|pie <dump.bin>
       check_veranda4.py bmp <CHART.BMP> <dump.bin of the pie>

The session (tools/veranda_script.py c4) charts B1:B5 = 12 30 -5 22 17,
labels Jan..May. The layout is src/ui/chart.c's on Veranda: 80 x 29 cells
of 8 units, a unit one pixel across and two down from y 16 (VY); AXIS_W 9,
PLOT_TOP 3, the plot down to row 26; the bars in pattern 2 (50 % grey)
framed, the line's points 16 x 16 pixel squares of ink, the pie at (200,
256) radius 180 pixels, its slices in patterns 2 5 1 14 19 (slice order),
negative values left out. Pixel positions allow 3 pixels for rounding.
"""
import math
import struct
import sys

VALS = [12, 30, -5, 22, 17]
PATS = {0: [0] * 8, 1: [0xFF] * 8, 2: [0xAA, 0x55] * 4, 5: [0x88, 0, 0x22, 0] * 2,
        14: [0x11, 0x22, 0x44, 0x88] * 2, 19: [0xCC, 0xCC, 0x33, 0x33] * 2}
SLICE = [2, 5, 1, 14, 19, 22]


def px(v, x, y):
    return (v[y * 80 + (x >> 3)] >> (7 - (x & 7))) & 1


def pat_px(p, x, y):
    return (PATS[p][y & 7] >> (7 - (x & 7))) & 1


def vy(u):
    return 16 + 2 * u


def scale():
    lo, hi = min(0, min(VALS)), max(0, max(VALS))
    top, h = 3 * 8, (26 - 3) * 8
    return lambda v: top + h - int((v - lo) * h / (hi - lo))


def bars(v):
    row = scale()
    yz = row(0)
    w = (80 - 9) // len(VALS)
    bad = []
    for i, val in enumerate(VALS):
        x0 = (9 + w * i) * 8
        cx = x0 + (w - 1) * 4
        y = row(val)
        a, b = (vy(y), vy(yz)) if y <= yz else (vy(yz + 1), vy(y))
        inside = all(px(v, x, yy) == pat_px(2, x, yy)
                     for yy in range(a + 4, b - 4) for x in range(cx - 8, cx + 8))
        above = all(px(v, cx, yy) == 0 for yy in range(a - 12, a - 4)) if y <= yz else True
        if not (inside and above):
            bad.append(i)
    return bad, '%d bars, each of its height in pattern 2 (50 %% grey), nothing above it' % len(VALS)


def line(v):
    row = scale()
    w = (80 - 9) // len(VALS)
    bad = []
    for i, val in enumerate(VALS):
        cx = (9 + ((w - 1) >> 1) + w * i) * 8
        cy = vy(row(val))
        if not all(px(v, x, y) for x in range(cx - 2, cx + 3) for y in range(cy - 2, cy + 3)):
            bad.append(i)
    # the line between the first two points, half way
    x1 = (9 + ((w - 1) >> 1)) * 8 + 3
    x2 = x1 + w * 8
    xm, ym = (x1 + x2) // 2, (vy(row(VALS[0])) + vy(row(VALS[1]))) // 2
    if not any(px(v, x, y) for x in range(xm - 3, xm + 4) for y in range(ym - 3, ym + 4)):
        bad.append('segment')
    return bad, '%d points (squares of ink) and the line joining them' % len(VALS)


def pie(v):
    pos = [max(0, x) for x in VALS]
    tot = sum(pos)
    acc, bad = 0, []
    cx, cy = 200, vy(120)
    for i, val in enumerate(pos):
        a0, acc = acc, acc + val
        if val == 0:
            continue
        mid = math.radians((a0 + acc) / 2 * 360 / tot)
        x = int(cx + 100 * math.cos(mid))
        y = int(cy - 100 * math.sin(mid))
        p = SLICE[i % 6]
        if not all(px(v, xx, yy) == pat_px(p, xx, yy)
                   for xx in range(x - 3, x + 4) for yy in range(y - 3, y + 4)):
            bad.append(i)
    rim = px(v, cx - 180, cy) or px(v, cx - 179, cy)
    if not rim:
        bad.append('rim')
    return bad, '%d slices, each in its pattern half way out along its middle; the rim' % sum(1 for x in pos if x)


def bmp(path, dump):
    b = open(path, 'rb').read()
    v = open(dump, 'rb').read()
    if b[:2] != b'BM':
        return ['not a BMP'], 'CHART.BMP'
    off = struct.unpack_from('<I', b, 10)[0]
    w, h = struct.unpack_from('<ii', b, 18)
    bpp = struct.unpack_from('<H', b, 28)[0]
    if (w, abs(h), bpp) != (640, 464, 1):
        return ['%dx%d %d bpp' % (w, h, bpp)], 'CHART.BMP'
    ink1 = b[54 + 4:54 + 7] != b[54:54 + 3]     # palette: entry 1 the ink
    bad = 0
    for r in range(464):
        src = b[off + (463 - r) * 80:off + (463 - r) * 80 + 80] if h > 0 else b[off + r * 80:off + r * 80 + 80]
        want = v[(16 + r) * 80:(16 + r) * 80 + 80]
        if src != want:
            bad += 1
    return ([] if bad == 0 and ink1 else ['%d rows differ' % bad]), \
        'CHART.BMP: 640 x 464, 1 bpp, its rows equal the screen\'s rows 16-479'


def main():
    kind = sys.argv[1]
    if kind == 'bmp':
        bad, what = bmp(sys.argv[2], sys.argv[3])
    else:
        v = open(sys.argv[2], 'rb').read()
        bad, what = {'bars': bars, 'line': line, 'pie': pie}[kind](v)
    print('%s  %s: %s%s' % ('ok   ' if not bad else 'FAIL ', kind, what, '' if not bad else ' (wrong: %s)' % bad))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
