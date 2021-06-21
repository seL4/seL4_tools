/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>

#include <printf.h>
#include <types.h>
#include <strops.h>
#include <binaries/elf/elf.h>
#include <cpio/cpio.h>

#include <elfloader.h>
#include <fdt.h>

#ifdef CONFIG_HASH_SHA
#include "crypt_sha256.h"
#elif CONFIG_HASH_MD5
#include "crypt_md5.h"
#endif

#include "hash.h"

#ifdef CONFIG_ELFLOADER_ROOTSERVERS_LAST
#include <platform_info.h> // this provides memory_region
#endif

#define KEEP_HEADERS_SIZE BIT(PAGE_BITS)

extern char _bss[];
extern char _bss_end[];

/*
 * Clear the BSS segment
 */
void clear_bss(void)
{
    char *start = _bss;
    char *end = _bss_end;
    while (start < end) {
        *start = 0;
        start++;
    }
}

/*
 * print CPIO file info
 */
static void print_info_cpio_file(
    char const *indent_str,
    char const *name,
    void const *blob,
    size_t size)
{
    printf("%sCPIO %s file [%p..%p], %zu byte\n",
           indent_str,
           name,
           blob,
           (void *)((uintptr_t)blob + size - 1),
           size);
}

/*
 * print data block copy operation
 */
static void print_copy_operation(
    char const *indent_str,
    char const *op_str,
    void const *src,
    void const *dst,
    size_t size)
{
    if (0 == size) {
        printf("%s%s [%p -> %p, size 0]\n", indent_str, op_str, src, dst);
    } else {
        printf("%s%s [%p..%p] -> [%p..%p], %d byte\n",
               indent_str,
               op_str,
               src,
               (void *)((uintptr_t)src + size - 1),
               dst,
               (void *)((uintptr_t)dst + size - 1),
               size);
    }
}

/*
 * Determine if two intervals overlap.
 */
static int regions_overlap(
    uintptr_t startA,
    uintptr_t endA,
    uintptr_t startB,
    uintptr_t endB)
{
    if (endA < startB) {
        return 0;
    }
    if (endB < startA) {
        return 0;
    }
    return 1;
}

/*
 * Ensure that we are able to use the given physical memory range.
 *
 * We fail if the destination physical range overlaps us, or if it goes outside
 * the bounds of memory.
 */
static int ensure_phys_range_valid(
    paddr_t paddr_min,
    paddr_t paddr_max)
{
    /*
     * Ensure that the physical load address of the object we're loading (called
     * `name`) doesn't overwrite us.
     */
    if (regions_overlap(paddr_min,
                        paddr_max - 1,
                        (uintptr_t)_text,
                        (uintptr_t)_end - 1)) {
        printf("ERROR: image load address overlaps with ELF-loader!\n");
        return -1;
    }

    return 0;
}

/*
 * check hash of ELF
 */
