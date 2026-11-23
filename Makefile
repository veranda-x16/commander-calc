# x16sheet — dual-target build.
#
#   make            build the X16 program and its overlays
#   make test       build and run the host unit tests (fast, every change)
#   make run        launch in the emulator
#   make debug      launch in the emulator with the visual debugger
#   make echo       headless run, KERNAL output to stdout (for scripts/CI)
#   make release    build the distribution zip (program, overlays, examples,
#                   manual) into build/dist
#   make veranda   Veranda Calc: build/veranda/Veranda Calc.cvt, for Veranda
#                  (SDK= its installed SDK; see the Veranda section below)
#   make clean
#
# Sources are split by directory and by filename suffix:
# Suffix decides the target, not directory: *_x16.c / *_x16.s are X16-only,
# *_host.c is host-only, everything else builds for both. src/ui is portable
# too — the grid's layout arithmetic is tested against a buffer backend.

# ---- toolchain ------------------------------------------------------
# cc65 was built from source into ~/.local. A non-default prefix must have
# CC65_HOME set or .macpack/.include lookups fail.
CC65_HOME ?= $(HOME)/.local/share/cc65
export CC65_HOME

CL65 ?= $(shell command -v cl65 2>/dev/null || echo $(HOME)/.local/bin/cl65)
EMU  ?= $(HOME)/Projects/x16-emulator/build/x16emu
ROM  ?= $(HOME)/Projects/x16-rom/build/x16/rom.bin

CC   ?= gcc

NAME    := CMDRCALC.PRG
BUILD   := build
OBJDIR  := $(BUILD)/obj
HOSTDIR := $(BUILD)/host

# ---- flags ----------------------------------------------------------
# --cpu 65c02 excludes Rockwell/65816 opcodes, per the project convention.
# How many overlays there are. MUST match OVL_COUNT in src/x16sheet.h.
#
# It was a hand-written list of numbers, in two places, and adding a
# fifteenth overlay did not add it to either -- so ld65 wrote the image and
# nothing turned it into the OVL15.BIN the loader asks for. It linked, it
# passed every test, and it would have failed on the machine the first time
# a chart was drawn.
OVL_N := 19

# One version number for the whole build: the About box shows it and
# `make release` names the archive with it, so they cannot disagree.
# Defined here rather than beside the release rules because the compiler
# flags below need it. `?=` there leaves an override on the command line
# working: make release VERSION=1.1
VERSION ?= 1.1

X16FLAGS := -t cx16 --cpu 65c02 -Osir -DX16S_XLSX_UI \
            -DCC_VERSION='"$(VERSION)"'

# --codesize is cc65's size/speed dial: below 100 it calls runtime helpers
# rather than expanding them inline -- smaller code, an extra jsr on the
# operations it touches.
#
# Its effect is per file and not monotonic: most of the program gets bigger
# at 90, and only the workbook layer, dense handle arithmetic against banked
# RAM, comes out smaller. Check the size of any file moved in or out of a
# group, on a clean build -- a partial one links stale objects.
#
# src/formula and src/util/number stay at the default: they are the slowest
# code in the program and their cycles are worth more than their bytes.
CODESIZE_DIRS := src/workbook/% src/platform/banked_ram.c
SMALLFLAGS := $(X16FLAGS) --codesize 90

# Pick the flags for one source file.
# Files that only fit their overlay at codesize 100, which -Osir on its own
# does not give them. The dial runs the other way here than for
# src/workbook: lower is bigger.
#
# workbook.c is in CODESIZE_DIRS too; this group is tested first, so 100
# applies to it.
TINY_DIRS := src/ui/filedlg.c src/platform/filedir_x16.c \
             src/ui/grid.c src/util/errors.c src/workbook/sheet_admin.c \
             src/workbook/workbook.c src/import/ods_scan.c \
             src/import/xml_ods_x16.c src/import/blob_ods_x16.c \
             src/import/xlsx_sheet.c src/import/xml_sheet_x16.c \
             src/import/blob_sheet_x16.c src/import/xlsx_styles.c
TINYFLAGS := $(X16FLAGS) --codesize 100

# menu.c on its own: it decides whether OVL14 fits, so its setting is swept
# separately. Override to measure:
#   make x16 MENU_CODESIZE=90
MENU_CODESIZE ?= 100
MENUFLAGS := $(X16FLAGS) --codesize $(MENU_CODESIZE)

ccflags = $(if $(filter src/ui/menu.c,$1),$(MENUFLAGS),\
            $(if $(filter $(TINY_DIRS),$1),$(TINYFLAGS),\
              $(if $(filter $(CODESIZE_DIRS),$1),$(SMALLFLAGS),$(X16FLAGS))))

LDFLAGS  := -t cx16 -C cfg/x16sheet.cfg -m $(BUILD)/x16sheet.map \
            -Ln $(BUILD)/labels.txt

HOSTFLAGS := -std=c99 -Wall -Wextra -Wno-unused-parameter -g -O1 -DX16S_HOST -DX16S_XLSX_UI \
             -DCC_VERSION='"$(VERSION)"' \
             -DX16S_POOL_VERIFY
HOSTLIBS  := -lm

