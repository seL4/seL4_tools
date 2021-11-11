/*
 * Copyright 2020, DornerWorks
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>
#include <elfloader/gen_config.h>

#include <types.h>
#include <binaries/elf/elf.h>
#include <elfloader.h>
#include <abort.h>
#include <cpio/cpio.h>
#include <sbi.h>

#define PT_LEVEL_1 1
#define PT_LEVEL_2 2

#define PT_LEVEL_1_BITS 30
#define PT_LEVEL_2_BITS 21

#define PTE_TYPE_TABLE 0x00
#define PTE_TYPE_SRWX 0xCE

#define RISCV_PGSHIFT 12
#define RISCV_PGSIZE BIT(RISCV_PGSHIFT)

// page table entry (PTE) field
#define PTE_V     0x001 // Valid

#define PTE_PPN0_SHIFT 10

#if __riscv_xlen == 32
#define PT_INDEX_BITS  10
#else
#define PT_INDEX_BITS  9
#endif

#define PTES_PER_PT BIT(PT_INDEX_BITS)

#define PTE_CREATE_PPN(PT_BASE)  (unsigned long)(((PT_BASE) >> RISCV_PGSHIFT) << PTE_PPN0_SHIFT)
#define PTE_CREATE_NEXT(PT_BASE) (unsigned long)(PTE_CREATE_PPN(PT_BASE) | PTE_TYPE_TABLE | PTE_V)
#define PTE_CREATE_LEAF(PT_BASE) (unsigned long)(PTE_CREATE_PPN(PT_BASE) | PTE_TYPE_SRWX | PTE_V)

#define GET_PT_INDEX(addr, n) (((addr) >> (((PT_INDEX_BITS) * ((CONFIG_PT_LEVELS) - (n))) + RISCV_PGSHIFT)) % PTES_PER_PT)

#define VIRT_PHYS_ALIGNED(virt, phys, level_bits) (IS_ALIGNED((virt), (level_bits)) && IS_ALIGNED((phys), (level_bits)))

struct image_info kernel_info;
struct image_info user_info;

unsigned long l1pt[PTES_PER_PT] __attribute__((aligned(4096)));
#if __riscv_xlen == 64
unsigned long l2pt[PTES_PER_PT] __attribute__((aligned(4096)));
unsigned long l2pt_elf[PTES_PER_PT] __attribute__((aligned(4096)));
#endif

/* Stacks for each core are set up in the assembly startup code. */
char elfloader_stack[CONFIG_MAX_NUM_NODES * BIT(CONFIG_KERNEL_STACK_BITS)] __attribute__((aligned(4096)));

/* first HART will initialise these */
void const *dtb = NULL;
size_t dtb_size = 0;

static inline void sfence_vma(void)
{
    asm volatile("sfence.vma" ::: "memory");
}

static inline void ifence(void)
{
    asm volatile("fence.i" ::: "memory");
}

#if CONFIG_PT_LEVELS == 2
uint64_t vm_mode = 0x1llu << 31;
#elif CONFIG_PT_LEVELS == 3
uint64_t vm_mode = 0x8llu << 60;
#elif CONFIG_PT_LEVELS == 4
uint64_t vm_mode = 0x9llu << 60;
#else
#error "Wrong PT level"
#endif

static inline void enable_virtual_memory(void)
{
    sfence_vma();
    asm volatile(
        "csrw satp, %0\n"
        :
        : "r"(vm_mode | (uintptr_t)l1pt >> RISCV_PGSHIFT)
        :
    );
    ifence();
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
        asm volatile("wfi" ::: "memory");
    }

    UNREACHABLE();
}

