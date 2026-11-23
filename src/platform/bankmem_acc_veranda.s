;
; bankmem_acc_veranda.s -- bankmem_acc_x16.s for Veranda: each accessor maps
; bank_home again before it returns (overlays run in place in the bank
; window: platform/overlay_veranda.c). bankmem_map leaves the heap's bank
; mapped, as on the X16; its one user (workbook/cells.c) calls another
; accessor straight after, which puts the program's bank back.
;
;
; bankmem_acc_x16.s — the banked-RAM accessors, in assembly.
;
; These are the narrowest and most-used routines in the program: every cell
; the renderer draws, every node of the sparse index, every byte of a blob
; goes through one of them. In C each was a bank-register store followed by
; a pointer computation cc65 builds with its 16-bit helpers; here each is a
; handful of instructions.
;
; The window is 8 KB at $A000, so an offset is always below $2000 and adding
; $A0 to its high byte can never carry. That is what makes these as short as
; they are, and it is a property of the hardware rather than an assumption:
; a block that straddled a bank boundary would break it, which is exactly
; why the allocator never places one.
;
; The bulk operations (read, write, set) stay in C: they are memcpy and
; memset with a bank store in front, and cc65's library versions are already
; better than anything worth hand-writing here.
;
; EVERY ROUTINE HERE MUST MATCH ITS PROTOTYPE IN bankmem.h ARGUMENT FOR
; ARGUMENT. cc65 passes the last argument in registers and the rest on the C
; stack, so a routine written for one argument count and called with another
; does not fail to link or to run — it silently reads the wrong values and
; corrupts whatever the resulting pointer happens to address. bankmem_map
; was written for two arguments when the header said three, and the symptom
; was a crash in an unrelated part of the program a hundred cells later.
;

        .export         _bankmem_peek, _bankmem_poke
        .export         _bankmem_peek16, _bankmem_poke16
        .export         _bankmem_peek32, _bankmem_poke32
        .export         _bankmem_map

        .export         _bank_peek, _bank_peek16, _bank_peek32
        .export         _bankmem_read, _bankmem_write, _bankmem_set
        .import         popa, popax, _bank_home, incsp4
        .importzp       ptr1, ptr2, ptr3, tmp1, tmp2, tmp3, sreg, c_sp

RAM_BANK = $00
WINDOW_HI = $A0

; Turn the offset in .A/.X into a pointer in ptr1, and select `bank`.
; Leaves .Y = 0, ready for indirect indexed access.
.macro  HOME                            ; the program's bank back (a, x kept)
        pha
        lda     _bank_home
        sta     RAM_BANK
        pla
.endmacro

.macro  SETUP_PTR
        sta     ptr1
        stx     ptr1+1
        jsr     popa                    ; bank
        sta     RAM_BANK
        lda     ptr1+1
        clc
        adc     #WINDOW_HI              ; offset < $2000, so never carries
        sta     ptr1+1
        ldy     #$00
.endmacro

; --- uint8_t __fastcall__ bankmem_peek(uint8_t bank, uint16_t off) ----
.proc   _bankmem_peek
        SETUP_PTR
        lda     (ptr1),y
        ldx     #$00
        HOME
        rts
.endproc

; --- void __fastcall__ bankmem_poke(uint8_t bank, uint16_t off, uint8_t v)
.proc   _bankmem_poke
        sta     tmp1                    ; value (last argument)
        jsr     popax                   ; offset
        SETUP_PTR
        lda     tmp1
        sta     (ptr1),y
        HOME
        rts
.endproc

; --- uint16_t __fastcall__ bankmem_peek16(uint8_t bank, uint16_t off) -
; Little-endian in banked RAM, and cc65 returns 16 bits in .A low, .X high.
.proc   _bankmem_peek16
        SETUP_PTR
        lda     (ptr1),y
        pha
        iny
        lda     (ptr1),y
        tax
        pla
        HOME
        rts
.endproc

; --- void __fastcall__ bankmem_poke16(uint8_t bank, uint16_t off, uint16_t v)
.proc   _bankmem_poke16
        sta     tmp1                    ; value low
        stx     tmp2                    ; value high
        jsr     popax                   ; offset
        SETUP_PTR
        lda     tmp1
        sta     (ptr1),y
        iny
        lda     tmp2
        sta     (ptr1),y
        HOME
        rts
