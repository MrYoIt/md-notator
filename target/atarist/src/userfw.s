; userfw.s - md-notator user firmware module
; Minimal m68k stub that just boots to GEMDOS
; The dongle logic runs entirely on the RP2040

    section .text
    xdef    userfw

userfw:
    ; Print boot message
    pea     boot_msg(pc)
    move.w  #9,-(sp)        ; Cconws
    trap    #1
    addq.l  #6,sp

    ; Boot to GEM
    clr.w   -(sp)
    trap    #1              ; Pterm0

boot_msg:
    dc.b    27,'E'          ; Clear screen
    dc.b    'md-notator v1.0',13,10
    dc.b    'Notator Dongle Active',13,10
    dc.b    'Booting...',13,10
    dc.b    0
    even
