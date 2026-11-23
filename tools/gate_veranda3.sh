#!/bin/bash
# Gate veranda3 (Veranda Calc, phase C3 of the port): the files, played by
# a script and compared with the host build.
#
#   tools/gate_veranda3.sh [SDK=<Veranda's installed SDK>] [VERANDA=<its tree>]
#
# The SDK's system must have Veranda's V39 (GetString's text 0-terminated
# after every key).
#
# 1. make veranda-test3 against the installed SDK; the folder holds
#    BUDGET.XLSX (tools/make_examples.py) and RICH.ODS
#    (tools/make_ods_fixtures.py), and nothing else of the program's
# 2. the session (tools/veranda_script.py c3): typing; File > Save as
#    T1.X16S and File > Export T1.CSV (dialog boxes); File > New; File >
#    Open (the file chooser) T1.X16S; New, File > Import T1.CSV; New,
#    Import BUDGET.XLSX; New, Import RICH.ODS -- the cells logged after
#    each step. Played by Veranda's mouseSource (src/tools/veranda_play2.s)
#    in x16emu, headless; on the host by tools/veranda_golden2.c
#    (PLAY_SESSION 3) in a folder with the same two files.
# 3. every logged step's cells equal the host build's; T1.X16S and T1.CSV
#    equal the host build's byte for byte; the last screen equals the host
#    build's text grid (tools/check_veranda.py); on the host folder and on
#    an SD card image (Veranda's tools/vcard.py, the files read back out)
# 4. part 2 (below): the chooser on a card laid out as a user's, under R49
# The large workbook (ZBIG): 600 rows at 512 KB (BIG_ROWS; the host folder
# and the SD card), 600 at 2 MB (BIG2_ROWS; the host folder, x16emu -ram
# 2048); 800 at 512 KB (NOMEM_ROWS) does not fit Calc's heap (since
# Veranda's V83 a program has 44 RAM banks there, the test build's heap is
# 18; Look 2's Open and Save dialog, overlay 22, has one): its import of
# ZBIG.XLSX ends in the out-of-memory box, "Out of memory" and "<rows> rows,
# <cells> cells (failed)", also on the console (part 1b). A run that fits
# has no such box: a partial import is a failure even when the logged
# cells (rows 1-6) got in.
# The heap: the importer stages a worksheet's text under the heap's top and
# gives each bank back as the parser has read it (src/import/blob.c), so
# what fits is one size, the same after any steps before: in the test
# build's 18 banks ZBIG.XLSX of up to 740 rows (its 810 in the release
# build's 19). Before, the whole text (160 bytes a row, Calc writes the
# parts stored) sat in the heap beside the cells being built, with a bank
# for each small part before it, never given back: from 330 rows on the
# import ran out, the more rows the earlier (350: at row 256, 450: at 87,
# 500: at the first), and this gate passed or not as the first rows got
# in or not. Each log puts the heap on the console: "HEAP first <bank>
# banks <n> peak <most in use since the last> now <in use> free <bytes>"
# (src/tools/veranda_play2.c); heap.txt in each run's folder.
# The file chooser is the SDK's Open and Save dialog (Veranda Look 2,
# Veranda's DECISIONS V79; tools/check_vrfile.py reads it in the dumps).
# Pictures: build/gate/veranda3/screen.png (the end), stop6.png and stop7.png
# (the Open dialog, at the top and in SUB), stop4.png (a report); card1-4.png.
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
VERANDA=${VERANDA:-$HOME/Projects/geos}
OUT=build/gate/veranda3
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
T3=build/veranda/test3

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/veranda.h" ] && [ -f "$SDK/bin/VERANDA.SYS" ] || fail "no Veranda SDK with its system in $SDK"
grep -q "KEY_PGUP" "$SDK/include/veranda.h" || fail "the SDK in $SDK is older than API level 2"
rm -rf "$OUT"
mkdir -p "$OUT/fx" "$OUT/golden"

$PY tools/veranda_script.py c3 tools/veranda_session3.h >/dev/null
make -s veranda-test3 SDK="$SDK" PLAY_STOP= >"$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; fail "make veranda-test3"; }
make -s build/host/veranda_golden3 >>"$OUT/build.log" 2>&1 || fail "the golden's host program"
ok "make veranda-test3: built against $SDK"