# ---- sources --------------------------------------------------------
# Suffix decides the target: *_x16.c/*_x16.s are X16-only, *_host.c is
# host-only, everything else in these directories builds for both.
ALL_C := $(wildcard src/util/*.c) $(wildcard src/workbook/*.c) \
         $(wildcard src/formula/*.c) $(wildcard src/import/*.c) \
         $(wildcard src/export/*.c) \
         $(wildcard src/platform/*.c) $(wildcard src/ui/*.c)
ALL_S := $(wildcard src/util/*.s) $(wildcard src/platform/*.s)

# file_io_csv_x16.c #includes file_io_x16.c to place a second, renamed copy
# in the CSV overlay. Both are compiled; neither includes the other's names.

# zipw.c and cells_iter.c are #included by the modules that need them and
# are all static; neither is ever compiled on its own, on either target.
PORTABLE := $(filter-out %_x16.c %_host.c %_veranda.c src/export/zipw.c \
                         src/workbook/cells_iter.c \
                         src/workbook/sheet_new.c,$(ALL_C))

# blob.c is compiled into each overlay that needs it (blob_*_x16.c include
# it), never resident: see src/import/blob_ovl.h.
# blob.c has no segment of its own and would land resident, so only the
# per-overlay copies compile it. xml.c already names OVERLAY7, so it builds
# normally for OVL_XLSX and the renamed copies handle the other two.
# xlsx_import.c is the resident XLSX driver — the loop only, ~200 bytes; the
# work is in the xlsx_step_*.c files, which name their own overlays.
# xlsx_export.c is #included by src/export/xlsx_out{1,2}_x16.c, which
# compile it into their two overlays with XW_PART set; it is not a
# translation unit of its own on the X16. On the host there are no overlays
# and one copy holds both halves.
X16_PORTABLE := $(filter-out src/import/blob.c src/export/xlsx_export.c,\
                             $(PORTABLE))
X16_C    := $(X16_PORTABLE) $(filter %_x16.c,$(ALL_C)) src/main.c
X16_S    := $(filter %_x16.s,$(ALL_S))

HOST_C   := $(PORTABLE) $(filter %_host.c,$(ALL_C)) $(wildcard tests/*.c)

# Assembly objects get a distinct suffix: bankmem_x16.c and bankmem_x16.s
# would otherwise both want to be bankmem_x16.o.
X16_OBJ  := $(patsubst %.c,$(OBJDIR)/%.o,$(X16_C)) \
            $(patsubst %.s,$(OBJDIR)/%.s.o,$(X16_S))
HOST_OBJ := $(patsubst %.c,$(HOSTDIR)/%.o,$(HOST_C))

# Every object depends on every header. Coarse, but a full rebuild takes a
# few seconds and the alternative is silently linking stale objects — which
# is exactly how an edit to charmap.h once appeared to have no effect.
# file_io_x16.c is #included by the per-overlay copies (file_io_*_x16.c), so
# it is a prerequisite of them exactly like a header. Leaving it out means an
# edit to the file layer silently fails to reach any overlay's copy.
HEADERS := $(wildcard src/*.h) $(wildcard src/*/*.h) $(wildcard tests/*.h) \
           src/platform/file_io_x16.c src/platform/file_io_veranda.c src/import/blob.c src/import/xml.c \
           src/util/crc32.c src/export/xlsx_export.c src/export/zipw.c \
           src/workbook/cells_iter.c \
           src/workbook/cells_iter.c src/workbook/sheet_new.c
$(X16_OBJ) $(HOST_OBJ): $(HEADERS) Makefile

# ---- X16 ------------------------------------------------------------
.PHONY: all
all: x16

.PHONY: x16
x16: $(BUILD)/$(NAME)

$(BUILD)/$(NAME): $(X16_OBJ) cfg/x16sheet.cfg
	@mkdir -p $(BUILD)
	@# The link's warnings go through a sieve, and only this one.
	@#
	@# ld65 reports an "address size mismatch" for every zero-page
	@# variable another module reads -- 47 of them. All expected and all
	@# correct: a zero-page address IS a valid absolute address, and cc65
	@# emits .import rather than .importzp for an extern with no way in C
	@# to say otherwise (tested; see src/x16sheet.h). The count grows
	@# every time something moves to zero page, which is the technique
	@# holding this program together.
	@#
	@# They were burying the warnings that matter -- a signed-overflow
	@# warning caught a real bug in the chart clear loop and would not
	@# have been noticed in that pile. So they are COUNTED, not hidden,
	@# and every other line still comes through untouched.
	@echo "  ld65 -o $@"
	@$(CL65) $(LDFLAGS) -o $@ $(X16_OBJ) 2>$(BUILD)/link.log; st=$$?; \
	  grep -v 'Address size mismatch' $(BUILD)/link.log || true; \
	  n=$$(grep -c 'Address size mismatch' $(BUILD)/link.log || true); \
	  [ "$$n" -eq 0 ] || echo "  note: $$n zero-page cross-module reads, all expected (src/x16sheet.h)"; \
	  exit $$st
	@# ld65 writes overlay areas as raw images. KERNAL LOAD needs the
	@# 2-byte little-endian load address ($8000) in front of each. Keep
	@# this in step with __OVLSTART__ in cfg/x16sheet.cfg — an overlay
	@# loaded to the wrong address runs as whatever the bytes there mean.
	@for n in $$(seq 1 $(OVL_N)); do \
	    if [ -s $@.$$n ]; then \
	        printf '\000\200' | cat - $@.$$n > $(BUILD)/OVL$$n.BIN; \
	        rm -f $@.$$n; \
	    fi; \
	done
	@python3 tools/check_overlays.py $(OBJDIR) || \
	    { echo "  (an overlay calling another hangs the machine)"; exit 1; }
	@echo "--- size ---"
	@ls -l $(BUILD)/$(NAME) $(BUILD)/OVL*.BIN 2>/dev/null | awk '{printf "  %-22s %6d\n", $$9, $$5}'

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CL65) $(call ccflags,$<) -c -o $@ $<

$(OBJDIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(CL65) $(call ccflags,$<) -c -o $@ $<

# ---- host tests -----------------------------------------------------
.PHONY: host
host: $(HOSTDIR)/run_tests

$(HOSTDIR)/run_tests: $(HOST_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -o $@ $(HOST_OBJ) $(HOSTLIBS)

$(HOSTDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -c -o $@ $<

.PHONY: test
test: $(HOSTDIR)/run_tests
	@mkdir -p $(HOSTDIR)/sd
	@python3 tools/make_zip_fixtures.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_xlsx_fixtures.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_ods_fixtures.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_sheets_xlsx.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_demo_xlsx.py $(HOSTDIR)/sd > /dev/null
	@# The shipped .X16S examples, so the tests open the same files the
	@# release does. An example whose formulas this build cannot compute is
	@# a broken example, and nothing else would notice.
	@# The shipped .xlsx examples too: BUDGET.XLSX is the only
	@# fixture with a bold font in its styles.xml.
	@python3 tools/make_examples.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_x16s.py $(HOSTDIR)/sd > /dev/null
	@$(HOSTDIR)/run_tests

# ---- on-target XLSX import test --------------------------------------
#
#   make xlsx                  run the pipeline in the emulator on DEMO.XLSX
#   make xlsx FILE=BOOK.XLSX   ...on another fixture
#
# A separate program with its own link, not a mode of x16sheet: the resident
# budget in the application belongs to the user interface, and a harness
# would not fit beside it. Its own link also means the importer is exercised
# with nothing else in the way.
#
# The host suite proves the parsing. This proves the parts only the machine
# has: that the overlay swaps happen in the right order, that the blobs live
# in banked RAM across bank boundaries, and that the ZIP reader really seeks
# on CMDR-DOS.
XLSXDIR := $(BUILD)/xlsxtest

# Everything except the user interface and the application's own main.
#
# fref.c goes with them: it rewrites the references Insert Row and Sort
# move, so it reads the grid's cursor, clip rows and sort undo. Those are
# in src/ui, which this harness leaves out, and the importer never calls
# it.
XT_C   := $(X16_PORTABLE) $(filter %_x16.c,$(filter-out src/ui/%,$(ALL_C))) \
          src/tools/xlsxtest.c
XT_C   := $(filter-out src/ui/% src/formula/fref.c,$(XT_C))
XT_OBJ := $(patsubst %.c,$(XLSXDIR)/%.o,$(XT_C)) \
          $(patsubst %.s,$(XLSXDIR)/%.s.o,$(X16_S))

$(XT_OBJ): $(HEADERS) Makefile

# The application's per-file flags, not just X16FLAGS: the overlays this
# links are the same ones and only fit at the same settings.
$(XLSXDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CL65) $(call ccflags,$<) -DX16S_POOL_VERIFY -c -o $@ $<

$(XLSXDIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(CL65) $(X16FLAGS) -DX16S_POOL_VERIFY -c -o $@ $<

$(XLSXDIR)/XLSXTEST.PRG: $(XT_OBJ) cfg/x16sheet.cfg
	@mkdir -p $(XLSXDIR)
	$(CL65) -t cx16 -C cfg/x16sheet.cfg -m $(XLSXDIR)/map.txt \
	    -o $@ $(XT_OBJ)
	@for n in $$(seq 1 $(OVL_N)); do \
	    if [ -s $@.$$n ]; then \
	        printf '\000\200' | cat - $@.$$n > $(XLSXDIR)/OVL$$n.BIN; \
	        rm -f $@.$$n; \
	    fi; \
	done

.PHONY: xlsx
xlsx: $(XLSXDIR)/XLSXTEST.PRG
	@mkdir -p $(HOSTDIR)/sd
	@python3 tools/make_demo_xlsx.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_xlsx_fixtures.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_ods_fixtures.py $(HOSTDIR)/sd > /dev/null
	@python3 tools/make_sheets_xlsx.py $(HOSTDIR)/sd > /dev/null
	@# The harness also opens RAIN.X16S and NASDAQ.XLSX. Without these it
	@# reports them FAILED, which reads as a broken importer rather than a
	@# missing fixture. make_nasdaq needs the network; the rest do not.
	@python3 tools/make_x16s.py $(XLSXDIR) >/dev/null 2>&1 || true
	@python3 tools/make_nasdaq.py $(XLSXDIR) >/dev/null 2>&1 || \
	    echo "  no network: NASDAQ.XLSX absent, the harness will say FAILED"
	@rm -f $(XLSXDIR)/ndq_daily.json
	@cp $(HOSTDIR)/sd/*.XLSX $(HOSTDIR)/sd/*.ODS $(XLSXDIR)/
	@# A lower-case directory name, which HostFS matches exactly and
	@# to_dosname() upper-cases: the harness imports from here to check
	@# that file_open_read() falls back to the name as given.
	@mkdir -p $(XLSXDIR)/lowerdir
	@cp $(HOSTDIR)/sd/RICH.ODS $(XLSXDIR)/lowerdir/
	@python3 tools/check_overlays.py $(XLSXDIR) || exit 1
	@echo "--- running $(if $(FILE),$(FILE),DEMO.XLSX) in the emulator ---"
	@# 8 MHz deliberately: this is the test that says the importer works on
	@# the hardware, so it runs at the hardware's speed. -warp removes the
	@# frame-rate cap without touching the emulated clock.
	@timeout 180 stdbuf -o0 $(EMU) -rom $(ROM) -fsroot $(XLSXDIR) \
	    -prg $(XLSXDIR)/XLSXTEST.PRG -run -warp -mhz 8 -echo 2>/dev/null \
	    | sed -n '/xlsx import test/,/DONE/p'

# ---- inspect any .xlsx with the real pipeline ------------------------
# The fixtures only prove the importer agrees with the fixture generator.
# This runs the same code against a workbook nobody wrote for us:
#     make inspect FILE=/path/to/whatever.xlsx
INSPECT_OBJ := $(filter-out $(HOSTDIR)/tests/%,$(HOST_OBJ))

$(HOSTDIR)/inspect_xlsx: tools/inspect_xlsx.c $(INSPECT_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -o $@ $< $(INSPECT_OBJ) $(HOSTLIBS)

.PHONY: xlsxdemo
# Regenerate the sample .xlsx this program writes, for opening in a real
# spreadsheet. The host suite produces it as a side effect of the export
# round-trip test, which is also what proves it is worth looking at.
xlsxdemo: test
	@cp $(HOSTDIR)/sd/OUT.XLSX CMDRCALC-EXPORT.XLSX
	@python3 tools/check_xlsx.py CMDRCALC-EXPORT.XLSX
	@echo "wrote CMDRCALC-EXPORT.XLSX"

.PHONY: checkovl
# Nothing in one overlay may call into another; see tools/check_overlays.py.
# Run as part of `make x16`, because the failure mode is a hang with no
# diagnostic rather than anything the linker objects to.
checkovl:
	@python3 tools/check_overlays.py $(OBJDIR)

.PHONY: checkxlsx
checkxlsx:
	@python3 tools/check_xlsx.py $(if $(FILE),$(FILE),$(HOSTDIR)/sd/OUT.XLSX)

.PHONY: inspect
inspect: $(HOSTDIR)/inspect_xlsx
	@$(HOSTDIR)/inspect_xlsx $(FILE)

# ---- Veranda -------------------------------------------------------
# Veranda Calc: this program on Veranda, the GEOS-style desktop for the
# X16 (its SDK: SDK=, API level 2). The engine and the grid are the X16
# build's files; what touches the machine is *_veranda.c/.s:
#   screen   PutCards and the colour map on the 640x480 bitmap
#   keys     keyVector (main_veranda.c), the ROM's keys in modal loops
#   overlays each in a RAM bank of its own, run in place (cfg/veranda.cfg)
#   heap     the banks BankAlloc gives; files: Veranda's streams
#   menus    Veranda's (src/ui/vmenu_veranda.c), dialog boxes for the
#            questions (src/ui/vdlg_veranda.h), the pointer and the wheel
#   look     Veranda Look 2 (src/platform/look_veranda.c): its palette, the
#            kernel's menu bar, the title band and status bar, the SDK's
#            Open and Save dialog (overlay 22), its own tile
#            (cfg/veranda-tile.txt); API level 5 (long names)
#
#   make veranda        build/veranda/Veranda Calc.cvt ("packed": its stamp)
#   make veranda-test   build/veranda/test/: with the keys' session
#                       (src/tools/veranda_play.c); tools/gate_veranda.sh
#   make veranda-test2  build/veranda/test2/: with the C2 session (the
#                       pointer, menus, dialog boxes: src/tools/veranda_play2.*,
#                       tools/veranda_script.py); tools/gate_veranda2.sh
#   make veranda-test3  build/veranda/test3/: with the C3 session (the
#                       files); tools/gate_veranda3.sh
#   make veranda-test4  build/veranda/test4/: with the C4 session (charts,
#                       borders); tools/gate_veranda4.sh
#   make veranda-test5  build/veranda/test5/: with the C5 session (opened
#                       from Veranda's shell); tools/gate_veranda5.sh
#   make veranda-test7  build/veranda/test7/: Veranda's clipboard (the
#                       Text Scrap: src/platform/clip_veranda.c);
#                       tools/gate_veranda7.sh
#   make veranda-test8  build/veranda/test8/: the file chooser on a card
#                       laid out as a user's (tools/veranda_card.py);
#                       tools/gate_veranda3.sh, part 2
#   make veranda-test6  build/veranda/test6/: with the C6 session (the
#                       redraw's timings); tools/gate_veranda6.sh
SDK    ?= $(HOME)/Projects/geos/build/sdk
VRDIR  := $(BUILD)/veranda
VRFLAGS = -t none --cpu 65c02 -Osir -DX16S_XLSX_UI -DX16S_VERANDA -DX16S_LOOK2 \
          -DCC_VERSION='"$(VERSION)"' -I $(SDK)/include -I $(VRDIR)/gen --asm-include-dir $(SDK)/include \
          --asm-define X16S_VERANDA
vrflags = $(if $(filter src/ui/menu.c,$1),$(VRFLAGS) --codesize $(MENU_CODESIZE),\
            $(if $(filter $(TINY_DIRS),$1),$(VRFLAGS) --codesize 100,\
              $(if $(filter $(CODESIZE_DIRS),$1),$(VRFLAGS) --codesize 90,$(VRFLAGS))))

VR_NOT_X16 := src/platform/screen_x16.c src/platform/overlay_x16.c src/platform/keyboard_x16.c \
              src/platform/bankmem_x16.c src/platform/filedir_x16.c \
              src/platform/keyboard_x16.c
VR_C := $(filter-out src/ui/chart_bmp.c,$(X16_PORTABLE)) \
        $(filter-out $(VR_NOT_X16),$(filter %_x16.c,$(ALL_C))) \
        $(filter-out src/platform/file_io_veranda.c,$(filter %_veranda.c,$(ALL_C))) src/main_veranda.c
VR_S := src/util/number_x16.s src/util/crc32_x16.s src/platform/stackfloor_x16.s \
        $(filter %_veranda.s,$(ALL_S))
# Veranda's clipboard (src/platform/clip_veranda.c): the SDK's scrap
# library compiled into overlay 21 (OVL_CLIP), not linked from veranda.lib
# into the resident image
VR_SCRAP := $(VRDIR)/sdk/scrapio.o $(VRDIR)/sdk/scraptbeg.o
# Veranda Look 2: the SDK's Open and Save dialog (src/vrchoose.c) in overlay
# 22, a bank of its own (it fills it: compiled -Os), and its window chrome
# (src/vrwin.c) in overlay 21; their BSS in cfg/veranda.cfg's VRFB and VRWB
# (screen_font's and scr_line's: platform/screen_veranda.c)
VR_SDKSRC := $(VRDIR)/sdk/vrchoose.o $(VRDIR)/sdk/vrwin.o
VR_OBJ := $(patsubst %.c,$(VRDIR)/%.o,$(VR_C)) $(patsubst %.s,$(VRDIR)/%.s.o,$(VR_S)) $(VR_SCRAP) $(VR_SDKSRC)
VR_TOBJ := $(patsubst $(VRDIR)/src/main_veranda.o,$(VRDIR)/test/main_veranda.o,$(VR_OBJ)) \
           $(VRDIR)/test/veranda_play.o
VR_T2OBJ := $(patsubst $(VRDIR)/src/main_veranda.o,$(VRDIR)/test2/main_veranda.o,$(VR_OBJ)) \
            $(VRDIR)/test2/veranda_play2.o $(VRDIR)/test2/veranda_play2.s.o
VR_T3OBJ := $(filter-out $(VRDIR)/test2/veranda_play2.o,$(VR_T2OBJ)) $(VRDIR)/test3/veranda_play2.o
VR_T4OBJ := $(filter-out $(VRDIR)/test2/veranda_play2.o,$(VR_T2OBJ)) $(VRDIR)/test4/veranda_play2.o
VR_T5OBJ := $(filter-out $(VRDIR)/test2/veranda_play2.o,$(VR_T2OBJ)) $(VRDIR)/test5/veranda_play2.o
VR_T6OBJ := $(filter-out $(VRDIR)/test2/veranda_play2.o,$(VR_T2OBJ)) $(VRDIR)/test6/veranda_play2.o
VR_T7OBJ := $(filter-out $(VRDIR)/test2/veranda_play2.o,$(VR_T2OBJ)) $(VRDIR)/test7/veranda_play2.o
VR_T8OBJ := $(filter-out $(VRDIR)/test2/veranda_play2.o,$(VR_T2OBJ)) $(VRDIR)/test8/veranda_play2.o
VRNAME := Veranda Calc
VR_OVL_N := 22                          # OVL_N, the test builds' player (OVL_PLAY), OVL_CLIP, OVL_VRFILE

$(VR_SCRAP): $(VRDIR)/sdk/%.o: $(SDK)/src/%.c $(SDK)/include/scrap.h
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) --code-name OVERLAY21 --rodata-name OVL21RO --bss-name OVL21BSS \
	    -DSCRAP_PATH=fio_bounce -c -o $@ $<

$(VRDIR)/sdk/vrchoose.o: $(SDK)/src/vrchoose.c $(SDK)/include/vrchoose.h
	@mkdir -p $(dir $@)
	$(CL65) -t none --cpu 65c02 -Os -I $(SDK)/include --code-name OVERLAY22 --rodata-name OVL22RO \
	    --bss-name VRFBSS -c -o $@ $<

$(VRDIR)/sdk/vrwin.o: $(SDK)/src/vrwin.c $(SDK)/include/vrwin.h
	@mkdir -p $(dir $@)
	$(CL65) -t none --cpu 65c02 -Os -I $(SDK)/include --code-name OVERLAY21 --rodata-name OVL21RO \
	    --bss-name VRWBSS -c -o $@ $<

# the kernel's jump table as C constants (src/ui/vcall_veranda.h): an
# overlay calls the kernel in place, not through veranda.lib's bindings,
# which would be linked into the resident image
$(VRDIR)/gen/vaddr.h: $(SDK)/include/veranda.inc
	@mkdir -p $(dir $@)
	sed -n 's/^\([A-Za-z][A-Za-z0-9_]*\) *= *\$$\([0-9A-F]\{4\}\) *;.*/#define VA_\1 0x\2/p' $< > $@

VR_ALLOBJ := $(VR_OBJ) $(VR_TOBJ) $(VR_T2OBJ) $(VR_T3OBJ) $(VR_T4OBJ) $(VR_T5OBJ) $(VR_T6OBJ) $(VR_T7OBJ) $(VR_T8OBJ)
$(VR_ALLOBJ): $(VRDIR)/gen/vaddr.h
$(VR_ALLOBJ): $(HEADERS) Makefile tools/veranda_keys.h \
    tools/veranda_session2.h tools/veranda_session3.h tools/veranda_session4.h \
    tools/veranda_session5.h tools/veranda_session6.h

$(VRDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CL65) $(call vrflags,$<) -c -o $@ $<

$(VRDIR)/%.s.o: %.s
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -c -o $@ $<

$(VRDIR)/test/main_veranda.o: src/main_veranda.c
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY -c -o $@ $<

$(VRDIR)/test/veranda_play.o: src/tools/veranda_play.c
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY -c -o $@ $<

# the C2 session (tools/gate_veranda2.sh); PLAY_STOP=n: stop at its stop point n
PLAY_STOP ?=
VR_STOPFLAG = $(if $(PLAY_STOP),-DPLAY_STOP=$(PLAY_STOP))
$(VRDIR)/test2/main_veranda.o: src/main_veranda.c
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -c -o $@ $<

$(VRDIR)/test2/veranda_play2.o: src/tools/veranda_play2.c FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test3/veranda_play2.o: src/tools/veranda_play2.c FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -DPLAY_SESSION=3 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test4/veranda_play2.o: src/tools/veranda_play2.c FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -DPLAY_SESSION=4 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test5/veranda_play2.o: src/tools/veranda_play2.c FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -DPLAY_SESSION=5 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test7/veranda_play2.o: src/tools/veranda_play2.c tools/veranda_session7.h FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -DPLAY_SESSION=7 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test8/veranda_play2.o: src/tools/veranda_play2.c tools/veranda_session8.h FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -DPLAY_SESSION=8 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test6/veranda_play2.o: src/tools/veranda_play2.c FORCE_STOP
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -DCALC_PLAY2 -DPLAY_SESSION=6 $(VR_STOPFLAG) -c -o $@ $<

$(VRDIR)/test2/veranda_play2.s.o: src/tools/veranda_play2.s
	@mkdir -p $(dir $@)
	$(CL65) $(VRFLAGS) -c -o $@ $<

tools/veranda_session2.h: tools/veranda_script.py
	python3 -I -B tools/veranda_script.py c2 $@

tools/veranda_session3.h: tools/veranda_script.py
	python3 -I -B tools/veranda_script.py c3 $@

tools/veranda_session4.h: tools/veranda_script.py
	python3 -I -B tools/veranda_script.py c4 $@

tools/veranda_session5.h: tools/veranda_script.py
	python3 -I -B tools/veranda_script.py c5 $@

tools/veranda_session6.h: tools/veranda_script.py
	python3 -I -B tools/veranda_script.py c6 $@

tools/veranda_session8.h: tools/veranda_script.py tools/veranda_card.py
	python3 -I -B tools/veranda_script.py c8 $@

.PHONY: FORCE_STOP
FORCE_STOP:

# link $(1) objects into $(2)/CALC.BIN (+ .1 .. .19), pack $(2)/$(VRNAME).cvt
define vr_link
	@mkdir -p $(2)
	@echo "  ld65 -o $(2)/CALC.BIN"
	@$(CL65) -t none -C cfg/veranda.cfg -m $(2)/veranda.map -Ln $(2)/labels.txt \
	    -o $(2)/CALC.BIN $(1) $(SDK)/lib/veranda.lib 2>$(2)/link.log; st=$$?; \
	  grep -v 'Address size mismatch' $(2)/link.log || true; exit $$st
	python3 -I -B $(SDK)/tools/vpack.py --load 0x0801 -o "$(2)/$(VRNAME).cvt" cfg/veranda.hdr \
	    $(2)/CALC.BIN $(foreach n,$(shell seq 1 $(VR_OVL_N)),$(2)/CALC.BIN.$(n))
	@touch $(2)/packed
	@echo "--- size ---"
	@ls -l $(2)/CALC.BIN $(2)/CALC.BIN.* | awk '{printf "  %-34s %6d\n", $$9, $$5}'
endef

.PHONY: veranda veranda-test veranda-test2 veranda-test3 veranda-test4 veranda-test5 veranda-test6 veranda-test7 veranda-test8
veranda-test8: $(VR_T8OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T8OBJ),$(VRDIR)/test8)
veranda-test7: $(VR_T7OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T7OBJ),$(VRDIR)/test7)
veranda-test6: $(VR_T6OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T6OBJ),$(VRDIR)/test6)
veranda-test5: $(VR_T5OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T5OBJ),$(VRDIR)/test5)
veranda-test4: $(VR_T4OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T4OBJ),$(VRDIR)/test4)
veranda-test2: $(VR_T2OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T2OBJ),$(VRDIR)/test2)
veranda-test3: $(VR_T3OBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_T3OBJ),$(VRDIR)/test3)
veranda: $(VRDIR)/packed
$(VRDIR)/packed: $(VR_OBJ) cfg/veranda.cfg cfg/veranda.hdr cfg/veranda-tile.txt
	$(call vr_link,$(VR_OBJ),$(VRDIR))

