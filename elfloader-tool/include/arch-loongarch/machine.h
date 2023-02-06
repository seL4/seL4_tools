/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#define PASTE(a, b) a ## b

#ifdef __ASSEMBLER__

#define UL_CONST(x) x
#define ULL_CONST(x) x
#define NULL 0

#else /* not __ASSEMBLER__ */

#define UL_CONST(x) PASTE(x, ul)
#define ULL_CONST(x) PASTE(x, llu)
#define NULL ((void *)0)

#endif

#define LOONGARCH_CSR_CRMD		0x0	/* Current mode info */
#define LOONGARCH_CSR_PRMD		0x1	/* Prev-exception mode info */
#define LOONGARCH_CSR_EUEN		0x2	/* Extended unit enable */
#define LOONGARCH_CSR_MISC		0x3	/* Misc config */
#define LOONGARCH_CSR_ECFG		0x4	/* Exception config */
#define LOONGARCH_CSR_ESTAT		0x5	/* Exception status */
#define LOONGARCH_CSR_ERA		0x6	/* ERA */
#define LOONGARCH_CSR_BADV		0x7	/* Bad virtual address */
#define LOONGARCH_CSR_BADI		0x8	/* Bad instruction */
#define LOONGARCH_CSR_EENTRY		0xc	/* Exception entry */

/* TLB related CSR registers */
#define LOONGARCH_CSR_TLBIDX		0x10	/* TLB Index, EHINV, PageSize, NP */
#define  CSR_TLBIDX_EHINV_SHIFT		31
#define  CSR_TLBIDX_EHINV		(UL_CONST(1) << CSR_TLBIDX_EHINV_SHIFT)
#define  CSR_TLBIDX_PS_SHIFT		24
#define  CSR_TLBIDX_PS_WIDTH		6
#define  CSR_TLBIDX_PS			(UL_CONST(0x3f) << CSR_TLBIDX_PS_SHIFT)
#define  CSR_TLBIDX_IDX_SHIFT		0
#define  CSR_TLBIDX_IDX_WIDTH		12
#define  CSR_TLBIDX_IDX			(UL_CONST(0xfff) << CSR_TLBIDX_IDX_SHIFT)
#define  CSR_TLBIDX_SIZEM		0x3f000000
#define  CSR_TLBIDX_SIZE		CSR_TLBIDX_PS_SHIFT
#define  CSR_TLBIDX_IDXM		0xfff
#define  CSR_INVALID_ENTRY(e)		(CSR_TLBIDX_EHINV | e)

#define LOONGARCH_CSR_TLBEHI		0x11	/* TLB EntryHi */
#define LOONGARCH_CSR_TLBELO0		0x12	/* TLB EntryLo0 */
#define  CSR_TLBLO0_RPLV_SHIFT		63
#define  CSR_TLBLO0_RPLV		(UL_CONST(0x1) << CSR_TLBLO0_RPLV_SHIFT)
#define  CSR_TLBLO0_NX_SHIFT		62
#define  CSR_TLBLO0_NX			(UL_CONST(0x1) << CSR_TLBLO0_NX_SHIFT)
#define  CSR_TLBLO0_NR_SHIFT		61
#define  CSR_TLBLO0_NR			(UL_CONST(0x1) << CSR_TLBLO0_NR_SHIFT)
#define  CSR_TLBLO0_PFN_SHIFT		12
#define  CSR_TLBLO0_PFN_WIDTH		36
#define  CSR_TLBLO0_PFN			(UL_CONST(0xfffffffff) << CSR_TLBLO0_PFN_SHIFT)
#define  CSR_TLBLO0_GLOBAL_SHIFT	6
#define  CSR_TLBLO0_GLOBAL		(UL_CONST(0x1) << CSR_TLBLO0_GLOBAL_SHIFT)
#define  CSR_TLBLO0_CCA_SHIFT		4
#define  CSR_TLBLO0_CCA_WIDTH		2
#define  CSR_TLBLO0_CCA			(UL_CONST(0x3) << CSR_TLBLO0_CCA_SHIFT)
#define  CSR_TLBLO0_PLV_SHIFT		2
#define  CSR_TLBLO0_PLV_WIDTH		2
#define  CSR_TLBLO0_PLV			(UL_CONST(0x3) << CSR_TLBLO0_PLV_SHIFT)
#define  CSR_TLBLO0_WE_SHIFT		1
#define  CSR_TLBLO0_WE			(UL_CONST(0x1) << CSR_TLBLO0_WE_SHIFT)
#define  CSR_TLBLO0_V_SHIFT		0
#define  CSR_TLBLO0_V			(UL_CONST(0x1) << CSR_TLBLO0_V_SHIFT)

