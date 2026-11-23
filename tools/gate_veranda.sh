#!/bin/bash
# Gate veranda (Veranda Calc, phase C1 of the port): the spreadsheet on
# Veranda, played by a script and compared with the host build.
#
#   tools/gate_veranda.sh [SDK=<Veranda's installed SDK>] [VERANDA=<its tree>]
#
# 1. make veranda and make veranda-test against the installed SDK (its
#    API level 2: PutCards, FSize, banked overlays, the keys)
# 2. the map: the resident image ends below screen_font ($82B0: the font
#    and scr_line, shared with the BSS of the SDK's Open and Save dialog
#    and window chrome, cfg/veranda.cfg) and the C stack ($8600), cc65's
#    zero page and ZPBSS2 in $D4-$FF, ZPBSS in $50-$7F ($A9-$D3 stays the
#    ROM maths library's), each overlay at most 8191 bytes (a bank); no
#    overlay calls another (tools/check_overlays.py)
# 3. the session (tools/veranda_keys.h: 12, 30, =SUM(A1:A2)*2, up to A1,
#    5, PgDn, PgUp ...) in x16emu, headless, through Veranda's key queue:
#    the console shows "ZP OK" (the ROM maths library left the program's
#    zero page alone), the recalculated "CALC A3 70" and the redraw time;
#    VERA shows the bitmap (Veranda's tools/check_vera.py); the grid equals
#    the host build's text grid after the same keys, glyph for glyph and
#    colour for colour (tools/veranda_golden.c, tools/check_veranda.py)
# 4. the same from an SD card image (Veranda's tools/vcard.py)
# Pictures: build/gate/veranda/screen.png (and screen-sd.png).
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
VERANDA=${VERANDA:-$HOME/Projects/geos}
OUT=build/gate/veranda
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/veranda.h" ] && [ -f "$SDK/bin/VERANDA.SYS" ] || fail "no Veranda SDK with its system in $SDK"
grep -q "KEY_PGUP" "$SDK/include/veranda.h" || fail "the SDK in $SDK is older than API level 2"
rm -rf build/gate/veranda
mkdir -p "$OUT"

make -s veranda veranda-test SDK="$SDK" >"$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; fail "make veranda"; }
ok "make veranda, make veranda-test: built against $SDK"
make -s build/host/veranda_golden >>"$OUT/build.log" 2>&1 || fail "the golden's host program"

# the map
M=build/veranda/veranda.map
seg() { sed -n "s/^$1 *\([0-9A-F]*\) *\([0-9A-F]*\) .*/\1 \2/p" "$M" | head -1; }
end=0
for s in CODE RODATA DATA BSS GOLDENBSS; do
	set -- $(seg $s)
	if [ -n "$2" ] && [ $((16#$2)) -gt $end ]; then end=$((16#$2)); fi
done
[ $end -lt $((16#82B0)) ] || fail "the resident image ends at \$$(printf %04X $end): into screen_font's (cfg/veranda.cfg)"
set -- $(seg ZPBSS); zp1="$1-$2"
[ $((16#$1)) -ge $((16#50)) ] && [ $((16#$2)) -le $((16#7F)) ] || fail "ZPBSS at $zp1"
set -- $(seg ZEROPAGE); zp2="$1"
[ $((16#$1)) -ge $((16#D4)) ] || fail "cc65's zero page at $1"
set -- $(seg ZPBSS2)
[ -z "$2" ] || [ $((16#$2)) -le $((16#FF)) ] || fail "ZPBSS2 ends at $2"
for f in build/veranda/CALC.BIN.*; do
	[ "$(stat -c %s "$f")" -le 8191 ] || fail "$f: more than a bank"
done
ok "the map: resident \$0801-\$$(printf %04X $end) ($((end - 0x801 + 1)) bytes, $((0x82B0 - end - 1)) free below the font and the C stack); ZPBSS \$$zp1, cc65's zero page from \$$zp2; $(ls build/veranda/CALC.BIN.* | wc -l) overlays, each in a bank"

$PY tools/check_overlays.py build/veranda >/dev/null || fail "an overlay calls into another (tools/check_overlays.py)"
ok "no overlay calls into another (each runs in its own bank: one mapped at a time)"

# the golden
build/host/veranda_golden >"$OUT/golden.txt" 2>"$OUT/golden.log" || fail "the golden"
grep -q "^A3 70$" "$OUT/golden.log" || fail "the host build: A3 is not 70 ($(cat "$OUT/golden.log"))"

check_run() {   # dir, picture
	d=$1
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: the session did not finish"; }
	grep -a -q "ZP OK" "$d/console.txt" || fail "$d: the ROM maths library changed the program's zero page"
	grep -a -q "CALC A3 70" "$d/console.txt" || { grep -a "CALC" "$d/console.txt"; fail "$d: A3 is not 70"; }
	ok "$d: ZP OK; the session's keys reached the grid; the formula recalculated: A3 = 70"
	if [ -f "$VERANDA/tools/check_vera.py" ]; then
		$PY "$VERANDA/tools/check_vera.py" "$d/dump.bin" 640x480 || fail "$d: the display does not show the bitmap"
	else
		echo "SKIP  no Veranda tree ($VERANDA): check_vera.py"
	fi
	$PY tools/check_veranda.py "$d/dump.bin" "$OUT/golden.txt" "$ROM" || fail "$d: the screen differs from the host build's"
	$PY "$SDK/tools/vram2png.py" --color "$d/dump.bin" "$2" 2>/dev/null || true
}

# on the host folder
H=$OUT/host
mkdir -p "$H"
cp "build/veranda/test/Veranda Calc.cvt" "$H/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$H/"
(cd "$H" && SDL_VIDEODRIVER=dummy timeout -s INT 120 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump V > console.txt 2>&1) || true
check_run "$H" "$OUT/screen.png"
t=$(grep -a -o "TIME render [0-9A-F]*" "$H/console.txt" | awk '{print $3}')
t0=$(grep -a -o "TIME render0 [0-9A-F]*" "$H/console.txt" | awk '{print $3}')
if [ -n "$t" ]; then
	echo "      a full redraw (grid_render_all, 80 x 60): $((16#$t / 8000)) ms; the first (empty sheet): $((16#$t0 / 8000)) ms"
fi

# on an SD card image
S=$OUT/sd
mkdir -p "$S/card"
cp "build/veranda/test/Veranda Calc.cvt" "$S/card/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.SYS" "$S/card/"
$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
cp "$SDK/bin/VERANDA.PRG" "$S/"
(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 120 "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump V > console.txt 2>&1) || true
check_run "$S" "$OUT/screen-sd.png"
echo "GATE veranda: PASS"
