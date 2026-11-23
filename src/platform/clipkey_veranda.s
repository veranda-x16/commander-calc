; clipkey_veranda.s -- the resident half of Veranda's clipboard in Veranda
; Calc (clip_veranda.c has the rest, in OVL_CLIP). In assembly: the
; resident image has a few dozen bytes to spare.
;
;   grid_action_t clip_key(uint8_t k);     a command key: 0x03 (Copy) and
;                                           0x16 (Paste) here, others grid_key's
;
; Copy: clip_run() puts the selection on the Text Scrap (before grid_key:
; Calc's copy ends the selection), then grid_key() copies it to Calc's own
; clipboard (clip_run's 3). Paste: clip_run() reads another program's
; Text Scrap a cell at a time (2): the cell is set here, in resident code
; (wb_set_text() loads the compiler's overlay for a formula), and the loop
; goes on with k = 0; 3: the scrap is Calc's own, grid_key() pastes; 1:
; done, the recalculation and the grid drawn.

.export _clip_key
.import _grid_key, _ovl_require, _clip_run, _wb_set_text, _wb_after_change, _grid_render_all
.import _clip_r, _clip_c, _fio_bounce, pushax

OVL_CLIP = 21

.segment "BSS"
key:	.res 1

.segment "CODE"
_clip_key:
	sta key
	cmp #$16
	beq @loop
	cmp #$03
	beq @loop
	jmp _grid_key           ; not ours
@loop:	lda #OVL_CLIP
	jsr _ovl_require
	lda key
	jsr _clip_run
	cmp #2
	bne @done
	lda _clip_r             ; a cell: wb_set_text(clip_r, clip_c, cell)
	ldx _clip_r+1
	jsr pushax
	lda _clip_c
	ldx _clip_c+1
	jsr pushax
	lda #<(_fio_bounce + 32)
	ldx #>(_fio_bounce + 32)
	jsr _wb_set_text
	stz key
	bra @loop
@done:	cmp #3
	bne @set
	lda key
	jmp _grid_key           ; Calc's own copy (after the Text Scrap's), or paste
@set:	cmp #1
	bne @r
	jsr _wb_after_change
	jsr _grid_render_all
@r:	lda #0                  ; GRID_CONTINUE
	tax
	rts