# the fixtures into $1 (BUDGET.XLSX, SUB/RICH.ODS) and a large workbook,
# ZBIG.CSV: $2 rows of a number, a fraction, a text, a formula
fixtures() {
	mkdir -p "$1/SUB"
	python3 tools/make_examples.py "$1" >/dev/null
	python3 tools/make_ods_fixtures.py "$1" >/dev/null
	mv "$1/RICH.ODS" "$1/SUB/"
	python3 -c "
import sys
for i in range(1, $2 + 1):
    print('%d,%d.5,item %d,=A%d*2' % (i, i * 3, i, i), end='\r\n')
" > "$1/ZBIG.CSV"
}
# the host build, in a folder of its own ($1, at $OUT's depth) with the files
golden() {
	mkdir -p "$1"
	cp -r $FILES "$1/"
	(cd "$1" && ../../../host/veranda_golden3 >golden.txt 2>cells.txt) || fail "the golden ($1)"
	[ "$(grep -c '^LOG' "$1/cells.txt")" = 7 ] || fail "the host build's session ($1): $(head -3 "$1/cells.txt")"
	for f in T1.X16S T1.CSV T1.XLSX ZBIG.X16S ZBIG.XLSX; do
		[ -s "$1/$f" ] || fail "the host build wrote no $f ($1)"
	done
}
# The large workbook: 600 rows at 512 KB (the X16's base memory), 600 at
# 2 MB; 800 at 512 KB is more than the heap holds: the out-of-memory box
# (part 1b below).
BIG_ROWS=${BIG_ROWS:-600}
BIG2_ROWS=${BIG2_ROWS:-600}
NOMEM_ROWS=${NOMEM_ROWS:-800}
fixtures "$OUT/fx" "$BIG_ROWS"
FILES="$OUT/fx/BUDGET.XLSX $OUT/fx/SUB $OUT/fx/ZBIG.CSV"
GOLD=$OUT/golden
ROWS=$BIG_ROWS
EMUX=
golden "$GOLD"