static int check_hash(
    void const *cpio,
    size_t cpio_len,
    void const *elf_blob,
    size_t elf_blob_size,
    char const *elf_hash_filename)
{

#ifdef CONFIG_HASH_NONE

    UNUSED_VARIABLE(cpio);
    UNUSED_VARIABLE(cpio_len);
    UNUSED_VARIABLE(elf_blob);
    UNUSED_VARIABLE(elf_blob_size);
    UNUSED_VARIABLE(elf_hash_filename);

#else

    /* Get the binary file that contains the Hash */
    unsigned long cpio_file_size = 0;
    void const *file_hash = cpio_get_file(cpio,
                                          cpio_len,
                                          elf_hash_filename,
                                          &cpio_file_size);

    /* If the file hash doesn't have a pointer, the file doesn't exist, so we
     * cannot confirm the file is what we expect.
     */
    if (file_hash == NULL) {
        printf("ERROR: hash file '%s' doesn't exist\n", elf_hash_filename);
        return -1;
    }

    /* Ensure we can safely cast the CPIO API type to our preferred type. */
    _Static_assert(sizeof(cpio_file_size) <= sizeof(size_t),
                   "integer model mismatch");
    size_t file_hash_len = (size_t)cpio_file_size;

#ifdef CONFIG_HASH_SHA
    uint8_t calculated_hash[32];
    hashes_t hashes = { .hash_type = SHA_256 };
#else
    uint8_t calculated_hash[16];
    hashes_t hashes = { .hash_type = MD5 };
#endif

    if (file_hash_len < sizeof(calculated_hash)) {
        printf("ERROR: hash file '%s' size %u invalid, expected at least %u\n",
               elf_hash_filename, file_hash_len, sizeof(calculated_hash));
    }

    /* Print the Hash for the user to see */
    printf("Hash from ELF File: ");
    print_hash(file_hash, sizeof(calculated_hash));

    get_hash(hashes, elf_blob, elf_blob_size, calculated_hash);

    /* Print the hash so the user can see they're the same or different */
    printf("Hash for ELF Input: ");
    print_hash(calculated_hash, sizeof(calculated_hash));

    /* Check the hashes are the same. There is no memcmp() in the striped down
     * runtime lib of ELF Loader, so we compare here byte per byte. */
    for (unsigned int i = 0; i < sizeof(calculated_hash); i++) {
        if (((char const *)file_hash)[i] != ((char const *)calculated_hash)[i]) {
            printf("ERROR: Hashes are different\n");
            return -1;
        }
    }

#endif  /* CONFIG_HASH_NONE */

    return 0;

}

/*
 * Load an ELF file into physical memory at the given physical address.
 *
 * 'paddr' holds the physical start address and returns the byte after the last
 * byte of the physical area used.
 */
static int load_elf(
    void const *elf_blob,
    int keep_headers,
    paddr_t *p_paddr,
    struct image_info *info)
{
    int ret;
    paddr_t paddr = *p_paddr;

    /* Ensure that the ELF file itself is 4-byte aligned in memory, so libelf
     * can perform word accesses on it. */
    if (!IS_ALIGNED((uintptr_t)elf_blob, 2)) {
        printf("ERROR: ELF in CPIO not 4-byte aligned!\n", paddr);
        return -1;
    }

    /* get the memory bounds, but unlike other functions, this returns 1 on
     * success, anything else is an error.
     */
    uint64_t elf_vaddr_start = 0;
    uint64_t elf_vaddr_end = 0;
    ret = elf_getMemoryBounds(elf_blob, 0, &elf_vaddr_start, &elf_vaddr_end);
    if (ret != 1) {
        printf("ERROR: Could not get image bounds!\n");
        return -1;
    }

    /* we can only handle ELF files where the virtual addresses fit in the
     * memory we can address. Usually this should not be an issue, because a
     * 32-bit ElfLoader can't be expected to deal with ELF files for 64-bit
     * platforms. Nevertheless the ELF format uses 64-bit addresses, so we have
     * to check range violations before we cast the integers down.
     */
    if ((elf_vaddr_start > UINTPTR_MAX) || (elf_vaddr_end > UINTPTR_MAX)) {
        printf("ERROR: ELF file uses virtual addresses beyond UINTPTR_MAX!\n");
        return -1;
    }
    uintptr_t vaddr_start = (uintptr_t)elf_vaddr_start;
    uintptr_t vaddr_end = (uintptr_t)elf_vaddr_end;


    /* round up size to the end of the page next page */
    vaddr_end = ROUND_UP(vaddr_end, PAGE_BITS);
    size_t image_size = (size_t)(vaddr_end - vaddr_start);
    vaddr_t entry = (vaddr_t)elf_getEntryPoint(elf_blob);

    /* print diagnostics first, then do error checks*/
    printf("  paddr=[%p..%p], %zu byte\n",
           (void *)paddr,
           (void *)(paddr + image_size - 1),
           image_size);
    printf("  vaddr=[%p..%p]\n", (void *)vaddr_start, (void *)(vaddr_end - 1));
    printf("  virt_entry=%p\n", (void *)entry);


