; strpool_veranda.s -- workbook/strings.c's strpool_get on Veranda, in
; assembly: the grid's redraw reads every text cell's string through it,
; and the C (strp_rec_of, then three accessors, each mapping a bank and
; putting the program's back) was a quarter of what a full redraw cost.
;
; uint16_t __fastcall__ strpool_get(uint16_t id, char *buf, uint16_t max)
;   The string's bytes, at most max - 1 of them, and a 0 to buf; its whole
;   length. An id with no string: "" and 0. strings.c has the layout: the
;   handle of id's record at 4 (id - 1) % 2048 in chunk (id - 1) / 2048, a
;   record's length at REC_LEN (2), its bytes from REC_TEXT (4). A handle:
;   its offset (bytes 0-1) and bank (byte 2); bank 0 is no record (a free
;   id holds the next free one there, strings.c's alloc_id).

.export _strpool_get
.import popax, _strp_chunk, _bank_home
.importzp _strp_next_id, ptr1, ptr2, ptr3, tmp1

RAM_BANK = $00

.segment "BSS"
max:	.res 2
len:	.res 2

.segment "CODE"

_strpool_get:
	sta max
	stx max+1
	jsr popax
	sta ptr2                ; buf
	stx ptr2+1
	jsr popax               ; id
	sta ptr1
	stx ptr1+1
	lda max
	ora max+1
	bne @m
	tax                     ; max 0: nothing at all
	rts
@nonh:	lda _bank_home
	sta RAM_BANK
@none:	lda #0
	tay
	sta (ptr2),y
	tax
	rts
@m:	lda ptr1                ; 1 <= id <= strp_next_id
	ora ptr1+1
	beq @none
	lda _strp_next_id
	cmp ptr1
	lda _strp_next_id+1
	sbc ptr1+1
	bcc @none
	lda ptr1                ; i = id - 1
	bne @d
	dec ptr1+1
@d:	dec ptr1
	lda ptr1+1              ; the chunk: i / 2048, 4 bytes a handle
	lsr a
	and #$FC
	tax
	lda _strp_chunk+2,x     ; its bank (0: none)
	beq @none
	sta tmp1
	lda ptr1                ; ptr3 = $A000 + its offset + 4 (i % 2048)
	asl a
	sta ptr3
	lda ptr1+1
	and #$07
	rol a
	asl ptr3
	rol a
	sta ptr3+1
	lda ptr3
	clc
	adc _strp_chunk,x
	sta ptr3
	lda ptr3+1
	adc _strp_chunk+1,x
	adc #$A0
	sta ptr3+1
	lda tmp1
	sta RAM_BANK
	ldy #2
	lda (ptr3),y            ; the record's bank (0: a free id)
	beq @nonh
	tax
	ldy #0
	lda (ptr3),y            ; ptr1 = $A000 + its offset
	sta ptr1
	iny
	lda (ptr3),y
	clc
	adc #$A0
	sta ptr1+1
	stx RAM_BANK
	ldy #2
	lda (ptr1),y            ; its length
	sta len
	iny
	lda (ptr1),y
	sta len+1
	lda max                 ; max - 1 (max >= 1)
	bne @m1
	dec max+1
@m1:	dec max
	lda len                 ; n = min(len, max - 1): in max
	cmp max
	lda len+1
	sbc max+1
	bcs @cut
	lda len
	sta max
	lda len+1
	sta max+1
@cut:	lda ptr1                ; the bytes from REC_TEXT
	clc
	adc #4
	sta ptr1
	bcc @c0
	inc ptr1+1
@c0:	lda ptr2+1
	cmp #$A0
	bcs @slow               ; buf in the caller's bank (an overlay's)
	ldy #0
	ldx max+1               ; whole pages first
	beq @part
@pg:	lda (ptr1),y
	sta (ptr2),y
	iny
	bne @pg
	inc ptr1+1
	inc ptr2+1
	dex
	bne @pg
@part:	cpy max
	beq @end
	lda (ptr1),y
	sta (ptr2),y
	iny
	bra @part
@slow:	stx tmp1                ; the record's bank
@sl:	lda max                 ; each byte with both banks in turn
	ora max+1
	beq @sd
	lda tmp1
	sta RAM_BANK
	lda (ptr1)
	tax
	lda _bank_home
	sta RAM_BANK
	txa
	sta (ptr2)
	inc ptr1
	bne @s1
	inc ptr1+1
@s1:	inc ptr2
	bne @s2
	inc ptr2+1
@s2:	lda max
	bne @s3
	dec max+1
@s3:	dec max
	bra @sl
@sd:	ldy #0
@end:	lda _bank_home
	sta RAM_BANK
	lda #0
	sta (ptr2),y
	lda len
	ldx len+1
	rts