run() {   # dir, [sd]
	d=$1
	mkdir -p "$d"
	if [ -n "$2" ]; then
		mkdir -p "$d/card"
		cp "$T3/Veranda Calc.cvt" "$d/card/AUTOSTART.cvt"
		cp -r "$SDK/bin/VERANDA.SYS" $FILES "$d/card/"
		$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
		cp "$SDK/bin/VERANDA.PRG" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG -run \
			-warp -sound none $EMUX -echo -dump V > console.txt 2>&1) || true
		for f in T1.X16S T1.CSV T1.XLSX ZBIG.X16S ZBIG.XLSX; do
			$PY "$SDK/tools/vcard.py" get "$d/sd.img" "$f" "$d/$f" >/dev/null 2>&1 || true
		done
	else
		cp "$T3/Veranda Calc.cvt" "$d/AUTOSTART.cvt"
		cp -r "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" $FILES "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
			-warp -sound none $EMUX -echo -dump V > console.txt 2>&1) || true
	fi
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: the session did not finish"; }
	grep -a -q "^HUNG" "$d/console.txt" && fail "$d: the program hung (the player's watchdog)"
	grep -a -q "CALC DONE" "$d/console.txt" || { tail -5 "$d/console.txt"; fail "$d: the script did not end"; }
	tr -d '\r' < "$d/console.txt" | grep -a -o -E "LOG$|^(CELL|CURSOR|SHEETS|DIRTY|WIDTH) .*" > "$d/cells.txt" || true
	diff "$GOLD/cells.txt" "$d/cells.txt" > "$d/cells.diff" || { head -20 "$d/cells.diff"; fail "$d: the cells differ from the host build's"; }
	ok "$d: the cells after each of the 7 steps (typed and saved, the .X16S opened, the CSV, BUDGET.XLSX, SUB/RICH.ODS, ZBIG.CSV then ZBIG.X16S, ZBIG.XLSX) equal the host build's"
	tr -d '\r' < "$d/console.txt" | grep -a -o -E "^HEAP .*" > "$d/heap.txt" || true
	if tr -d '\r' < "$d/console.txt" | sed 's/\\X[0-9A-F][0-9A-F]//g' | grep -a -q -E "[0-9]+ rows, [0-9]+ cells \(failed\)"; then
		fail "$d: an import ran out of memory: $(tr -d '\r' < "$d/console.txt" | sed 's/\\X[0-9A-F][0-9A-F]//g' | grep -a -o -E "[0-9]+ rows, [0-9]+ cells \(failed\)")"
	fi
	[ "$(wc -l < "$d/heap.txt")" = 7 ] || fail "$d: the heap was not logged at each step"
	ok "$d: no import ran out of memory; the heap after the import of ZBIG.XLSX: $(tail -1 "$d/heap.txt" | cut -d' ' -f2-)"
	for f in T1.X16S T1.CSV T1.XLSX; do
		cmp -s "$d/$f" "$GOLD/$f" || fail "$d: $f differs from the host build's"
	done
	ok "$d: T1.X16S ($(stat -c %s "$d/T1.X16S") bytes: Save as, then Save), T1.CSV ($(stat -c %s "$d/T1.CSV") bytes) and T1.XLSX ($(stat -c %s "$d/T1.XLSX") bytes) equal the host build's, byte for byte"
	$PY tools/check_xlsx.py "$d/T1.XLSX" > "$d/check_xlsx.txt" 2>&1 || { cat "$d/check_xlsx.txt"; fail "$d: T1.XLSX is not a valid .xlsx (tools/check_xlsx.py)"; }
	ok "$d: T1.XLSX passes tools/check_xlsx.py"
	for f in ZBIG.X16S ZBIG.XLSX; do
		cmp -s "$d/$f" "$GOLD/$f" || fail "$d: $f differs from the host build's"
	done
	$PY tools/check_xlsx.py "$d/ZBIG.XLSX" > "$d/check_xlsx2.txt" 2>&1 || { cat "$d/check_xlsx2.txt"; fail "$d: ZBIG.XLSX is not a valid .xlsx"; }
	ok "$d: the large workbook ($ROWS rows): ZBIG.X16S ($(stat -c %s "$d/ZBIG.X16S") bytes) and ZBIG.XLSX ($(stat -c %s "$d/ZBIG.XLSX") bytes) equal the host build's; check_xlsx.py passes"
	tr -d '\r' < "$d/console.txt" | grep -a -o "^TIME . [0-9A-F]*" | while read -r _ c hex; do
		case $c in I) w="import ZBIG.CSV";; S) w="save ZBIG.X16S";; X) w="export ZBIG.XLSX";;
			O) w="open ZBIG.X16S";; M) w="import ZBIG.XLSX";; *) w=$c;; esac
		echo "time  $d: $w: $(( 16#$hex / 8000 )) ms"
	done | tee "$d/times.txt"
	[ "$(wc -l < "$d/times.txt")" = 5 ] || fail "$d: the timings were not all logged"
	$PY tools/check_veranda.py "$d/dump.bin" "$GOLD/golden.txt" "$ROM" || fail "$d: the screen differs from the host build's"
}

run "$OUT/host"
$PY "$SDK/tools/vram2png.py" --color "$OUT/host/dump.bin" "$OUT/screen.png" 2>/dev/null || true
run "$OUT/sd" sd

# Part 1b. The large workbook of $BIG2_ROWS rows at 2 MB: the whole session
# as above (the host folder). Of $NOMEM_ROWS rows at 512 KB: its import of
# ZBIG.XLSX does not fit the heap, and Calc says so -- the out-of-memory box
# ("Out of memory", the rows and cells that got in, "(failed)"; its text on
# the console too) -- instead of a partial workbook passed off as the
# import; the steps before it (the CSV, saved, exported, opened) fit.
fixtures "$OUT/fx2" "$BIG2_ROWS"
FILES1=$FILES
FILES="$OUT/fx2/BUDGET.XLSX $OUT/fx2/SUB $OUT/fx2/ZBIG.CSV"
GOLD=$OUT/golden2
ROWS=$BIG2_ROWS
golden "$GOLD"
EMUX="-ram 2048"
run "$OUT/host2mb"
EMUX=
fixtures "$OUT/fx3" "$NOMEM_ROWS"
FILES="$OUT/fx3/BUDGET.XLSX $OUT/fx3/SUB $OUT/fx3/ZBIG.CSV"
GOLD=$OUT/golden3
golden "$GOLD"
d=$OUT/nomem
mkdir -p "$d"
cp "$T3/Veranda Calc.cvt" "$d/AUTOSTART.cvt"
cp -r "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" $FILES "$d/"
(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump V > console.txt 2>&1) || true
grep -a -q "CALC DONE" "$d/console.txt" || { tail -5 "$d/console.txt"; fail "$d: the script did not end"; }
grep -a -q "^HUNG" "$d/console.txt" && fail "$d: the program hung"
tr -d '\r' < "$d/console.txt" | grep -a -o -E "^HEAP .*" > "$d/heap.txt" || true
# (the console: the emulator's \XHH escapes of the DOS's replies taken out
# first, as the box's line can follow one on the same line)
nomem=$(tr -d '\r' < "$d/console.txt" | sed 's/\\X[0-9A-F][0-9A-F]//g' \
	| grep -a -o -E "[0-9]+ rows, [0-9]+ cells \(failed\)" | head -1)
