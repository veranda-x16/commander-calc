; shadow_veranda.s -- what Veranda Calc's screen shows, card by card
; (screen_veranda.c), so a redraw draws only what changed.
;
; Two RAM banks of the program's own (screen_init: BankAlloc(2)), the
; shadow at $A000 + 80 y + x for the grid's 80 x 57 cards: in the first
; bank each card's character (bit 7: bold), in the second its paper colour
; (bits 0-3) and its borders (bits 4-7, SCREEN_B_*).
;
; screen_vdiff: the run of scr_vn cards at offset scr_voff against the
; characters scr_line[] (each OR scr_vc) and the paper byte scr_vp; the
; shadow takes the new values. scr_va / scr_vb: the first and last card
; that differ ($FF: none); scr_vpc: 1 if a paper byte did. The program's
; bank is put back (the interrupt keeps whichever is mapped).

.include "veranda.inc"

TOPROW = 3                      ; the grid's row y is card row y + 3 (screen_veranda.c)
ROWS   = 57                     ; the grid's rows

.export _screen_vdiff, _scr_out, _scr_reset
.export _screen_put, _screen_fill, _screen_text, _screen_text_right, _scr_paper, _screen_recolor
.export _scr_papers
.export _scr_voff, _scr_vn, _scr_vc, _scr_vp, _scr_va, _scr_vb, _scr_vpc, _scr_sbank
.export _scr_ox, _scr_oy, _scr_on, _scr_op, _scr_by, _scr_bold, _screen_scroll
.import _scr_line, _screen_font, _scr_bx, _scr_bbits, _screen_vborders
.import popa, popax
.importzp ptr1, ptr2, tmp1

RAM_BANK = $00

.segment "BSS"
_scr_voff:  .res 2
_scr_vn:    .res 1
_scr_vc:    .res 1
_scr_vp:    .res 1
_scr_va:    .res 1
_scr_vb:    .res 1
_scr_vpc:   .res 1
_scr_sbank: .res 1              ; the first of the two banks (0: none)
_scr_ox:    .res 1              ; scr_out: card column, the grid's row,
_scr_oy:    .res 1              ; count, paper
_scr_on:    .res 1
_scr_op:    .res 1
_scr_by:    .res 1              ; the kept borders' row (screen_border)
_scr_bold:  .res 1              ; PutCards' flags: 2 bold
here:       .res 1
attr:       .res 1              ; the colour (color_t) of the run
scy0:       .res 1              ; screen_scroll's rows and direction
scy1:       .res 1
scup:       .res 1

.segment "CODE"

_screen_vdiff:
	lda RAM_BANK
	pha
	lda _scr_voff
	sta ptr1
	lda _scr_voff+1
	clc
	adc #$A0
	sta ptr1+1
	lda #$FF
	sta _scr_va
	stz _scr_vb
	stz _scr_vpc
	lda _scr_sbank
	sta RAM_BANK
	ldy #0
@c:	lda _scr_line,y
	ora _scr_vc
	cmp (ptr1),y
	beq @cs
	sta (ptr1),y
	jsr mark
@cs:	iny
	cpy _scr_vn
	bne @c
	inc RAM_BANK            ; the paper bank
	ldy #0
@p:	lda _scr_vp
	cmp (ptr1),y
	beq @ps
	sta (ptr1),y
	jsr mark
	lda #1
	sta _scr_vpc
@ps:	iny
	cpy _scr_vn
	bne @p
	pla
	sta RAM_BANK
	rts

; scr_out: scr_line[0 .. scr_on - 1] at card column scr_ox of the grid's
; row scr_oy (card row scr_oy + TOPROW), paper scr_op, clipped already: only the
; cards that changed (with the shadow), then the kept borders if they are
; this run's (screen_veranda.c's screen_border). In assembly: the same in C
; was 260 bytes more of the resident image.
_scr_out:
	stz here
	lda _scr_bbits
	beq @nh
	lda _scr_ox
	cmp _scr_bx
	bne @nh
	lda _scr_oy
	cmp _scr_by
	bne @nh
	inc here
@nh:	lda _scr_sbank
	bne @sh
	stz _scr_va             ; no shadow: all of it
	lda _scr_on
	sta _scr_vn
	lda #1
	sta _scr_vpc
	bra @draw
