/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>

.extern main
.extern __global_pointer$
.extern elfloader_stack
#if CONFIG_MAX_NUM_NODES > 1
.extern secondary_hart_main
.extern next_logical_core_id
#endif

#define BIT(n) (1 << (n))

/* SBI commands */
#define SBI_HSM_BASE 0x48534DULL
#define SBI_HSM_BASE_HART_START 0
#define SBI_HSM_BASE_HART_STOP 1
#define SBI_EXT_BASE 0x10
#define SBI_EXT_BASE_PROBE_EXT 3

.section ".text.start"

/* OpenSBI starts us these parameters:
 *   a0: hart id
 *   a1: dtb
 *
 * On RISC-V, only M-Mode can access the CSR mhartid to get the actual hart ID,
 * the SBI running there is responsible for passing this ID up. In S-Mode there
 * is no way to ever query it again, so we have to preserve what we get passed
 * here. This is a RISC-V design decision, more background can be found at
 * https://github.com/riscv/riscv-sbi-doc/issues/25.
 * It seems that OpenSBI starts us at a random hart and keeps all other harts
 * suspended or spinning. However, even on non-SMP configurations there might
 * be an expectation that we are running on CONFIG_FIRST_HART_ID. If the current
 * hart turns out to be a different one, we have to switch harts somehow. The
 * SBI Heart State Management (HSM) extension exists for this, but it might not
 * be implemented. In this case, there is nothing we can do here in the assembly
 * startup code, but C boot code might still have platform specific proprietary
 * ways to switch harts.
 */

.global _start
_start:

  /* save the parameters passed */
  mv s0, a0 /* preserve a0 (hart id) in s0 */
  mv s1, a1 /* preserve a1 (dtb) in s1 */

  /* prepare a minimal C environment with registers gp and sp set up. */
.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop
  la sp, (elfloader_stack + BIT(CONFIG_KERNEL_STACK_BITS))

  /* Clear the BSS before we get to do anything more specific */
  jal clear_bss

  /* Check if the Heart State Management (HSM) extension exists, so it can be
   * used to switch harts if we are not running on hart CONFIG_FIRST_HART_ID.
   * The SBI returns SBI_SUCCESS (0) in a0 if the call could be processed or an
   * error code if not. On SBI_SUCCESS the value in a1 is 0 if the extension is
   * not available or an extension-specific non-zero value if it is available.
   */
  li a7, SBI_EXT_BASE
  li a6, SBI_EXT_BASE_PROBE_EXT
  li a0, SBI_HSM_BASE
  ecall /* call SBI to probe for HSM extension */
  seqz t0, a0 /* t0 = (a0 == 0) to check SBI returned SBI_SUCCESS (0)  */
  snez t1, a1 /* t1 = (a1 != 0) to HSM extension exist */
  and a2, t0, t1 /* a2 = 1 if HSM extension is available, otherwise 0 */
  li t0, CONFIG_FIRST_HART_ID
  bne s0, t0, start_on_secondary
  mv a0, s0 /* restore a0 to hold hart ID */
  mv a1, s1 /* restore a1 to hold DTB passed on entry */
  /* a2 still hold the "HSM extension exists" flag, sp is already set up. Jump
   * to C code via function address in a register, as this extends the jump
   * range. Register t1 instead of t0 (x5) is used, because this is a designated
   * additional link register making jr a call and not a jump then.
   */
  la t1, main
  jr t1

/*----------------------------------------------------------------------------*/
start_on_secondary:
  /* We end up here if the startup code has detected that SBI has started us on
   * a hart that is not the designated primary hart. Try to switch to the
   * primary hart and continue the boot process there. This must be supported
   * even if CONFIG_MAX_NUM_NODES is set to 1. The register setup is:
   *   s0: hard ID
   *   s1: DTB passed from SBI
   *   a2: HSM extension exists flag
   */
  beqz a2, no_hsm_start_secondary
  /* Try to bring up the primary hart via the HSM extension */
  li a7, SBI_HSM_BASE
  li a6, SBI_HSM_BASE_HART_START
  li a0, CONFIG_FIRST_HART_ID /* hart id to start */
  la a1, hsm_entry_on_primary_hart /* where to start the hart */
  mv a2, s1 /* custom parameter passed in a1 is the DTB */
  ecall /* call SBI to start hart FIRST_HART_ID */
  /* Stop current hart, the boot code may bring it up again when needed. */
