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
#if __riscv_xlen == 32
#define PT_LEVEL_2_BITS 22
#elif __riscv_xlen == 64
#define PT_LEVEL_2_BITS 21
#else
#error "unsupported RISC-V architecture"
#endif


#define PTE_TYPE_TABLE  WORD_CONST(0x00)
#define PTE_TYPE_SRWX   WORD_CONST(0xCE)

#define RISCV_PGSHIFT 12
#define RISCV_PGSIZE BIT(RISCV_PGSHIFT)

// page table entry (PTE) field
#define PTE_V            WORD_CONST(0x001) // Valid

#define PTE_PPN0_SHIFT 10

#if __riscv_xlen == 32
#define PT_INDEX_BITS  10
#elif __riscv_xlen == 64
#define PT_INDEX_BITS  9
#else
#error "unsupported RISC-V architecture"
#endif

#define PTES_PER_PT BIT(PT_INDEX_BITS)

#define PTE_CREATE_PPN(PT_BASE)  (( ((word_t)(PT_BASE)) >> RISCV_PGSHIFT) << PTE_PPN0_SHIFT)
#define PTE_CREATE_NEXT(PT_BASE) (PTE_CREATE_PPN(PT_BASE) | PTE_TYPE_TABLE | PTE_V)
#define PTE_CREATE_LEAF(PT_BASE) (PTE_CREATE_PPN(PT_BASE) | PTE_TYPE_SRWX | PTE_V)

#define GET_PT_INDEX(addr, n) (((addr) >> (((PT_INDEX_BITS) * ((CONFIG_PT_LEVELS) - (n))) + RISCV_PGSHIFT)) % PTES_PER_PT)

struct image_info kernel_info;
struct image_info user_info;

word_t l1pt[PTES_PER_PT] __attribute__((aligned(4096)));
#if __riscv_xlen == 64
word_t l2pt[PTES_PER_PT] __attribute__((aligned(4096)));
word_t l2pt_elf[PTES_PER_PT] __attribute__((aligned(4096)));
#endif

/* first HART will initialise these */
void const *dtb = NULL;
size_t dtb_size = 0;

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

static void map_kernel_window(struct image_info *kernel_info)
{
    unsigned int index;
    word_t *lpt;

    /* Map the elfloader into the new address space */

    index = GET_PT_INDEX((uintptr_t)_text, PT_LEVEL_1);

#if __riscv_xlen == 32
    lpt = l1pt;
#else
    lpt = l2pt_elf;
    l1pt[index] = PTE_CREATE_NEXT((uintptr_t)l2pt_elf);
    index = GET_PT_INDEX((uintptr_t)_text, PT_LEVEL_2);
#endif

    for (unsigned int page = 0; index < PTES_PER_PT; index++, page++) {
        lpt[index] = PTE_CREATE_LEAF(ROUND_DOWN((uintptr_t)_text, PT_LEVEL_2_BITS) +
                                     (page << PT_LEVEL_2_BITS));
    }

    /* Map the kernel into the new address space */

    index = GET_PT_INDEX(kernel_info->virt_region_start, PT_LEVEL_1);

#if __riscv_xlen == 64
    lpt = l2pt;
    l1pt[index] = PTE_CREATE_NEXT((uintptr_t)l2pt);
    index = GET_PT_INDEX(kernel_info->virt_region_start, PT_LEVEL_2);
#endif

    for (unsigned int page = 0; index < PTES_PER_PT; index++, page++) {
        lpt[index] = PTE_CREATE_LEAF(ROUND_DOWN(kernel_info->phys_region_start, PT_LEVEL_2_BITS) +
                                     (page << PT_LEVEL_2_BITS));
    }
}

static inline void sfence_vma(void)
{
    asm volatile("sfence.vma" ::: "memory");
}

static inline void ifence(void)
{
    asm volatile("fence.i" ::: "memory");
}

static inline void enable_virtual_memory(void)
{
    sfence_vma();

#if CONFIG_PT_LEVELS == 2
    const word_t vm_mode = WORD_CONST(0x1) << 31;
#elif CONFIG_PT_LEVELS == 3
    const word_t vm_mode = WORD_CONST(0x8) << 60;
#elif CONFIG_PT_LEVELS == 4
    const word_t vm_mode = WORD_CONST(0x9) << 60;
#else
#error "Wrong PT level"
#endif

    asm volatile(
        "csrw satp, %0\n"
        :
        : "r"(vm_mode | (uintptr_t)l1pt >> RISCV_PGSHIFT)
        :
    );
    ifence();
}

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

    /* Create MMU tables, but don't enable MMU yet. */
    map_kernel_window(&kernel_info);

    return 0;
}

