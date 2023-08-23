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
#include <assert.h>
#include <mode/aarch64.h>
#include <armv/machine.h>     /* dsb() */
#include <drivers/uart.h>

/* Note: "typeof()" is a GCC extension that is supported by Clang, too. */
#define READ_ONCE(x)    (*(const volatile typeof(x) *)&(x))
#define WRITE_ONCE(var, value) \
    *((volatile typeof(var) *)(&(var))) = (value);


//#define DEBUG_PAGETABLES

#ifndef DEBUG_PAGETABLES
#define dbg_printf(...) /* empty */
static void dgb_print_2M_mapping_details(const char *map_name UNUSED,
                                         paddr_t pa UNUSED, size_t size UNUSED) {}
#else
#define dbg_printf(...) printf(__VA_ARGS__)

static int dgb_print_2M_mapping_indices(paddr_t pa)
{
    return printf("%u.%u.%u.X",
                  GET_PGD_INDEX(pa),
                  GET_PUD_INDEX(pa),
                  GET_PMD_INDEX(pa));
}

static void dgb_print_2M_mapping_details(const char *map_name, paddr_t pa, size_t size)
{
    int cnt = 0;
    paddr_t pa_start = pa;
    size_t orig_sz   = size;

    pa    = ROUND_DOWN(pa, ARM_2MB_BLOCK_BITS);
    size += (pa_start - pa);
    size  = ROUND_UP(size, ARM_2MB_BLOCK_BITS);

    cnt += dgb_print_2M_mapping_indices(pa);
    if (orig_sz) {
        while (cnt < 11) {
            printf(" ");
            cnt++;
        }
        cnt += printf("--");
        while (cnt < 16) {
            printf(" ");
            cnt++;
        }
        cnt += dgb_print_2M_mapping_indices(pa + size - 1);
    }
    while (cnt < 27) {
        printf(" ");
        cnt++;
    }
    if (orig_sz) {
        printf("PA 0x%lx - 0x%lx (size: %lu MiB): %s\n", pa, pa + size - 1, size / 1024u / 1024, map_name);
    } else {
        /* No range given, just a single 2 MiB page */
        printf("PA 0x%lx: %s\n", pa, map_name);
    }
}
#endif /* DEBUG_PAGETABLES */

/* Page allocator. Contains a fixed number of pages. All page-aligned. No returning possible. */

#define NUM_PAGES    7
static char pages[BIT(PAGE_BITS) * NUM_PAGES] ALIGN(BIT(PGD_SIZE_BITS));
static unsigned page_cnt;

static void *get_page(void)
{
    void *ret = NULL;

    if (page_cnt == 0) {
        dbg_printf("get_page(): pages @ 0x%p\n", pages);
    }

    if (page_cnt < NUM_PAGES) {
        ret = &pages[BIT(PAGE_BITS) * page_cnt];
        dbg_printf("get_page(): ret: 0x%p (%u->%u)\n", ret, page_cnt, page_cnt + 1);
        page_cnt ++;
    }

    return ret;
}

/* Translate a PA to a VA such that when accessing the VA we end up at that PA.
 * Usually done in OS kernels via a physical memory map which has a constant
 * virt-to-phys offset. Here this is the same, since either the MMU is off or
 * we're running on the identity mapping.
 */
static inline uint64_t pa_to_va(uint64_t pa)
{
    return pa;
}

static inline uint64_t va_to_pa(uint64_t va)
{
    return va;
}

typedef uint64_t pte_t;

/* This can be used to clear unwanted bits from a PA that is supposed to be put
 * into a PTE/PDE; or it can be used to extract the PA from a PTE/PDE.
 */
static inline uint64_t mask_pa(uint64_t pa)
{
    /* Mask out the upper 16 bits and lower 12 bits. Only 48-bit OA for now. */
    return (pa & 0x0000FFFFFFFFF000);
}