[ -n "$nomem" ] || { tr -d '\r' < "$d/console.txt" | grep -a -o -E "[0-9]+ (rows|cells), .*" | tail -5; fail "$d: no out-of-memory box after the import of ZBIG.XLSX ($NOMEM_ROWS rows) at 512 KB"; }
[ "$(tr -d '\r' < "$d/console.txt" | sed 's/\\X[0-9A-F][0-9A-F]//g' | grep -a -c -E "[0-9]+ rows, [0-9]+ cells \(failed\)")" = 1 ] \
	|| fail "$d: more than one import ran out of memory"
got=${nomem%% rows*}
[ "$got" -lt "$NOMEM_ROWS" ] || fail "$d: the out-of-memory box says $got rows of $NOMEM_ROWS"
# the steps before the import ($NOMEM_ROWS-row CSV, saved, exported, opened) as the host build's
tr -d '\r' < "$d/console.txt" | grep -a -o -E "LOG$|^(CELL|CURSOR|SHEETS|DIRTY|WIDTH) .*" > "$d/cells.txt" || true
n7=$(grep -n '^LOG' "$GOLD/cells.txt" | sed -n 7p | cut -d: -f1)
head -n $((n7 - 1)) "$GOLD/cells.txt" > "$d/golden6.txt"
head -n $((n7 - 1)) "$d/cells.txt" | cmp -s - "$d/golden6.txt" || fail "$d: the steps before the import differ from the host build's"
for f in ZBIG.X16S ZBIG.XLSX; do
	cmp -s "$d/$f" "$GOLD/$f" || fail "$d: $f differs from the host build's"
done
ok "$d (512 KB): ZBIG.XLSX of $NOMEM_ROWS rows does not fit the heap: the out-of-memory box \"Out of memory: $nomem\" (the console), not a partial import; the steps before it equal the host build's (ZBIG.X16S, ZBIG.XLSX too); the heap after it: $(tail -1 "$d/heap.txt" | cut -d' ' -f2-)"
FILES=$FILES1
GOLD=$OUT/golden
ROWS=$BIG_ROWS

# stop points (host folder): the file chooser at the top (6) and in SUB (7);
# the report of an import over the grid, the busy box gone (4)
cp "$T3/Veranda Calc.cvt" "$OUT/full.cvt"
for stop in 6 7 4; do
	make -s veranda-test3 SDK="$SDK" PLAY_STOP=$stop >>"$OUT/build.log" 2>&1 || fail "make veranda-test3 PLAY_STOP=$stop"
	d="$OUT/stop$stop"
	mkdir -p "$d"
	cp "$T3/Veranda Calc.cvt" "$d/AUTOSTART.cvt"
	cp -r "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" $FILES "$OUT/host/T1.X16S" "$OUT/host/T1.CSV" "$OUT/host/T1.XLSX" "$d/"
	(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
		-warp -sound none -echo -dump V > console.txt 2>&1) || true
	[ -f "$d/dump.bin" ] || fail "stop point $stop: no dump"
	$PY "$SDK/tools/vram2png.py" --color "$d/dump.bin" "$OUT/stop$stop.png" 2>/dev/null || true
done
cp "$OUT/full.cvt" "$T3/Veranda Calc.cvt"
$PY - "$OUT/stop6/dump.bin" "$OUT/stop7/dump.bin" "$OUT/stop4/dump.bin" <<'PYEOF' || fail "the stop points"
import sys
sys.path.insert(0, 'tools')
import check_vrfile as vf       # the SDK's Open and Save dialog (Veranda Look 2)
v6, v7, v4 = (open(f, 'rb').read() for f in sys.argv[1:4])
res = [('at the top, SUB (the first row) selected: ' + t, g) for t, g in vf.check(v6, sel=0, rows=5)]
res += [('in SUB, ".." selected: ' + t, g) for t, g in vf.check(v7, sel=0, rows=2)]
MAP = 0x1AC00
paper = [v4[MAP + (9 * 128 + x) * 2 + 1] >> 4 for x in range(14, 60)]
res.append(('under the import\'s report the grid is drawn again: the busy box is gone', all(p == 0 for p in paper)))
for t, g in res:
    print(('ok   ' if g else 'FAIL ') + '  ' + t)