#define LOONGARCH_CSR_TLBELO1		0x13	/* TLB EntryLo1 */
#define  CSR_TLBLO1_RPLV_SHIFT		63
#define  CSR_TLBLO1_RPLV		(UL_CONST(0x1) << CSR_TLBLO1_RPLV_SHIFT)
#define  CSR_TLBLO1_NX_SHIFT		62
#define  CSR_TLBLO1_NX			(UL_CONST(0x1) << CSR_TLBLO1_NX_SHIFT)
#define  CSR_TLBLO1_NR_SHIFT		61
#define  CSR_TLBLO1_NR			(UL_CONST(0x1) << CSR_TLBLO1_NR_SHIFT)
#define  CSR_TLBLO1_PFN_SHIFT		12
#define  CSR_TLBLO1_PFN_WIDTH		36
#define  CSR_TLBLO1_PFN			(UL_CONST(0xfffffffff) << CSR_TLBLO1_PFN_SHIFT)
#define  CSR_TLBLO1_GLOBAL_SHIFT	6
#define  CSR_TLBLO1_GLOBAL		(UL_CONST(0x1) << CSR_TLBLO1_GLOBAL_SHIFT)
#define  CSR_TLBLO1_CCA_SHIFT		4
#define  CSR_TLBLO1_CCA_WIDTH		2
#define  CSR_TLBLO1_CCA			(UL_CONST(0x3) << CSR_TLBLO1_CCA_SHIFT)
#define  CSR_TLBLO1_PLV_SHIFT		2
#define  CSR_TLBLO1_PLV_WIDTH		2
#define  CSR_TLBLO1_PLV			(UL_CONST(0x3) << CSR_TLBLO1_PLV_SHIFT)
#define  CSR_TLBLO1_WE_SHIFT		1
#define  CSR_TLBLO1_WE			(UL_CONST(0x1) << CSR_TLBLO1_WE_SHIFT)
#define  CSR_TLBLO1_V_SHIFT		0
#define  CSR_TLBLO1_V			(UL_CONST(0x1) << CSR_TLBLO1_V_SHIFT)

#define LOONGARCH_CSR_GTLBC		0x15	/* Guest TLB control */
#define LOONGARCH_CSR_TRGP		0x16	/* TLBR read guest info */

#define LOONGARCH_CSR_ASID		0x18	/* ASID */
#define LOONGARCH_CSR_PGDL		0x19	/* Page table base address when VA[47] = 0 */
#define LOONGARCH_CSR_PGDH		0x1a	/* Page table base address when VA[47] = 1 */
#define LOONGARCH_CSR_PGD		0x1b	/* Page table base */
#define LOONGARCH_CSR_PWCL		0x1c	/* PWCL */
#define LOONGARCH_CSR_PWCH		0x1d	/* PWCH */
#define LOONGARCH_CSR_STLBPGSIZE	0x1e
#define LOONGARCH_CSR_RVACFG		0x1f

/* Config CSR registers */
#define LOONGARCH_CSR_CPUID		0x20	/* CPU core id */
#define LOONGARCH_CSR_PRCFG1		0x21	/* Config1 */
#define LOONGARCH_CSR_PRCFG2		0x22	/* Config2 */
#define LOONGARCH_CSR_PRCFG3		0x23	/* Config3 */

