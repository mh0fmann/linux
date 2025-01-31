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

#ifndef __ASM_ECO32_UACCESS_H
#define __ASM_ECO32_UACCESS_H

/*
 * User space memory access functions
 */

#include <linux/errno.h>
#include <linux/thread_info.h>
#include <linux/prefetch.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/page.h>
#include <asm/ptrace.h>

#define VERIFY_READ     0
#define VERIFY_WRITE    1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

/*
 * Addr_limit is the maximum accessible address for the task. We misuse
 * the KERNEL_DS and USER_DS values to both assign and compare the
 * addr_limit values through the equally misnamed get/set_fs macros.
 */

#define KERNEL_DS   (~0UL)
#define get_ds()    (KERNEL_DS)

#define USER_DS     (TASK_SIZE)
#define get_fs()    (current_thread_info()->addr_limit)
#define set_fs(x)   (current_thread_info()->addr_limit = (x))

#define segment_eq(a, b)    ((a) == (b))

/*
 * Ensure that the range from addr to addr+size is all within
 * the process' address space.
 */
#define __range_ok(addr, size) (size <= get_fs() && addr <= (get_fs()-size))

/* Ensure that addr is below task's addr_limit. */
#define __addr_ok(addr) ((unsigned long) addr < get_fs())

#define access_ok(addr, size) \
    __range_ok((unsigned long) addr, (unsigned long) size)

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry {
    unsigned long insn, fixup;
};

/* Fix a exception */
extern int fixup_exception(struct pt_regs *state);

