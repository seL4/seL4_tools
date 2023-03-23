/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * 
 * Derived from:
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2015, 2016 Hesham Almatary <heshamelmatary@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <machine.h>
#define REGBYTES 8

.section elfloader_debug_eentry

.global elfloader_trap_entry
.extern elfloader_panic

elfloader_trap_entry:

    csrrd $s0, LOONGARCH_CSR_ESTAT // read csr_estat to s0

    b elfloader_panic