/* Kscratch registers */
#define LOONGARCH_CSR_KS0		0x30
#define LOONGARCH_CSR_KS1		0x31
#define LOONGARCH_CSR_KS2		0x32
#define LOONGARCH_CSR_KS3		0x33
#define LOONGARCH_CSR_KS4		0x34
#define LOONGARCH_CSR_KS5		0x35
#define LOONGARCH_CSR_KS6		0x36
#define LOONGARCH_CSR_KS7		0x37
#define LOONGARCH_CSR_KS8		0x38

/* Timer registers */
#define LOONGARCH_CSR_TMID		0x40	/* Timer ID */
#define LOONGARCH_CSR_TCFG		0x41	/* Timer config */
#define LOONGARCH_CSR_TVAL		0x42	/* Timer value */
#define LOONGARCH_CSR_CNTC		0x43	/* Timer offset */
#define LOONGARCH_CSR_TINTCLR		0x44	/* Timer interrupt clear */

/* Guest registers */
#define LOONGARCH_CSR_GSTAT		0x50	/* Guest status */
#define LOONGARCH_CSR_GCFG		0x51	/* Guest config */
#define LOONGARCH_CSR_GINTC		0x52	/* Guest interrupt control */
#define LOONGARCH_CSR_GCNTC		0x53	/* Guest timer offset */

/* LLBCTL register */
#define LOONGARCH_CSR_LLBCTL		0x60	/* LLBit control */

/* Implement dependent */
#define LOONGARCH_CSR_IMPCTL1		0x80	/* Loongson config1 */
#define LOONGARCH_CSR_IMPCTL2		0x81	/* Loongson config2 */
#define LOONGARCH_CSR_GNMI		0x82

/* TLB Refill registers */
#define LOONGARCH_CSR_TLBRENTRY		0x88	/* TLB refill exception entry */
#define LOONGARCH_CSR_TLBRBADV		0x89	/* TLB refill badvaddr */
#define LOONGARCH_CSR_TLBRERA		0x8a	/* TLB refill ERA */
#define LOONGARCH_CSR_TLBRSAVE		0x8b	/* KScratch for TLB refill exception */
#define LOONGARCH_CSR_TLBRELO0		0x8c	/* TLB refill entrylo0 */
#define LOONGARCH_CSR_TLBRELO1		0x8d	/* TLB refill entrylo1 */
#define LOONGARCH_CSR_TLBREHI		0x8e	/* TLB refill entryhi */
#define  CSR_TLBREHI_PS_SHIFT		0
#define  CSR_TLBREHI_PS			(UL_CONST(0x3f) << CSR_TLBREHI_PS_SHIFT)
#define LOONGARCH_CSR_TLBRPRMD		0x8f	/* TLB refill mode info */

/* Machine Error registers */
#define LOONGARCH_CSR_MERRCTL		0x90	/* MERRCTL */
#define LOONGARCH_CSR_MERRINFO1		0x91	/* MError info1 */
#define LOONGARCH_CSR_MERRINFO2		0x92	/* MError info2 */
#define LOONGARCH_CSR_MERRENTRY		0x93	/* MError exception entry */
#define LOONGARCH_CSR_MERRERA		0x94	/* MError exception ERA */
#define LOONGARCH_CSR_MERRSAVE		0x95	/* KScratch for machine error exception */

#define LOONGARCH_CSR_CTAG		0x98	/* TagLo + TagHi */

#define LOONGARCH_CSR_PRID		0xc0

/* Shadow MCSR : 0xc0 ~ 0xff */
#define LOONGARCH_CSR_MCSR0		0xc0	/* CPUCFG0 and CPUCFG1 */
#define LOONGARCH_CSR_MCSR1		0xc1	/* CPUCFG2 and CPUCFG3 */
#define LOONGARCH_CSR_MCSR2		0xc2	/* CPUCFG4 and CPUCFG5 */
#define LOONGARCH_CSR_MCSR3		0xc3	/* CPUCFG6 */
#define LOONGARCH_CSR_MCSR8		0xc8	/* CPUCFG16 and CPUCFG17 */
#define LOONGARCH_CSR_MCSR9		0xc9	/* CPUCFG18 and CPUCFG19 */
#define LOONGARCH_CSR_MCSR10		0xca	/* CPUCFG20 */
#define LOONGARCH_CSR_MCSR24		0xf0	/* cpucfg48 */

