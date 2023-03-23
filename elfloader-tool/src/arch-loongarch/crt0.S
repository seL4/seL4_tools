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
#include <machine.h>

.extern main
.extern elfloader_stack_alloc

#define BIT(n) (1 << (n))

.section ".text.start"

.global _start
_start:

  csrrd $t0, LOONGARCH_CSR_CPUID
  move $s0, $t0
  li.d $t1, CONFIG_FIRST_HART_ID
  bne $t0, $t1, spin_hart

#ifdef CONFIG_IMAGE_BINARY
/* Clear the BSS before we get to do anything more specific */
  jal clear_bss
#endif

  move $a0, $t0
  b _start1

_start1: /* a0 must hold current hard ID passed by bootloader */
         /* a1 must hold dtb address passed by bootloader */
  li.d    $t0, 0x6000000000000009 # no cache for uart, PLV0|PLV3, for hardware platforms
  csrwr   $t0, LOONGARCH_CSR_DMWIN0
  li.d		$t0, 0x9000000000000011	# CA, PLV0, 0x9000 xxxx xxxx xxxx
	csrwr		$t0, LOONGARCH_CSR_DMWIN1
  # li.d		$t0, 0x0000000000000011	# CA, PLV0, 0x9000 xxxx xxxx xxxx
	# csrwr		$t0, LOONGARCH_CSR_DMWIN2

	li.w		$t0, 0xb0		# PLV=0, IE=0, PG=1
	csrwr		$t0, LOONGARCH_CSR_CRMD
	li.w		$t0, 0x04		# PLV=0, PIE=1, PWE=0
	csrwr		$t0, LOONGARCH_CSR_PRMD
	li.w		$t0, 0x00		# FPE=0, SXE=0, ASXE=0, BTE=0
	csrwr		$t0, LOONGARCH_CSR_EUEN

  la.abs	$t0, 0f
	jirl		$zero, $t0, 0

0:
  /* Attach the stack to sp before calling any C functions */
  la $sp, elfloader_stack_alloc
  li.w $t0, 1
  slli.w $t0, $t0, 12
  add.d $sp, $sp, $t0
  /* The C code expects the registers to be set up as:
   *   a0 = hart id
   *   a1 = dtb
   */
  b main

spin_hart:
  idle 0
  b spin_hart