veranda-test: $(VRDIR)/test/packed
$(VRDIR)/test/packed: $(VR_TOBJ) cfg/veranda.cfg cfg/veranda.hdr
	$(call vr_link,$(VR_TOBJ),$(VRDIR)/test)

# The goldens' host objects: the host build's, the grid's two files built
# again with -DX16S_LOOK2 (Veranda Look 2's colours: a selected block and
# the cursor's headers in the selection colour, the formula bar's field,
# the sheets' tabs; src/ui/grid_priv.h, grid.c)
VR_LOOK2_SRC := src/ui/grid.c src/ui/grid_rows.c
VR_LOOK2_OBJ := $(patsubst %.c,$(HOSTDIR)/look2/%.o,$(VR_LOOK2_SRC))
VR_GOLDEN_OBJ := $(filter-out $(patsubst %.c,$(HOSTDIR)/%.o,$(VR_LOOK2_SRC)),$(INSPECT_OBJ)) $(VR_LOOK2_OBJ)
$(HOSTDIR)/look2/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -DX16S_LOOK2 -c -o $@ $<

# the golden for the session: the host build's text grid after the same keys
$(HOSTDIR)/veranda_golden: tools/veranda_golden.c tools/veranda_keys.h $(VR_GOLDEN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -o $@ $< $(VR_GOLDEN_OBJ) $(HOSTLIBS)

# the goldens for the C2 and C3 sessions (tools/gate_veranda2.sh, 3.sh)
$(HOSTDIR)/veranda_golden2: tools/veranda_golden2.c tools/veranda_session2.h $(VR_GOLDEN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -o $@ $< $(VR_GOLDEN_OBJ) $(HOSTLIBS)

$(HOSTDIR)/veranda_golden3: tools/veranda_golden2.c tools/veranda_session3.h $(VR_GOLDEN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -DPLAY_SESSION=3 -o $@ $< $(VR_GOLDEN_OBJ) $(HOSTLIBS)

$(HOSTDIR)/veranda_golden4: tools/veranda_golden2.c tools/veranda_session4.h $(VR_GOLDEN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -DPLAY_SESSION=4 -o $@ $< $(VR_GOLDEN_OBJ) $(HOSTLIBS)

$(HOSTDIR)/veranda_golden5: tools/veranda_golden2.c tools/veranda_session5.h $(VR_GOLDEN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -DPLAY_SESSION=5 -o $@ $< $(VR_GOLDEN_OBJ) $(HOSTLIBS)

$(HOSTDIR)/veranda_golden6: tools/veranda_golden2.c tools/veranda_session6.h $(VR_GOLDEN_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(HOSTFLAGS) -DPLAY_SESSION=6 -o $@ $< $(VR_GOLDEN_OBJ) $(HOSTLIBS)

# ---- emulator -------------------------------------------------------
# -fsroot serves build/ over HostFS, so the program finds its overlays.
#
# MHZ overclocks the emulated CPU. A real Commander X16 is 8 MHz and nothing
# else, so this is a development convenience — inflating a worksheet is the
# slowest thing the program does and 16 MHz halves the wait while working on
# it. Anything being judged on how it will behave on the machine has to run
# at 8: `make run MHZ=8`, and the on-target harness below is pinned there.
MHZ := 8
# 2 MB of banked RAM, the most the emulator offers. The workbook heap is
# banked RAM and nothing else competes for it, so this is the difference
# between a few thousand cells and a few hundred thousand. The machine
# ships with 512 KB; anything being judged on how it will behave on the
# stock hardware wants `make run RAM=512`.
RAM := 2048
EMUFLAGS := -rom $(ROM) -fsroot $(BUILD) -prg $(BUILD)/$(NAME) -run \
            -mhz $(MHZ) -ram $(RAM)

# Workbooks to open on the machine, as opposed to fixtures to test against.
# Written into $(BUILD), which is what -fsroot serves, so `make examples run`
# puts them in front of F7 with no copying about.
.PHONY: examples
# Two sets, and they are not the same thing. make_examples writes .xlsx --
# workbooks to IMPORT, which is what proves the importer. make_x16s writes
# the native format directly, without the machine, so the reader is tested
# against files it did not produce itself.
examples:
	@python3 tools/make_examples.py $(BUILD)
	@python3 tools/make_x16s.py $(BUILD) | sed 's/^/  /'

.PHONY: run
run: x16 examples
	$(EMU) $(EMUFLAGS)

# The same build on an emulated 65C816.
#
# Nothing here is compiled for the 816 -- the project convention is
# `--cpu 65c02` and it stays that way. The point is the opposite: the 816
# starts in emulation mode, where it runs 65C02 code, so this says whether
# the program still works on a machine fitted with one. Worth a run after
# touching hand-written assembly, which is where an opcode the two CPUs
# disagree about would come from.
.PHONY: runc816
runc816: x16 examples
	$(EMU) $(EMUFLAGS) -c816

.PHONY: debugc816
debugc816: x16
	$(EMU) $(EMUFLAGS) -c816 -debug

.PHONY: debug
debug: x16
	$(EMU) $(EMUFLAGS) -debug

.PHONY: echo
echo: x16
	timeout 30 $(EMU) $(EMUFLAGS) -warp -echo

# ---- the manual -----------------------------------------------------
# docs/manual has a Makefile of its own -- pdflatex three times plus
# makeindex, which is its own small world and does not belong in here.
# These just save you the cd.
MANUALDIR := docs/manual

.PHONY: manual
manual:
	@$(MAKE) --no-print-directory -C $(MANUALDIR)

# Build it and publish the result to docs/manual/pdf/, which is committed so
# the manual can be read without a TeX installation.
.PHONY: manualpdf
manualpdf:
	@$(MAKE) --no-print-directory -C $(MANUALDIR) pdf

.PHONY: manualview
manualview:
	@$(MAKE) --no-print-directory -C $(MANUALDIR) view

.PHONY: manualclean
manualclean:
	@$(MAKE) --no-print-directory -C $(MANUALDIR) clean

# ---- release --------------------------------------------------------
#
#   make release                 build build/dist/commander-calc-1.1.zip
#   make release VERSION=1.1     ...under another name
#
# Layout of the zip:
#
#   commander-calc-1.1/
#     README.TXT                  what to copy where, and how to start it
#     README.md                   the developer readme
#     commander-calc-manual.pdf   the reference manual
#     CMDRCALC/                   <-- copy THIS onto the card
#       CMDRCALC.PRG
#       OVL1.BIN ... OVL17.BIN
#       BUDGET.XLSX STOCK.XLSX TOTALS.XLSX FUNCS.XLSX
#
# The examples go IN with the program, not in an EXAMPLES/ directory beside
# it: filedlg.c skips e.is_dir and offers no way to change directory, so a
# workbook one level down cannot be opened from inside the program at all.
VERSION ?= 1.1

DISTDIR := $(BUILD)/dist
RELDIR  := $(DISTDIR)/commander-calc-$(VERSION)
CARDDIR := $(RELDIR)/CMDRCALC
RELZIP  := $(DISTDIR)/commander-calc-$(VERSION).zip

MANUAL_PDF_BUILT := $(MANUALDIR)/build/commander-calc-manual.pdf
MANUAL_PDF_KEPT  := $(MANUALDIR)/pdf/commander-calc-manual.pdf

# No '#' in here -- make eats it. Exported, and written out with printf,
# because $(file >...) in a recipe runs before the mkdir: make expands the
# whole recipe before the first line of it.
export RELEASE_README
define RELEASE_README
Commander Calc $(VERSION)
The Professional Spreadsheet for the Commander X16
https://github.com/sandlbn/commander-calc


WHAT IS IN HERE

  CMDRCALC/                  the program, ready to run
    CMDRCALC.PRG             the program itself
    OVL1.BIN .. OVL$(OVL_N).BIN    its overlays, every one of them required
    BUDGET.XLSX              money, percentages, and a total that adds up
    STOCK.XLSX               in no order at all: try Sheet > Sort up
    TOTALS.XLSX              three sheets, and Summary reads the other two
    FUNCS.XLSX               every function, beside what it should say
  commander-calc-manual.pdf  the reference manual: every command, every key
  README.md                  building it from source


INSTALLING

  Copy the CMDRCALC directory onto a CMDR-DOS-formatted SD card, or copy the
  files in it into a directory of your own choosing. CMDRCALC.PRG and all
  $(OVL_N) overlays must stay together in the one directory, on the one device:
  the program brings an overlay in whenever you open a file, write one, or
  draw a chart, and it looks for them beside itself.

  Then:

    LOAD"CMDRCALC.PRG",8
    RUN

  Under the emulator:

    x16emu -rom rom.bin -fsroot . -prg CMDRCALC.PRG -run


THE EXAMPLE WORKBOOKS

  They are in with the program on purpose. The file dialog lists files and
  not directories, so a workbook one level down cannot be opened from inside
  the program. Delete them once you have had a look; nothing depends on them.


WHAT IT NEEDS

  A Commander X16 with 512 KB of banked RAM -- 2 MB is worth having for large
  imports -- an 80-column display, and an SD card or host filesystem. A mouse
  if you like, but every command has keys.
endef

.PHONY: release
release: test x16
	@rm -rf $(RELDIR) $(RELZIP) $(RELZIP).sha256
	@mkdir -p $(CARDDIR)
	@cp $(BUILD)/$(NAME) $(CARDDIR)/
	@# One by one, not a glob: a missing overlay is a hang on the machine
	@# with no diagnostic, and this is the last place to catch it.
	@for n in $$(seq 1 $(OVL_N)); do \
	    cp $(BUILD)/OVL$$n.BIN $(CARDDIR)/ 2>/dev/null || \
	      { echo "release: $(BUILD)/OVL$$n.BIN missing -- try make clean x16"; \
	        exit 1; }; \
	done
	@python3 tools/make_examples.py $(CARDDIR) >/dev/null
	@python3 tools/make_x16s.py $(CARDDIR) >/dev/null
	@# make_nasdaq caches its download beside what it writes, and that is a
	@# megabyte of JSON -- more than half the archive, and of no use on the
	@# card. NDQNAT.X16S is what it was fetched for.
	@rm -f $(CARDDIR)/ndq_daily.json
	@# clean first: a stale .aux from an older source tree fails the pdflatex
	@# run outright ("Extra }, or forgotten \endgroup") instead of being
	@# overwritten. Falls back to the committed pdf/ copy with no TeX here.
	@if command -v pdflatex >/dev/null 2>&1; then \
	    $(MAKE) --no-print-directory -C $(MANUALDIR) clean >/dev/null && \
	    $(MAKE) --no-print-directory -C $(MANUALDIR) >/dev/null; \
	 fi
	@if [ -f $(MANUAL_PDF_BUILT) ]; then \
	    cp $(MANUAL_PDF_BUILT) $(RELDIR)/; \
	 elif [ -f $(MANUAL_PDF_KEPT) ]; then \
	    echo "  note: no TeX here, using the committed $(MANUAL_PDF_KEPT)"; \
	    cp $(MANUAL_PDF_KEPT) $(RELDIR)/; \
	 else \
	    echo "release: no manual PDF -- install TeX Live or run make manualpdf"; \
	    exit 1; \
	 fi
	@cp README.md $(RELDIR)/
	@printf '%s\n' "$$RELEASE_README" > $(RELDIR)/README.TXT
	@# -X leaves out the uid/gid of whoever built it. Not reproducible even
	@# so -- zip stores mtimes -- hence the .sha256 beside it.
	@cd $(DISTDIR) && zip -qXr commander-calc-$(VERSION).zip \
	    commander-calc-$(VERSION)
	@cd $(DISTDIR) && sha256sum commander-calc-$(VERSION).zip \
	    > commander-calc-$(VERSION).zip.sha256
	@echo "--- $(RELZIP) ---"
	@unzip -l $(RELZIP) | sed -n '4,$$p' | head -40
	@cut -c1-16 $(RELZIP).sha256 | sed 's/^/  sha256 /'

# Unpack the zip and check it holds what it should, before it goes out.
.PHONY: releasecheck
releasecheck: release
	@rm -rf $(DISTDIR)/verify && mkdir -p $(DISTDIR)/verify
	@cd $(DISTDIR)/verify && unzip -q ../commander-calc-$(VERSION).zip
	@v=$(DISTDIR)/verify/commander-calc-$(VERSION)/CMDRCALC; \
	 test -f $$v/$(NAME) || { echo "no $(NAME) in the zip"; exit 1; }; \
	 n=$$(ls $$v/OVL*.BIN 2>/dev/null | wc -l); \
	 [ "$$n" -eq $(OVL_N) ] || { echo "$$n overlays in the zip, want $(OVL_N)"; exit 1; }; \
	 for x in BUDGET STOCK TOTALS FUNCS; do \
	     python3 tools/check_xlsx.py $$v/$$x.XLSX >/dev/null || exit 1; \
	 done; \
	 test -f $(DISTDIR)/verify/commander-calc-$(VERSION)/commander-calc-manual.pdf \
	     || { echo "no manual in the zip"; exit 1; }
	@echo "  release checks out: $(NAME), $(OVL_N) overlays, 4 examples, manual"

# ---- housekeeping ---------------------------------------------------
.PHONY: clean
clean:
	rm -rf $(BUILD)

.PHONY: env
env:
	@printf '%-10s %s\n' CL65 "$(CL65)" CC65_HOME "$(CC65_HOME)" \
	    EMU "$(EMU)" ROM "$(ROM)"
	@$(CL65) --version 2>&1 | head -1
	@test -f $(ROM) && echo "rom.bin present" || echo "rom.bin MISSING"