static inline uintptr_t pde_to_paddr(uint64_t pde_val)
{
    /* ARM DDI ARM DDI 0487I.a, page D8-5124 */
    return mask_pa(pde_val);
}

static inline uint64_t make_pde(uintptr_t pa)
{
    /* For now we set all (upper) attributes to zero */
    return (mask_pa(pa) | BIT(1) | BIT(0));
}

/* Accept a pointer, otherwise same as make_pde() */
static inline uint64_t make_pde_from_ptr(pte_t *pagetable_target)
{
    return make_pde(va_to_pa((uintptr_t)pagetable_target));
}

/* ARM DDI 0487I.a, section D8.5.2 */
#define INNER_SHAREABLE   3
static inline uint64_t make_pte(paddr_t pa, uint8_t mem_attr_index)
{
    /* Note: As per R_PYFVQ from the ARM spec, we can always safely set the
     *       shareability to inner, even for device-type memory.
     */
    return mask_pa(pa)
           | BIT(10) /* access flag */
#if CONFIG_MAX_NUM_NODES > 1
           | (INNER_SHAREABLE << 8)
#endif
           | (mem_attr_index << 2)
           | BIT(0); /* valid page/block mapping */
}

static inline _Bool pte_is_valid(pte_t pte)
{
    return (pte & 1);
}

static inline _Bool pte_is_block(pte_t pte)
{
    return ((pte & 3) == 1);
}

/* Take care about atomicity */
static inline void pte_set(pte_t *ptep, pte_t val)
{
    WRITE_ONCE(*ptep, val);
}

static inline pte_t pte_get(pte_t *ptep)
{
    return READ_ONCE(*ptep);
}

static_assert(PGD_BITS == BITS_PER_LEVEL, "Mismatch in expected pagetable size");
static_assert(PUD_BITS == BITS_PER_LEVEL, "Mismatch in expected pagetable size");
static_assert(PMD_BITS == BITS_PER_LEVEL, "Mismatch in expected pagetable size");
/* ARM VMSAv8-64: Each table entry is always eight bytes large */
static_assert(PAGE_BITS == (BITS_PER_LEVEL + 3), "Mismatch in expected page size");

/* A valid PA can be maximum 48 or 52 bit large, so upper bits are always zero */
#define INVALID_PA     ((uint64_t)-1)
static paddr_t walk_pagetables(vaddr_t va, uint64_t *l0_table,
                               unsigned *level, pte_t **fault_pde)
{
    paddr_t ret = INVALID_PA;
    /* All levels have the same size and therefore number of index bits
     * (9 for 4kiB Translation Granule) on ARMv8.
     */
    uint64_t index_mask_bits = PGD_BITS + PUD_BITS + PMD_BITS + PAGE_BITS;
    uint64_t *tbl = l0_table;

    unsigned idx, lvl;
    paddr_t pa;
    pte_t pte;

    /* Walk up to four levels */
    for (lvl = 0; lvl <= 3; lvl++) {
        idx = (va >> index_mask_bits) & MASK(BITS_PER_LEVEL);
        pte = pte_get(&tbl[idx]);

        if (!pte_is_valid(pte)) {
            goto err_out;
        } else if (pte_is_block(pte)) {
            /* L0 giant pages (512 GiB) are not allowed by the architecture for
             * 4kiB Granule size and 48 bit OA. We don't support 52 bit OA.
             */
            if (lvl == 0) {
                goto err_out;
            }
            break;
        }
        if (lvl == 3) {
            /* ARM DDI 0487I.a, page D8-5126 (I_WYRBP), D8-5131 (I_VKPKF):
             * If the PTE in the last level is valid, it is interpreted as a page
             * table, irrespectively of bit 1. This allows for the "loopback
             * trick" - described in every (good) OS lecture at university :-)
             * Other architectures like RISC-V have screwed this up with their
             * pagetable format.
             */
            break;
        }
        /* We have a table descriptor. Descent to the next lower level */
        pa = pde_to_paddr(pte);
        vaddr_t va_next = pa_to_va(pa);
        tbl = (uint64_t *)va_next;

        index_mask_bits -= BITS_PER_LEVEL;
    }

    ret = (pa | (va & (MASK(index_mask_bits))));

err_out:
    *level     = lvl;
    *fault_pde = &tbl[idx];
    return ret;
}

