/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <elfloader_common.h>

/* This is a low level binary interface, thus we do not preserve the type
 * information here. All parameters are just register values (or stack values
 * that are register-sized).
 */
typedef void (*init_arm_kernel_t)(word_t ui_p_reg_start,
                                  word_t ui_p_reg_end,
                                  word_t pv_offset,
                                  word_t v_entry,
                                  word_t dtb,
                                  word_t dtb_size);


/* Enable the mmu. */
extern void arm_enable_mmu(void);

/* These functions are very similar however, there are some small differences
 * between the ARMv8 and legacy implementation.
 *
 * New ARMv8 implementation:
 * - Does the MMU disabling. This is to keep the time spent with MMU off low.
 * - Is only meant if seL4 runs in EL2.
 */
#if defined(CONFIG_ARCH_AARCH64)
/* Switches MMU-related stuff: pagetables, MAIR & TCR etc. Works also if the MMU
 * was off initially. EL2 translation regime only.
 */
extern void arm_switch_to_hyp_tables(void);
#else
extern void arm_enable_hyp_mmu(void);
#endif


/* Setup boot VSpace. */
void init_boot_vspace(struct image_info *kernel_info);
void init_hyp_boot_vspace(struct image_info *kernel_info);

/* Assembly functions. */
extern void flush_dcache(void);
extern void cpu_idle(void);


void smp_boot(void);

/* Secure monitor call */
uint32_t smc(uint32_t, uint32_t, uint32_t, uint32_t);
