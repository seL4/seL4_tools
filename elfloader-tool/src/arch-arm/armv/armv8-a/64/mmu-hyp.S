/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <assembler.h>
#include <armv/assembler.h>

#ifdef CONFIG_ARM_PA_SIZE_BITS_40
#define TCR_PS TCR_PS_1T
#else
#define TCR_PS TCR_PS_16T
#endif

.text

.extern flush_dcache
.extern invalidate_dcache
.extern invalidate_icache
.extern _boot_pgd_down

BEGIN_FUNC(disable_mmu_caches_hyp)
    /* Assume D-cache already cleaned to PoC */
    disable_mmu sctlr_el2, x9
    ret
END_FUNC(disable_mmu_caches_hyp)

BEGIN_FUNC(clean_dcache_by_range)
    /* Ordering needed for strongly-ordered mem, not needed for NORMAL mem.
     * See ARM DDI 0487I.a, page D7-5063.
     */
    dmb     sy

    /* Extract minimum DCache CL size into x3 and CL mask into x4 */
    mrs x2, ctr_el0
    ubfx x4, x2, #16, #4
    mov x3, #4
    lsl x3, x3, x4
    sub x4, x3, #1

    /* Apply mask to start address before entering the loop */
    bic x4, x0, x4
clean_dcache_by_range_loop:
    dc cvac, x4
    add x4, x4, x3
    cmp x4, x1
    b.lt clean_dcache_by_range_loop
    dsb sy
    isb
    ret
END_FUNC(clean_dcache_by_range)

BEGIN_FUNC(leave_hyp)
    /* We call nested functions, follow the ABI. */
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    /* Ensure I-cache, D-cache and mmu are disabled for EL1/Stage2 */
    mov     x9, #(1 << 31)
    msr     hcr_el2, x9

    /* Ensure traps to EL2 are disabled */
    mov     x9, #0x33ff
    msr     cptr_el2, x9
    msr     hstr_el2, xzr
    msr     vttbr_el2, xzr

    /* Ensure I-cache, D-cache and mmu are disabled for EL1/Stage1 */
    disable_mmu sctlr_el1 , x9

    mov     x9, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_D_BIT | PSR_MODE_EL1h)
    msr     spsr_el2, x9

    /* Let's the caller use our stack, in case it needs to pop something */
    ldp     x29, x30, [sp], #16
    mov     x10, sp
    msr     sp_el1, x10
    msr     elr_el2, x30
    eret
END_FUNC(leave_hyp)

BEGIN_FUNC(arm_enable_hyp_mmu)
    /*
     *   DEVICE_nGnRnE      000     00000000
     *   DEVICE_nGnRE       001     00000100
     *   DEVICE_GRE         010     00001100
     *   NORMAL_NC          011     01000100
     *   NORMAL             100     11111111
     *   NORMAL_WT          101     10101010
     */
    ldr     x5, =MAIR(0x00, MT_DEVICE_nGnRnE) | \
                 MAIR(0x04, MT_DEVICE_nGnRE) | \
                 MAIR(0x0c, MT_DEVICE_GRE) | \
                 MAIR(0x44, MT_NORMAL_NC) | \
                 MAIR(0xff, MT_NORMAL) | \
                 MAIR(0xaa, MT_NORMAL_WT)
    msr     mair_el2, x5
    ldr     x8, =TCR_T0SZ(48) | TCR_IRGN0_WBWC | TCR_ORGN0_WBWC | TCR_SH0_ISH | TCR_TG0_4K | TCR_PS | TCR_EL2_RES1
    msr     tcr_el2, x8
    isb

    adrp    x8, _boot_pgd_down
    msr     ttbr0_el2, x8
    isb

    tlbi    alle2is
    dsb     ish
    isb

    /* Invalidate icache */
    ic  ialluis
    dsb sy
    isb

    enable_mmu  sctlr_el2, x8
    /* NOTE: enable_mmu already contains an isb after enabling. */

    ret
END_FUNC(arm_enable_hyp_mmu)
