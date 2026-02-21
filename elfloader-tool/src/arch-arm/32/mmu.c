/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <types.h>
#include <elfloader.h>
#include <mode/structures.h>

#define ARM_VECTOR_TABLE    0xffff0000 // Configured by setting bit 13 in SCTLR
extern char arm_vector_table[1];
/*
 * Create a "boot" page directory, which contains a 1:1 mapping for Elfoader, so it
 * can continue executing till the jump to the kernel, and a virtual-to-physical
 * mapping of the kernel. The vector table is mapped to handle async aborts when
 * interrupts are briefly enabled in arm_enable_mmu(). To keep things simple and
 * robust, no device memory is mapped.
 */
void init_boot_vspace(struct image_info *kernel_info)
{
    uint32_t i, n;
    /* Kernel image: */
    vaddr_t first_vaddr = kernel_info->virt_region_start;
    vaddr_t last_vaddr = kernel_info->virt_region_end;
    paddr_t first_paddr = ROUND_DOWN(kernel_info->phys_region_start, ARM_SECTION_BITS);
    /* Elfloader image: */
    vaddr_t start = (vaddr_t)_text >> ARM_SECTION_BITS;
    vaddr_t end = (vaddr_t)_end >> ARM_SECTION_BITS;

    /* Identity mapping of Elfloader itself: */
    for (i = start; i <= end; i++) {
        _boot_pd[i] = (i << ARM_SECTION_BITS)
                      | BIT(10) /* kernel-only access */
                      | BIT(1); /* 1M section */
    }
    /* Kernel image: */
    start = first_vaddr >> ARM_SECTION_BITS;
    end = last_vaddr >> ARM_SECTION_BITS;
    n = end - start;
    /* mapping of kernel image: */
    for (i = 0; i <= n; i++) {
        _boot_pd[i + start] = ((i << ARM_SECTION_BITS) + first_paddr)
                              | BIT(10) /* kernel-only access */
                              | BIT(1); /* 1M section */
    }
    /* map page table covering last 1M of virtual address space to page directory */
    _boot_pd[GET_PD_INDEX(ARM_VECTOR_TABLE)] = ((uintptr_t)_boot_pt)
                                               | BIT(9)
                                               | BIT(0); /* page table */
    /* map vector table */
    _boot_pt[GET_PT_INDEX(ARM_VECTOR_TABLE)] = ((uintptr_t)arm_vector_table)
                                               | BIT(4)  /* kernel-only access */
                                               | BIT(1); /* 4K page */
}

/**
 * Performs the same operation as init_boot_pd, but initialises
 * the LPAE page table. In this case, 3 L2 tables are concatenated.
 * PGD entries point to the appropriate L2 table.
 */
void init_hyp_boot_vspace(struct image_info *kernel_info)
{
    uint32_t i, n;
    /* Kernel image: */
    vaddr_t first_vaddr = kernel_info->virt_region_start;
    vaddr_t last_vaddr = kernel_info->virt_region_end;
    paddr_t first_paddr = ROUND_DOWN(kernel_info->phys_region_start, ARM_2MB_BLOCK_BITS);
    /* Elfloader image: */
    vaddr_t start = (vaddr_t)_text >> ARM_2MB_BLOCK_BITS;
    vaddr_t end = (vaddr_t)_end >> ARM_2MB_BLOCK_BITS;

    /* Map in L2 page tables */
    for (i = 0; i < 4; i++) {
        _lpae_boot_pgd[i] = ((uintptr_t)_lpae_boot_pmd + (i << PAGE_BITS))
                            | BIT(1)  /* Page table */
                            | BIT(0); /* Valid */
    }
    /* Identity mapping of Elfloader itself: */
    for (i = start; i <= end; i++) {
        _lpae_boot_pmd[i] = (i << ARM_2MB_BLOCK_BITS)
                            | BIT(10) /* AF - Not always HW managed */
                            | BIT(0); /* Valid */
    }
    /* Kernel image: */
    start = first_vaddr >> ARM_2MB_BLOCK_BITS;
    end = last_vaddr >> ARM_2MB_BLOCK_BITS;
    n = end - start;
    /* mapping of kernel image: */
    for (i = 0; i <= n; i++) {
        _lpae_boot_pmd[start + i] = ((i << ARM_2MB_BLOCK_BITS) + first_paddr)
                                    | BIT(10) /* AF - Not always HW managed */
                                    | BIT(0); /* Valid */
    }
}
