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

/*
 *
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines, compile
 * this file to assembler, and then extract the #defines
 * from the assembly-language output.
 */

#include <linux/ptrace.h>
#include <linux/thread_info.h>
#include <linux/kbuild.h>


int main(int argc, char* argv[])
{
    /* offsets into task_struct */

    /* offsets into thread_info */
    DEFINE(TI_TASK, offsetof(struct thread_info, task));
    DEFINE(TI_FLAGS, offsetof(struct thread_info, flags));
    DEFINE(TI_PREEMPT, offsetof(struct thread_info, preempt_count));
    DEFINE(TI_KSP, offsetof(struct thread_info, ksp));

    DEFINE(PT_SIZE, sizeof(struct pt_regs));

    return 0;
}
