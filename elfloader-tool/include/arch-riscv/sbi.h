/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <elfloader_common.h>
#include <types.h>

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI 4
#define SBI_REMOTE_FENCE_I 5
#define SBI_REMOTE_SFENCE_VMA 6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN 8

#define SBI_CALL(which, arg0, arg1, arg2) ({            \
    register word_t a0 asm ("a0") = (word_t)(arg0);   \
    register word_t a1 asm ("a1") = (word_t)(arg1);   \
    register word_t a2 asm ("a2") = (word_t)(arg2);   \
    register word_t a7 asm ("a7") = (word_t)(which);  \
    asm volatile ("ecall"                   \
              : "+r" (a0)               \
              : "r" (a1), "r" (a2), "r" (a7)        \
              : "memory");              \
    a0;                         \
})

typedef enum {
    SBI_SUCCESS                 = 0,
    SBI_ERR_FAILED              = -1,
    SBI_ERR_NOT_SUPPORTED       = -2,
    SBI_ERR_INVALID_PARAM       = -3,
    SBI_ERR_DENIED              = -4,
    SBI_ERR_INVALID_ADDRESS     = -5,
    SBI_ERR_ALREADY_AVAILABLE   = -6,
    SBI_ERR_ALREADY_STARTED     = -7,
    SBI_ERR_ALREADY_STOPPED     = -8
} sbi_call_ret_t;

#define  SBI_HSM 0x48534DULL
#define  SBI_HSM_HART_START 0

typedef struct {
    sbi_call_ret_t code;
    word_t data;
} sbi_hsm_ret_t;

#define SBI_EXT_CALL(extension, which, arg0, arg1, arg2, var_sbi_hsm_ret) \
    do {  \
        register word_t a0 asm ("a0") = (word_t)(arg0);   \
        register word_t a1 asm ("a1") = (word_t)(arg1);   \
        register word_t a2 asm ("a2") = (word_t)(arg2);   \
        register word_t a6 asm ("a6") = (word_t)(which);  \
        register word_t a7 asm ("a7") = (word_t)(extension); \
        asm volatile ("ecall"                   \
                : "+r" (a0), "+r" (a1)               \
                : "r" (a2), "r" (a6), "r" (a7)      \
                : "memory");              \
        (var_sbi_hsm_ret).code = a0;                         \
        (var_sbi_hsm_ret).data = a1;                         \
    } while(0)

#define SBI_HSM_CALL(which, arg0, arg1, arg2, var_sbi_hsm_ret) \
    SBI_EXT_CALL(SBI_HSM, (which), (arg0), (arg1), (arg2), var_sbi_hsm_ret)

/* Lazy implementations until SBI is finalized */
#define SBI_CALL_0(which) SBI_CALL(which, 0, 0, 0)
#define SBI_CALL_1(which, arg0) SBI_CALL(which, arg0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, arg0, arg1, 0)

static inline void sbi_console_putchar(int ch)
{
    /* OpenSBI implements a generic console, it hides any UART specific details
     * like writing a '\r' (CR) before a '\n' (LF).
     */
    SBI_CALL_1(SBI_CONSOLE_PUTCHAR, ch);
}

static inline int sbi_console_getchar(void)
{
    return SBI_CALL_0(SBI_CONSOLE_GETCHAR);
}

static inline void sbi_set_timer(uint64_t stime_value)
{
#if __riscv_xlen == 32
    SBI_CALL_2(SBI_SET_TIMER, stime_value, stime_value >> 32);
#else
    SBI_CALL_1(SBI_SET_TIMER, stime_value);
#endif
}

static inline void sbi_shutdown(void)
{
    SBI_CALL_0(SBI_SHUTDOWN);
}

static inline void sbi_clear_ipi(void)
{
    SBI_CALL_0(SBI_CLEAR_IPI);
}

static inline void sbi_send_ipi(const word_t *hart_mask)
{
    SBI_CALL_1(SBI_SEND_IPI, hart_mask);
}

static inline void sbi_remote_fence_i(const word_t *hart_mask)
{
    SBI_CALL_1(SBI_REMOTE_FENCE_I, hart_mask);
}

static inline void sbi_remote_sfence_vma(const word_t *hart_mask,
                                         UNUSED word_t start,
                                         UNUSED word_t size)
{
    SBI_CALL_1(SBI_REMOTE_SFENCE_VMA, hart_mask);
}

static inline void sbi_remote_sfence_vma_asid(const word_t *hart_mask,
                                              UNUSED word_t start,
                                              UNUSED word_t size,
                                              UNUSED word_t asid)
{
    SBI_CALL_1(SBI_REMOTE_SFENCE_VMA_ASID, hart_mask);
}

static inline sbi_hsm_ret_t sbi_hart_start(const word_t hart_id,
                                           void (*start)(word_t hart_id,
                                                         word_t arg),
                                           word_t arg)
{
    sbi_hsm_ret_t ret = { 0 };
    SBI_HSM_CALL(SBI_HSM_HART_START, hart_id, start, arg, ret);
    return ret;
}