sys.exit(0 if all(g for _, g in res) else 1)
PYEOF
ok "pictures: $OUT/stop6.png (the chooser), stop7.png (in SUB), stop4.png (a report)"
# Part 2: the chooser on a card laid out as a user's (tools/veranda_card.py:
# Veranda in /VERANDA with the shell, Calc in /VERANDA/APPS (Veranda's
# layout, its DECISIONS V71), some 35 folders and 30 files
# in the top folder, long names, spaces, hidden ones), booted as a user
# does -- DOS"CD:VERANDA", LOAD"VERANDA.PRG", RUN -- under the user's ROM
# (R49) and this one; the shell (in /DOCUMENTS) starts Calc from its pin
# (a click on it in the dock, V83): Calc works in /DOCUMENTS. The session
# (tools/veranda_script.py c8): Open in /DOCUMENTS, ".." to the top, into
# "Household Accounts 2024", open "Monthly family budget version 2.x16s";
# Open again starts there; up, into VERANDA, Cancel; Save; Open it again;
# Save as "Budget copy with a long name" (.X16S added), New, Open it;
# into "Older years", Budget 2019.X16S; up twice, Quick sums.X16S in the
# top folder. The five are the same workbook: their cells are equal, and
# the saved ones are where they belong, no temporary file left.
# Pictures: card1.png (Open in /DOCUMENTS), card2.png (the top folder),
# card3.png (in "Household Accounts 2024"), card4.png (Open again).
ROM49=${ROM49:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
[ -f "$ROM49" ] || fail "no R49 ROM at $ROM49 (ROM49=...)"
C8=$OUT/card
mkdir -p "$C8"
cp -r "$VERANDA/shell" "$C8/shell"
rm -f "$C8/shell"/*.cvt "$C8/shell"/*.map
make -s -C "$C8/shell" SDK="$SDK" >>"$OUT/build.log" 2>&1 || fail "Veranda's shell does not build"
$PY tools/veranda_script.py c8 tools/veranda_session8.h >/dev/null
for stop in "" 1 2 3 4; do
	make -s veranda-test8 SDK="$SDK" PLAY_STOP=$stop >>"$OUT/build.log" 2>&1 || fail "make veranda-test8 PLAY_STOP=$stop"
	cp "build/veranda/test8/Veranda Calc.cvt" "$C8/calc$stop.cvt"
done
ok "make veranda-test8 (the session and its stop points 1-4) and the shell: built"

card() {   # dir, cvt, rom
	d=$1
	mkdir -p "$d/card"
	python3 -B tools/veranda_card.py make "$d/card" >/dev/null
	mkdir -p "$d/card/VERANDA/APPS" "$d/card/VERANDA/.SYSTEM"
	cp "$2" "$d/card/VERANDA/APPS/Veranda Calc.cvt"
	cp "$C8/shell/SHELL.cvt" "$SDK/bin/VERANDA.SYS" "$SDK/bin/VERANDA.PRG" "$d/card/VERANDA/"
	# the shell's desk file: Calc pinned (/VERANDA/APPS); the script (in the
	# top folder): a click on the pin in the dock, then the end
	python3 -B -c "
import sys
d = bytearray(16 + 61 + 1 + 1 + 4 * (62 + 22))
d[0:4] = b'VSD\x01'
d[16] = ord('/')
d[78] = 1
d[79:79 + 13] = b'/VERANDA/APPS'
d[79 + 62:79 + 62 + 16] = b'Veranda Calc.cvt'
open(sys.argv[1], 'wb').write(bytes(d))" "$d/card/VERANDA/.SYSTEM/SHELL.DSK"
	python3 -B -c "
import struct, sys
def part(recs, f=10):
    out = []
    for gap, x, y, b, k in recs:
        out.append(struct.pack('<HHHBB', f, x, y, b, k)); f += gap
    return out
x, y = 296, 448                   # (Veranda V83, V152: the pin is the dock's only item, then the trash: tests/shell_script.py dock_item(0, 1); a click starts it; 320, 440 before V152)
dbl = [(4, x, y, 0, 0), (3, x, y, 1, 0), (40, x, y, 0, 0), (0, 0, 0, 0x40, 0)]
end = [(30, 0, 0, 0, 0), (0, 0, 0, 0x40, 1)]
open(sys.argv[1], 'wb').write(b''.join(part(dbl) + part(end)))" "$d/card/SHELL.TST"
	$PY "$SDK/tools/vcard.py" make --mb 256 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
	printf 'DOS"CD:VERANDA"\nLOAD"VERANDA.PRG"\nRUN\n' > "$d/boot.bas"
	(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 400 "$EMU" -rom "$3" -sdcard sd.img -bas boot.bas \
		-warp -sound none -echo -dump V > console.txt 2>&1) || true
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: no dump"; }
}

for r in r49 master; do
	[ $r = r49 ] && rom=$ROM49 || rom=$ROM
	d=$C8/$r
	card "$d" "$C8/calc.cvt" "$rom"
	grep -a -q "^HUNG" "$d/console.txt" && fail "$d: the program hung"
	grep -a -q "CALC DONE" "$d/console.txt" || { tail -5 "$d/console.txt"; fail "$d: the script did not end"; }
	tr -d '\r' < "$d/console.txt" | grep -a -o -E "LOG$|^(CELL|CURSOR|SHEETS|DIRTY|WIDTH) .*" \
		| awk -v o="$d/log" '/^LOG/{n++} {print > (o n ".txt")}'
	for i in 2 3 4 5; do
		cmp -s "$d/log1.txt" "$d/log$i.txt" || fail "$d: workbook $i's cells differ from the first's"
	done
	grep -q "^CELL A1 Monthly outgoings" "$d/log1.txt" && grep -q "^SHEETS Budget" "$d/log1.txt" \
		|| fail "$d: the first workbook is not the budget: $(head -3 "$d/log1.txt")"
	$PY "$SDK/tools/vcard.py" ls "$d/sd.img" > "$d/ls.txt"
	grep -q "^Household Accounts 2024/Monthly family budget version 2.x16s " "$d/ls.txt" \
		|| fail "$d: the saved workbook is not in its folder"
	grep -q "^Household Accounts 2024/Budget copy with a long name.X16S " "$d/ls.txt" \
		|| fail "$d: Save as did not write \"Budget copy with a long name.X16S\" in its folder"
	grep -q -i "CMDRCALC.TMP" "$d/ls.txt" && fail "$d: Save left its temporary file: $(grep -i CMDRCALC "$d/ls.txt")"
	ok "$d ($r ROM): from /DOCUMENTS up to the top folder, into \"Household Accounts 2024\" and \"Older years\" and back; five workbooks opened (36 characters, spaces; the top folder's too; a copy saved as \"Budget copy with a long name\", .X16S added, and opened again) with equal cells; Save wrote it back in its folder"
done

for stop in 1 2 3 4; do
	card "$C8/stop$stop" "$C8/calc$stop.cvt" "$ROM49"
	$PY "$SDK/tools/vram2png.py" --color "$C8/stop$stop/dump.bin" "$OUT/card$stop.png" 2>/dev/null || true
done
$PY - "$C8"/stop{1,2,3,4}/dump.bin <<'PYEOF' || fail "the card's stop points"
import sys
sys.path.insert(0, 'tools')
import check_vrfile as vf       # the SDK's Open and Save dialog (Veranda Look 2)
v = [open(f, 'rb').read() for f in sys.argv[1:5]]
checks = [
    ('Open in /DOCUMENTS: ".." only, selected', vf.check(v[0], sel=0, rows=1)),
    ('the top folder: the list full, the first selected', vf.check(v[1], sel=0, rows=7)),
    ('in "Household Accounts 2024": "..", Older years, the budget', vf.check(v[2], sel=0, rows=3)),
    ('Open again starts there', vf.check(v[3], rows=3)),
]
for t, res in checks:
    for w, good in res:
        print(('ok   ' if good else 'FAIL ') + '  ' + t + ': ' + w)
sys.exit(0 if all(g for _, r in checks for _, g in r) else 1)
PYEOF
ok "pictures: $OUT/card1.png (Open in /DOCUMENTS), card2.png (the top folder), card3.png (in a folder), card4.png (Open again)"
echo "GATE veranda3: PASS"
