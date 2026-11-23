#!/bin/bash
# Gate veranda7 (Veranda Calc): Veranda's clipboard, the Text Scrap
# (src/platform/clip_veranda.c; Veranda's DECISIONS V54 and its SDK's
# scrap library, compiled into overlay 21).
#
#   tools/gate_veranda7.sh [SDK=<Veranda's installed SDK, with src/scrap*.c>]
#
# 1. make veranda-test7 (tools/veranda_script.py c7) against the SDK
# 2. a folder with Calc as AUTOSTART.cvt and Veranda's system folder
#    VERANDA/ holding a Text Scrap another program wrote (GEOS's format: the
#    count, a NEWCARDSET, "1 TAB 2 CR 3 TAB =A1+B1 CR"). In x16emu,
#    headless: Edit > Paste at A1 takes the Text Scrap (A1:B2, B2 the
#    formula: 3); A1:B2 selected, Edit > Copy: VERANDA/Text  Scrap.cvt is
#    the cells as text ("1 TAB 2 CR 3 TAB 3 CR", GEOS's format, checked on
#    the host); D1, Edit > Paste: Calc's own clipboard (the scrap is the
#    one it wrote), D2 the formula moved (3); then Veranda Calc > Desk
#    accessory (Veranda Look 2's first menu): no desk accessories with the
#    applications here, the box that says so, OK -- the session ends
# 3. the same on an SD card image
set -e
cd "$(dirname "$0")/.."
for a in "$@"; do export "$a"; done
SDK=${SDK:-$HOME/Projects/geos/build/sdk}
OUT=build/gate/veranda7
PY="python3 -I -B"
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
T7=build/veranda/test7

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

[ -f "$SDK/include/scrap.h" ] && [ -f "$SDK/src/scrapio.c" ] && [ -f "$SDK/bin/VERANDA.SYS" ] \
	|| fail "no Veranda SDK with the clipboard (include/scrap.h, src/scrapio.c) and its system in $SDK"
rm -rf "$OUT"
# (Veranda's layout, its DECISIONS V71: the scraps in /VERANDA/.SYSTEM)
mkdir -p "$OUT/host/VERANDA/.SYSTEM" "$OUT/sd/card/VERANDA/.SYSTEM" "$OUT/sd/out"
$PY tools/veranda_script.py c7 tools/veranda_session7.h >/dev/null
make -s veranda-test7 SDK="$SDK" PLAY_STOP= >"$OUT/build.log" 2>&1 || { tail -20 "$OUT/build.log"; fail "make veranda-test7"; }
ok "make veranda-test7: built against $SDK (the scrap library in overlay 21)"

# a Text Scrap as another program writes it (GEOS's: Veranda's APPFORMAT)
$PY -c "
import sys
t = bytes([23, 9, 0, 0]) + b'1\t2\r3\t=A1+B1\r'
d = bytes([len(t), 0]) + t
e = bytearray(30); e[0] = 0x83; e[3:19] = b'Text  Scrap'.ljust(16, b'\xa0'); e[22] = 4; e[28] = 2
i = bytearray(254); i[0:3] = b'\x03\x15\xbf'; i[66:69] = b'\x83\x04\x00'; i[75:91] = b'Text  Scrap V2.0'
open(sys.argv[1], 'wb').write(bytes(e) + b'PRG formatted GEOS file V1.0'.ljust(224, b'\0') + bytes(i) + d)
" "$OUT/scrap.cvt"

check() {      # the run's console and its Text Scrap
	local con=$1 scrap=$2
	grep -a -q "CALC DONE" "$con" || fail "the script did not end"
	tr -d '\r' < "$con" | grep -a -o -E "^CELL [A-E][12] .*" | sed 's/  */ /g; s/ $//' > "$OUT/cells.txt"
	printf '%s\n' "CELL A1 1" "CELL B1 2" "CELL C1" "CELL D1" "CELL E1" "CELL A2 3" "CELL B2 3" "CELL C2" "CELL D2" "CELL E2" \
		"CELL A1 1" "CELL B1 2" "CELL C1" "CELL D1 1" "CELL E1 2" "CELL A2 3" "CELL B2 3" "CELL C2" "CELL D2 3" "CELL E2 3" \
		"CELL A1 1" "CELL B1 2" "CELL C1" "CELL D1 1" "CELL E1 2" "CELL A2 3" "CELL B2 3" "CELL C2" "CELL D2 3" "CELL E2 3" \
		> "$OUT/want.txt"     # (the two logs, and the end's)
	diff "$OUT/want.txt" "$OUT/cells.txt" || fail "the cells"
	$PY -c "
import sys
f = open(sys.argv[1], 'rb').read()
e, i, d = f[:30], f[254:508], f[508:]
assert e[3:19] == b'Text  Scrap'.ljust(16, b'\xa0') and e[21] == 0 and e[22] == 4, 'the entry'
assert i[75:91] == b'Text  Scrap V2.0', 'the class'
n = d[0] | d[1] << 8
assert n == len(d) - 2, 'the count'
assert d[2:] == bytes([23, 9, 0, 0]) + b'1\t2\r3\t3\r', repr(d[2:])
" "$scrap" || fail "the Text Scrap after Edit > Copy"
}

d=$OUT/host
cp "$T7/Veranda Calc.cvt" "$d/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$d/"
cp "$OUT/scrap.cvt" "$d/VERANDA/.SYSTEM/Text  Scrap.cvt"
(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT 120 "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
	-warp -sound none -echo -dump V > console.txt 2>&1) || true
[ -f "$d/dump.bin" ] || { tail -5 "$d/console.txt"; fail "the session did not finish"; }
check "$d/console.txt" "$d/VERANDA/.SYSTEM/Text  Scrap.cvt"
$PY "$SDK/tools/vram2png.py" --color "$d/dump.bin" "$OUT/screen.png" 2>/dev/null || true
ok "Paste took another program's Text Scrap into A1:B2 (B2 =A1+B1: 3); Copy wrote A1:B2 to VERANDA/.SYSTEM/Text  Scrap.cvt as text (1 TAB 2 CR 3 TAB 3 CR, GEOS's format); Paste then used Calc's own clipboard (D2 =D1+E1: 3)"

S=$OUT/sd
cp "$T7/Veranda Calc.cvt" "$S/card/AUTOSTART.cvt"
cp "$SDK/bin/VERANDA.SYS" "$S/card/"
cp "$OUT/scrap.cvt" "$S/card/VERANDA/.SYSTEM/Text  Scrap.cvt"
$PY "$SDK/tools/vcard.py" make --mb 64 "$S/sd.img" "$S/card" >/dev/null || fail "vcard make"
cp "$SDK/bin/VERANDA.PRG" "$S/"
(cd "$S" && SDL_VIDEODRIVER=dummy timeout -s INT 200 "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG \
	-run -warp -sound none -echo -dump V > out/console.txt 2>&1) || true
$PY "$SDK/tools/vcard.py" get "$S/sd.img" "VERANDA/.SYSTEM/Text  Scrap.cvt" "$S/out/scrap.cvt" >/dev/null || fail "SD card: no Text Scrap"
check "$S/out/console.txt" "$S/out/scrap.cvt"
ok "the same on an SD card image"
echo "screenshot: $OUT/screen.png"
echo "GATE veranda7: PASS"
