;
; zpbss_veranda.s -- what golden_init() clears on Veranda (banked_ram.c):
; the zero-page variables (ZPBSS at $50, ZPBSS2 at $D4) and GOLDENBSS,
; which is ordinary BSS here ($0400-$07FF are Veranda's).
;
        .export         _zpbss_start, _zpbss_size
        .export         _zpbss2_start, _zpbss2_size
        .export         _golden_start, _golden_size
        .import         __ZPBSS_RUN__, __ZPBSS_SIZE__
        .import         __ZPBSS2_RUN__, __ZPBSS2_SIZE__
        .import         __GOLDENBSS_RUN__, __GOLDENBSS_SIZE__

        .rodata

_zpbss_start:   .word   __ZPBSS_RUN__
_zpbss_size:    .word   __ZPBSS_SIZE__
_zpbss2_start:  .word   __ZPBSS2_RUN__
_zpbss2_size:   .word   __ZPBSS2_SIZE__
_golden_start:  .word   __GOLDENBSS_RUN__
_golden_size:   .word   __GOLDENBSS_SIZE__
