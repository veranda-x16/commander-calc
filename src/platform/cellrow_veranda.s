; cellrow_veranda.s -- workbook/cells.c's cells_row on Veranda, in
; assembly: the grid's redraw reads each row's records through it, and the
; C (the band, the row record, the records: three accessors and a copy,
; each mapping a bank) was a tenth of a full redraw.
;
; uint8_t __fastcall__ cells_row(const cellstore_t *cs, uint16_t row)
;   The row's records in rc_rec, in column order: how many (0 for an empty
;   row, which is kept too), or RC_BIG (more than RC_MAX: not kept). The
;   row kept already (rc_ok, rc_row, rc_dir): at once. cells_priv.h has
;   the layout: cs->row_dir a handle of 256 bands' handles, a band 256 row
;   records of 8 bytes (the count at 0, the records' handle at 4), a record
;   8 bytes. A handle: its offset (bytes 0-1), bank (byte 2); bank 0 none.

.export _cells_row
.import popax, _bank_home, _rc_rec, _rc_dir, _rc_row, _rc_n, _rc_ok
.importzp ptr1, ptr2, ptr3, tmp1, tmp2

RAM_BANK = $00
RC_MAX = 16                     ; cells.h
RC_BIG = $FF

.segment "BSS"
row:	.res 2
dir:	.res 4
cnt:	.res 1

.segment "CODE"

_cells_row:
	sta row
	stx row+1
	jsr popax
	sta ptr1                ; cs: its row_dir
	stx ptr1+1
	ldy #3
@d:	lda (ptr1),y
	sta dir,y
	dey
	bpl @d
	lda _rc_ok              ; kept already?
	beq @look
	lda _rc_row
	cmp row
	bne @look
	lda _rc_row+1
	cmp row+1
	bne @look
	ldx #3
@c:	lda _rc_dir,x
	cmp dir,x
	bne @look
	dex
	bpl @c
	lda _rc_n
	ldx #0
	rts
@look:	stz cnt
	lda dir+2
	bne @dir
	jmp @keep               ; no directory: an empty row
@dir:
	lda row+1               ; ptr2 = the band's handle: 4 (row / 256)
	stz tmp2
	asl a
	rol tmp2
	asl a
	rol tmp2
	clc
	adc dir
	sta ptr2
	lda dir+1
	adc tmp2
	adc #$A0
	sta ptr2+1
	lda dir+2
	sta RAM_BANK
	ldy #2
	lda (ptr2),y            ; the band's bank
	beq @home
	tax
	ldy #0
	lda (ptr2),y
	sta ptr3
	iny
	lda (ptr2),y
	sta ptr3+1
	lda row                 ; ptr3 = the row record: 8 (row % 256)
	stz tmp2
	asl a
	rol tmp2
	asl a
	rol tmp2
	asl a
	rol tmp2
	clc
	adc ptr3
	sta ptr3
	lda tmp2
	adc ptr3+1
	adc #$A0
	sta ptr3+1
	stx RAM_BANK
	ldy #1
	lda (ptr3),y            ; the count
	bne @big
	dey
	lda (ptr3),y
	beq @home
	cmp #RC_MAX + 1
	bcs @big
	sta cnt
	ldy #4                  ; the records
	lda (ptr3),y
	sta ptr2
	iny
	lda (ptr3),y
	clc
	adc #$A0
	sta ptr2+1
	iny
	lda (ptr3),y
	sta RAM_BANK
	lda cnt
	asl a
	asl a
	asl a
	tay
	dey
@cp:	lda (ptr2),y
	sta _rc_rec,y
	dey
	bpl @cp
@home:	lda _bank_home
	sta RAM_BANK
@keep:	lda cnt                 ; kept
	sta _rc_n
	lda row
	sta _rc_row
	lda row+1
	sta _rc_row+1
	ldx #3
@k:	lda dir,x
	sta _rc_dir,x
	dex
	bpl @k
	lda #1
	sta _rc_ok
	lda cnt
	ldx #0
	rts
@big:	lda _bank_home
	sta RAM_BANK
	lda #RC_BIG
	ldx #0
	rts
