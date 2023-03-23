/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * Derived from:
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */


#include <autoconf.h>
#include <elfloader/gen_config.h>
#include "image_start_addr.h"

ENTRY( _start )

SECTIONS
{
    . = IMAGE_START_ADDR;
    _text = .;
    .start :
    {
        *(.text.start)
    }
    .text :
    {
        *(.text)
        . = ALIGN(16K);
        elfloader_tlbrefillentry = .;
        *(elfloader_tlbrefillentry)
        . = ALIGN(16K);
        elfloader_debug_eentry = .;
        *(elfloader_debug_eentry)
    }
    . = ALIGN(16K);
    .rodata :
    {
        *(.srodata*)
        . = ALIGN(16K);
        *(.rodata)
        *(.rodata.*)
        /*
         * ld crashes when we add this here: *(_driver_list)
         */
        . = ALIGN(16K);
        _archive_start = .;
        *(._archive_cpio)
        _archive_end = .;
    }
    . = ALIGN(16K);
    .data :
    {
        *(.sdata*)
        *(.data)
        *(.data.*)
    }
    . = ALIGN(16K);
    .bss :
    {
        _bss = .;
        *(.sbss*)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }
     _end = .;
}
