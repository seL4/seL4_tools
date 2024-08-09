/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <elfloader.h>
#include <types.h>
#include <mode/structures.h>

/* Top-level paging structures for kernel and identity mapping */
uint64_t _boot_pgd_up[BIT(PGD_BITS)] ALIGN(BIT(PGD_SIZE_BITS));
uint64_t _boot_pgd_down[BIT(PGD_BITS)] ALIGN(BIT(PGD_SIZE_BITS));