/* Uncached accelerate windows registers */
#define LOONGARCH_CSR_UCAWIN		0x100
#define LOONGARCH_CSR_UCAWIN0_LO	0x102
#define LOONGARCH_CSR_UCAWIN0_HI	0x103
#define LOONGARCH_CSR_UCAWIN1_LO	0x104
#define LOONGARCH_CSR_UCAWIN1_HI	0x105
#define LOONGARCH_CSR_UCAWIN2_LO	0x106
#define LOONGARCH_CSR_UCAWIN2_HI	0x107
#define LOONGARCH_CSR_UCAWIN3_LO	0x108
#define LOONGARCH_CSR_UCAWIN3_HI	0x109

/* Direct Map windows registers */
#define LOONGARCH_CSR_DMWIN0		0x180	/* 64 direct map win0: MEM & IF */
#define LOONGARCH_CSR_DMWIN1		0x181	/* 64 direct map win1: MEM & IF */
#define LOONGARCH_CSR_DMWIN2		0x182	/* 64 direct map win2: MEM */
#define LOONGARCH_CSR_DMWIN3		0x183	/* 64 direct map win3: MEM */

/* Performance Counter registers */
#define LOONGARCH_CSR_PERFCTRL0		0x200	/* 32 perf event 0 config */
#define LOONGARCH_CSR_PERFCNTR0		0x201	/* 64 perf event 0 count value */
#define LOONGARCH_CSR_PERFCTRL1		0x202	/* 32 perf event 1 config */
#define LOONGARCH_CSR_PERFCNTR1		0x203	/* 64 perf event 1 count value */
#define LOONGARCH_CSR_PERFCTRL2		0x204	/* 32 perf event 2 config */
#define LOONGARCH_CSR_PERFCNTR2		0x205	/* 64 perf event 2 count value */
#define LOONGARCH_CSR_PERFCTRL3		0x206	/* 32 perf event 3 config */
#define LOONGARCH_CSR_PERFCNTR3		0x207	/* 64 perf event 3 count value */

/* Debug registers */
#define LOONGARCH_CSR_MWPC		0x300	/* data breakpoint config */
#define LOONGARCH_CSR_MWPS		0x301	/* data breakpoint status */

#define LOONGARCH_CSR_DB0ADDR		0x310	/* data breakpoint 0 address */
#define LOONGARCH_CSR_DB0MASK		0x311	/* data breakpoint 0 mask */
#define LOONGARCH_CSR_DB0CTL		0x312	/* data breakpoint 0 control */
#define LOONGARCH_CSR_DB0ASID		0x313	/* data breakpoint 0 asid */

#define LOONGARCH_CSR_DB1ADDR		0x318	/* data breakpoint 1 address */
#define LOONGARCH_CSR_DB1MASK		0x319	/* data breakpoint 1 mask */
#define LOONGARCH_CSR_DB1CTL		0x31a	/* data breakpoint 1 control */
#define LOONGARCH_CSR_DB1ASID		0x31b	/* data breakpoint 1 asid */

#define LOONGARCH_CSR_DB2ADDR		0x320	/* data breakpoint 2 address */
#define LOONGARCH_CSR_DB2MASK		0x321	/* data breakpoint 2 mask */
#define LOONGARCH_CSR_DB2CTL		0x322	/* data breakpoint 2 control */
#define LOONGARCH_CSR_DB2ASID		0x323	/* data breakpoint 2 asid */

#define LOONGARCH_CSR_DB3ADDR		0x328	/* data breakpoint 3 address */
#define LOONGARCH_CSR_DB3MASK		0x329	/* data breakpoint 3 mask */
#define LOONGARCH_CSR_DB3CTL		0x32a	/* data breakpoint 3 control */
#define LOONGARCH_CSR_DB3ASID		0x32b	/* data breakpoint 3 asid */

