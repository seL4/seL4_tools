/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * Derived from:
 * Copyright 2020, DornerWorks
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>
#include <elfloader/gen_config.h>

#include <drivers.h>
#include <drivers/uart.h>
#include <printf.h>
#include <types.h>
#include <binaries/elf/elf.h>
#include <elfloader.h>
#include <abort.h>
#include <cpio/cpio.h>

#include <machine.h>

#define PT_LEVELS 3

#define PS_DEFAULT_64GSIZE 36
#define PS_DEFAULT_32MSIZE 25

#define PT_LEVEL_1 1
#define PT_LEVEL_2 2
#define PT_LEVEL_3 3

#define PT_LEVEL_1_BITS 36
#define PT_LEVEL_2_BITS 25
#define PT_LEVEL_3_BITS 14

#define LOONGARCH_L1PGSHIFT PT_LEVEL_1_BITS
#define LOONGARCH_L2PGSHIFT PT_LEVEL_2_BITS
#define LOONGARCH_L3PGSHIFT PT_LEVEL_3_BITS

#define PT_INDEX_BITS  11

#define PTES_PER_PT BIT(PT_INDEX_BITS)

#define PTE_HUGE_PA_SHIFT 24

#define PTE_64GHUGE_PA(PT_BASE) (unsigned long)(((PT_BASE) >> LOONGARCH_L1PGSHIFT) << LOONGARCH_L1PGSHIFT)
#define PTE_HUGE_PA(PT_BASE) (unsigned long)(((PT_BASE) >> LOONGARCH_L2PGSHIFT) << LOONGARCH_L2PGSHIFT)
#define PTE_GSRWXV 0x11D3

#define PTE_CREATE_NEXT(PT_BASE) (unsigned long)PT_BASE
#define PTE_CREATE_64GHUGE_LEAF(PT_BASE) (unsigned long)(PTE_64GHUGE_PA(PT_BASE)|PTE_GSRWXV)
#define PTE_CREATE_HUGE_LEAF(PT_BASE) (unsigned long)(PTE_HUGE_PA(PT_BASE) | PTE_GSRWXV)

#define GET_PT_INDEX(addr, n) (((addr) >> (((PT_INDEX_BITS) * ((PT_LEVELS) - (n))) + LOONGARCH_L3PGSHIFT)) % PTES_PER_PT)

#define VIRT_PHYS_ALIGNED(virt, phys, level_bits) (IS_ALIGNED((virt), (level_bits)) && IS_ALIGNED((phys), (level_bits)))

unsigned long l1pt[PTES_PER_PT] __attribute__((aligned(16384)));
unsigned long l2pt[PTES_PER_PT] __attribute__((aligned(16384)));

struct image_info kernel_info;
struct image_info user_info;

char elfloader_stack_alloc[BIT(CONFIG_KERNEL_STACK_BITS)];

/* first HART will initialise these */
void const *dtb = NULL;
size_t dtb_size = 0;

// unsigned long tlbrentry;


void NORETURN elfloader_panic(){
    printf("Oh man, entered trap in elfloader!\n");
    abort();
}


/*
 * overwrite the default implementation for abort()
 */
void NORETURN abort(void)
{
    printf("HALT due to call to abort()\n");

    /* We could call the SBI shutdown now. However, it's likely there is an
     * issue that needs to be debugged. Instead of doing a busy loop, spinning
     * over a wfi is the better choice here, as it allows the core to enter an
     * idle state until something happens.
     */
    for (;;) {
        asm volatile("idle 0" ::: "memory");
    }

    UNREACHABLE();
}

static void setup_pw(void)
{
    unsigned long pt_b, pt_w;
    unsigned long dir1_b, dir1_w;
    unsigned long dir2_b, dir2_w;

    pt_b = PT_LEVEL_3_BITS;
    dir1_b = PT_LEVEL_2_BITS;
    dir2_b = PT_LEVEL_1_BITS;
    pt_w = dir1_w = dir2_w = PT_INDEX_BITS;

    write_csr_pwcl(dir1_w << 15 | dir1_b << 10 | pt_w << 5 | pt_b);
    write_csr_pwch(dir2_w << 6 | dir2_b);

    write_csr_pgdh((unsigned long)l1pt);
}

static inline void invtlb(void)
{
    asm volatile("invtlb 0x1, $r0, $r0" :::);
}

static inline void dbar(void)
{
    asm volatile("dbar 0" ::: "memory");
}

static inline void ibar(void)
{
    asm volatile("ibar 0" ::: "memory");
}

extern void handle_tlb_refill(void);
extern void elfloader_trap_entry(void);

static void setup_tlb_handler(void)
{
    write_csr_tlbrentry((unsigned long)handle_tlb_refill);
}