hsm_suspend_hart:
  li a7, SBI_HSM_BASE
  li a6, SBI_HSM_BASE_HART_STOP
  ecall /* call SBI to suspend current HART */
  /* this is not supposed to return. Seem the hart could not be stopped, there
   * is nothing we can do in this case as SBI seem broken.
   */
1:
  wfi
  j 1b

/*----------------------------------------------------------------------------*/
no_hsm_start_secondary:
 /* We end up here if we were not started on the designated primary core and SBI
  * does no implement the HSM extension, so we can't switch to the designated
  * primary hart. Looks like we are running on a legacy platform where all harts
  start in parallel. The register setup is:
  *   s0: hard ID
  *   s1: DTB passed from SBI
  *   a2: HSM extension exists flag, set to 0
  */
#if CONFIG_MAX_NUM_NODES > 1
  /* Simulate an SBI HSM extension entry, where a0 holds the hart ID and a1 a
   * custom value, which is the logical core ID in our usage. Determine it from
   * an atomic increment operation on the global variable next_logical_core_id,
   * what we use as our ID is the value it had before incrementing it.
   */
  mv a0, s0 /* restore a0 with hart ID */
  la t0, next_logical_core_id
  li t1, 1
  amoadd.w a1, t1, (t0) /* a1 is set to old value of next_logical_core_id */
  /* The logic core must be > 0, because we are not the primary core. Do another
   * atomic increment if it is 0.
   */
  bgtz a1, 1f
  amoadd.w a1, t1, (t0) /* a1 is set to old value of next_logical_core_id */
1:
  /* The logical core ID is valid only if less than CONFIG_MAX_NUM_NODES. */
  li t0, CONFIG_MAX_NUM_NODES
  blt a1, t0, hsm_entry_on_secondary_hart

#endif /* CONFIG_MAX_NUM_NODES > 1*/

 /* If we arrive here, this hart cannot be used because the number of supported
  * secondary hart has been exceeded. Maybe multi core support is not even
  * enabled at all. There is no SBI HSM extension here to turn off this hart, so
  * all we can do is spinning over a WFI. However, this is not guaranteed to
  * work forever, because the memory where the ELF loader keeps the loop can be
  * reused and overwritten by the kernel. This will lead to undefined behavior,
  * as we don't know what the new contents will be. If we are lucky, the loop
  * keeps running from a hart specific instruction cache, so the new memory
  * contents are ignored because no synchronization is triggered.
  */
1:
  wfi
  j 1b
/*----------------------------------------------------------------------------*/
hsm_entry_on_primary_hart:
  /* SBI has started us on a designated secondary hart, so we used the SBI HSM
   * extension to switch to the designated primary hart. The secondary hart was
   * shut down, so we can bring is up via the HSM extension when needed.
   * The register setup is:
   *   a0: hard ID
   *   a1: custom parameter: DTB from bootloader
   */

.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop
  la sp, (elfloader_stack + BIT(CONFIG_KERNEL_STACK_BITS))
  li a2, 1 /* remember that the HSM extension is available */
  la t1, main
  jr t1

/*----------------------------------------------------------------------------*/
#if CONFIG_MAX_NUM_NODES > 1

.global hsm_entry_on_secondary_hart
hsm_entry_on_secondary_hart:
  /* We enter here when the ELF-Loader starts a secondary hart via the SBI HSM
   * extension.
   * The register setup is:
   *   a0: hard ID
   *   a1: custom parameter: logical core ID
   * All we have to do here is set up a the register gp and sp before jumping to
   * C code.
   */

.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop

  /* setup stack based on the logical ID */
  la sp, elfloader_stack
  addi t0, a1, 1 /* increment by one because we need to set sp to the end */
  slli t0, t0, CONFIG_KERNEL_STACK_BITS /* t0 = t0 * BIT(CONFIG_KERNEL_STACK_BITS) */
  add sp, sp, t0

   * Jump to C code using a register, as this extends the jump range. Register
   * t1 instead of t0 (x5) is used, because this is a designated additional link
   * register making jr a call and not a jump then.
   */
  la t1, secondary_hart_main
  jr t1

#endif /* CONFIG_MAX_NUM_NODES > 1 */