#define LOONGARCH_CSR_DB4ADDR		0x330	/* data breakpoint 4 address */
#define LOONGARCH_CSR_DB4MASK		0x331	/* data breakpoint 4 maks */
#define LOONGARCH_CSR_DB4CTL		0x332	/* data breakpoint 4 control */
#define LOONGARCH_CSR_DB4ASID		0x333	/* data breakpoint 4 asid */

#define LOONGARCH_CSR_DB5ADDR		0x338	/* data breakpoint 5 address */
#define LOONGARCH_CSR_DB5MASK		0x339	/* data breakpoint 5 mask */
#define LOONGARCH_CSR_DB5CTL		0x33a	/* data breakpoint 5 control */
#define LOONGARCH_CSR_DB5ASID		0x33b	/* data breakpoint 5 asid */

#define LOONGARCH_CSR_DB6ADDR		0x340	/* data breakpoint 6 address */
#define LOONGARCH_CSR_DB6MASK		0x341	/* data breakpoint 6 mask */
#define LOONGARCH_CSR_DB6CTL		0x342	/* data breakpoint 6 control */
#define LOONGARCH_CSR_DB6ASID		0x343	/* data breakpoint 6 asid */

#define LOONGARCH_CSR_DB7ADDR		0x348	/* data breakpoint 7 address */
#define LOONGARCH_CSR_DB7MASK		0x349	/* data breakpoint 7 mask */
#define LOONGARCH_CSR_DB7CTL		0x34a	/* data breakpoint 7 control */
#define LOONGARCH_CSR_DB7ASID		0x34b	/* data breakpoint 7 asid */

#define LOONGARCH_CSR_FWPC		0x380	/* instruction breakpoint config */
#define LOONGARCH_CSR_FWPS		0x381	/* instruction breakpoint status */

#define LOONGARCH_CSR_IB0ADDR		0x390	/* inst breakpoint 0 address */
#define LOONGARCH_CSR_IB0MASK		0x391	/* inst breakpoint 0 mask */
#define LOONGARCH_CSR_IB0CTL		0x392	/* inst breakpoint 0 control */
#define LOONGARCH_CSR_IB0ASID		0x393	/* inst breakpoint 0 asid */

#define LOONGARCH_CSR_IB1ADDR		0x398	/* inst breakpoint 1 address */
#define LOONGARCH_CSR_IB1MASK		0x399	/* inst breakpoint 1 mask */
#define LOONGARCH_CSR_IB1CTL		0x39a	/* inst breakpoint 1 control */
#define LOONGARCH_CSR_IB1ASID		0x39b	/* inst breakpoint 1 asid */

#define LOONGARCH_CSR_IB2ADDR		0x3a0	/* inst breakpoint 2 address */
#define LOONGARCH_CSR_IB2MASK		0x3a1	/* inst breakpoint 2 mask */
#define LOONGARCH_CSR_IB2CTL		0x3a2	/* inst breakpoint 2 control */
#define LOONGARCH_CSR_IB2ASID		0x3a3	/* inst breakpoint 2 asid */

#define LOONGARCH_CSR_IB3ADDR		0x3a8	/* inst breakpoint 3 address */
#define LOONGARCH_CSR_IB3MASK		0x3a9	/* breakpoint 3 mask */
#define LOONGARCH_CSR_IB3CTL		0x3aa	/* inst breakpoint 3 control */
#define LOONGARCH_CSR_IB3ASID		0x3ab	/* inst breakpoint 3 asid */

#define LOONGARCH_CSR_IB4ADDR		0x3b0	/* inst breakpoint 4 address */
#define LOONGARCH_CSR_IB4MASK		0x3b1	/* inst breakpoint 4 mask */
#define LOONGARCH_CSR_IB4CTL		0x3b2	/* inst breakpoint 4 control */
#define LOONGARCH_CSR_IB4ASID		0x3b3	/* inst breakpoint 4 asid */