.endproc

; --- handle_t __fastcall__ bankmem_peek32(uint8_t bank, uint16_t off) -
; cc65 returns a long in sreg (high word) and .X:.A (low word).
.proc   _bankmem_peek32
        SETUP_PTR
        lda     (ptr1),y
        sta     tmp1                    ; byte 0
        iny
        lda     (ptr1),y
        sta     tmp2                    ; byte 1
        iny
        lda     (ptr1),y
        sta     sreg                    ; byte 2
        iny
        lda     (ptr1),y
        sta     sreg+1                  ; byte 3
        lda     tmp1
        ldx     tmp2
        HOME
        rts
.endproc

; --- void __fastcall__ bankmem_poke32(uint8_t bank, uint16_t off, handle_t v)
; The value arrives in sreg:.X:.A; the offset and bank are on the C stack.
.proc   _bankmem_poke32
        sta     tmp1                    ; byte 0
        stx     tmp2                    ; byte 1
        jsr     popax                   ; offset
        SETUP_PTR
        lda     tmp1
        sta     (ptr1),y
        iny
        lda     tmp2
        sta     (ptr1),y
        iny
        lda     sreg
        sta     (ptr1),y
        iny
        lda     sreg+1
        sta     (ptr1),y
        HOME
        rts
.endproc

; --- void * __fastcall__ bankmem_map(uint8_t bank, uint16_t off,
;                                     uint16_t len) --------------------
;
; THREE arguments, not two. `len` is what the host backend range-checks;
; this backend ignores it, because the allocator guarantees a block never
; crosses a bank. But it still arrives in .A/.X as the last argument, so it
; has to be stepped over before the offset can be popped.
;
; Getting this wrong is silent and vicious: the routine then takes `len` as
; the offset and the offset's low byte as the bank, hands back a pointer
; into an arbitrary bank, and the caller's memmove writes through it. The
; symptom is corruption somewhere else entirely — in this case stray writes
; across the I/O page.
.proc   _bankmem_map
        jsr     popax                   ; discard .A/.X (len), fetch offset
        SETUP_PTR
        lda     ptr1
        ldx     ptr1+1
        rts
.endproc

