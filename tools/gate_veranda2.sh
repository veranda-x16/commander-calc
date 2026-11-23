#!/bin/bash
# Gate veranda2 (Veranda Calc, phase C2 of the port): the mouse, Veranda's
# menus and dialog boxes, played by a script and compared with the host
# build.
#
#   tools/gate_veranda2.sh [SDK=<Veranda's installed SDK>] [VERANDA=<its tree>]
#
# The SDK's system must have Veranda's V39 (GetString's text 0-terminated
# after every key): the session closes dialog boxes with OK, not Return.
#
# 1. make veranda and make veranda-test2 against the installed SDK; the map
#    as gate veranda checks it (the resident image below the C stack)
# 2. the session (tools/veranda_script.py writes both forms of it):
#    typing, clicks on cells, Layout > Bold, Edit > Copy and Paste,
#    Edit > Find (a dialog box, typed into with a backspace), Layout >
#    Column width (its field changed, OK clicked), Sheet > Rename and Add,
#    clicks on the tabs, Edit > Replace (two fields, then Yes and No for
#    each match), Layout > Delete row (Yes), the wheel. Played by a scripted
#    pointer and keyboard (Veranda's mouseSource, src/tools/veranda_play2.s)
#    in x16emu, headless; on the host by tools/veranda_golden2.c as the X16
#    build's code sees it.
# 3. the cells, the cursor, the sheets and column B's width the program
#    logs equal the host build's; the screen equals the host build's text
#    grid, glyph for glyph and colour for colour (tools/check_veranda.py);
#    on the host folder and on an SD card image (Veranda's tools/vcard.py)
# 4. two stop points (PLAY_STOP): the Layout menu open; the column width
#    dialog box with "14" typed -- their pictures, and that each shows
#    (tools/check_veranda2.py)
# Pictures: build/gate/veranda2/screen.png, menu.png, dialog.png.
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
VERANDA=${VERANDA:-$HOME/Projects/geos}
OUT=build/gate/veranda2
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
T2=build/veranda/test2

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/veranda.h" ] && [ -f "$SDK/bin/VERANDA.SYS" ] || fail "no Veranda SDK with its system in $SDK"
grep -q "KEY_PGUP" "$SDK/include/veranda.h" || fail "the SDK in $SDK is older than API level 2"
rm -rf "$OUT"
mkdir -p "$OUT/cvt"

# the session's two forms, then the builds: the two stop points, the whole
$PY tools/veranda_script.py c2 tools/veranda_session2.h >/dev/null
make -s veranda SDK="$SDK" >"$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; fail "make veranda"; }
for stop in 1 2 ""; do
	make -s veranda-test2 SDK="$SDK" PLAY_STOP=$stop >>"$OUT/build.log" 2>&1 \
		|| { tail -20 "$OUT/build.log"; fail "make veranda-test2 PLAY_STOP=$stop"; }
	cp "$T2/Veranda Calc.cvt" "$OUT/cvt/stop${stop:-0}.cvt"
done
ok "make veranda, make veranda-test2 (and its stop points 1, 2): built against $SDK"
make -s build/host/veranda_golden2 >>"$OUT/build.log" 2>&1 || fail "the golden's host program"

M=build/veranda/veranda.map
end=0
for f in "$M" "$T2/veranda.map"; do
	e=0
	for s in CODE RODATA DATA BSS GOLDENBSS; do
		v=$(sed -n "s/^$s *[0-9A-F]* *\([0-9A-F]*\) .*/\1/p" "$f" | head -1)
		if [ -n "$v" ] && [ $((16#$v)) -gt $e ]; then e=$((16#$v)); fi
	done
	[ $e -lt $((16#82B0)) ] || fail "$f: the resident image ends at \$$(printf %04X $e): into screen_font's (cfg/veranda.cfg)"
	[ "$f" = "$M" ] && end=$e
done
ok "the map: resident \$0801-\$$(printf %04X $end) ($((0x82B0 - end - 1)) bytes free below the font and the C stack); the test build's fits too"
[ "$(stat -c %s build/veranda/CALC.BIN.14)" -le 8191 ] || fail "the menu overlay is more than a bank"

build/host/veranda_golden2 >"$OUT/golden.txt" 2>"$OUT/cells.txt" || fail "the golden"
grep -q "^CURSOR " "$OUT/cells.txt" || fail "the host build's session: $(cat "$OUT/cells.txt")"

run() {   # cvt, dir, [sd]
	d=$2
	rm -rf "$d"
	mkdir -p "$d"
	if [ -n "$3" ]; then
		mkdir -p "$d/card"
		cp "$1" "$d/card/AUTOSTART.cvt"
		cp "$SDK/bin/VERANDA.SYS" "$d/card/"
		$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
		cp "$SDK/bin/VERANDA.PRG" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 180 "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
	else
		cp "$1" "$d/AUTOSTART.cvt"
		cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 180 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
	fi
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: the session did not finish"; }
}

check_run() {   # dir, picture
	d=$1
	grep -a -q "ZP OK" "$d/console.txt" || fail "$d: the ROM maths library changed the program's zero page"
	grep -a -q "CALC DONE" "$d/console.txt" || { tail -5 "$d/console.txt"; fail "$d: the script did not end"; }
	tr -d '\r' < "$d/console.txt" | grep -a -E "^(LOG|CELL |CURSOR |SHEETS|DIRTY |WIDTH )" > "$d/cells.txt" || true
	diff "$OUT/cells.txt" "$d/cells.txt" > "$d/cells.diff" || { cat "$d/cells.diff"; fail "$d: the cells differ from the host build's"; }
	ok "$d: $(grep -c CELL "$d/cells.txt") cells, the cursor ($(sed -n 's/^CURSOR //p' "$d/cells.txt")), the sheets ($(sed -n 's/^SHEETS //p' "$d/cells.txt")) and column B's width ($(sed -n 's/^WIDTH B //p' "$d/cells.txt")) equal the host build's"
	if [ -f "$VERANDA/tools/check_vera.py" ]; then
		$PY "$VERANDA/tools/check_vera.py" "$d/dump.bin" 640x480 || fail "$d: the display does not show the bitmap"
	fi
	$PY tools/check_veranda.py "$d/dump.bin" "$OUT/golden.txt" "$ROM" || fail "$d: the screen differs from the host build's"
	$PY "$SDK/tools/vram2png.py" --color "$d/dump.bin" "$2" 2>/dev/null || true
}

run "$OUT/cvt/stop0.cvt" "$OUT/host"
check_run "$OUT/host" "$OUT/screen.png"
run "$OUT/cvt/stop0.cvt" "$OUT/sd" sd
check_run "$OUT/sd" "$OUT/screen-sd.png"

# the stop points
run "$OUT/cvt/stop1.cvt" "$OUT/menu"
$PY tools/check_veranda2.py menu "$OUT/menu/dump.bin" || fail "the Layout menu is not open at stop point 1"
$PY "$SDK/tools/vram2png.py" --color "$OUT/menu/dump.bin" "$OUT/menu.png" 2>/dev/null || true
run "$OUT/cvt/stop2.cvt" "$OUT/dialog"
$PY tools/check_veranda2.py dialog "$OUT/dialog/dump.bin" || fail "the column width dialog box is not open at stop point 2"
$PY "$SDK/tools/vram2png.py" --color "$OUT/dialog/dump.bin" "$OUT/dialog.png" 2>/dev/null || true
echo "GATE veranda2: PASS"