    /* Ensure the physical and virtual start address is page aligned. */
    if (!IS_ALIGNED(paddr, PAGE_BITS) || !IS_ALIGNED(vaddr_start, PAGE_BITS)) {
        printf("ERROR: physical or virtual address not 2^%d page aligend!\n",
               PAGE_BITS);
        return -1;
    }

    /* Ensure that region we want to write to is sane. */
    ret = ensure_phys_range_valid(paddr, paddr + image_size);
    if (0 != ret) {
        printf("ERROR: Physical address range invalid\n");
        return -1;
    }

    /* Ensure the ELF file is valid. */
    ret = elf_checkFile(elf_blob);
    if (0 != ret) {
        printf("ERROR: Invalid ELF file\n");
        return -1;
    }

    /* Record information about the placement of the image. */
    info->phys_region_start = paddr;
    info->phys_region_end = paddr + image_size;
    info->virt_region_start = (vaddr_t)vaddr_start;
    info->virt_region_end = (vaddr_t)vaddr_end;
    info->virt_entry = entry;
    info->phys_virt_offset = paddr - (paddr_t)vaddr_start;

    /* Zero out all memory in the region, as the ELF file may be sparse. */
    memset((void *)paddr, 0, image_size);

    /* Load each segment in the ELF file. */
    for (unsigned int i = 0; i < elf_getNumProgramHeaders(elf_blob); i++) {

        size_t elf_offset = elf_getProgramHeaderOffset(elf_blob, i);
        void *src = (void *)((uintptr_t)elf_blob + elf_offset);

        vaddr_t dst_vaddr = elf_getProgramHeaderVaddr(elf_blob, i);
        void *dst = (void *)(dst_vaddr + info->phys_virt_offset);

        size_t size = elf_getProgramHeaderFileSize(elf_blob, i);

        /* Skip segments that are not marked as being loadable. */
        if (PT_LOAD != elf_getProgramHeaderType(elf_blob, i)) {
            // print_copy_operation("  ", "ignore non-PT_LOAD segment", src,
            //                      dst, size);
            continue;
        }

        /* Skip empty segments */
        if (0 == size) {
            // print_copy_operation("  ", "ignore empty segment", src, dst,
            //                      size);
            continue;
        }

        /* copy segment from CPIO to target memory location */
        print_copy_operation("  ", "copy segment", src, dst, size);
        memcpy(dst, src, size);
    }

    /* Round up the destination address to the next page */
    paddr = ROUND_UP(paddr + image_size, PAGE_BITS);

    if (keep_headers) {
        /* Put the ELF headers in this page */
        uint32_t phnum = elf_getNumProgramHeaders(elf_blob);
        uint32_t phsize;
        paddr_t source_paddr;
        if (ISELF32(elf_blob)) {
            phsize = ((struct Elf32_Header const *)elf_blob)->e_phentsize;
            source_paddr = (paddr_t)elf32_getProgramHeaderTable(elf_blob);
        } else {
            phsize = ((struct Elf64_Header const *)elf_blob)->e_phentsize;
            source_paddr = (paddr_t)elf64_getProgramHeaderTable(elf_blob);
        }
        /* We have no way of sharing definitions with the kernel so we just
         * memcpy to a bunch of magic offsets. Explicit numbers for sizes and
         * offsets are used so that it is clear exactly what the layout is
         */
        memcpy((void *)paddr, &phnum, 4);
        memcpy((void *)(paddr + 4), &phsize, 4);
        memcpy((void *)(paddr + 8), (void *)source_paddr, phsize * phnum);
        /* return the frame after our headers */
        paddr += KEEP_HEADERS_SIZE;
    }

    *p_paddr = paddr;
    return 0;
}

/*
 * Load the kernel ELF
 */