static int map_kernel_window(struct image_info *kernel_info)
{
    uint32_t index;
    unsigned long *lpt;

    /* Map the elfloader into the new address space */

    if (!IS_ALIGNED((uintptr_t)_text, PT_LEVEL_2_BITS)) {
        printf("ERROR: ELF Loader not properly aligned\n");
        return -1;
    }

    index = GET_PT_INDEX((uintptr_t)_text, PT_LEVEL_1);

#if __riscv_xlen == 32
    lpt = l1pt;
#else
    lpt = l2pt_elf;
    l1pt[index] = PTE_CREATE_NEXT((uintptr_t)l2pt_elf);
    index = GET_PT_INDEX((uintptr_t)_text, PT_LEVEL_2);
#endif

    for (unsigned int page = 0; index < PTES_PER_PT; index++, page++) {
        lpt[index] = PTE_CREATE_LEAF((uintptr_t)_text +
                                     (page << PT_LEVEL_2_BITS));
    }

    /* Map the kernel into the new address space */

    if (!VIRT_PHYS_ALIGNED(kernel_info->virt_region_start,
                           kernel_info->phys_region_start, PT_LEVEL_2_BITS)) {
        printf("ERROR: Kernel not properly aligned\n");
        return -1;
    }

    index = GET_PT_INDEX(kernel_info->virt_region_start, PT_LEVEL_1);

#if __riscv_xlen == 64
    lpt = l2pt;
    l1pt[index] = PTE_CREATE_NEXT((uintptr_t)l2pt);
    index = GET_PT_INDEX(kernel_info->virt_region_start, PT_LEVEL_2);
#endif

    for (unsigned int page = 0; index < PTES_PER_PT; index++, page++) {
        lpt[index] = PTE_CREATE_LEAF(kernel_info->phys_region_start +
                                     (page << PT_LEVEL_2_BITS));
    }

    return 0;
}

int hsm_exists = 0; /* assembly startup code will initialise this */

#if CONFIG_MAX_NUM_NODES > 1

extern void secondary_harts(word_t hart_id, word_t core_id);
extern void hsm_entry_on_secondary_hart(void);

int secondary_go = 0;
int next_logical_core_id = 1; /* incremented by assembly code  */
int mutex = 0;
int core_ready[CONFIG_MAX_NUM_NODES] = { 0 };

static void acquire_multicore_lock(void)
{
    while (__atomic_exchange_n(&mutex, 1, __ATOMIC_ACQUIRE) != 0) {
        /* busy waiting loop */
    }
}

static void release_multicore_lock(void)
{
    __atomic_store_n(&mutex, 0, __ATOMIC_RELEASE);
}

static void set_secondary_cores_go(void)
{
    __atomic_store_n(&secondary_go, 1, __ATOMIC_RELEASE);
}

static void block_until_secondary_cores_go(void)
{
    while (__atomic_load_n(&secondary_go, __ATOMIC_ACQUIRE) == 0) {
        /* busy waiting loop */
    }
}

static void mark_core_ready(int core_id)
{
    core_ready[core_id] = 1;
}

static int is_core_ready(int core_id)
{
    return (0 != __atomic_load_n(&core_ready[core_id], __ATOMIC_RELAXED));
}

static void start_secondary_harts(word_t primary_hart_id)
{
    /* Take the multicore lock first, then start all secondary cores. This
     * ensures the boot process on the primary core can continue without running
     * into concurrency issues, until things can really run in parallel. The
     * main use case for this currently is printing nicely serialized boot
     * messages,
     */
    acquire_multicore_lock();
    set_secondary_cores_go();
    /* Start all cores */
    if (!hsm_exists) {
        /* Without the HSM extension, we can't start the cores explicitly. But
         * they might be running already, so we do nothing here and just hope
         * things work out. If the secondary cores don't start we are stuck.
         */
        printf("no HSM extension, let's hope secondary cores have been started\n");
        return;
    }

    /* If we are running on a platform with SBI HSM extension support, no other
     * hart is running. The system starts harts in a random hart, but the
     * assembly startup code has done the migration to the designated primary
     * hart already and stopped the others. The global variable logical_core_id
     * must still be untouched here, otherwise something is badly wrong.
     */
    if (1 != next_logical_core_id) {
        printf("ERROR: logical core IDs have been assigned already\n");
        abort();
        UNREACHABLE();
    }
    /* Start all harts */
    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        word_t remote_hart_id = i + 1; /* hart IDs start at 1 */
        if (remote_hart_id == CONFIG_FIRST_HART_ID) {
            assert(remote_hart_id == primary_hart_id);
            continue; /* this is the current hart */
        }
        /* Start secondary hart, there is nothing to pass as custom
         * parameter thus it's 0.
         */
        sbi_hsm_ret_t ret = sbi_hart_start(remote_hart_id,
                                           hsm_entry_on_secondary_hart,
                                           0);
        if (SBI_SUCCESS != ret.code) {
            printf("ERROR: could not start hart %"PRIu_word", failure"
                   " (%d, %d)\n", remote_hart_id, ret.code, ret.data);
            abort();
            UNREACHABLE();
        }
    }
}

