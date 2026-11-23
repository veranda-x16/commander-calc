; crc32_x16.s -- crc32.c's crc32_update in assembly, for the machine (the
; X16 build and Veranda's), once in each overlay that checksums: the .X16S
; reader (OVL_FILEIO), its writer (OVL_X16S_SAVE), and the .xlsx writer's
; two (OVL_XLSX_OUT1, OVL_XLSX_OUT2), under crc32_ovl.h's names. The C was
; a 32-bit shift and a table look-up per nibble in cc65's long arithmetic:
; about 2,000 cycles a byte, a third to a half of opening, saving and
; exporting a large workbook; this is about 350.
;
; uint32_t __fastcall__ crc32_update(uint32_t crc, const void *data,
;                                    uint16_t len)
;   The reflected CRC-32 ($EDB88320), a nibble at a time: crc ^= byte,
;   then twice crc = (crc >> 4) ^ table[crc & 15].

.importzp c_sp, sreg, ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
.import incsp6

.macro CRC32_UPDATE name, seg
.segment seg
.export name
.proc name
	sta ptr2                ; len
	stx ptr2+1
	ldy #0
	lda (c_sp),y            ; data
	sta ptr1
	iny
	lda (c_sp),y
	sta ptr1+1
	iny
	lda (c_sp),y            ; crc, least significant byte first
	sta tmp1
	iny
	lda (c_sp),y
	sta tmp2
	iny
	lda (c_sp),y
	sta tmp3
	iny
	lda (c_sp),y
	sta tmp4
	jsr incsp6
next:	lda ptr2
	ora ptr2+1
	beq done
	lda (ptr1)
	eor tmp1
	sta tmp1
	ldy #2                  ; two nibbles
nib:	lda tmp1                ; the low nibble, 4 bytes an entry
	and #$0F
	asl a
	asl a
	pha
	ldx #4                  ; crc >> 4
:	lsr tmp4
	ror tmp3
	ror tmp2
	ror tmp1
	dex
	bne :-
	plx
	lda tmp1
	eor table,x
	sta tmp1
	lda tmp2
	eor table+1,x
	sta tmp2
	lda tmp3
	eor table+2,x
	sta tmp3
	lda tmp4
	eor table+3,x
	sta tmp4
	dey
	bne nib
	inc ptr1
	bne :+
	inc ptr1+1
:	lda ptr2
	bne :+
	dec ptr2+1
:	dec ptr2
	bra next
done:	lda tmp3
	sta sreg
	lda tmp4
	sta sreg+1
	lda tmp1
	ldx tmp2
	rts
table:	.dword $00000000, $1DB71064, $3B6E20C8, $26D930AC
	.dword $76DC4190, $6B6B51F4, $4DB26158, $5005713C
	.dword $EDB88320, $F00F9344, $D6D6A3E8, $CB61B38C
	.dword $9B64C2B0, $86D3D2D4, $A00AE278, $BDBDF21C
.endproc
.endmacro

CRC32_UPDATE _crc32_update, "OVERLAY2"      ; crc32.c (the .X16S reader)
CRC32_UPDATE _scrc32_update, "OVERLAY11"    ; crc32_save_x16.c
CRC32_UPDATE _xcrc32_update, "OVERLAY12"    ; crc32_xsave_x16.c
CRC32_UPDATE _hcrc32_update, "OVERLAY13"    ; crc32_xsheet_x16.c
