#!/bin/bash
# Gate veranda5 (Veranda Calc, phase C5 of the port): Veranda's shell opens
# a workbook with Calc.
#
#   tools/gate_veranda5.sh [SDK=<Veranda's installed SDK>] [VERANDA=<its tree>]
#
# The SDK and the tree must have Veranda's V44 (an application's documents:
# the shell opens a plain file with the application that declares it) and
# V82 (long names: dataLongName, API 5).
#
# 1. Veranda's shell (VERANDA/shell, built against the SDK); make
#    veranda-test5 (tools/veranda_script.py c5) and its stop points 1 and 2;
#    "Veranda Calc.cvt" declares X16S XLSX ODS CSV and carries its own tile
#    (vpack.py --list; Veranda Look 2, V77): the shell's for the class
#    VCalc (VERANDA/shell/tiles.txt), the same colour and pixels
# 2. a folder with the shell, Calc and "$DOC" (tools/make_x16s.py's
#    budget; a long name: 23 characters, so dataFileName holds its short
#    form and Calc must open dataLongName), and the shell's script
#    (SHELL.TST: a double click on it, then the end when the shell comes
#    back). In x16emu, headless: the shell starts Calc with it; Calc opens
#    it itself (not changed: DIRTY 0), its title band says its name (CALC
#    TITLE); the right button on A21, 99 typed; Veranda Calc > About; File
#    > Quit, discard: Yes; the shell again, SHELL DONE.
# 3. the console in order (the shell, Calc, the shell); the cells after the
#    open and after the edit equal the host build's (it opens the workbook
#    by File > Open); stop point 1: the grid equals the host build's after
#    the open (tools/check_veranda.py: Veranda Look 2's chrome too), the
#    name in the status line; stop point 2: the About box is open; on the
#    host folder and an SD card image
# Pictures: build/gate/veranda5/opened.png (Calc, the workbook opened from
# the shell), about.png, shell.png (the shell after Calc).
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
VERANDA=${VERANDA:-$HOME/Projects/geos}
OUT=build/gate/veranda5
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
T5=build/veranda/test5
DOC="Annual budget 2026.X16S"           # a long name (23: more than GEOS's 16;
                                        # first by name: the console's file is beside it)

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/veranda.h" ] && [ -f "$SDK/bin/VERANDA.SYS" ] || fail "no Veranda SDK with its system in $SDK"
grep -q "ST_LD_DATA" "$SDK/include/veranda.h" || fail "the SDK in $SDK is older than V44 (no ST_LD_DATA)"
rm -rf "$OUT"
mkdir -p "$OUT/cvt" "$OUT/golden"

cp -r "$VERANDA/shell" "$OUT/shell"
rm -f "$OUT/shell"/*.cvt "$OUT/shell"/*.map
make -s -C "$OUT/shell" SDK="$SDK" >"$OUT/build.log" 2>&1 || { tail "$OUT/build.log"; fail "Veranda's shell does not build"; }
$PY tools/veranda_script.py c5 tools/veranda_session5.h >/dev/null
for stop in 1 2 ""; do
	make -s veranda-test5 SDK="$SDK" PLAY_STOP=$stop >>"$OUT/build.log" 2>&1 \
		|| { tail -20 "$OUT/build.log"; fail "make veranda-test5 PLAY_STOP=$stop"; }
	cp "$T5/Veranda Calc.cvt" "$OUT/cvt/stop${stop:-0}.cvt"
done
make -s build/host/veranda_golden5 >>"$OUT/build.log" 2>&1 || fail "the golden's host program"
docs=$($PY "$SDK/tools/vpack.py" --list "$OUT/cvt/stop0.cvt" | sed -n 's/^documents *//p')
[ "$docs" = "X16S XLSX ODS CSV" ] || fail "Veranda Calc.cvt declares \"$docs\", not X16S XLSX ODS CSV"
ok "the shell, make veranda-test5 and its stop points: built against $SDK; Calc opens $docs"
$PY - "$OUT/cvt/stop0.cvt" "$VERANDA/shell/tiles.txt" <<'PYEOF' || fail "Calc's tile is not the shell's for VCalc"
import sys
# the tile block (Veranda's docs/APPFORMAT.md, V77): the CVT's first block,
# byte 96 the colour, 97-224 the glyph (32 rows of 4 bytes, bit 7 the left)
d = open(sys.argv[1], 'rb').read()
lines = open(sys.argv[2]).read().split('\n')
i = lines.index('for class VCalc')
colour = lines[i + 1].split()[1]
rows = [l for l in lines[i + 2:i + 34]]
want = b''.join(int(r.replace('#', '1').replace('.', '0'), 2).to_bytes(4, 'big') for r in rows)
names = ['paper', 'ink', 'chrome', 'sidebar', 'select', 'title', 'yellow', 'red', 'green', 'purple', 'orange',
         'cyan', 'sky', 'sky2', 'hill', 'dusk']
ok = d[96] == names.index(colour) and d[97:225] == want
print('%s  Calc\'s own tile (%s, %d ink pixels) is the shell\'s for the class VCalc'
      % ('ok   ' if ok else 'FAIL ', colour, sum(bin(b).count('1') for b in want)))
sys.exit(0 if ok else 1)
PYEOF