/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the uglyness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 *
 * As we use the same address space for kernel and user data on the
 * ECO32, we can just do these as direct assignments.  (Of course, the
 * exception handling means that it's no longer "just"...)
 */
#define get_user(x, ptr) \
    __get_user_check((x), (ptr), sizeof(*(ptr)))
#define put_user(x, ptr) \
    __put_user_check((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

#define __get_user(x, ptr) \
    __get_user_nocheck((x), (ptr), sizeof(*(ptr)))
#define __put_user(x, ptr) \
    __put_user_nocheck((__typeof__(*(ptr)))(x), (ptr), sizeof(*(ptr)))

extern long __put_user_bad(void);

#define __put_user_nocheck(x, ptr, size)            \
    ({                                              \
     long __pu_err;                                 \
     __put_user_size((x), (ptr), (size), __pu_err); \
     __pu_err;                                      \
    })

#define __put_user_check(x, ptr, size)                  \
    ({                                                  \
     long __pu_err = -EFAULT;                           \
     __typeof__(*(ptr)) *__pu_addr = (ptr);             \
     if (access_ok(__pu_addr, size))                    \
     __put_user_size((x), __pu_addr, (size), __pu_err); \
     __pu_err;                                          \
     })

#define __put_user_size(x, ptr, size, retval)                       \
    do {                                                            \
        retval = 0;                                                 \
        switch (size) {                                             \
            case 1: __put_user_asm(x, ptr, retval, "stb"); break;   \
            case 2: __put_user_asm(x, ptr, retval, "sth"); break;   \
            case 4: __put_user_asm(x, ptr, retval, "stw"); break;   \
            case 8: __put_user_asm2(x, ptr, retval); break;         \
            default: __put_user_bad();                              \
        }                                                           \
    } while (0)

struct __large_struct {
    unsigned long buf[100];
};
#define __m(x)  (*(struct __large_struct *)(x))

/*
 * We don't tell gcc that we are accessing memory, but this is OK
 * because we do not write to any memory gcc knows about, so there
 * are no aliasing issues.
 */
#define __put_user_asm(x, addr, err, op)                \
    __asm__ __volatile__(                               \
            "1: "op" %1,%2,0\n"                         \
            "2: \n"                                     \
            "   .section .fixup,\"ax\"\n"               \
            "3: addi %0,$0,%3\n"                        \
            "   j 2b\n"                                 \
            "   .previous\n"                            \
            "   .section __ex_table,\"a\"\n"            \
            "   .align 4\n"                             \
            "   .long 1b,3b\n"                          \
            "   .previous"                              \
            : "=r"(err)                                 \
            : "r"(x), "r"(addr), "i"(-EFAULT), "0"(err))

#define __put_user_asm2(x, addr, err)           \
    __asm__ __volatile__(                       \
            "1: stw %1,%2,0\n"                  \
            "2: stw %L1,%2,4\n"                 \
            "3: \n"                             \
            "   .section .fixup,\"ax\"\n"       \
            "4: addi %0,$0,%3\n"                \
            "   j 3b\n"                         \
            "   .previous\n"                    \
            "   .section __ex_table,\"a\"\n"    \
            "   .align 4\n"                     \
            "   .long 1b,4b\n"                  \
            "   .long 2b,4b\n"                  \
            "   .previous"                      \
            : "=r"(err)                         \
            : "r"(x), "r"(addr), "i"(-EFAULT), "0"(err))

#define __get_user_nocheck(x, ptr, size)                    \
    ({                                                      \
     long __gu_err, __gu_val;                               \
     __get_user_size(__gu_val, (ptr), (size), __gu_err);    \
     (x) = (__force __typeof__(*(ptr)))__gu_val;            \
     __gu_err;                                              \
    })

#define __get_user_check(x, ptr, size)                          \
    ({                                                          \
     long __gu_err = -EFAULT, __gu_val = 0;                     \
     const __typeof__(*(ptr)) * __gu_addr = (ptr);              \
     if (access_ok(__gu_addr, size))                            \
     __get_user_size(__gu_val, __gu_addr, (size), __gu_err);    \
     (x) = (__force __typeof__(*(ptr)))__gu_val;                \
     __gu_err;                                                  \
    })

extern long __get_user_bad(void);

#define __get_user_size(x, ptr, size, retval)                       \
    do {                                                            \
        retval = 0;                                                 \
        switch (size) {                                             \
            case 1: __get_user_asm(x, ptr, retval, "ldbu"); break;  \
            case 2: __get_user_asm(x, ptr, retval, "ldhu"); break;  \
            case 4: __get_user_asm(x, ptr, retval, "ldw"); break;   \
            case 8: __get_user_asm2(x, ptr, retval);                \
            default: (x) = __get_user_bad();                        \
        }                                                           \
    } while (0)

#define __get_user_asm(x, addr, err, op)        \
    __asm__ __volatile__(                       \
            "1: "op" %1,%2,0\n"                 \
            "2: \n"                             \
            "   .section .fixup,\"ax\"\n"       \
            "3: addi %0,$0,%3\n"                \
            "   addi %1,$0,0\n"                 \
            "   j 2b\n"                         \
            "   .previous\n"                    \
            "   .section __ex_table,\"a\"\n"    \
            "   .align 4\n"                     \
            "   .long 1b,3b\n"                  \
            "   .previous"                      \
            : "=r"(err), "=r"(x)                \
            : "r"(addr), "i"(-EFAULT), "0"(err))

#define __get_user_asm2(x, addr, err)           \
    __asm__ __volatile__(                       \
            "1: ldw %1,%2,0\n"                  \
            "2: ldw %L1,%2,4\n"                 \
            "3: \n"                             \
            "   .section .fixup,\"ax\"\n"       \
            "4: addi %0,$0,%3\n"                \
            "   addi %1,$0,0\n"                 \
            "   addi %L1,$0,0\n"                \
            "   j 3b\n"                         \
            "   .previous\n"                    \
            "   .section __ex_table,\"a\"\n"    \
            "   .align 4\n"                     \
            "   .long 1b,4b\n"                  \
            "   .long 2b,4b\n"                  \
            "   .previous"                      \
            : "=r"(err), "=&r"(x)               \
            : "r"(addr), "i"(-EFAULT), "0"(err))

/* more complex routines */

extern unsigned long __must_check
__copy_tofrom_user(void* to, const void* from, unsigned long size);

static inline unsigned long
raw_copy_from_user(void* to, const void __user* from, unsigned long len)
{
    return __copy_tofrom_user(to, from, len);
}

static inline unsigned long
raw_copy_to_user(void __user* to, const void* from, unsigned long len)
{
    return __copy_tofrom_user(to, from, len);
}

#define INLINE_COPY_FROM_USER
#define INLINE_COPY_TO_USER

extern unsigned long __clear_user(void* addr, unsigned long size);

static inline __must_check unsigned long
clear_user(void* addr, unsigned long size)
{

    if (access_ok(addr, size)) {
        return __clear_user(addr, size);
    }

    if ((unsigned long)addr < TASK_SIZE) {
        unsigned long over = (unsigned long)addr + size - TASK_SIZE;
        return __clear_user(addr, size - over) + over;
    }

    return size;
}

#define user_addr_max()     (segment_eq(get_fs(), USER_DS) ? TASK_SIZE : ~0UL)

extern long strncpy_from_user(char* dest, const char __user* src, long count);

extern __must_check long strlen_user(const char __user* str);
extern __must_check long strnlen_user(const char __user* str, long n);

#endif /* __ASM_ECO32_UACCESS_H */