static int load_kernel_elf(
    void const *cpio,
    size_t cpio_len,
    struct image_info *kernel_info,
    paddr_t *next_phys_addr)
{
    int ret;
    char const *const kernel_filename = "kernel.elf";

    printf("ELF-loading kernel\n");

    /* Locate the kernel in the CPIO archive */
    unsigned long cpio_file_size = 0;
    void const *elf_blob = cpio_get_file(cpio, cpio_len, kernel_filename,
                                         &cpio_file_size);
    if (!elf_blob) {
        printf("ERROR: No kernel image present in archive\n");
        return -1;
    }

    /* Ensure we can safely cast the CPIO API type to our preferred type. */
    _Static_assert(sizeof(cpio_file_size) <= sizeof(size_t),
                   "integer model mismatch");
    size_t elf_blob_size = cpio_file_size;
    print_info_cpio_file("  ", "ELF", elf_blob, elf_blob_size);

    /* Kernel must be the first image in the archive. */
    char const *filename = NULL;
    cpio_get_entry(cpio, cpio_len, 0, &filename, NULL);
    ret = strcmp(kernel_filename, filename);
    if (0 != ret) {
        printf("ERROR: Kernel image not first image in archive\n");
        return -1;
    }

    /* check hash (if enabled) */
    ret = check_hash(cpio, cpio_len, elf_blob, elf_blob_size, "kernel.bin");
    if (0 != ret) {
        printf("Hash check failed!\n");
        return -1;
    }

    /* ensure the ELF file is a valid ELF */
    if (0 != elf_checkFile(elf_blob)) {
        printf("ERROR: Kernel image not a valid ELF file\n");
        return -1;
    }

    /* Get physical memory bounds. Unlike most other functions, this returns 1
     * on success and anything else is an error.
     */
    uint64_t phys_start = 0;
    uint64_t phys_end = 0;
    ret = elf_getMemoryBounds(elf_blob, 1, &phys_start, &phys_end);
    if (1 != ret) {
        printf("ERROR: could not get kernel memory bounds\n");
        return -1;
    }

    /* Load the kernel from the ELF blob, don't keep ELF headers, throw the
     * value returned phys_addr away. */
    paddr_t phys_addr = (paddr_t)phys_start;
    ret = load_elf(elf_blob, 0, &phys_addr, kernel_info);
    if (0 != ret) {
        printf("ERROR: could not load kernel ELF!\n");
        return -1;
    }

    /* keep it page aligned */
    *next_phys_addr = (paddr_t)ROUND_UP(phys_end, PAGE_BITS);
    return 0;
}

/*
 * install the DTB
 */
