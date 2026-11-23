; numint_veranda.s -- grid.c's num_int_text on Veranda, in OVL_FAR's bank
; (the grid's redraw): a whole number below 2^29 written without the ROM's
; floating point, its bits turned into decimal with the 65C02's decimal
; mode (double dabble, a bit at a time).
;
;   Up to 16 bits: the bits shifted down to a 16-bit number (none of the
;   fraction's may be set) and its digits by subtracting powers of ten;
;   more: the 65C02's decimal mode, a bit at a time (double dabble).
;
; uint8_t __fastcall__ num_int_text(const uint8_t *v, char *out)
;   v: a 5-byte MBF number (number.h). Writes its digits (a '-' first if it
;   is negative) and a 0 to out, as the ROM's FOUT would less its sign
;   column; the length. 0 if it is not a whole number below 2^29.

.export _num_int_text
.import popax
.importzp ptr1, ptr2, tmp1, tmp2

.segment "OVL16BSS"
m:	.res 4                  ; the mantissa, most significant first
bcd:	.res 5                  ; ten digits, most significant first
sign:	.res 1

.segment "OVERLAY16"

_num_int_text:
	sta ptr2
	stx ptr2+1
	jsr popax
	sta ptr1
	stx ptr1+1
	ldy #0
	lda (ptr1),y            ; the exponent: 0 is zero
	bne @nz
	lda #'0'
	sta (ptr2),y
	iny
	lda #0
	sta (ptr2),y
	lda #1
	ldx #0
	rts
@nz:	sec
	sbc #128                ; the bits before the point
	beq @no1                ; (below 1)
	bcc @no1
	cmp #30
	bcc @ok
@no1:	jmp @no                 ; 2^29 or more
@ok:
	sta tmp1
	iny
	lda (ptr1),y
	sta sign
	ora #$80                ; the leading 1 the sign displaced
	sta m
	iny
	lda (ptr1),y
	sta m+1
	iny
	lda (ptr1),y
	sta m+2
	iny
	lda (ptr1),y
	sta m+3
	lda tmp1
	cmp #17
	bcs @dd
	jmp short16             ; 16 bits or fewer: by subtraction
@dd:	stz bcd
	stz bcd+1
	stz bcd+2
	stz bcd+3
	stz bcd+4
	ldx tmp1
	sed
@bit:	asl m+3                 ; the next bit into the carry
	rol m+2
	rol m+1
	rol m
	lda bcd+4               ; bcd = 2 bcd + it
	adc bcd+4
	sta bcd+4
	lda bcd+3
	adc bcd+3
	sta bcd+3
	lda bcd+2
	adc bcd+2
	sta bcd+2
	lda bcd+1
	adc bcd+1
	sta bcd+1
	lda bcd
	adc bcd
	sta bcd
	dex
	bne @bit
	cld
	lda m                   ; what is left is the fraction
	ora m+1
	ora m+2
	ora m+3
	bne @no
	ldy #0
	lda sign
	bpl @pos
	lda #'-'
	sta (ptr2),y
	iny
@pos:	sty tmp1                ; where the next character goes
	ldx #0
	stz tmp2                ; 1: a digit written (no leading zeros)
@byte:	lda bcd,x
	lsr a
	lsr a
	lsr a
	lsr a
	jsr digit
	lda bcd,x
	and #$0F
	jsr digit
	inx
	cpx #5
	bne @byte
	ldy tmp1
	lda #0
	sta (ptr2),y
	tya
	ldx #0
	rts
@no:	cld
	lda #0
	tax
	rts

digit:	cmp #0
	bne @d
	ldy tmp2
	beq @r                  ; a leading zero
@d:	ldy #1
	sty tmp2
	ldy tmp1
	ora #'0'
	sta (ptr2),y
	inc tmp1
@r:	rts

; tmp1 bits (1-16) in m, m+1 (the rest of the mantissa must be 0)
short16:
	lda m+2
	ora m+3
	bne fail
	ldx tmp1
@sh:	cpx #16                 ; down to a whole number
	beq @v
	lsr m
	ror m+1
	bcs fail                ; a 1 after the point
	inx
	bra @sh
@v:	ldy #0
	lda sign
	bpl @p
	lda #'-'
	sta (ptr2),y
	iny
@p:	sty tmp1
	stz tmp2
	ldx #0
@pow:	ldy #0                  ; the digit: how many times p10[x] goes
@sub:	lda m+1
	sec
	sbc p10lo,x
	sta bcd
	lda m
	sbc p10hi,x
	bcc @emit
	sta m
	lda bcd
	sta m+1
	iny
	bra @sub
@emit:	tya
	jsr digit
	inx
	cpx #4
	bne @pow
	lda #1                  ; the units: always written
	sta tmp2
	lda m+1
	jsr digit
	ldy tmp1
	lda #0
	sta (ptr2),y
	tya
	ldx #0
	rts
fail:	lda #0
	tax
	rts

p10lo:	.byte <10000, <1000, <100, <10
p10hi:	.byte >10000, >1000, >100, >10
