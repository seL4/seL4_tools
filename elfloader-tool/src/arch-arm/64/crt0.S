/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>

#include <assembler.h>

.extern main

.section ".text.start"
BEGIN_FUNC(_start)
    adrp    x19, core_stack_alloc
    add     x19, x19, #0xff0
    mov     sp, x19
#ifdef CONFIG_IMAGE_BINARY
    stp     x0, x1, [sp, #-16]!
    bl      clear_bss
    /*
     * Binary images may not be loaded in the correct location.
     * Try and move ourselves so we're in the right place.
     */
    mov     x0, #0 /* Arg 0 of fixup_image_base is NULL */
    bl      fixup_image_base
    mov     x2, x0
    /* restore original arguments for next step */
    ldp     x0, x1, [sp, #-16]!
    /* fixup_image_base returns 0 if no need to move */
    cmp     x2, #0
    beq     1f
    /* otherwise, jump to the start of the new elfloader */

    br      x2
1:
#endif
    b       main
END_FUNC(_start)

/* Move the elf loader out of the kernel's way */
BEGIN_FUNC(finish_relocation)
    /*
     * On aarch64 the kernel is loaded at a very high address:
     * at least above 0x0000ff8080000000. We assume that
     * the ELF loader will never get loaded in a way that overlaps
     * with the kernel, so reaching this function is an error.
     */
    b abort // should never get here!
END_FUNC(finish_relocation)

/* secondary cpu startup */
BEGIN_FUNC(secondary_startup)
    /*
     * secondary_data is a struct that looks like this:
     * 0x0 void *entry
     * 0x8 void *stack
     */
    adrp    x19, secondary_data
    add     x19, x19, #:lo12:secondary_data

    ldr     x0, [x19, #0x8]     // load stack
    mov     sp, x0
    ldr     x1, [x19, #0x0]     // load entry point

    br x1
END_FUNC(secondary_startup)
