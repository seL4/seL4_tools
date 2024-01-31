/*
 * Copyright 2019, ARM Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>
#include <assembler.h>
#include <armv/assembler.h>

#if CONFIG_MAX_NUM_NODES > 1

.text

.extern core_entry
.extern core_stack
BEGIN_FUNC(core_entry_head)
    ldr x0, =core_stack
    ldr x3, [x0]
    mov x0, x3
    mov sp, x3
    b   core_entry
END_FUNC(core_entry_head)

#endif
