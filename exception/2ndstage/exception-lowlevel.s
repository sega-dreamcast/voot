    .section .text

    .global _exception_handler_lowlevel
    .global _general_sub_handler
    .global _general_sub_handler_base
    .global _general_sub_handler_end
    .global _my_exception_finish
    .global _cache_sub_handler
    .global _cache_sub_handler_base
    .global _cache_sub_handler_end
    .global _interrupt_sub_handler
    .global _interrupt_sub_handler_base
    .global _interrupt_sub_handler_end
    .global _ubc_wait

!
! HANDLE GENERAL EXCEPTIONS
!

_exception_handler_lowlevel:
general_exception:
!BEGIN REGISTER SAVE
!GENERAL
    mov.l   r0, @-r15
    mov     #1, r0
    mov.l   r0, @-r15
general_exception_xt:
    mov.l   r1, @-r15
    mov.l   r2, @-r15
    mov.l   r3, @-r15
    mov.l   r4, @-r15
    mov.l   r5, @-r15
    mov.l   r6, @-r15
    mov.l   r7, @-r15
    mov.l   r8, @-r15
    mov.l   r9, @-r15
    mov.l   r10, @-r15
    mov.l   r11, @-r15
    mov.l   r12, @-r15
    mov.l   r13, @-r15
    mov.l   r14, @-r15
!CONTROL
    stc.l   r0_bank, @-r15
    stc.l   r1_bank, @-r15
    stc.l   r2_bank, @-r15
    stc.l   r3_bank, @-r15
    stc.l   r4_bank, @-r15
    stc.l   r5_bank, @-r15
    stc.l   r6_bank, @-r15
    stc.l   r7_bank, @-r15
    stc.l   dbr, @-r15
    stc.l   sr, @-r15
    stc.l   gbr, @-r15
    stc.l   vbr, @-r15
!SAVED CONTROL (exception)
!    stc.l   ssr, @-r15
!    stc.l   sgr, @-r15
!    stc.l   spc, @-r15
!SYSTEM
    sts.l   macl, @-r15
    sts.l   mach, @-r15
    sts.l   pr, @-r15
    sts.l   fpul, @-r15
    sts.l   fpscr, @-r15
!END REGISTER SAVE

    ! Arguments are passed on r4
    mov     r15, r4

    mov.l   hdl_except, r2
    jsr     @r2
    nop

    ! The return value is put in r0

!BEGIN REGISTER RESTORE
!SYSTEM
    lds.l   @r15+, fpscr
    lds.l   @r15+, fpul
    lds.l   @r15+, pr
    lds.l   @r15+, mach
    lds.l   @r15+, macl
!CONTROL
    ldc.l   @r15+, vbr
    ldc.l   @r15+, gbr
    ldc.l   @r15+, sr
    ldc.l   @r15+, dbr
    ldc.l   @r15+, r7_bank
    ldc.l   @r15+, r6_bank
    ldc.l   @r15+, r5_bank
    ldc.l   @r15+, r4_bank
    ldc.l   @r15+, r3_bank
    ldc.l   @r15+, r2_bank
    ldc.l   @r15+, r1_bank
    ldc.l   @r15+, r0_bank
!GENERAL
    mov.l   @r15+, r14
    mov.l   @r15+, r13
    mov.l   @r15+, r12
    mov.l   @r15+, r11
    mov.l   @r15+, r10
    mov.l   @r15+, r9
    mov.l   @r15+, r8
    mov.l   @r15+, r7
    mov.l   @r15+, r6
    mov.l   @r15+, r5
    mov.l   @r15+, r4
    mov.l   @r15+, r3
    mov.l   @r15+, r2
    mov.l   @r15+, r1

! Skip the exception code
    add     #4, r15

    jmp     @r0
    mov.l   @r15+, r0
!END REGISTER RESTORE

_general_sub_handler:
general_exception_handler:
    nop
    mov.l   r0, @-r15
    mov     #3, r0
    mov.l   r0, @-r15
    mov.l   general_handler, r0
    jmp     @r0
    nop

    .align 4
general_handler:
    .long   general_exception_xt

_general_sub_handler_base:
    nop
    bra     general_exception_handler
    nop
_general_sub_handler_end:

_my_exception_finish:
    stc     sgr, r15    ! Recover in case of missing stack (paranoia)
    rte
    nop

! All 32-bit reference variables go after this. They need to be 4-byte
! aligned, I guess.
    .align  4
hdl_except:
    .long   _exception_handler

!
! HANDLE CACHE EXCEPTIONS
!

cache_exception:
    nop
    bra     cache_exception_handler
    nop

_cache_sub_handler:
cache_exception_handler:
    nop
    mov.l   r0, @-r15
    mov     #3, r0
    mov.l   r0, @-r15
    mov.l   cache_handler, r0
    jmp     @r0
    nop

    .align 4
cache_handler:
    .long   general_exception_xt

_cache_sub_handler_base:
    nop
    bra     cache_exception_handler
    nop
_cache_sub_handler_end:

!
! HANDLE INTERRUPT EXCEPTIONS
!

interrupt_exception:
    nop
    bra     interrupt_exception_handler
    nop

_interrupt_sub_handler:
interrupt_exception_handler:
    nop
    mov.l   r0, @-r15
    mov     #3, r0
    mov.l   r0, @-r15
    mov.l   interrupt_handler, r0
    jmp     @r0
    nop

    .align 4
interrupt_handler:
    .long   general_exception_xt

_interrupt_sub_handler_base:
    nop
    bra     interrupt_exception_handler
    nop
_interrupt_sub_handler_end:

!
! Wait enough instructions for the UBC to be refreshed
!

_ubc_wait:
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    rts
    nop