/* Returns NULL if there is already something mappped at the requested VA. Fills
 * in page tables if needed until the desired level is reached.
 */
static pte_t *fill_pt_tree(vaddr_t va, uint64_t *l0_table, unsigned target_lvl)
{
    paddr_t pa;
    unsigned lvl;
    pte_t *fault_pde;

    pa = walk_pagetables(va, l0_table, &lvl, &fault_pde);

    while ((lvl < target_lvl) && (pa == INVALID_PA)) {
        /* fault_pde points to the entry to write. Add a new pagetable */
        pte_set(fault_pde, make_pde_from_ptr(get_page()));

        pa = walk_pagetables(va, l0_table, &lvl, &fault_pde);
    }

    if ((lvl == target_lvl) && fault_pde && !pte_is_valid(pte_get(fault_pde))) {
        return fault_pde;
    }
    return NULL;
}

extern char _text[];
extern char _end[];

extern size_t dtb_size;

static inline void clean_inval_cl(void *addr)
{
    asm volatile("dc civac, %0\n\t" :: "r"(addr));
}

static void clean_inval_pagetables(void)
{
    dsb();
    /* Whole image for now; EFI case: Maybe our image is loaded on the boot
     * CPU with caches enabled (and still being dirty), but the secondary CPUs
     * start with caches disabled. Further, assume CL size is >= 64 Bytes.
     * Maybe this is too cautious. Can we relax this?
     */
    for (vaddr_t va = (vaddr_t)_text; va < (vaddr_t)(_end); va += 64) {
        clean_inval_cl((void *)va);
    }
    dsb();
}

static void map_uart(paddr_t base)
{
    pte_t *pte;

    base = ROUND_DOWN(base, ARM_2MB_BLOCK_BITS);
    pte  = fill_pt_tree(base, _boot_pgd_down, 2);
    if (pte) {
        pte_set(pte, make_pte(base, MT_DEVICE_nGnRnE));
    } else {
        printf("Unable to map the UART at PA 0x%lx\n", base);
        abort();
    }
    dbg_printf("Done mapping UART at PA: 0x%lx\n", base);
}


static paddr_t uart_base_mmio;
void mmu_set_uart_base(volatile void *base)
{
    uart_base_mmio = (paddr_t)base;
}

/*
 * Create a "boot" page table, which contains a 1:1 mapping for the ELFloader and
 * the DTB. Moreover create a mapping for the kernel image at the desired VA with the
 * physical memory that was used when extracting the kernel from the elfloader
 * image previously.
 */