static void init_tlb(void)
{
    write_csr_pagesize(PS_DEFAULT_32MSIZE);
    write_csr_stlbpgsize(PS_DEFAULT_32MSIZE);
    write_csr_tlbrefill_pagesize(PS_DEFAULT_32MSIZE);

    if (read_csr_pagesize() != PS_DEFAULT_32MSIZE)
        printf("MMU doesn't support PAGE_SIZE\n");

    setup_tlb_handler();
    // invtlb();
}

static int map_kernel_window(struct image_info *kernel_info)
{
    uint32_t index;

    /* Map the kernel into the new address space */

    if (!VIRT_PHYS_ALIGNED((unsigned long)(kernel_info->virt_region_start),
                           (unsigned long)(kernel_info->phys_region_start), PT_LEVEL_2_BITS)) {
        printf("ERROR: Kernel not properly aligned\n");
        return -1;
    }
    

    index = GET_PT_INDEX(kernel_info->virt_region_start, PT_LEVEL_1);

    // l1pt[index] =PTE_CREATE_64GHUGE_LEAF(kernel_info->phys_region_start);

    l1pt[index] = PTE_CREATE_NEXT((uintptr_t)l2pt);
    index = GET_PT_INDEX(kernel_info->virt_region_start, PT_LEVEL_2);

    for (unsigned int page = 0; index < PTES_PER_PT; index++, page++) {
        l2pt[index] = PTE_CREATE_HUGE_LEAF(kernel_info->phys_region_start +
                                     (page << PT_LEVEL_2_BITS));
    }

    return 0;
}

static inline void enable_virtual_memory(void)
{
    setup_pw();
    init_tlb();
    enable_pg(0xb0);
}

static int run_elfloader(UNUSED int hart_id, void *bootloader_dtb)
{
    int ret;

    /* Unpack ELF images into memory. */
    unsigned int num_apps = 0;
    ret = load_images(&kernel_info, &user_info, 1, &num_apps,
                      bootloader_dtb, &dtb, &dtb_size);
    if (0 != ret) {
        printf("ERROR: image loading failed, code %d\n", ret);
        return -1;
    }

    if (num_apps != 1) {
        printf("ERROR: expected to load just 1 app, actually loaded %u apps\n",
               num_apps);
        return -1;
    }

    ret = map_kernel_window(&kernel_info);
    if (0 != ret) {
        printf("ERROR: could not map kernel window, code %d\n", ret);
        return -1;
    }

    printf("Enabling MMU and paging\n");
    enable_virtual_memory();

    printf("setting trap entry\n");
    write_csr_elf_debug_eentry((unsigned long)elfloader_trap_entry);

    printf("Jumping to kernel-image entry point...\n\n");
    printf("kernel_phys_region_start: %p\n", kernel_info.phys_region_start);
    printf("kernel_phys_region_end: %p\n", kernel_info.phys_region_end);
    printf("kernel_phys_virt_offset: %p\n", kernel_info.phys_virt_offset);
    printf("kernel_virt_entry: %p\n", kernel_info.virt_entry);
    printf("ui_phys_region_start: %p\n", user_info.phys_region_start);
    printf("ui_phys_region_end: %p\n", user_info.phys_region_end);
    printf("ui_phys_virt_offset: %p\n", user_info.phys_virt_offset);
    printf("ui_virt_entry: %p\n", user_info.virt_entry);
    printf("dtb physical address: %p\n", (word_t)dtb);
    printf("dtb size: %d\n", dtb_size);

    ((init_loongarch_kernel_t)kernel_info.virt_entry)(user_info.phys_region_start,
                                                  user_info.phys_region_end,
                                                  user_info.phys_virt_offset,
                                                  user_info.virt_entry,
                                                  (word_t)dtb,
                                                  dtb_size
#if CONFIG_MAX_NUM_NODES > 1
                                                  ,
                                                  hart_id,
                                                  0
#endif
                                                 );

    /* We should never get here. */
    printf("ERROR: Kernel returned back to the ELF Loader\n");
    return -1;
}

void main(int hart_id, void *bootloader_dtb)
{
    /* initialize platform so that we can print to a UART */
    initialise_devices();

    /* Printing uses UART*/
    printf("ELF-loader started on (HART %d) (NODES %d)\n",
           hart_id, CONFIG_MAX_NUM_NODES);

    printf("  paddr=[%p..%p]\n", _text, _end - 1);

    /* Run the actual ELF loader, this is not expected to return unless there
     * was an error.
     */
    int ret = run_elfloader(hart_id, bootloader_dtb);
    if (0 != ret) {
        printf("ERROR: ELF-loader failed, code %d\n", ret);
        /* There is nothing we can do to recover. */
        abort();
        UNREACHABLE();
    }

    /* We should never get here. */
    printf("ERROR: ELF-loader didn't hand over control\n");
    abort();
    UNREACHABLE();
}
