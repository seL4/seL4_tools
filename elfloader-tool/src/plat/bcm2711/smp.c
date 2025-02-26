/*
 * Copyright 2019, ARM Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>
#include <types.h>

#if CONFIG_MAX_NUM_NODES > 1
#include <types.h>
#include <elfloader.h>
#include <armv/machine.h>
#include <armv/smp.h>
#include <printf.h>
#include <abort.h>

#define MAX_CORES 4

extern void core_entry_head(unsigned long stack);

unsigned long core_stack;

uint64_t spin_table[4] = { 0xd8, 0xe0, 0xe8, 0xf0 };

void init_cpus(void)
{
    int nodes = CONFIG_MAX_NUM_NODES;
    if (nodes > MAX_CORES) {
        printf("CONFIG_MAX_NUM_NODES %d is greater than max number cores %d, will abort\n",
               CONFIG_MAX_NUM_NODES, MAX_CORES);
        abort();
    }
    for (int i = 1; i < nodes; i++) {
        /* all cores read the stack pointer from the same place */
        core_stack = (unsigned long) &core_stacks[i][0];
        *((volatile unsigned long *)(spin_table[i])) = (unsigned long)core_entry_head;
        dsb();
        asm volatile("sev");
        while (!is_core_up(i));
        printf("Core %d is up with logic ID %d\n", i, i);
    }

    /* set the logic id for the booting core */
    MSR("tpidr_el1", 0);
}

#endif /* CONFIG_MAX_NUM_NODES > 1 */
