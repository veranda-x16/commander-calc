#!/bin/bash
# Gate veranda6 (Veranda Calc, phase C6 of the port): the redraw's speed.
#
#   tools/gate_veranda6.sh [SDK=<Veranda's installed SDK>]
#
# 1. make veranda-test6 (tools/veranda_script.py c6), and its stop point 1;
#    BIG6.CSV: 120 rows of eight columns (numbers, words)
# 2. the session, in x16emu headless at 8 MHz, each command timed by the
#    program itself (main_veranda.c's timed: the emulator's cycle counter
#    around grid_key, "KTIME"): a full redraw of the empty sheet (Data >
#    Recalc now, no formulas: grid_render_all); Import BIG6.CSV; a full
#    redraw on the full screen, the cursor down, right, PgDn (to the
#    bottom row; its cell then boxed and bold, not timed), down (the grid
#    scrolls a row), up, the full redraw again
# 3. stop point 1, the screen the row scroll left (the rows moved, only
#    the row that came into view drawn), equals the host build's at the
#    same point; the cells and the last screen equal the host build's
#    (tools/check_veranda.py), from the folder and from an SD card image
# 4. the timings within their budgets, set at what phase C7 reached with a
#    margin, so a regression fails: the empty sheet's redraw (EMPTY_MS), a
#    full redraw of 120 x 8 cells, the slower of the two (FULL_MS: the
#    300 ms target), PgDn (PGDN_MS), a row scroll (SCROLL_MS), a cursor
#    step, the slowest of three (CURSOR_MS). Before C6 the same were 676,
#    2345, about 2300, about 2300 and 139 ms; C6 reached 320, 1147, 1305,
#    1305 and 39 ms.
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
OUT=build/gate/veranda6
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
EMPTY_MS=${EMPTY_MS:-115}
FULL_MS=${FULL_MS:-300}
PGDN_MS=${PGDN_MS:-480}
SCROLL_MS=${SCROLL_MS:-170}
CURSOR_MS=${CURSOR_MS:-30}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
T6=build/veranda/test6

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/veranda.h" ] && [ -f "$SDK/bin/VERANDA.SYS" ] || fail "no Veranda SDK with its system in $SDK"
rm -rf "$OUT"
mkdir -p "$OUT/golden" "$OUT/host" "$OUT/scroll" "$OUT/sd/card" "$OUT/cvt"
$PY tools/veranda_script.py c6 tools/veranda_session6.h >/dev/null
for stop in 1 ""; do
	make -s veranda-test6 SDK="$SDK" PLAY_STOP=$stop >>"$OUT/build.log" 2>&1 \
		|| { tail -20 "$OUT/build.log"; fail "make veranda-test6 PLAY_STOP=$stop"; }
	cp "$T6/Veranda Calc.cvt" "$OUT/cvt/stop${stop:-0}.cvt"
done
make -s build/host/veranda_golden6 >>"$OUT/build.log" 2>&1 || fail "the golden's host program"
ok "make veranda-test6 (and its stop point 1): built against $SDK"

python3 -c "
w = ['alpha', 'beta', 'gamma', 'delta', 'epsilon', 'zeta', 'eta', 'theta']
for r in range(1, 121):
    print(','.join(str(r * 10 + c) if c % 2 == 0 else '%s %d' % (w[(r + c) % 8], r) for c in range(8)), end='\r\n')
" > "$OUT/golden/BIG6.CSV"
(cd "$OUT/golden" && ../../../host/veranda_golden6 >golden.txt 2>cells.txt) || fail "the golden"

# run <cvt> <dir> [sd]: the session in x16emu, headless, from the folder or
# from an SD card image made of it
run() {
	d=$2
	if [ "$3" = sd ]; then
		cp "$1" "$d/card/AUTOSTART.cvt"
		cp "$SDK/bin/VERANDA.SYS" "$OUT/golden/BIG6.CSV" "$d/card/"
		$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
		cp "$SDK/bin/VERANDA.PRG" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -sdcard sd.img \
			-prg VERANDA.PRG -run -warp -sound none -echo -dump V > console.txt 2>&1) || true
	else
		cp "$1" "$d/AUTOSTART.cvt"
		cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$OUT/golden/BIG6.CSV" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
	fi
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: the session did not finish"; }
}

