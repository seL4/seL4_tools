/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>
#include <types.h>
#include <elfloader.h>
#include <mode/structures.h>
#include <printf.h>
#include <abort.h>
#include <drivers/uart.h>

/*
* Create the 1:1 uart mapping so that the UART can be accessed after MMU enable.

* For now we assume the uart memory ranges take no more than 2 MiB. We map the 2MiB block containing uart base together with a following block to avoid alignment issues.
*/
static inline void init_uart_downpages(vaddr_t elfloader_pud, volatile void *uart_base)
{
    vaddr_t uart_gb = (vaddr_t)uart_base & ~MASK(ARM_1GB_BLOCK_BITS);
    vaddr_t uart_block;


    if (GET_PUD_INDEX(uart_gb) != elfloader_pud) {
        /* The UART is in a different 1 GiB block than the elfloader.
            * We have no dedicated PMD table for that PUD entry, so use a
            * 1 GiB block descriptor directly in the PUD.
            */
        _boot_pud_down[GET_PUD_INDEX(uart_gb)] = (uintptr_t)uart_gb
                                                    | BIT(10)  /* access flag */
                                                    | (0 << 2) /* MT_DEVICE_nGnRnE */
                                                    | BIT(0);  /* 1G block */
    } else {
        /* Same 1 GiB block as the elfloader: reuse its PMD table. */
        uart_block = (vaddr_t)uart_base & ~MASK(ARM_2MB_BLOCK_BITS);
        _boot_pmd_down[GET_PMD_INDEX(uart_block)] = (uintptr_t)uart_block
                                                    | BIT(10)  /* access flag */
                                                    | (0 << 2) /* MT_DEVICE_nGnRnE */
                                                    | BIT(0);  /* 2M block */
        
        /* Set one more 2MiB block to avoid alignment issues. */
        uart_block += BIT(ARM_2MB_BLOCK_BITS);
        _boot_pmd_down[GET_PMD_INDEX(uart_block)] = (uintptr_t)uart_block
                                                    | BIT(10)  /* access flag */
                                                    | (0 << 2) /* MT_DEVICE_nGnRnE */
                                                    | BIT(0);  /* 2M block */
    }
}


/*
* Create the 1:1 elfloader mapping to jump into the kernel after enabling the MMU.
*/
static void init_downpages(void)
{
    word_t i;
    vaddr_t start_vaddr = (vaddr_t)_text & ~MASK(ARM_2MB_BLOCK_BITS);
    vaddr_t end_vaddr = (vaddr_t)_end;
    volatile void *uart_base = uart_get_mmio();

    _boot_pgd_down[0] = ((uintptr_t)_boot_pud_down) | BIT(1) | BIT(0); /* its a page table */

    /* We only map in 1 GiB, so check that the loader doesn't cross 1GiB boundary. */
    if (GET_PUD_INDEX(start_vaddr) != GET_PUD_INDEX(end_vaddr)) {
        printf("We only map 1GiB, but elfloader paddr range covers multiple GiB.\n");
        abort();
    }

    _boot_pud_down[GET_PUD_INDEX(start_vaddr)] = ((uintptr_t)_boot_pmd_down) | BIT(1) | BIT(0);

    for (i = GET_PMD_INDEX(start_vaddr); i <= GET_PMD_INDEX(end_vaddr); i++) {
        _boot_pmd_down[i] = (uintptr_t) start_vaddr
                            | BIT(10)  /* access flag */
                            | (4 << 2) /* MT_NORMAL memory */
                            | BIT(0);  /* 2M block */
        start_vaddr += BIT(ARM_2MB_BLOCK_BITS);
    }

    /* Identity-map the UART MMIO region so debug output works after the MMU
     * is enabled.
     */
    if (uart_base != NULL) {
        init_uart_downpages(GET_PUD_INDEX(start_vaddr), uart_base);
    }
}

/*
* Create a "boot" page table, which contains a 1:1 mapping below
* the kernel's first vaddr, and a virtual-to-physical mapping above the
* kernel's first vaddr.
*/
void init_boot_vspace(struct image_info *kernel_info)
{
    word_t i;
    vaddr_t first_vaddr = kernel_info->virt_region_start;
    vaddr_t last_vaddr = kernel_info->virt_region_end;
    paddr_t first_paddr = kernel_info->phys_region_start;

    init_downpages();

    _boot_pgd_up[GET_PGD_INDEX(first_vaddr)]
        = ((uintptr_t)_boot_pud_up) | BIT(1) | BIT(0); /* its a page table */

    _boot_pud_up[GET_PUD_INDEX(first_vaddr)]
        = ((uintptr_t)_boot_pmd_up) | BIT(1) | BIT(0); /* its a page table */

    /* We only map in 1 GiB, so check that the kernel doesn't cross 1GiB boundary. */
    if ((first_vaddr & ~MASK(ARM_1GB_BLOCK_BITS)) != (last_vaddr & ~MASK(ARM_1GB_BLOCK_BITS))) {
        printf("We only map 1GiB, but kernel vaddr range covers multiple GiB.\n");
        abort();
    }

    for (i = GET_PMD_INDEX(first_vaddr); i < BIT(PMD_BITS); i++) {
        _boot_pmd_up[i] = first_paddr
                          | BIT(10) /* access flag */
#if CONFIG_MAX_NUM_NODES > 1
                          | (3 << 8) /* make sure the shareability is the same as the kernel's */
#endif
                          | (4 << 2) /* MT_NORMAL memory */
                          | BIT(0); /* 2M block */
        first_paddr += BIT(ARM_2MB_BLOCK_BITS);
    }
}

void init_hyp_boot_vspace(struct image_info *kernel_info)
{
    word_t i;
    word_t pmd_index;
    vaddr_t first_vaddr = kernel_info->virt_region_start;
    paddr_t first_paddr = kernel_info->phys_region_start;

    init_downpages();

    _boot_pgd_down[GET_PGD_INDEX(first_vaddr)]
        = ((uintptr_t)_boot_pud_up) | BIT(1) | BIT(0); /* its a page table */

    _boot_pud_up[GET_PUD_INDEX(first_vaddr)]
        = ((uintptr_t)_boot_pmd_up) | BIT(1) | BIT(0); /* its a page table */

    pmd_index = GET_PMD_INDEX(first_vaddr);
    for (i = pmd_index; i < BIT(PMD_BITS); i++) {
        _boot_pmd_up[i] = (((i - pmd_index) << ARM_2MB_BLOCK_BITS) + first_paddr)
                          | BIT(10) /* access flag */
#if CONFIG_MAX_NUM_NODES > 1
                          | (3 << 8)
#endif
                          | (4 << 2) /* MT_NORMAL memory */
                          | BIT(0); /* 2M block */
    }
}