; --- banked_ram.h's bank_peek, bank_peek16, bank_peek32 (handle_t h,
; uint16_t off), straight from the handle: banked_ram.c's are C wrappers
; that take the handle apart with 32-bit arithmetic and call the ones
; above, about a thousand cycles a call, and the grid's redraw makes
; thousands of them (Veranda Calc's phase C6). The handle is on the C
; stack: its offset (bytes 0-1) and bank (byte 2); off in .A/.X.

; ptr1 = $A000 + the handle's offset + off; its bank mapped; .Y = 0
hsetup: clc
        ldy     #0
        adc     (c_sp),y
        sta     ptr1
        txa
        iny
        adc     (c_sp),y
        adc     #WINDOW_HI              ; (carry clear: the sum is below $2000)
        sta     ptr1+1
        iny
        lda     (c_sp),y
        sta     RAM_BANK
        ldy     #0
        rts

.proc   _bank_peek
        jsr     hsetup
        lda     (ptr1),y
        sta     tmp1
        HOME
        jsr     incsp4
        lda     tmp1
        ldx     #0
        rts
.endproc

.proc   _bank_peek16
        jsr     hsetup
        lda     (ptr1),y
        sta     tmp1
        iny
        lda     (ptr1),y
        sta     tmp2
        HOME
        jsr     incsp4
        lda     tmp1
        ldx     tmp2
        rts
.endproc

.proc   _bank_peek32
        jsr     hsetup
        lda     (ptr1),y
        sta     tmp1
        iny
        lda     (ptr1),y
        sta     tmp2
        iny
        lda     (ptr1),y
        pha
        iny
        lda     (ptr1),y
        pha
        HOME
        jsr     incsp4
        pla
        sta     sreg+1
        pla
        sta     sreg
        lda     tmp1
        ldx     tmp2
        rts
.endproc


; --- the bulk copies (bankmem.h), in assembly: the C was 570 bytes of the
; resident image. A buffer at $A000 or above is in the caller's bank (an
; overlay's), not mapped while the heap's is: then every byte is carried
; across with both banks mapped in turn; below $A000 the heap's bank stays
; mapped for the whole copy.

; void bankmem_read(uint8_t bank, uint16_t off, void *dst, uint16_t len)
_bankmem_read:
        jsr     bulk                    ; ptr1 window, ptr2 dst, ptr3 len, tmp3 bank
        bcs     @slow
        lda     ptr3+1                  ; under 256 bytes (a record, a string)
        bne     @f
        ldy     #0
@s:     cpy     ptr3
        bne     @c
        jmp     home
@c:     lda     (ptr1),y
        sta     (ptr2),y
        iny
        bra     @s
@f:     jsr     next
        beq     @hm
        lda     (ptr1)
        sta     (ptr2)
        jsr     step
        bra     @f
@slow:  jsr     next
        beq     @hm
        lda     tmp3
        sta     RAM_BANK
        lda     (ptr1)
        tax
        lda     _bank_home
        sta     RAM_BANK
        txa
        sta     (ptr2)
        jsr     step
        bra     @slow
@hm:    jmp     home

; void bankmem_write(uint8_t bank, uint16_t off, const void *src, uint16_t len)
_bankmem_write:
        jsr     bulk
        bcs     @slow
        lda     ptr3+1
        bne     @f
        ldy     #0
@s:     cpy     ptr3
        bne     @c
        jmp     home
@c:     lda     (ptr2),y
        sta     (ptr1),y
        iny
        bra     @s
@f:     jsr     next
        beq     @hm
        lda     (ptr2)
        sta     (ptr1)
        jsr     step
        bra     @f
@slow:  jsr     next
        beq     @hm
        lda     _bank_home
        sta     RAM_BANK
        lda     (ptr2)
        tax
        lda     tmp3
        sta     RAM_BANK
        txa
        sta     (ptr1)
        jsr     step
        bra     @slow
@hm:    jmp     home

; void bankmem_set(uint8_t bank, uint16_t off, uint8_t value, uint16_t len)
_bankmem_set:
        sta     ptr3
        stx     ptr3+1
        jsr     popa
        sta     tmp1                    ; the value
        jsr     popax
        sta     ptr1
        txa
        clc
        adc     #WINDOW_HI
        sta     ptr1+1
        jsr     popa
        sta     RAM_BANK
@f:     jsr     next
        beq     @hm
        lda     tmp1
        sta     (ptr1)
        jsr     step
        bra     @f
@hm:    jmp     home

home:   lda     _bank_home
        sta     RAM_BANK
        rts

; the arguments of read and write: len (.A/.X) to ptr3, the buffer to
; ptr2, $A000 + off to ptr1, the bank to tmp3 (and mapped); c set if the
; buffer reaches $A000 (then the program's bank is mapped)
bulk:   sta     ptr3
        stx     ptr3+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        jsr     popax
        sta     ptr1
        txa
        clc
        adc     #WINDOW_HI
        sta     ptr1+1
        jsr     popa
        sta     tmp3
        sta     RAM_BANK
        lda     ptr2                    ; buffer + len > $A000?
        clc
        adc     ptr3
        lda     ptr2+1
        adc     ptr3+1
        cmp     #$A0
        bcc     @ok
        bne     @hi
        lda     ptr2                    ; exactly to $A000 is below
        clc
        adc     ptr3
        cmp     #1
        bcc     @ok
@hi:    sec
        rts
@ok:    clc
        rts

; z set when ptr3 (the count) is 0; else it is counted down
next:   lda     ptr3
        ora     ptr3+1
        beq     @r
        lda     ptr3
        bne     @d
        dec     ptr3+1
@d:     dec     ptr3
        lda     #1                      ; (z clear)
@r:     rts

; both pointers on a byte
step:   inc     ptr1
        bne     @1
        inc     ptr1+1
@1:     inc     ptr2
        bne     @2
        inc     ptr2+1
@2:     rts
