; theme_veranda.s -- two of the SDK's theme calls (theme.h), resident, for
; the Open and Save dialog's overlay (vrchoose.c, overlay 22) and the
; chrome's (platform/look_veranda.c, overlay 21). The SDK's own are C in
; themedraw.c and themecov.c, which bring theme_palette and theme_rect
; with them into the resident image; these do what they do in a third of
; the bytes (Veranda Calc's resident image is that short).
;
;   unsigned char theme_map_on(void)       1 if the colour map is shown
;                                          (VERA's layer 1 on: V30)
;   void theme_cover_rect(x, w, y, h, c)   colours inside a box over the
;                                          colour map: the cards' attribute
;                                          bytes (the map at $1AC00, 256
;                                          bytes a row, 2 a card), so they
;                                          go when the box goes (V79)

.export _theme_map_on, _theme_cover_rect
.import popa

VADDRL = $9F20
VADDRM = $9F21
VADDRH = $9F22
VDATA0 = $9F23
VCTRL  = $9F25
VVIDEO = $9F29

.segment "BSS"
col:	.res 1
hh:	.res 1
yy:	.res 1
ww:	.res 1
xx:	.res 1

.segment "CODE"

_theme_map_on:
	stz VCTRL
	lda VVIDEO
	ldx #0
	and #$20
	beq @r
	lda #1
@r:	rts

; cc65's order: c in a, then h, y, w, x from the C stack
_theme_cover_rect:
	asl a
	asl a
	asl a
	asl a
	sta col
	jsr popa
	sta hh
	jsr popa
	sta yy
	jsr popa
	sta ww
	jsr popa
	asl a
	ina
	sta xx                  ; the attribute byte of card x: 2 x + 1
	jsr _theme_map_on
	beq @x
@row:	lda hh
	beq @x
	stz VCTRL
	lda xx
	sta VADDRL
	lda yy
	clc
	adc #$AC
	sta VADDRM
	lda #$21                ; increment 2: the attribute bytes
	sta VADDRH
	ldx ww
	beq @n
	lda col
@c:	sta VDATA0
	dex
	bne @c
@n:	inc yy
	dec hh
	bra @row
@x:	rts