python3 -B -c "
import sys; sys.path.insert(0, 'tools'); import make_x16s as m
b = m.Book(); m.budget(b); b.write('$OUT/golden/$DOC')" || fail "$DOC"
(cd "$OUT/golden" && ../../../host/veranda_golden5 >golden.txt 2>cells.txt) || fail "the golden"

# the shell's script: a double click on the workbook (the first icon: the
# listing is by name), then, at the shell's second start, the end
shell_tst() {
	python3 -B -c "
import struct, sys
def part(recs, f=10):
    out = []
    for gap, x, y, b, k in recs:
        out.append(struct.pack('<HHHBB', f, x, y, b, k)); f += gap
    return out
x, y = 152, 96                    # (Veranda Look 2, V77: the first tile of window 0, tests/shell_script.py cell(0))
dbl = [(4, x, y, 0, 0), (3, x, y, 1, 0), (4, x, y, 0, 0), (3, x, y, 1, 0), (40, x, y, 0, 0), (0, 0, 0, 0x40, 0)]
end = [(30, 0, 0, 0, 0), (0, 0, 0, 0x40, 1)]
open(sys.argv[1], 'wb').write(b''.join(part(dbl) + part(end)))" "$1/SHELL.TST"
}

run() {   # cvt, dir, [sd]
	d=$2
	mkdir -p "$d"
	if [ -n "$3" ]; then
		mkdir -p "$d/card"
		cp "$1" "$d/card/Veranda Calc.cvt"
		cp "$OUT/shell/SHELL.cvt" "$SDK/bin/VERANDA.SYS" "$OUT/golden/$DOC" "$d/card/"
		shell_tst "$d/card"
		$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
		cp "$SDK/bin/VERANDA.PRG" "$d/"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
	else
		cp "$1" "$d/Veranda Calc.cvt"
		cp "$OUT/shell/SHELL.cvt" "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$OUT/golden/$DOC" "$d/"
		shell_tst "$d"
		(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 300 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
			-warp -sound none -echo -dump V > console.txt 2>&1) || true
	fi
	[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "$d: the session did not finish"; }
	grep -a -q "^HUNG" "$d/console.txt" && fail "$d: the program hung (the player's watchdog)"
	return 0
}

whole() {   # dir
	d=$1
	got=$(tr -d '\r' < "$d/console.txt" | grep -a -o -E '^(SHELL STARTED|CALC RUNNING|SHELL DONE)' | tr '\n' ',')
	[ "$got" = "SHELL STARTED,CALC RUNNING,SHELL STARTED,SHELL DONE," ] || { tail -20 "$d/console.txt"; fail "$d: the console has $got"; }
	ok "$d: the shell started Calc with \"$DOC\" (a double click), Calc quit, the shell came back"
	tr -d '\r' < "$d/console.txt" | grep -a -q "CALC TITLE $DOC\$" \
		|| fail "$d: the title band does not name \"$DOC\": $(tr -d '\r' < "$d/console.txt" | grep -a -o 'CALC TITLE.*' | tr '\n' ',')"
	ok "$d: Calc opened dataLongName, the whole name (its title band: CALC TITLE $DOC)"
	tr -d '\r' < "$d/console.txt" | grep -a -o -E "LOG$|^(CELL|CURSOR|SHEETS|DIRTY|WIDTH) .*" > "$d/cells.txt" || true
	head -n "$(wc -l < "$d/cells.txt")" "$OUT/golden/cells.txt" > "$d/golden-cells.txt"
	[ "$(grep -c '^LOG' "$d/cells.txt")" = 2 ] || fail "$d: Calc logged $(grep -c '^LOG' "$d/cells.txt") times, not 2"
	diff "$d/golden-cells.txt" "$d/cells.txt" > "$d/cells.diff" || { head -20 "$d/cells.diff"; fail "$d: the cells differ from the host build's"; }
	grep -q "^DIRTY 0" "$d/cells.txt" || fail "$d: $DOC opened counts as changed"
	grep -q "^CELL A21 99" "$d/cells.txt" || fail "$d: the right button did not edit A21"
	ok "$d: the workbook's cells equal the host build's after File > Open; not changed (DIRTY 0); the right button on A21 edited it (99)"
}

run "$OUT/cvt/stop0.cvt" "$OUT/host"
whole "$OUT/host"
$PY "$SDK/tools/vram2png.py" --color "$OUT/host/dump.bin" "$OUT/shell.png" 2>/dev/null || true
run "$OUT/cvt/stop0.cvt" "$OUT/sd" sd
whole "$OUT/sd"

# stop point 1: the grid as opened, against the host build's after its Open
run "$OUT/cvt/stop1.cvt" "$OUT/opened"
$PY tools/check_veranda.py "$OUT/opened/dump.bin" "$OUT/golden/screen1.txt" "$ROM" \
	|| fail "the workbook opened from the shell does not look as the host build's"
$PY "$SDK/tools/vram2png.py" --color "$OUT/opened/dump.bin" "$OUT/opened.png" 2>/dev/null || true
run "$OUT/cvt/stop2.cvt" "$OUT/about"
$PY tools/check_veranda2.py dialog "$OUT/about/dump.bin" || fail "the About box is not open at stop point 2"
$PY "$SDK/tools/vram2png.py" --color "$OUT/about/dump.bin" "$OUT/about.png" 2>/dev/null || true
ok "pictures: $OUT/opened.png, about.png, shell.png"
echo "GATE veranda5: PASS"
