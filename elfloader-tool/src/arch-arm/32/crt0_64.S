/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/* This file is used to switch from aarch64 EL3/EL2 to aarch32 HYP/SVC
 * before continuing in the aarch32 elfloader. This allows aarch32 seL4 kernel
 * configurations to be loaded from a 64bit entry point
 */
#include <kernel/gen_config.h>
#include <assembler.h>
#include <armv/assembler.h>

.section ".text.start"

BEGIN_FUNC(_start)
    /* Decide the current exception level */
    mrs x9, CurrentEL

    cmp x9, CUR_EL3
    b.eq 3f
    cmp x9, CUR_EL2
    b.eq 2f

3:
    /* Disable all traps to EL3 and EL2 */
    msr cptr_el3, xzr
    mov x9, #CPTR_EL2_RES1
    msr cptr_el2, x9

    /* Ensure I-cache, D-cache and mmu are disabled for EL2/Stage1 */
    disable_mmu sctlr_el2, x9

    /* AArch32 HYP only exists in non-secure world */
    ldr x9, =(SCR_HCE_BIT | SCR_SMD_BIT | SCR_EL3_RES1 | SCR_NS_BIT)
    msr scr_el3, x9

    /* Setup spsr to return to */
#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
    ldr x9, =(PSR_M_BIT | PSR_MODE_HYP_32)
#else
    ldr x9, =(PSR_M_BIT | PSR_MODE_SVC_32)
#endif
    msr     spsr_el3, x9

    /* Set return address to the instruction after eret. */
    adr     x9, 1f
    msr     elr_el3, x9
    eret

2:
    /* Clean EL2 dcache */
    dcache  cisw

    /* Ensure I-cache, D-cache and mmu are disabled for EL2/Stage1 */
    disable_mmu sctlr_el2, x9

    /*
     * Invalidate the local I-cache so that any instructions fetched
     * speculatively are discarded.
     */
    ic      iallu
    dsb     nsh
    isb

    msr     sctlr_el1, xzr
    /* Ensure traps to EL2 are disabled */
    mov     x9, #0x33ff
    msr     cptr_el2, x9
    msr     hstr_el2, xzr
    msr     mdcr_el2, xzr

    /* Set a zero VMID */
    msr     vttbr_el2, xzr

    /* Set lower level execution state to aarch32 */
    msr     hcr_el2, xzr
    mov     x17, #0
    bic     x17, x17, #(1 << 31)
    msr     hcr_el2, x17

    /* Setup spsr to return to */
    mov     x18, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_MODE_SVC_32)
    msr     spsr_el2, x18

    /* Set our return address to the instruction after eret. */
    adr     x19, 1f
    msr     elr_el2, x19
    eret
    1:
