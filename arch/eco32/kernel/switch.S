/*
 * Linux architectural port borrowing liberally from similar works of
 * others, namely OpenRISC and RISC-V.  All original copyrights apply
 * as per the original source declaration.
 *
 * Modifications for ECO32:
 * Copyright (c) 2018 Hellwig Geisse
 * Copyright (c) 2018 Martin Hofmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/ptrace.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/asm-offsets.h>

#include <linux/linkage.h>


    .text

/*
 * struct thread_info *_switch(
 *     struct thread_info *old_ti,
 *     struct thread_info *new_ti);
 *
 * Finally switch to another thread by saving the callee-save registers on
 * the stack and load the respective ones from the new thread stack
 *
 * standard ABI conventions are applicable
 */
ENTRY(_switch)
    subi    $29,$29,PT_SIZE     # allocate pt_regs on old stack
    stw     $16,$29,r16_OFF     # save callee-save regs
    stw     $17,$29,r17_OFF
    stw     $18,$29,r18_OFF
    stw     $19,$29,r19_OFF
    stw     $20,$29,r20_OFF
    stw     $21,$29,r21_OFF
    stw     $22,$29,r22_OFF
    stw     $23,$29,r23_OFF
    stw     $28,$29,r28_OFF     # save frame pointer
    stw     $31,$29,r31_OFF     # save return address
    stw     $29,$4,TI_KSP       # save stack pointer in old_ti
    # --------old----------
    addi    $27,$5,0            # set new thread_info
    # --------new----------
    ldw     $29,$5,TI_KSP       # load stack pointer from new_ti
    ldw     $16,$29,r16_OFF     # load callee-save regs
    ldw     $17,$29,r17_OFF
    ldw     $18,$29,r18_OFF
    ldw     $19,$29,r19_OFF
    ldw     $20,$29,r20_OFF
    ldw     $21,$29,r21_OFF
    ldw     $22,$29,r22_OFF
    ldw     $23,$29,r23_OFF
    ldw     $28,$29,r28_OFF     # load frame pointer
    ldw     $31,$29,r31_OFF     # load return address
    addi    $29,$29,PT_SIZE     # free pt_regs on new stack
    # ---------------------
    addi    $2,$4,0             # return old_ti
    ldw     $4,$4,TI_TASK       # setup arg for schedule_tail
    jr      $31
ENDPROC(_switch)

/*
 * after fork/clone we will arive here
 * it all depends on if we have a new kernel thread or a user thread
 * we need to call schedule tail no matter what.
 * Kernel threads can be called directly while user threads are
 * passed over to the kernel exit.
 */
ENTRY(ret_from_fork)
    jal     schedule_tail       # task_struct *prev must be in $4
    beq     $16,$0,no_kthread   # skip if we are not a kernel thread
    addi    $4,$17,0            # else get arg for callback function
    jalr    $16                 # and execute the callback function
no_kthread:
    j       rfxcept
END(ret_from_fork)