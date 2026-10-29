/* PIC18 instruction implementation of the state machine recovered at
 * 0x00ad7a-0x00aedc. Bank-local state avoids C pointer arithmetic in the
 * 96-instruction-cycle Timer3 sampling loop. Original GPIO is RB2.
 * Timer3 is polled while the radio interrupt owns the CPU, as in the original.
 */
#include <xc.inc>
GLOBAL _radio_capture_asm, _raw_capture
PSECT radioCapture,class=CODE,reloc=2
_radio_capture_asm:
    movlb 0x0e
capture_loop:
    movlw 12
    subwf 0x0c,w,b
    bc capture_done
    movlw 6
    subwf 0x12,w,b
    bc capture_done
    movlw 0x88
    subwf 0x14,w,b
    movlw 0x13
    subwfb 0x15,w,b
    bc capture_done
capture_wait:
    btfss PIR2,1,c
    bra capture_wait
    setf TMR3H,c
    movlw 0xa0
    movwf TMR3L,c
    bcf PIR2,1,c
    incf 0x14,f,b
    movlw 0
    addwfc 0x15,f,b
    movf 0x0e,w,b
    addwf WREG,w,c
    movwf 0x0e,b
    btfsc PORTB,2,c
    bsf 0x0e,0,b
    movlw 7
    andwf 0x0e,w,b
    movwf 0x0f,b
    xorlw 7
    bz capture_high
    movf 0x0f,w,b
    xorlw 6
    bz capture_high
    movf 0x0f,w,b
    xorlw 5
    bz capture_high
    movf 0x0f,w,b
    xorlw 3
    bz capture_high
    incf 0x10,f,b
    clrf 0x11,b
    bra capture_dispatch
capture_high:
    incf 0x11,f,b
    clrf 0x10,b
capture_dispatch:
    movf 0x12,w,b
    bz capture_phase0
    xorlw 1
    bz capture_phase1
    xorlw 3
    bz capture_phase2
    xorlw 1
    bz capture_phase3
    xorlw 7
    bz capture_phase4
    xorlw 1
    bz capture_phase5
    bra capture_loop
capture_phase0:
    movlw 100
    subwf 0x10,w,b
    bnc capture_loop
    incf 0x12,f,b
    clrf 0x10,b
    bra capture_loop
capture_phase1:
    tstfsz 0x11,b
    incf 0x12,f,b
    bra capture_loop
capture_phase2:
    tstfsz 0x10,b
    bra capture_restart
    movlw 20
    subwf 0x11,w,b
    bnc capture_loop
    incf 0x12,f,b
    bra capture_loop
capture_restart:
    clrf 0x12,b
    bra capture_loop
capture_phase3:
    tstfsz 0x10,b
    bra capture_start_data
    bra capture_loop
capture_start_data:
    incf 0x12,f,b
    setf 0x13,b
    clrf 0x0c,b
    clrf 0x0d,b
    movlw 0x80
    movwf 0x16,b
    bra capture_loop
capture_phase4:
    movlw 21
    subwf 0x10,w,b
    bc capture_done
    tstfsz 0x10,b
    bra capture_low_data
    incf 0x13,f,b
    incf 0x12,f,b
    bra capture_loop
capture_low_data:
    decf 0x13,f,b
    bra capture_loop
capture_phase5:
    movlw 21
    subwf 0x11,w,b
    bc capture_done
    tstfsz 0x11,b
    bra capture_high_data
    btfss 0x13,7,b
    bra capture_bit_done
capture_store:
    movf 0x0c,w,b
    movwf FSR0L,c
    movlw 0x0e
    movwf FSR0H,c
    movf 0x16,w,b
    iorwf INDF0,f,c
capture_bit_done:
    bcf STATUS,0,c
    rrcf 0x16,f,b
    incf 0x0d,f,b
    movlw 8
    subwf 0x0d,w,b
    bnz capture_next_bit
    clrf 0x0d,b
    incf 0x0c,f,b
    movlw 0x80
    movwf 0x16,b
capture_next_bit:
    setf 0x13,b
    decf 0x12,f,b
    bra capture_loop
capture_high_data:
    incf 0x13,f,b
    bra capture_loop
capture_done:
    return