#endif /* CONFIG_MAX_NUM_NODES > 1 */

static int run_elfloader(void *bootloader_dtb)
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

    /* Setup MMU tables. */
    ret = map_kernel_window(&kernel_info);
    if (0 != ret) {
        printf("ERROR: could not map kernel window, code %d\n", ret);
        return -1;
    }

    return 0;
}

static NORETURN void boot_hart(word_t hart_id, word_t core_id)
{
    /* Caller must hold the multicore lock here. */

    if (0 == core_id) {
        printf("Enabling MMU and paging\n");
    }
    enable_virtual_memory();

#if CONFIG_MAX_NUM_NODES > 1
    /* We are ready to hand over control to the kernel on this hart. Sync with
     * all other harts before doing this.
     */
    mark_core_ready(core_id);
    release_multicore_lock();
    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        while (!is_core_ready(i)) {
            /* busy waiting loop */
        }
    }
#endif /* CONFIG_MAX_NUM_NODES > 1 */

    if (0 == core_id) {
        printf("Jumping to kernel-image entry point...\n\n");
    }

    /* The hand over interface is the same on all cores. We avoid making
     * assumption how the parameters are used. The current seL4 kernel
     * implementation only cares about the DTB on the primary core.
     */
    ((init_riscv_kernel_t)kernel_info.virt_entry)(
        user_info.phys_region_start,
        user_info.phys_region_end,
        user_info.phys_virt_offset,
        user_info.virt_entry,
        (word_t)dtb,
        dtb_size
#if CONFIG_MAX_NUM_NODES > 1
        ,
        hart_id,
        core_id
#endif /* CONFIG_MAX_NUM_NODES > 1 */
    );

    /* We should never get here. */
    printf("ERROR: back in ELF-loader hart %"PRIu_word" (core ID %"PRIu_word")\n",
           hart_id, core_id);
    abort();
    UNREACHABLE();
}

#if CONFIG_MAX_NUM_NODES > 1
NORETURN void secondary_hart_main(word_t hart_id, word_t core_id)
{
    block_until_secondary_cores_go();
    acquire_multicore_lock();
    printf("started hart %"PRIu_word" (core id %"PRIu_word")\n",
           hart_id, core_id);

    if (core_id >= CONFIG_MAX_NUM_NODES) {
        printf("ERROR: max number of harts exceeded (%d)\n",
               (int)CONFIG_MAX_NUM_NODES);
        abort();
        UNREACHABLE();
    }

    boot_hart(hart_id, core_id);
    UNREACHABLE();

}
#endif /* CONFIG_MAX_NUM_NODES > 1 */

void main(word_t hart_id, void *bootloader_dtb)
{
    /* Printing uses SBI, so there is no need to initialize any UART. */
    printf("ELF-loader started on (HART %"PRIu_word") (NODES %d)\n",
           hart_id, (unsigned int)CONFIG_MAX_NUM_NODES);

    printf("  paddr=[%p..%p]\n", _text, _end - 1);

    int ret = run_elfloader(bootloader_dtb);
    if (0 != ret) {
        printf("ERROR: ELF-loader failed, code %d\n", ret);
        /* There is nothing we can do to recover. */
        abort();
        UNREACHABLE();
    }

#if CONFIG_MAX_NUM_NODES > 1
    start_secondary_harts(hart_id);
#endif /* CONFIG_MAX_NUM_NODES > 1 */

    boot_hart(hart_id, 0);
    UNREACHABLE();
}