#define LOONGARCH_CSR_IB5ADDR		0x3b8	/* inst breakpoint 5 address */
#define LOONGARCH_CSR_IB5MASK		0x3b9	/* inst breakpoint 5 mask */
#define LOONGARCH_CSR_IB5CTL		0x3ba	/* inst breakpoint 5 control */
#define LOONGARCH_CSR_IB5ASID		0x3bb	/* inst breakpoint 5 asid */

#define LOONGARCH_CSR_IB6ADDR		0x3c0	/* inst breakpoint 6 address */
#define LOONGARCH_CSR_IB6MASK		0x3c1	/* inst breakpoint 6 mask */
#define LOONGARCH_CSR_IB6CTL		0x3c2	/* inst breakpoint 6 control */
#define LOONGARCH_CSR_IB6ASID		0x3c3	/* inst breakpoint 6 asid */

#define LOONGARCH_CSR_IB7ADDR		0x3c8	/* inst breakpoint 7 address */
#define LOONGARCH_CSR_IB7MASK		0x3c9	/* inst breakpoint 7 mask */
#define LOONGARCH_CSR_IB7CTL		0x3ca	/* inst breakpoint 7 control */
#define LOONGARCH_CSR_IB7ASID		0x3cb	/* inst breakpoint 7 asid */

#define LOONGARCH_CSR_DEBUG		0x500	/* debug config */
#define LOONGARCH_CSR_DERA		0x501	/* debug era */
#define LOONGARCH_CSR_DESAVE		0x502	/* debug save */

#ifndef __ASSEMBLER__
#include <larchintrin.h>
#include <types.h>

static inline uint32_t csr_readl(uint32_t reg)
{
	return __csrrd(reg);
}

static inline uint64_t csr_readq(uint32_t reg)
{
	return __dcsrrd(reg);
}

static inline void csr_writel(uint32_t val, uint32_t reg)
{
	__csrwr(val, reg);
}

static inline void csr_writeq(unsigned long val, unsigned int reg)
{
	__dcsrwr(val, reg);
}

static inline void enable_pg(unsigned long val)
{
	asm volatile("csrwr %0, 0x0" : : "r"(val));
}

static inline void write_csr_pgdl(unsigned long val)
{
    asm volatile("csrwr %0, 0x19" : : "r" (val));
}

static inline void write_csr_pgdh(unsigned long val)
{
    asm volatile("csrwr %0, 0x1a" : : "r" (val));
}

static inline void write_csr_pwcl(unsigned long val)
{
    asm volatile("csrwr %0, 0x1c" : : "r" (val));
}

static inline void write_csr_pwch(unsigned long val)
{
    asm volatile("csrwr %0, 0x1d" : : "r" (val));
}

static inline void write_csr_tlbrentry(unsigned long val)
{
    asm volatile("csrwr %0, 0x88" : : "r" (val));
}

static inline void write_csr_elf_debug_eentry(unsigned long val)
{
    asm volatile("csrwr %0, 0xc"::"r"(val));
}

static inline unsigned int read_csr_pagesize(void)
{
	return (__csrrd(LOONGARCH_CSR_TLBIDX) & CSR_TLBIDX_SIZEM) >> CSR_TLBIDX_SIZE;
}

static inline void write_csr_pagesize(unsigned int size)
{
	__csrxchg(size << CSR_TLBIDX_SIZE, CSR_TLBIDX_SIZEM, LOONGARCH_CSR_TLBIDX);
}

static inline unsigned int read_csr_tlbrefill_pagesize(void)
{
	return (__dcsrrd(LOONGARCH_CSR_TLBREHI) & CSR_TLBREHI_PS) >> CSR_TLBREHI_PS_SHIFT;
}

static inline void write_csr_tlbrefill_pagesize(unsigned int size)
{
	__dcsrxchg(size << CSR_TLBREHI_PS_SHIFT, CSR_TLBREHI_PS, LOONGARCH_CSR_TLBREHI);
}

#define read_csr_stlbpgsize()		__csrrd(LOONGARCH_CSR_STLBPGSIZE)
#define write_csr_stlbpgsize(val)	__csrwr(val, LOONGARCH_CSR_STLBPGSIZE)

#endif