@sh:	jsr voffset
	lda _scr_on
	sta _scr_vn
	lda _scr_bold
	asl a
	asl a
	asl a
	asl a
	asl a
	asl a
	sta _scr_vc             ; bit 7: bold
	lda here
	beq @np
	lda _scr_bbits
	asl a
	asl a
	asl a
	asl a
@np:	ora _scr_op
	sta _scr_vp
	jsr _screen_vdiff
	lda _scr_va
	cmp #$FF
	bne @df
	stz _scr_bbits          ; it shows all that already
	rts
@df:	lda _scr_vb
	sec
	sbc _scr_va
	ina
	sta _scr_vn             ; the cards from the first change to the last
@draw:	lda _scr_va
	clc
	adc #<_scr_line
	sta r0L
	lda #>_scr_line
	adc #0
	sta r0H
	lda #<(_screen_font - $100)
	sta r1L
	lda #>(_screen_font - $100)
	sta r1H
	lda _scr_ox
	clc
	adc _scr_va
	pha
	sta r3L
	lda _scr_vn
	sta r3H
	lda _scr_oy
	clc
	adc #TOPROW            ; card row y + TOPROW
	sta r11L
	stz r11H
	asl r11L
	rol r11H
	asl r11L
	rol r11H
	asl r11L
	rol r11H
	lda _scr_bold
	jsr PutCards
	pla
	ldx _scr_vpc
	beq @nc
	sta r3L
	lda _scr_vn
	sta r3H
	lda _scr_oy
	clc
	adc #TOPROW
	sta r11L
	lda #1
	sta r11H
	lda _scr_op
	jsr ColorRect
@nc:	lda here
	beq @r
	lda _scr_oy
	clc
	adc #TOPROW
	jmp _screen_vborders
@r:	rts

; --- screen.h's text calls (screen_veranda.c's, in assembly: the C was
; 900 bytes of the resident image). cc65's order: the last argument in a,
; the others on the C stack, the first pushed first.

; screen_put(x, y, ch, c)
_screen_put:
	sta attr
	jsr popa
	jsr glyph
	sta _scr_line
	lda #1
	jmp xy

; screen_fill(x, y, len, ch, c)
_screen_fill:
	sta attr
	jsr popa
	jsr glyph
	pha
	jsr popa                ; len
	jsr clamp
	tay
	pla
	cpy #0
	beq @e
@f:	dey
	sta _scr_line,y
	bne @f
@e:	lda ptr2
	jmp xy

; screen_text(x, y, s, width, c): s, then spaces to width
_screen_text:
	sta attr
	jsr popa                ; width
	jsr clamp
	jsr popax
	sta ptr1
	stx ptr1+1
copy:	ldy #0                  ; s (ptr1) into scr_line, spaces to ptr2
@t:	cpy ptr2
	beq @d
	lda (ptr1),y
	beq @sp
	jsr glyph
	sta _scr_line,y
	iny
	bra @t
@sp:	lda #' '
@s:	cpy ptr2
	beq @d
	sta _scr_line,y
	iny
	bra @s
@d:	lda ptr2
	jmp xy

; screen_text_right(x, y, s, width, c): spaces, then s, ending at width;
; s as long as width or longer: as screen_text
_screen_text_right:
	sta attr
	jsr popa
	jsr clamp
	jsr popax
	sta ptr1
	stx ptr1+1
	ldy #0
@l:	lda (ptr1),y            ; its length
	beq @n
	iny
	bne @l
@n:	cpy ptr2
	bcs copy                ; too long: from the left
	sty here
	lda ptr2
	sec
	sbc here
	tax                     ; the spaces before it
	ldy #0
	lda #' '
@p:	cpx #0
	beq @c
	sta _scr_line,y
	iny
	dex
	bra @p
@c:	sty here                ; where s goes
	ldx #0
@g:	txa
	tay
	lda (ptr1),y
	beq @dn
	jsr glyph
	ldy here
	sta _scr_line,y
	inc here
	inx
	bra @g
@dn:	lda ptr2
	; fall into xy