# stop point 1: the screen the row scroll left (only the row that came
# into view drawn) against the host build's at the same point
run "$OUT/cvt/stop1.cvt" "$OUT/scroll"
[ "$(tr -d '\r' < "$OUT/scroll/console.txt" | grep -a -c "^KTIME")" = 6 ] || fail "stop point 1 is not after the row scroll"
$PY tools/check_veranda.py "$OUT/scroll/dump.bin" "$OUT/golden/screen1.txt" "$ROM" \
	|| fail "the screen after the row scroll differs from the host build's"
ok "the screen after the row scroll equals the host build's"
$PY "$SDK/tools/vram2png.py" --color "$OUT/scroll/dump.bin" "$OUT/scroll.png" 2>/dev/null || true

for d in "$OUT/host" "$OUT/sd"; do
	run "$OUT/cvt/stop0.cvt" "$d" $([ "$d" = "$OUT/sd" ] && echo sd)
	grep -a -q "CALC DONE" "$d/console.txt" || fail "$d: the script did not end"
	tr -d '\r' < "$d/console.txt" | grep -a -o -E "LOG$|^(CELL|CURSOR|SHEETS|DIRTY|WIDTH) .*" > "$d/cells.txt" || true
	diff "$OUT/golden/cells.txt" "$d/cells.txt" > "$d/cells.diff" || { head "$d/cells.diff"; fail "$d: the cells differ from the host build's"; }
	$PY tools/check_veranda.py "$d/dump.bin" "$OUT/golden/golden.txt" "$ROM" || fail "$d: the screen differs from the host build's"
	ok "$d: the cells and the last screen equal the host build's"
done
$PY "$SDK/tools/vram2png.py" --color "$OUT/host/dump.bin" "$OUT/screen.png" 2>/dev/null || true

d=$OUT/host
tr -d '\r' < "$d/console.txt" | grep -a -o "^KTIME . [0-9A-F]*" | awk '{print $2, $3}' > "$d/times.raw"
[ "$(wc -l < "$d/times.raw")" = 8 ] || fail "the timings were not all logged"
n=0
while read -r c hex; do
	n=$((n + 1))
	us=$(( 16#$hex / 8 ))
	case $n in 1) w="full redraw, empty sheet";; 2) w="full redraw, 120 x 8 cells";; 3) w="cursor down";;
		4) w="cursor right";; 5) w="PgDn";; 6) w="down: the grid scrolls a row";; 7) w="cursor up";;
		8) w="full redraw again";; esac
	printf "time  %-28s %4d.%d ms\n" "$w:" $((us / 1000)) $((us % 1000 / 100))
	echo "$n $us" >> "$d/times.txt"
done < "$d/times.raw"
t() { awk -v n="$1" '$1 == n {print int($2 / 1000)}' "$d/times.txt"; }
empty=$(t 1); full=$(t 2); full2=$(t 8); pgdn=$(t 5); scroll=$(t 6); cur=$(t 3)
[ "$full2" -gt "$full" ] && full=$full2
for c in 4 7; do [ "$(t $c)" -gt "$cur" ] && cur=$(t $c); done
[ "$empty" -lt "$EMPTY_MS" ] || fail "a full redraw of the empty sheet takes $empty ms (the budget: $EMPTY_MS)"
[ "$full" -lt "$FULL_MS" ] || fail "a full redraw takes $full ms (the budget: $FULL_MS)"
[ "$pgdn" -lt "$PGDN_MS" ] || fail "PgDn takes $pgdn ms (the budget: $PGDN_MS)"
[ "$scroll" -lt "$SCROLL_MS" ] || fail "a row scroll takes $scroll ms (the budget: $SCROLL_MS)"
[ "$cur" -lt "$CURSOR_MS" ] || fail "a cursor step takes $cur ms (the budget: $CURSOR_MS)"
ok "within the budgets: the empty sheet's redraw $empty ms (< $EMPTY_MS), a full one $full ms (< $FULL_MS), PgDn $pgdn ms (< $PGDN_MS), a row scroll $scroll ms (< $SCROLL_MS), a cursor step $cur ms (< $CURSOR_MS)"
echo "GATE veranda6: PASS"