static void init_boot_vspace_impl(const struct image_info *kernel_info, _Bool has_one_va_range)
{
    /* We may be running with MMU & caches off. Before we write new values
     * make sure to clean & invalidate all previous data in those locations.
     */
    clean_inval_pagetables();

    /* Map UART, using strongly ordered memory; one 2 MiB page; 1:1 VA/PA */
    paddr_t uart_base = ROUND_DOWN(uart_base_mmio, ARM_2MB_BLOCK_BITS);
    map_uart(uart_base);

    /* Map Elfloader image, using NORMAL memory; 1:1 VA/PA */
    paddr_t start_paddr = ROUND_DOWN(((paddr_t)_text), ARM_2MB_BLOCK_BITS);
    paddr_t end_paddr   = ROUND_UP(((paddr_t)_end), ARM_2MB_BLOCK_BITS);

    for (paddr_t pa = start_paddr; pa < end_paddr; pa += BIT(ARM_2MB_BLOCK_BITS)) {
        pte_t *pte = fill_pt_tree(pa, _boot_pgd_down, 2);
        if (pte) {
            pte_set(pte, make_pte(pa, MT_NORMAL));
        } else {
            printf("Unable to map ELFloader at PA: 0x%lx\n", pa);
            abort();
        }
        dbg_printf("Map Elfloader PA: 0x%lx\n", pa);
    }
    dbg_printf("Done mapping Elfloader\n");

    paddr_t dtb_map_start, dtb_map_end;
    if (dtb && (dtb_size > 0)) {
        /* Device Tree Blob (DTB):
         * An UEFI-supplied DTB lies outside of the image memory => Add mapping.
         * For other DTBs the ELFloader of course saves the *target* address of
         * the copied DTB in "dtb".
         * So we also need to add a mapping here in those cases.
         */
        paddr_t dtb_end = (paddr_t)dtb + dtb_size;

        dtb_map_start = ROUND_DOWN((paddr_t)dtb, ARM_2MB_BLOCK_BITS);
        dtb_map_end   = ROUND_UP(dtb_end, ARM_2MB_BLOCK_BITS);
        for (paddr_t pa = dtb_map_start; pa < dtb_map_end; pa += BIT(ARM_2MB_BLOCK_BITS)) {
            pte_t *pte = fill_pt_tree(pa, _boot_pgd_down, 2);
            if (pte) {
                pte_set(pte, make_pte(pa, MT_NORMAL));
            } else {
                printf("Unable to map DTB at PA: 0x%lx\n", pa);
            }
            dbg_printf("Map DTB PA: 0x%lx\n", pa);
        }
        dbg_printf("Done mapping DTB\n");
    }

    /* Map the kernel */
    vaddr_t first_vaddr = kernel_info->virt_region_start;
    vaddr_t last_vaddr  = kernel_info->virt_region_end;
    paddr_t first_paddr = kernel_info->phys_region_start;

    uint64_t *l0_table = has_one_va_range ? _boot_pgd_down : _boot_pgd_up;
    paddr_t pa = first_paddr;
    for (vaddr_t va = first_vaddr; va < last_vaddr;
         va += BIT(ARM_2MB_BLOCK_BITS),
         pa += BIT(ARM_2MB_BLOCK_BITS)) {

        pte_t *pte = fill_pt_tree(va, l0_table, 2);
        if (pte) {
            pte_set(pte, make_pte(pa, MT_NORMAL));
        } else {
            printf("Unable to map kernel at VA/PA: 0x%lx / 0x%lx\n", va, pa);
        }
        dbg_printf("Map kernel VA -> PA: 0x%lx -> 0x%lx\n", va, pa);
    }
    dbg_printf("Done mapping kernel\n");

    dbg_printf("Mapping indices:\n");
    dgb_print_2M_mapping_details("UART", uart_base, /* one 2 MiB page */ 2u * 1024 * 1024);
    dgb_print_2M_mapping_details("ELFloader image", (paddr_t)_text, (paddr_t)_end - (paddr_t)_text);
    if (dtb && (dtb_size > 0)) {
        dgb_print_2M_mapping_details("dtb", dtb_map_start, dtb_map_end - dtb_map_start - 1);
    }


    /* Architecturally required barrier to make all writes to pagetable memories
     * visible to the pagetable walker. See ARM DDI 0487I.a, section D8.2.6.
     */
    dsb();

    /* Maintenance again, just to be sure. This is only necessary for the secondary
     * CPUs; they may come up with caches & MMU disabled. What they should usually
     * do is enable caches & MMU together! The following code is only necessary
     * if they enable ONLY the MMU first and after that they enable the cache.
     * That would be totally ... well ... suboptimal, but we play "better safe
     * than sorry" here.
     */
    clean_inval_pagetables();
}

void init_boot_vspace(struct image_info *kernel_info)
{
    init_boot_vspace_impl(kernel_info, 0);
}

void init_hyp_boot_vspace(struct image_info *kernel_info)
{
    init_boot_vspace_impl(kernel_info, 1);
}
