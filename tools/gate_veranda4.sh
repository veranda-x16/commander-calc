#!/bin/bash
# Gate veranda4 (Veranda Calc, phase C4 of the port): charts on the 1 bpp
# bitmap, CHART.BMP, cell borders.
#
#   tools/gate_veranda4.sh [SDK=<Veranda's installed SDK>]
#
# 1. make veranda-test4 (the session: tools/veranda_script.py c4), and its
#    stop points 1-3 (the bar, line and pie charts on screen)
# 2. the session, in x16emu headless: months and numbers typed, Chart >
#    Bars, Line, Pie of B1:B5 (S on the pie writes CHART.BMP), a box round
#    A2:B4 (Layout > Border), B1 bold. The host build plays the same session
#    (tools/veranda_golden2.c, PLAY_SESSION 4).
# 3. each chart is checked against the data (tools/check_veranda4.py: bar
#    heights and patterns, the line's points, each slice's pattern);
#    CHART.BMP is 640 x 464 1 bpp and its rows are the pie's screen rows;
#    the cells and the last screen -- the box's lines over the text --
#    equal the host build's (tools/check_veranda.py); on the host folder
#    and on an SD card image.
# Pictures: build/gate/veranda4/bars.png, line.png, pie.png, screen.png.
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
OUT=build/gate/veranda4
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
T4=build/veranda/test4

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/veranda.h" ] && [ -f "$SDK/bin/VERANDA.SYS" ] || fail "no Veranda SDK with its system in $SDK"
rm -rf "$OUT"
mkdir -p "$OUT/cvt" "$OUT/golden"

$PY tools/veranda_script.py c4 tools/veranda_session4.h >/dev/null
for stop in 1 2 3 ""; do
	make -s veranda-test4 SDK="$SDK" PLAY_STOP=$stop >>"$OUT/build.log" 2>&1 \
		|| { tail -20 "$OUT/build.log"; fail "make veranda-test4 PLAY_STOP=$stop"; }
	cp "$T4/Veranda Calc.cvt" "$OUT/cvt/stop${stop:-0}.cvt"
done
make -s build/host/veranda_golden4 >>"$OUT/build.log" 2>&1 || fail "the golden's host program"
ok "make veranda-test4 and its stop points 1-3: built against $SDK"

(cd "$OUT/golden" && ../../../host/veranda_golden4 >golden.txt 2>cells.txt) || fail "the golden"

run() {   # cvt, dir, [sd]
	d=$2
	mkdir -p "$d"
	if [ -n "$3" ]; then
		mkdir -p "$d/card"
		cp "$1" "$d/card/AUTOSTART.cvt"
		cp "$SDK/bin/VERANDA.SYS" "$d/card/"
		$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
		cp "$SDK/bin/VERANDA.PRG" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
		$PY "$SDK/tools/vcard.py" get "$d/sd.img" CHART.BMP "$d/CHART.BMP" >/dev/null 2>&1 || true
	else
		cp "$1" "$d/AUTOSTART.cvt"
		cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
	fi
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: the session did not finish"; }
	grep -a -q "^HUNG" "$d/console.txt" && fail "$d: the program hung (the player's watchdog)"
	return 0
}

# the stop points: each chart against the data
for k in 1:bars 2:line 3:pie; do
	n=${k%%:*}; kind=${k#*:}
	run "$OUT/cvt/stop$n.cvt" "$OUT/$kind"
	$PY tools/check_veranda4.py "$kind" "$OUT/$kind/dump.bin" || fail "the $kind chart"
	$PY "$SDK/tools/vram2png.py" --color "$OUT/$kind/dump.bin" "$OUT/$kind.png" 2>/dev/null || true
done

whole() {   # dir
	d=$1
	grep -a -q "CALC DONE" "$d/console.txt" || { tail -5 "$d/console.txt"; fail "$d: the script did not end"; }
	tr -d '\r' < "$d/console.txt" | grep -a -o -E "LOG$|^(CELL|CURSOR|SHEETS|DIRTY|WIDTH) .*" > "$d/cells.txt" || true
	diff "$OUT/golden/cells.txt" "$d/cells.txt" > "$d/cells.diff" || { head -20 "$d/cells.diff"; fail "$d: the cells differ from the host build's"; }
	ok "$d: the cells equal the host build's"
	[ -s "$d/CHART.BMP" ] || fail "$d: no CHART.BMP (S on the pie)"
	$PY tools/check_veranda4.py bmp "$d/CHART.BMP" "$OUT/pie/dump.bin" || fail "$d: CHART.BMP"
	$PY tools/check_veranda.py "$d/dump.bin" "$OUT/golden/golden.txt" "$ROM" || fail "$d: the screen (the box's lines, bold) differs from the host build's"
	cut -f4 "$OUT/golden/golden.txt" | grep -q "[1-9A-F]" || fail "the golden has no borders"
	ok "$d: the screen equals the host build's, the box round A2:B4 drawn over the text"
}
run "$OUT/cvt/stop0.cvt" "$OUT/host"
whole "$OUT/host"
$PY "$SDK/tools/vram2png.py" --color "$OUT/host/dump.bin" "$OUT/screen.png" 2>/dev/null || true
run "$OUT/cvt/stop0.cvt" "$OUT/sd" sd
whole "$OUT/sd"
echo "GATE veranda4: PASS"
