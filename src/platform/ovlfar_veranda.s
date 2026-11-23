; ovlfar_veranda.s -- call a routine of OVL_FAR's (overlay.h: ovl_far)
; from anywhere and come back to the bank it was called from.
;
; void __fastcall__ ovl_far(void (*fn)(void)): the overlay's bank mapped
; and made the program's (bank_home, appBank: the heap's accessors and
; Veranda map those back), fn called, and the caller's three put back. The
; C stack is not touched: fn takes no arguments and returns nothing.
;
; void __fastcall__ ovl_far21(void (*fn)(void)): the same into OVL_CLIP's
; bank (overlay 21: the clipboard, and Veranda's other own parts: the
; date's formatting, Veranda Look 2's chrome;
; overlay.h); ovl_far22 into OVL_VRFILE's (overlay 22: the SDK's Open and
; Save dialog).

.include "veranda.inc"

.export _ovl_far, _ovl_far21, _ovl_far22
.import _ovl_bank, _bank_home

RAM_BANK = $00
OVL_FAR = 16                    ; OVL_CHARTOUT (x16sheet.h)
OVL_CLIP = 21                   ; (x16sheet.h)
OVL_VRFILE = 22

.segment "CODE"

_ovl_far22:
	ldy #OVL_VRFILE
	bra go
_ovl_far21:
	ldy #OVL_CLIP
	bra go
_ovl_far:
	ldy #OVL_FAR
go:	sta vec+1
	stx vec+2
	lda RAM_BANK
	pha
	lda _bank_home
	pha
	lda appBank
	pha
	lda _ovl_bank,y
	sta RAM_BANK
	sta _bank_home
	sta appBank
vec:	jsr $0000
	pla
	sta appBank
	pla
	sta _bank_home
	pla
	sta RAM_BANK
	rts