; a: the run's length; pop y and x, clip, the paper, scr_out. The grid's
; last row (card row 59, a message's) is the status bar's second card row:
; on the chrome whatever its attribute (look_veranda.c colours it so).
xy:	jsr popxy
	bcs @x
	lda attr
	ldx _scr_oy
	cpx #ROWS - 1
	bne @p
	lda #$F1                ; black on light grey: the chrome
@p:	jsr _scr_paper
	sta _scr_op
	jmp _scr_out
@x:	rts

; scr_on = a; pop y, x; clip (c set: nothing to draw)
popxy:	sta _scr_on
	jsr popa
	sta _scr_oy
	jsr popa
	sta _scr_ox
	cmp #80
	bcs @x
	lda _scr_oy
	beq @x
	cmp #ROWS
	bcs @x
	lda _scr_on
	beq @x
	lda #80                 ; at most to the right edge
	sec
	sbc _scr_ox
	cmp _scr_on
	bcs @w
	sta _scr_on
@w:	clc
	rts
@x:	sec
	rts

; scr_voff = 80 scr_oy + scr_ox
voffset:
	lda _scr_oy
	stz _scr_voff+1
	ldx #4
@16:	asl a                   ; 16 y
	rol _scr_voff+1
	dex
	bne @16
	sta _scr_voff
	lda _scr_voff+1
	sta ptr1+1
	lda _scr_voff
	asl a
	rol ptr1+1
	asl a
	rol ptr1+1              ; 64 y
	clc
	adc _scr_voff
	sta _scr_voff
	lda ptr1+1
	adc _scr_voff+1
	sta _scr_voff+1
	lda _scr_ox
	clc
	adc _scr_voff
	sta _scr_voff
	bcc @o
	inc _scr_voff+1
@o:	rts

; screen_recolor(x, y, len, c): the paper only (the shadow's too, its
; borders kept)
_screen_recolor:
	sta attr
	jsr popa
	jsr popxy
	bcs @x
	lda attr
	jsr _scr_paper
	sta _scr_op
	lda _scr_ox
	sta r3L
	lda _scr_on
	sta r3H
	lda _scr_oy
	clc
	adc #TOPROW
	sta r11L
	lda #1
	sta r11H
	lda _scr_op
	jsr ColorRect
	lda _scr_sbank
	beq @x
	jsr voffset
	lda _scr_voff
	sta ptr1
	lda _scr_voff+1
	clc
	adc #$A0
	sta ptr1+1
	lda RAM_BANK
	pha
	lda _scr_sbank
	ina
	sta RAM_BANK
	ldy #0
@p:	lda (ptr1),y
	and #$F0
	ora _scr_op
	sta (ptr1),y
	iny
	cpy _scr_on
	bne @p
	pla
	sta RAM_BANK
@x:	rts

; scr_paper(c): the paper colour of an attribute, a palette entry of
; Veranda Look 2's (the SDK's theme.h; main_veranda.c's start-up sets the
; palette): its background, an X16 colour as Look 2's nearest light one
; (the greys the chrome, the blues the selection's); on black, the ink's
; (white: the selection's, the cursor's). Also
; tools/check_veranda.py's paper_of.
_scr_paper:
	pha
	lsr a
	lsr a
	lsr a
	lsr a
	tay
	pla
	cpy #1                  ; COL_BLACK
	bne @t
	and #15
	tay
	bne @t
	lda papers+6            ; COL_WHITE on black: the blues' (SELECT; the theme's
				; highlight, look_veranda.c's look_start)
	rts
@t:	lda papers,y
	rts
; the X16's 0-15 (white, black, red, cyan, purple, green, blue, yellow,
; orange, brown, light red, dark, mid and light grey, light green, light
; blue) as Look 2's PAPER .. DUSK (theme.h: 0 paper, 1 ink, 2 chrome,
; 3 sidebar, 4 select, 6 yellow, 7 red, 8 green, 9 purple, 10 orange,
; 11 cyan): mid grey the sidebar's (the sheets' tabs), the other greys the
; chrome; look_veranda.c's look_start makes the selection's 4s the theme's
; highlight (Sunroom's sky, Veranda Look 3: the cursor, the headers' marks)
_scr_papers:
papers:	.byte 0, 1, 7, 11, 9, 8, 4, 6, 10, 10, 7, 2, 3, 8, 4, 2

; a character's glyph: below $20 (after bit 7) a space
glyph:	and #$7F
	cmp #$20
	bcs @g
	lda #' '
@g:	rts

