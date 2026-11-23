; veranda_play2.s -- the player of tools/gate_veranda2.sh's session (C2):
; the pointer and the keys, scripted (test build only: make veranda-test2).
;
; Veranda calls mouseSource from its interrupt every frame: this gives it
; the script's pointer (absolute, carry set) and puts the script's keys
; into the ROM's key queue, as a mouse and a keyboard would. The script
; (tools/veranda_script.py writes tools/veranda_session2.h) is 5-byte
; records in overlay 20's bank (OVL_PLAY) (src/tools/veranda_play.c), mapped here
; while it is read.
;
; Paced by the program, not by the clock: a frame counts only when it is
; idle -- Veranda's main loop has called appMain since the last frame, or
; a modal loop of the program's has found no key (calc_idle, set by
; main_veranda.c and keyboard_veranda.c), or appMain is 0, which is a
; dialog box's own loop. A recalculation or a redraw that takes 40 frames is
; then no shorter or longer to the script than one that takes 2. Each
; record waits for both key queues (the ROM's, Veranda's) to be empty. After each record GAP idle frames pass.
;
;   'P' xl xh yl yh   the pointer to (x, y)
;   'T' t             to the menu bar's title t: its middle (vm_buf's VM_X:
;                     each title's left edge / 2, the kernel's Veranda look,
;                     src/ui/vmenu_veranda.c)
;   'I' t i           to item i of title t's sub menu (under the title, from
;                     its left; 16 lines an item from line 16)
;   'D' / 'U'         the left button down / up
;   'R'               the right button down ('U' up)
;   'X' c             "KTIME c <cycles, hex>": the last command the program
;                     ran (play_mark0 .. play_mark1, around grid_key)
;   'K' k             a key (the ROM's code) into the queue
;   'W' n             n idle frames more
;   'H' n             the wheel: n notches (signed)
;   'S' n             stop point n: if it is play_stop, the dump and the end
;   'L' n             play_log = n: the program logs the cells (veranda_play2.c)
;   'E'               the end: play_log = $FF (the program logs and dumps)
;   'Z'               the clock: start (EMU_CLOCK, 8 MHz cycles)
;   'Y' c             "TIME c <cycles, hex>" on the console: from 'Z' to the
;                     last frame the program was busy since (one frame's
;                     grain, 16.7 ms): how long the command the 'Z' came
;                     before took, the dialog boxes it waited in left out
;
; A program busy for HUNG frames on end is hung: "HUNG" on the console,
; the dump, the end.

.include "veranda.inc"
.include "x16hw.inc"

.export _play_src, _play_log, _play_stop, _play_mark0, _play_mark1, _play_hpeak
.import _ovl_bank, _vm_buf, _play_script, _calc_idle
.import _first_bank, _bank_count, _bankmem_top   ; the heap (platform/banked_ram.h)
.importzp _cur_bank

GAP     = 4
HUNG    = 30000                 ; frames (500 s) busy without a pause: hung
VM_X    = 103                   ; vmenu_veranda.h (checked in veranda_play2.c)
KBDBUF_PEEK = $FEBD

.segment "DATA"

_play_log:   .byte 0
_play_stop:  .byte $FF
tmpa:   .byte 0
kt:     .res 4                  ; play_mark0 .. play_mark1

; everything else in overlay 20's bank (OVL_PLAY): the resident image has
; no room for it. RAM, so its variables are written in place.
.segment "OVERLAY20"

cur:    .addr _play_script
wait:   .byte 30                ; the first: let the program settle
px:     .word 320
py:     .word 240
pb:     .byte 0
rec:    .res 5
ended:  .byte 0
busy:   .word 0
moved:  .byte 1
t0:     .res 4                  ; 'Z' .. 'Y'
tlast:  .res 4
tnow:   .res 4
_play_hpeak: .byte 0            ; the most banks of the heap in use, sampled
                                ; every frame (veranda_play2.c logs it)

.segment "CODE"

; resident: the clock around a command (main_veranda.c's timed)
_play_mark0:
	lda EMU_CLOCK
	sta kt
	lda EMU_CLOCK+1
	sta kt+1
	lda EMU_CLOCK+2
	sta kt+2
	lda EMU_CLOCK+3
	sta kt+3
	rts
_play_mark1:
	lda EMU_CLOCK
	sec
	sbc kt
	sta kt
	lda EMU_CLOCK+1
	sbc kt+1
	sta kt+1
	lda EMU_CLOCK+2
	sbc kt+2
	sta kt+2
	lda EMU_CLOCK+3
	sbc kt+3
	sta kt+3
	rts

; resident: map overlay 20, play, put the bank back
_play_src:
	lda $00
	pha
	lda _ovl_bank + 20      ; OVL_PLAY
	sta $00
	jsr play
	sta tmpa
	pla
	sta $00
	lda tmpa
	sec
	rts

.segment "OVERLAY20"

; the heap's banks in use now: the bump allocator's (first_bank ..
; cur_bank) and those under the top it lowered for an import's text
; (bankmem_top - first_bank - bank_count): their most since the last log
hsample:
	lda _cur_bank
	sec
	sbc _first_bank
	sec                     ; (+1)
	adc _bankmem_top
	sec
	sbc _first_bank
	sec
	sbc _bank_count
	cmp _play_hpeak
	bcc @n
	sta _play_hpeak
@n:	rts

play:	jsr hsample
	jsr step
	lda moved               ; a new place: there; else where the pointer is,
	bne @to                 ; which Veranda may have moved itself (a menu
	lda mouseXPos           ; opened puts it on its first item)
	sta px
	lda mouseXPos+1
	sta px+1
	lda mouseYPosW
	sta py
	lda mouseYPosW+1
	sta py+1
@to:	stz moved
	lda px
	sta r0L
	lda px+1
	sta r0H
	lda py
	sta r1L
	lda py+1
	sta r1H
	lda pb
	rts

step:	lda ended
	bne @r
	lda appMain
	ora appMain+1
	beq @idle
	lda _calc_idle
	beq @busy
	stz _calc_idle
@idle:	stz busy
	stz busy+1
	lda wait
	beq @q
	dec wait
@r:	rts
@busy:	jsr mark                ; busy now: the 'Z' clock's last busy frame
	inc busy                ; busy for HUNG frames on end: hung, so the
	bne @r                  ; dump now (the gate fails on "HUNG")
	inc busy+1
	lda busy+1
	cmp #>HUNG
	bcc @r
	ldx #0
@h:	lda hung,x
	sta EMU_STDOUT
	inx
	cpx #6
	bne @h
	lda #1
	sta EMU_SAVE_ON_EXIT
	jmp $FFFF
@q:	lda pressFlag           ; a key in Veranda's queue
	bmi @r
	jsr KBDBUF_PEEK         ; x: keys in the ROM's
	cpx #0
	bne @r
	; the record at cur
	lda cur
	sta @a+1
	lda cur+1
	sta @a+2
	ldx #0
@a:	lda $FFFF,x
	sta rec,x
	inx
	cpx #5
	bne @a
	lda cur
	clc
	adc #5
	sta cur
	bcc @n
	inc cur+1
@n:	lda #GAP
	sta wait
	ldx #0
@d:	lda ops,x
	beq opE
	cmp rec
	beq @go
	inx
	bra @d
@go:	txa
	asl
	tax
	jmp (opv,x)

ops:	.byte "PTIDUKWHSLZYRX", 0
hung:	.byte 10, "HUNG", 10
opv:	.addr opP, opT, opI, opD, opU, opK, opW, opH, opS, opL, opZ, opY, opR, opX


; 'E' and anything else: the end
opE:	lda #1
	sta ended
	lda #$FF
	sta _play_log
	rts
opL:	lda rec+1
	sta _play_log
	rts

opP:	inc moved
	lda rec+1
	sta px
	lda rec+2
	sta px+1
	lda rec+3
	sta py
	lda rec+4
	sta py+1
	rts
opT:	inc moved
	ldx rec+1
	lda _vm_buf + VM_X,x    ; (left + right) / 2: left / 2 + right / 2
	clc
	adc _vm_buf + VM_X + 1,x
	sta px
	lda #0
	adc #0
	sta px+1
	lda #8
	sta py
	stz py+1
	rts
opI:	inc moved
	ldx rec+1
	lda _vm_buf + VM_X,x    ; the title's left + 12
	stz px+1
	asl
	rol px+1
	clc
	adc #12
	sta px
	lda px+1
	adc #0
	sta px+1
	lda rec+2               ; 16 + 16 i + 8
	asl
	asl
	asl
	asl
	clc
	adc #24
	sta py
	stz py+1
	rts
opD:	lda #1
	sta pb
	rts
opR:	lda #2
	sta pb
	rts
opU:	stz pb
	rts
opK:	lda rec+1
	jmp k_kbdbuf_put
opW:	lda rec+1
	sta wait
	rts
opH:	lda mouseWheel
	clc
	adc rec+1
	sta mouseWheel
	rts
opS:	lda rec+1
	cmp _play_stop
	bne @r
	lda #1
	sta EMU_SAVE_ON_EXIT
	jmp $FFFF
@r:	rts

; the clock now into tlast
mark:	jsr clock
	ldx #3
@m:	lda tnow,x
	sta tlast,x
	dex
	bpl @m
	rts

; the emulator's clock into tnow
clock:	lda EMU_CLOCK           ; (latches the rest)
	sta tnow
	lda EMU_CLOCK+1
	sta tnow+1
	lda EMU_CLOCK+2
	sta tnow+2
	lda EMU_CLOCK+3
	sta tnow+3
	rts
opZ:	jsr clock
	ldx #3
@z:	lda tnow,x
	sta t0,x
	sta tlast,x
	dex
	bpl @z
	rts
opX:	ldx #3                  ; "KTIME c <kt>": as 'Y', from 0 to kt
@x:	stz t0,x
	lda kt,x
	sta tlast,x
	dex
	bpl @x
	lda #'K'
	ldx #10
	stx EMU_STDOUT
	sta EMU_STDOUT
	ldx #1                  ; (past timetx's newline)
	bra opY1
opY:	ldx #0                  ; "TIME c " then tlast - t0 in hex
opY1:
@y0:	lda timetx,x
	sta EMU_STDOUT
	inx
	cpx #6
	bne @y0
	lda rec+1
	sta EMU_STDOUT
	lda #' '
	sta EMU_STDOUT
	sec
	ldx #0
@y1:	lda tlast,x
	sbc t0,x
	sta tnow,x
	inx
	txa
	eor #4
	bne @y1
	ldx #3
@y2:	lda tnow,x
	lsr
	lsr
	lsr
	lsr
	tay
	lda hexd,y
	sta EMU_STDOUT
	lda tnow,x
	and #15
	tay
	lda hexd,y
	sta EMU_STDOUT
	dex
	bpl @y2
	lda #10
	sta EMU_STDOUT
	rts
timetx:	.byte 10, "TIME "
hexd:	.byte "0123456789ABCDEF"
