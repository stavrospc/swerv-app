// ; startup code to support HLL programs
// ; snatched from $RV_ROOT/testbench/asm/crt0.s

#include "defines.h"

.section .text.init
.global _start
_start:
        la sp, STACK

    	csrw minstret, zero
    	csrw minstreth, zero

    	li x1, 0x55555555
    	csrw 0x7c0, x1

        call main

.global _finish
_finish:
        la t0, tohost
#ifdef IS_WHISPER
        li t1, 1
        sw t1, 0(t0)
#else
        li t1, 0xff
        sw t1, 0(t0)
#endif
        beq x0, x0, _finish
        .rept 10
        nop
        .endr

.section .text
.global smc_snippet
smc_snippet:
// ; Load addr of target instr 
    la a5, target
// ; These instructions serve as a delay
// ; for the store argument address. They
// ; ensure that the execution window of
// ; spec_code is as long as possible.
    li t0, 2
    j target
continue:
    .rept 10
    mul a5, a5, t0
    .endr

// ;Code to write as data: 1 nop (overwriting lines of target)
    li a0, 0x0001

// ;Store at target addr. Also: the last retired instr
// ;from which the execution will resume after the SMC MC
    sh a0, 0(a5)

// ;Target instruction to be modified
target:
    j spec_code
    nop
    nop
    nop

// ;Architectural exit point of the function
    la a0, control
    ret

// ;Code executed speculatively (flushed after SMC MC).
spec_code:
    addi t0, t0, -1
    bne t0, zero, continue
    la a0, secret
    ret

.section .data
control:    .string "This is the expected behavior.\0"
secret:     .string "PASSWORD123\0"
.section .data.io
.global tohost
tohost: .word 0