static int install_dtb(
    void const *cpio,
    size_t cpio_len,
    void const *bootloader_dtb,
    paddr_t *next_phys_addr,
    unsigned int *user_elf_offset,
    void const **ret_dtb,
    size_t *ret_dtb_size)
{
    int ret;

    printf("installing DTB\n");

    /* set default to indicate there is no DTB */
    if (ret_dtb) {
        *ret_dtb = NULL;
    }

    if (ret_dtb_size) {
        *ret_dtb_size = 0;
    }

    void const *dtb = NULL;
    size_t dtb_cpio_file_size = 0;

#ifdef CONFIG_ELFLOADER_INCLUDE_DTB

    char const *const dtb_name = "kernel.dtb";
    unsigned long cpio_file_size = 0;
    dtb = cpio_get_file(cpio, cpio_len, dtb_name, &cpio_file_size);
    if (!dtb) {
        printf("  CPIO has no DTB\n");
    } else {
        /* Ensure we can safely cast the CPIO API type to our preferred type. */
        _Static_assert(sizeof(cpio_file_size) <= sizeof(size_t),
                       "integer model mismatch");
        dtb_cpio_file_size = (size_t)cpio_file_size;
        print_info_cpio_file("  ", "DTB", dtb, dtb_cpio_file_size);

        /* if a DTB is present, it must be the second image in the archive */
        char const *elf_filename;
        cpio_get_entry(cpio, cpio_len, 1, &elf_filename, NULL);
        ret = strcmp(elf_filename, dtb_name);
        if (0 !=  ret) {
            printf("ERROR: Kernel DTB not second image in archive.\n");
            return -1;
        }
        /* user images start after DTB */
        *user_elf_offset += 1;
    }

#endif /* CONFIG_ELFLOADER_INCLUDE_DTB */

    /* Use DTB from bootloader if CPIO does not contain one */
    if (!dtb) {
        if (!bootloader_dtb) {
            printf("DBT processing disabled\n");
            /* ret_dtb and ret_dtb_size were already set to indicate there is no
             * DTB, next_phys_addr is simply left unchanged
             */
            return 0;
        }

        dtb = bootloader_dtb;
        printf("  Using DBT from bootloader at %p.\n", dtb);
    }

    /* We have a DTB, either from the CPIO or from the booloader. Check that it
     * is valid and put it after the kernel. */
    size_t dtb_size = fdt_size(dtb);
    if (0 == dtb_size) {
        printf("ERROR: Invalid device tree blob supplied!\n");
        return -1;
    }

    if ((0 != dtb_cpio_file_size) && (dtb_size > dtb_cpio_file_size)) {
        printf("ERROR: parsed device tree (%zu byte) larger than CPIO file (%zu byte)\n",
               dtb_size, dtb_cpio_file_size);
        return -1;
    }

    paddr_t phys_start = *next_phys_addr;
    paddr_t phys_end = phys_start + dtb_size;

    print_copy_operation("  ", "put DTB behind kernel", dtb, (void *)phys_start,
                         dtb_size);

    /* Make sure the physical location is sane */
    ret = ensure_phys_range_valid(phys_start, phys_end);
    if (0 != ret) {
        printf("ERROR: Physical target address of DTB invalid!\n");
        return -1;
    }

    memmove((void *)phys_start, dtb, dtb_size);

    if (next_phys_addr) {
        *next_phys_addr = ROUND_UP(phys_end, PAGE_BITS);
    }

    if (ret_dtb) {
        *ret_dtb = (void *)phys_start;
    }

    if (ret_dtb_size) {
        *ret_dtb_size = dtb_size;
    }

    return 0;
}

/*
 * Install userspace images.
 *
 * All we support is loading the images into memory. The kernel or root task
 * may then decide how to handle this, e.g. run the n'th user image in the n'th
 * core.
 */
static int load_app_images(
    void const *cpio,
    size_t cpio_len,
    unsigned int user_elf_offset,
    unsigned int max_user_images,
    struct image_info *user_info,
    unsigned int *num_images,
    paddr_t *p_next_phys_addr)
{
    int ret;
    paddr_t next_phys_addr = *p_next_phys_addr;

#ifdef CONFIG_ELFLOADER_ROOTSERVERS_LAST

    /* calculate the size of the user images - this corresponds to how much
     * memory load_elf uses
     */
    unsigned int total_user_image_size = 0;
    for (unsigned int i = 0; i < max_user_images; i++) {
        void const *elf_blob = cpio_get_entry(cpio,
                                              cpio_len,
                                              user_elf_offset + i,
                                              NULL,
                                              NULL);
        if (!elf_blob) {
            break;
        }

        /* Get the memory bounds. Unlike most other functions, this returns 1 on
         * success and anything else is an error.
         */
        uint64_t min_vaddr, max_vaddr;
        int ret = elf_getMemoryBounds(elf_blob, 0, &min_vaddr, &max_vaddr);
        if (ret != 1) {
            printf("ERROR: Could not get bounds for image %u\n", i);
            return -1;
        }
        /* round up size to the end of the page next page */
        total_user_image_size += (ROUND_UP(max_vaddr, PAGE_BITS) - min_vaddr)
                                 + KEEP_HEADERS_SIZE;
    }

    /* calculate location if the the user image */
    next_phys_addr = ROUND_DOWN(memory_region[0].end, PAGE_BITS)
                     - ROUND_UP(total_user_image_size, PAGE_BITS);

#endif /* CONFIG_ELFLOADER_ROOTSERVERS_LAST */

    unsigned int img_cnt = 0;
    for (unsigned int i = 0; i < max_user_images; i++) {

        /* Fetch info about the next ELF file in the archive. */
        char const *elf_filename;
        unsigned long cpio_file_size = 0;
        void const *elf_blob = cpio_get_entry(cpio,
                                              cpio_len,
                                              user_elf_offset + i,
                                              &elf_filename,
                                              &cpio_file_size);
        if (!elf_blob) {
            break;
        }

        /* Ensure we can safely cast the CPIO API type to our preferred type. */
        _Static_assert(sizeof(cpio_file_size) <= sizeof(size_t),
                       "integer model mismatch");
        size_t elf_blob_size = (size_t)cpio_file_size;

        /* Print diagnostics. */
        printf("ELF loading app '%s'\n", elf_filename);
        print_info_cpio_file("  ", "ELF", elf_blob, elf_blob_size);

        /* check hash (if enabled) */
        ret = check_hash(cpio, cpio_len, elf_blob, elf_blob_size, "app.bin");
        if (0 != ret) {
            printf("ERROR: Hash check failed\n");
            return -1;
        }

        /* Load the file into memory, keep ELF headers */
        ret = load_elf(elf_blob, 1, &next_phys_addr, &user_info[i]);
        if (0 != ret) {
            printf("ERROR: could not load user image ELF!\n");
            return -1;
        }

        /* Since we could load the image successfully and we don't do any
         * cleanup on error, we can also let the caller know how many images
         * we could load successfully.
         */
        *num_images = ++img_cnt;
        *p_next_phys_addr = next_phys_addr;
    }

    return 0;
}