static NORETURN void handover_to_next_boot_stage(word_t hart_id, word_t core_id)
{
    /* log state infos on pimary core only */
    if (0 == core_id) {
        printf("Enabling MMU and paging\n");
    }

    enable_virtual_memory();

    if (0 == core_id) {
        printf("Jumping to kernel-image entry point...\n\n");
    }

    /* This is not supposed to return. Adding or modifying these parameters
     * required updating the registers in the nex't boot stage also, see head.S
     * from the seL4 kernel sources.
     */
    ((init_riscv_kernel_t)kernel_info.virt_entry)(
        user_info.phys_region_start,
        user_info.phys_region_end,
        user_info.phys_virt_offset,
        user_info.virt_entry,
        (word_t)dtb,
        dtb_size,
        hart_id,
        core_id);

    /* We should never get here. */
    printf("ERROR: ELF-loader didn't hand over control\n");
    abort();
    UNREACHABLE();
}

int hsm_exists = 0; /* assembly startup code will initialise this */

#if CONFIG_MAX_NUM_NODES > 1

extern void secondary_harts(word_t hart_id, word_t core_id);

int secondary_go = 0;
int next_logical_core_id = 1; /* incremented by assembly code  */
int mutex = 0;
int core_info[CONFIG_MAX_NUM_NODES] = { 0 };

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

static void set_core_info(int hart_id, int core_id)
{
    /* call should hold the multicore lock here */
    core_info[core_id] = BIT(31) | hart_id;
}

static int get_core_info(int core_id)
{
    return __atomic_load_n(&core_info[core_id], __ATOMIC_ACQUIRE);
}

static word_t smp_init(word_t hart_id)
{
    /* If we have an SBI with HSM, then secondary cores should not be running
     * here at all. We will start them and then they are synchronized on this
     * signal. For an older SBI, the cores  might be running, but we have
     * stopped them and they will also synchronize on this signal.
     */
    set_secondary_cores_go();

    /* If SBI implements HSM, then use it to start the other cores. Otherwise
     * all we can do here is hope they have been started somehow and report
     * they are ready.
     */
    if (hsm_exists) {
        for (unsigned int i = 0; i < CONFIG_MAX_NUM_NODES; ++i) {
            unsigned int h = i + CONFIG_FIRST_HART_ID;
            if (h != hart_id) {
                sbi_hart_start(h, secondary_harts, h);
            }
        }
    }

    /* We are core 0, but this might be just from the random boot order. Check
     * all other cores that have come up to ensure CONFIG_FIRST_HART_ID will be
     * core 0 when we enter the next boot stage.
     */
    word_t core_id = 0;
    for (int i = 1; i < CONFIG_MAX_NUM_NODES; i++) {
        /* wait for core to be ready */
        int info;
        do {
            info = get_core_info(i);
        } while (0 == info);
        /* Check if this is the designated primary core. If so, put our
         * hart ID into this slot, and once we are done with the loop give
         * our slot 0 to this core
         */
        if (CONFIG_FIRST_HART_ID == (info & 0x7fffffff)) {
            set_core_info(hart_id, i); /* put us in the other slot */
            core_id = i; /* we are no longer core 0 */
            acquire_multicore_lock();
            printf("becomming secondary core: hart ID %"PRIu_word"\n", hart_id);
            release_multicore_lock();
        }
    }

    /* All other cores are still waiting for the designated primary core,
     * set its info now to unblock then.
     */
    set_core_info(CONFIG_FIRST_HART_ID, 0);

    return core_id;
}

void secondary_entry(word_t hart_id, word_t core_id)
{
    block_until_secondary_cores_go();

    acquire_multicore_lock();
    printf("Secondary entry hart_id:%"PRIu_word" core_id:%"PRIu_word"\n",
           hart_id, core_id);
    set_core_info(hart_id, core_id);
    release_multicore_lock();

    /* Wait for all other cores to finish. */
    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        while (0 == get_core_info(i)) {
            /* busy waiting loop */
        }
    }

    /* Check if we are the primary core for the next stage. If so, things have
     * already been updated by the old primary core, and we can just start
     * using the core ID 0 from now on.
     */
    if (hart_id == CONFIG_FIRST_HART_ID) {
        acquire_multicore_lock();
        printf("Becomming primary core: hart_id:%"PRIu_word"\n", hart_id);
        release_multicore_lock();
        core_id = 0;
    }

    /* This will not return. */
    handover_to_next_boot_stage(hart_id, core_id);
    UNREACHABLE();
}

#endif /* CONFIG_MAX_NUM_NODES > 1 */

void main(word_t hart_id, void *bootloader_dtb)
{
    /* Printing uses SBI, so there is no need to initialize any UART. */
    printf("ELF-loader started on (HART %"PRIu_word") (NODES %u)\n",
           hart_id, (unsigned int)CONFIG_MAX_NUM_NODES);

    printf("  paddr=[%p..%p]\n", _text, (uintptr_t)_end - 1);

    /* Run the actual ELF loader: load ELF images, create MMU tables. */
    int ret = run_elfloader(bootloader_dtb);
    if (0 != ret) {
        printf("ERROR: ELF-loader failed, code %d\n", ret);
        /* There is nothing we can do to recover. */
        abort();
        UNREACHABLE();
    }

#if CONFIG_MAX_NUM_NODES > 1
    word_t core_id = smp_init(hart_id);
#else
    word_t core_id = 0;
#endif

    /* This will not return. */
    handover_to_next_boot_stage(hart_id, core_id);
    UNREACHABLE();
}