; a width at most 80, into ptr2 (and a)
clamp:	cmp #81
	bcc @c
	lda #80
@c:	sta ptr2
	rts

; scr_reset: the shadow blank -- spaces on paper 0 (the screen cleared)
_scr_reset:
	lda _scr_sbank
	beq @r
	lda RAM_BANK
	pha
	lda _scr_sbank
	sta RAM_BANK
	lda #' '
	jsr fill
	inc RAM_BANK
	lda #0
	jsr fill
	pla
	sta RAM_BANK
@r:	rts
fill:	ldx #$A0                ; $A000-$B2FF (80 x 59 = $1270 bytes and more)
	stz ptr1
@p:	stx ptr1+1
	ldy #0
@b:	sta (ptr1),y
	iny
	bne @b
	inx
	cpx #$B3
	bne @p
	rts

; card y differs: scr_va = min, scr_vb = max
mark:	cpy _scr_va
	bcs @1
	sty _scr_va
@1:	cpy _scr_vb
	bcc @2
	sty _scr_vb
@2:	rts

; screen_scroll(y0, y1, up): the grid's rows y0 .. y1 moved a row up (up
; 1: row y gets row y + 1's) or down (0), the whole width, on the screen
; (ScrollRect: the bitmap; the colour map stays) and in the shadow: each
; card's character and borders (bits 4-7) from the row it came from, its
; paper (bits 0-3, the colour map's) kept. The row uncovered (y1 up, y0
; down) is cleared on the screen and its characters invalid in the shadow
; (0: no glyph is), so the next text there draws all of it.
_screen_scroll:
	sta scup
	jsr popa
	sta scy1
	jsr popa
	sta scy0
	stz r3L
	lda #80
	sta r3H
	lda scy0
	jsr line8
	sta r11L
	stx r11H
	lda scy1
	jsr line8
	ora #7                  ; its last line
	sta r12L
	stx r12H
	lda #8
	ldx #0
	ldy scup
	beq @dy
	lda #<(-8)
	dex
@dy:	sta r13L
	stx r13H
	lda #0                  ; the paper
	jsr ScrollRect
	lda _scr_sbank
	bne @sh
	rts
@sh:	lda RAM_BANK
	pha
	stz _scr_ox
	lda scy0                ; ptr1: the first row written, ptr2: its source
	ldy scup
	bne @f
	lda scy1
@f:	sta _scr_oy
	jsr voffset
	lda _scr_voff
	sta ptr1
	lda _scr_voff+1
	clc
	adc #$A0
	sta ptr1+1
	lda scup
	beq @src
	lda #80
	ldx #0
	bra @s2
@src:	lda #<(-80)
	ldx #$FF
@s2:	sta here                ; (here, attr: the step, a row)
	stx attr
	clc
	adc ptr1
	sta ptr2
	txa
	adc ptr1+1
	sta ptr2+1
	lda scy1
	sec
	sbc scy0
	tax                     ; the rows moved
	beq @last
@row:	lda _scr_sbank
	sta RAM_BANK
	ldy #79
@c:	lda (ptr2),y
	sta (ptr1),y
	dey
	bpl @c
	inc RAM_BANK
	ldy #79
@p:	lda (ptr2),y
	and #$F0
	sta tmp1
	lda (ptr1),y
	and #$0F
	ora tmp1
	sta (ptr1),y
	dey
	bpl @p
	lda ptr1                ; the next row
	clc
	adc here
	sta ptr1
	lda ptr1+1
	adc attr
	sta ptr1+1
	lda ptr2
	clc
	adc here
	sta ptr2
	lda ptr2+1
	adc attr
	sta ptr2+1
	dex
	bne @row
@last:	lda _scr_sbank          ; ptr1: the row uncovered
	sta RAM_BANK
	ldy #79
	lda #0
@z:	sta (ptr1),y
	dey
	bpl @z
	inc RAM_BANK
	ldy #79
@q:	lda (ptr1),y
	and #$0F
	sta (ptr1),y
	dey
	bpl @q
	pla
	sta RAM_BANK
	rts

; a: the grid's row y; .A/.X: its first line, 8 (y + TOPROW)
line8:	clc
	adc #TOPROW
	asl a
	asl a
	ldx #0
	asl a
	bcc @r
	inx
@r:	rts