/*
 * Load ELF images
 *
 * We assume that the MMU is disabled and we are running on physical memory
 * here. We have to load a kerne ELF file and one or more ELF files for the
 * userspace applications. Typically there will only be one userspace ELF file,
 * though if we are running a multi-core CPU, we may have multiple userspace
 * images, one per CPU. All ELF files are packed into a CPIO archive.
 * The kernel ELF file indicates what physical address it wants to be loaded at,
 * while userspace images run purely from virtual memory and don't have any
 * requirements about where they are located physically. We place the kernel at
 * its desired  location, and then load userspace images straight after it in
 * physical memory.
 * Several things could go wrong here, the images (including the kernel) might
 * not even match the current memory configuration be broken, so we have to do a
 * lot of sanity checks. Especially, we should never overwrite our own code and
 * thn get completely lost.
 */
int load_images(
    struct image_info *kernel_info,
    struct image_info *user_info,
    unsigned int max_user_images,
    unsigned int *num_images,
    void const *bootloader_dtb,
    void const **chosen_dtb,
    size_t *chosen_dtb_size)
{
    int ret;

    /* Set safe defaults. */
    if (num_images) {
        *num_images = 0;
    }

    if (chosen_dtb) {
        *chosen_dtb = NULL;
    }

    if (chosen_dtb_size) {
        *chosen_dtb_size = 0;
    }

    void const *cpio = _archive_start;
    size_t cpio_len = _archive_start_end - _archive_start;
    paddr_t next_phys_addr = 0;
    unsigned int user_elf_offset = 1;

    /* load the kernel from CPIO */
    ret = load_kernel_elf(cpio, cpio_len, kernel_info, &next_phys_addr);
    if (0 != ret) {
        printf("ERROR: loading kernel failed!\n");
        return -1;
    }

    /* Install device tree binary after kernel, it can come either from CPIO or
     * is passed by the previous bootloader.
     */
    ret = install_dtb(cpio,
                      cpio_len,
                      bootloader_dtb,
                      &next_phys_addr,
                      &user_elf_offset,
                      chosen_dtb,
                      chosen_dtb_size);
    if (0 != ret) {
        printf("ERROR: loading DTB failed!\n");
        return -1;
    }

    /* Load app images. */
    ret = load_app_images(cpio, cpio_len, user_elf_offset, max_user_images,
                          user_info, num_images, &next_phys_addr);
    if (0 != ret) {
        printf("loading userspace failed!\n");
        return -1;
    }

    return 0;
}

/*
 * Platform specific ELF Loader initialization. Can be overwritten.
 */
WEAK void platform_init(void)
{
    /* nothing by default */
}
