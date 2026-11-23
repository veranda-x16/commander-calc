; border_veranda.s -- a cell's borders on Veranda's bitmap (screen_veranda.c)
;
; screen_vborders(y): the lines screen_border() kept (scr_bx, scr_blen,
; scr_bbits: SCREEN_B_L 1, R 2, T 4, B 8) on card row y, over the text just
; drawn there: top and bottom along the cards' first and last pixel rows,
; left and right down the first card's left and the last card's right
; pixel columns. In assembly: the same in C was 230 bytes of the resident
; image. Every register is set again before each call (the kernel's lines
; need not keep them).

.include "veranda.inc"

.export _screen_vborders
.import _scr_bx, _scr_blen, _scr_bbits

.segment "BSS"
ty:	.res 1                  ; the card row

.segment "CODE"

_screen_vborders:
	sta ty
	lda _scr_bbits
	and #4
	beq @b
	ldx #0
	jsr hline
@b:	lda _scr_bbits
	and #8
	beq @l
	ldx #7
	jsr hline
@l:	lda _scr_bbits
	and #1
	beq @r
	lda _scr_bx
	jsr vline
@r:	lda _scr_bbits
	and #2
	beq @x
	lda _scr_bx
	clc
	adc _scr_blen
	jsr vline_end
@x:	stz _scr_bbits
	rts

; r11 = ty * 8 + x
top:	stz r11+1
	lda ty
	asl a
	rol r11+1
	asl a
	rol r11+1
	asl a
	rol r11+1
	stx r11
	ora r11
	sta r11
	rts

; a * 8 into r4 (16 bits)
x8:	stz r4+1
	asl a
	rol r4+1
	asl a
	rol r4+1
	asl a
	rol r4+1
	sta r4
	rts

; HorizontalLine from the first card's left to the last's right at row x
hline:	jsr top
	lda _scr_bx
	jsr x8
	lda r4
	sta r3
	lda r4+1
	sta r3+1
	lda _scr_bx
	clc
	adc _scr_blen
	jsr x8m1
	lda #$FF
	jmp WHorizontalLine

; VerticalLine down card column a's left pixel column; vline_end: down
; the pixel column before card column a
vline_end:
	jsr x8m1
	bra vl
vline:	jsr x8
vl:	ldx #0
	jsr top
	lda r11
	ora #7
	sta r12
	lda r11+1
	sta r12+1
	lda #$FF
	jmp WVerticalLine

; a * 8 - 1 into r4
x8m1:	jsr x8
	lda r4
	bne @n
	dec r4+1
@n:	dec r4
	rts
