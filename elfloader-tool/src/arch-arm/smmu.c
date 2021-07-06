/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>
#include <elfloader/gen_config.h>
#include <types.h>
#include <elfloader.h>
#include <mode/structures.h>
#include <printf.h>
#include <abort.h>

typedef word_t bool_t;

/*the paddr address of the SMMU*/
#define SMMU_PADDR               0xfd800000

#ifdef CONFIG_ARCH_AARCH32
#define DMA_START                0x8ae000
#define DMA_END                  0xab0000
#define DMA_SIZE                 0x202000
#else
#define DMA_START                0xcb3000
#define DMA_END                  0xeb5000
#define DMA_SIZE                 0x202000
#endif /* CONFIG_ARCH_AARCH32 */

#define SMMU_PAGE_4KB            0x1000
#define SMMU_PAGE_64KB           0x10000

/*the high-level physical address layout according to SMMU definition*/
#define SMMU_GLOBAL_SIZE(num_page, page_size)   ((num_page) * (page_size))
#define SMMU_CB_SIZE(num_page, page_size)       ((num_page) * (page_size))
#define SMMU_CB_BASE_PADDR(global_size)         (SMMU_PADDR + (global_size))

/*SMMU's physical address space layout, defined by SMMU v2 standard*/
#define SMMU_GR0_PADDR                          SMMU_PADDR
#define SMMU_GR1_PADDR(page_size)               ((SMMU_GR0_PADDR) + 1 * (page_size))
#define SMMU_GID_PADDR(page_size)               ((SMMU_GR0_PADDR) + 2 * (page_size))
#define SMMU_PM_PADDR(page_size)                ((SMMU_GR0_PADDR) + 3 * (page_size))
#define SMMU_CBn_PADDR(cb_base, n ,page_size)   ((cb_base) + n * (page_size))

/* SMMU's virtual address space layout in kernel address space,
 * mapped by boot code.*/
#define SMMU_GR0_PPTR                           SMMU_PADDR
#define SMMU_GR1_PPTR                          (SMMU_PADDR + 1 * (SMMU_PAGE_4KB))
#define SMMU_GID_PPTR                          (SMMU_PADDR + 2 * (SMMU_PAGE_4KB))
#define SMMU_PM_PPTR                           (SMMU_PADDR + 3 * (SMMU_PAGE_4KB))
#define SSMU_CB_BASE_PPTR                      (SMMU_PADDR + 16 * (SMMU_PAGE_4KB))
#define SMMU_CBn_BASE_PPTR(n)                  ((SSMU_CB_BASE_PPTR) + (n) * (SMMU_PAGE_4KB))

/*global register space 0 registers*/
#define SMMU_sCR0                                0x000
#define SMMU_SCR1                                0x004
#define SMMU_sCR2                                0x008
#define SMMU_sACR                                0x010
#define SMMU_IDR0                                0x020
#define SMMU_IDR1                                0x024
#define SMMU_IDR2                                0x028
#define SMMU_IDR3                                0x02c
#define SMMU_IDR4                                0x030
#define SMMU_IDR5                                0x034
#define SMMU_IDR6                                0x038
#define SMMU_IDR7                                0x03c
#define SMMU_sGFAR                               0x040
#define SMMU_sGFSR                               0x048
#define SMMU_sGFSRRESTORE                        0x04c
#define SMMU_sGFSYNR0                            0x050
#define SMMU_sGFSYNR1                            0x054
#define SMMU_sGFSYNR2                            0x058
#define SMMU_STLBIALL                            0x060
#define SMMU_TLBIVMID                            0x064
#define SMMU_TLBIALLNSNH                         0x068
#define SMMU_TLBIALLH                            0x06c
#define SMMU_sTLBGSYNC                           0x070
#define SMMU_sTLBGSTATUS                         0x074
#define SMMU_TLBIVAH                             0x078
#define SMMU_STLBIVALM                           0x0a0
#define SMMU_STLBIVAM                            0x0a8
#define SMMU_TLBIVALH64                          0x0b0
#define SMMU_TLBIVMIDS1                          0x0b8
#define SMMU_STLBIALLM                           0x0bc
#define SMMU_TLBIVAH64                           0x0c0
#define SMMU_sGATS1UR                            0x100
#define SMMU_sGATS1UW                            0x108
#define SMMU_sGATS1PR                            0x110
#define SMMU_sGATS1PW                            0x118
#define SMMU_sGATS12UR                           0x120
#define SMMU_sGATS12UW                           0x128
#define SMMU_sGATS12PR                           0x130
#define SMMU_sGATS12PW                           0x138
#define SMMU_sGPAR                               0x180
#define SMMU_sGATSR                              0x188

/*SMMU_SMRn, stream matching register 0 to 127*/
#define SMMU_SMRn(n)                             (0x800 + (n) * 0x4)

/*SMMU_S2CRn, stream-to-context register 0 to 127*/
#define SMMU_S2CRn(n)                            (0xc00 + (n) * 0x4)

/*global register space 1*/
/*SMMU_CBARn, context bank attribute register 0 to 127*/
#define SMMU_CBARn(n)                            (0x000 + (n) * 0x4)

/*SMMU_CBFRSYNRAn, context bank fault restricted syndrome register A 0 to 127*/
#define SMMU_CBFRSYNRAn(n)                       (0x400 + (n) * 0x4)

/*SMMU_CBA2Rn, context bank attribute registers 0 to 127*/
#define SMMU_CBA2Rn(n)                           (0x800 + (n) * 0x4)

/*stage 1 and stage 2 translation context bank address space*/
#define SMMU_CBn_SCTLR                           0x000
#define SMMU_CBn_ACTLR                           0x004
#define SMMU_CBn_RESUME                          0x008
#define SMMU_CBn_TCR2                            0x010
#define SMMU_CBn_TTBR0                           0x020
#define SMMU_CBn_TTBR1                           0x028
#define SMMU_CBn_TCR                             0x030
#define SMMU_CBn_CONTEXTIDR                      0x034

/*the SMMU_CBn_MAIRm registers are used for AArch32 Long-descriptor or the AArch64*/
#define SMMU_CBn_MAIR0                           0x038
#define SMMU_CBn_MAIR1                           0x03c
/*the SMMU_CBn_PRRR and SMMU_CBn_NMRR registers are used for AArch32*/
#define SMMU_CBn_PRRR                            0x038
#define SMMU_CBn_NMRR                            0x03c

#define SMMU_CBn_PAR                             0x050
#define SMMU_CBn_FSR                             0x058
#define SMMU_CBn_FSRRESTORE                      0x05c
#define SMMU_CBn_FAR                             0x060
#define SMMU_CBn_FSYNR0                          0x068
#define SMMU_CBn_FSYNR1                          0x06c
#define SMMU_CBn_IPAFAR                          0x070

#define SMMU_CBn_TLBIVA                          0x600
#define SMMU_CBn_TLBIVAA                         0x608
#define SMMU_CBn_TLBIASID                        0x610
#define SMMU_CBn_TLBIALL                         0x618
#define SMMU_CBn_TLBIVAL                         0x620
#define SMMU_CBn_TLBIVAAL                        0x628
#define SMMU_CBn_TLBIIPAS2                       0x630
#define SMMU_CBn_TLBIIPAS2L                      0x638
#define SMMU_CBn_TLBSYNC                         0x7f0
#define SMMU_CBn_TLBSTATUS                       0x7f4

/*SMMU_CR0 non-secure register 0 bit assignments*/
#define CR0_VMID16EN                            BIT(31)
#define CR0_HYPMODE                             BIT(30)
#define CR0_WACFG(v)                            ((v) & 0x3 << 26)
#define CR0_RACFG(v)                            ((v) & 0x3 << 24)
#define CR0_SHCFG(v)                            ((v) & 0x3 << 22)
#define CR0_SMCFCFG                             BIT(21)
#define CR0_MTCFG                               BIT(20)
#define CR0_MemAttr(v)                          ((v) & 0xf << 16)
#define CR0_BSU(v)                              ((v) & 0x3 << 14)
#define CR0_FB                                  BIT(13)
#define CR0_PTM                                 BIT(12)
#define CR0_VMIDPNE                             BIT(11)
#define CR0_USFCFG                              BIT(10)
#define CR0_GSE                                 BIT(9)
#define CR0_STALLD                              BIT(8)
#define CR0_TRANSIENTCFG(v)                     ((v) & 0x3 << 6)
#define CR0_GCFGFIE                             BIT(5)
#define CR0_GCFGFRE                             BIT(4)
#define CR0_EXIDENABLE                          BIT(3)
#define CR0_GFIE                                BIT(2)
#define CR0_GFRE                                BIT(1)
#define CR0_CLIENTPD                            BIT(0)
#define CR0_BSU_ALL                             3

/*SMMU_IDR0 (read only) read mask*/
#define IDR0_SES                                 BIT(31)
#define IDR0_S1TS                                BIT(30)
#define IDR0_S2TS                                BIT(29)
#define IDR0_NTS                                 BIT(28)
#define IDR0_SMS                                 BIT(27)
#define IDR0_ATOSNS                              BIT(26)
#define IDR0_PTFS                                (0x3 << 24)
#define IDR0_PTFS_VAL(v)                         ((v) >> 24)
#define IDR0_NUMIRPT                             (0xff << 16)
#define IDR0_NUMIRPT_VAL(v)                      ((v) >> 16)
#define IDR0_EXSMRGS                             BIT(15)
#define IDR0_CTTW                                BIT(14)
#define IDR0_BTM                                 BIT(13)
#define IDR0_NUMSIDB                             (0xf << 9)
#define IDR0_NUMSIDB_VAL(v)                      ((v) >> 9)
#define IDR0_EXIDS                               BIT(8)
#define IDR0_NUMSMRG                             0xff

/*PTFS bits*/
#define PTFS_AARCH32S_AARCH32L                   0x0
#define PTFS_AARCH32L_ONLY                       0x1
#define PTFS_NO_AARCH32                          0x2

/*SMMU_IDR1 (read only) read mask*/
#define IDR1_PAGESIZE                            BIT(31)
#define IDR1_NUMPAGENDXB                         (0x7 << 28)
#define IDR1_NUMPAGENDXB_VAL(v)                  ((v) >> 28)
#define IDR1_HAFDBS                              (0x3 << 24)
#define IDR1_NUMS2CB                             (0xff << 16)
#define IDR1_NUMS2CB_VAL(v)                      ((v) >> 16)
#define IDR1_SMCD                                 BIT(15)
#define IDR1_SSDTP                               (0x3 << 12)
#define IDR1_NUMSSDNDXB                          (0xf << 8)
#define IDR1_NUMCB                               0xff

/*SMMU_IDR2 (read only) read mask*/
#define IDR2_VMID16S                             BIT(15)
#define IDR2_PTFSV8_64                           BIT(14)
#define IDR2_PTFSV8_16                           BIT(13)
#define IDR2_PTFSV8_4                            BIT(12)
#define IDR2_UBS                                 (0xf << 8)
#define IDR2_UBS_VAL(v)                          ((v) >> 8)
#define IDR2_OAS                                 (0xf << 4)
#define IDR2_OAS_VAL(v)                          ((v) >> 4)
#define IDR2_IAS                                 0xf

/*OAS bits*/
#define IDR2_OAS_32                                   0x0
#define IDR2_OAS_36                                   0x1
#define IDR2_OAS_40                                   0x2
#define IDR2_OAS_42                                   0x3
#define IDR2_OAS_44                                   0x4
#define IDR2_OAS_48                                   0x5

/*IAS bits*/
#define IDR2_IAS_32                                   0x0
#define IDR2_IAS_36                                   0x1
#define IDR2_IAS_40                                   0x2
#define IDR2_IAS_42                                   0x3
#define IDR2_IAS_44                                   0x4
#define IDR2_IAS_48                                   0x5

/*SMMU_IDR7*/
#define IDR7_MAJOR                             (0xf << 4)
#define IDR7_MAJOR_VAL(v)                      ((v) >> 4)
#define IDR7_MINOR                             (0xf)

/*SMMU_sGFSR r/w bit mask, write 1 to clear*/
#define GFSR_MULTI                              BIT(31)
#define GFSR_UUT                                BIT(8)
#define GFSR_PF                                 BIT(7)
#define GFSR_EF                                 BIT(6)
#define GFSR_CAF                                BIT(5)
#define GFSR_UCIF                               BIT(4)
#define GFSR_UCBF                               BIT(3)
#define GFSR_SMCF                               BIT(2)
#define GFSR_USF                                BIT(1)
#define GFSR_ICF                                BIT(0)

/*SMMU_S2CRn, r/w bit mask for translation context*/
#define S2CR_TRANSIENTCFG_SET(v)            ((v) << 28)
#define S2CR_INSTCFG_SET(v)                 ((v) << 26)
#define S2CR_PRIVCFG_SET(v)                 ((v) << 24)
#define S2CR_WACFG_SET(v)                   ((v) << 22)
#define S2CR_RACFG_SET(v)                   ((v) << 20)
#define S2CR_NSCFG_SET(v)                   ((v) << 18)
#define S2CR_TYPE_SET(v)                    ((v) << 16)
#define S2CR_MemAttr_SET(v)                 ((v) << 12)
#define S2CR_MTCFG_SET(v)                   ((v) << 11)
#define S2CR_EXIDVALID_SET(v)               ((v) << 10)
#define S2CR_SHCFG_SET(v)                   ((v) << 8)
#define S2CR_CBNDX_SET(v)                   ((v) & 0xff)

/*SMMU_S2CRn PRIVCFG values*/
#define S2CR_PRIVCFG_DEFAULT                0x0

/*SMMU_S2CRn type values*/
#define S2CR_TYPE_CB                        0x0
#define S2CR_TYPE_BYPASS                    0x1
#define S2CR_TYPE_FAULT                     0x2

/*SMMU_SMRn, r/w bit mask for stream match*/
#define SMR_VALID_SET(v)                    ((v) << 31)
#define SMR_MASK_SET(v)                     ((v) & 0x7fff << 16)
#define SMR_ID_SET(v)                       ((v) & 0x7fff)
/*valid /invalid*/
#define SMR_VALID_EN                         0x1
#define SMR_VALID_DIS                        0x0

/*SMMU_ACR, SMMU-500*/
#define ACR_CACHE_LOCK                       BIT(26)
#define ACR_S2CRB_TLBEN                      BIT(10)
#define ACR_SMTNMB_TLBEN                     BIT(8)

/*SMMU_CBn_FSR, write 1 to clear*/
#define CBn_FSR_MULTI                        BIT(31)
#define CBn_FSR_SS                           BIT(30)
#define CBn_FSR_UUT                          BIT(8)
#define CBn_FSR_ASF                          BIT(7)
#define CBn_FSR_TLBLKF                       BIT(6)
#define CBn_FSR_TLBLMCF                      BIT(5)
#define CBn_FSR_EF                           BIT(4)
#define CBn_FSR_PF                           BIT(3)
#define CBn_FSR_AFF                          BIT(2)
#define CBn_FSR_TF                           BIT(1)

#define CBn_FSR_CLEAR_ALL                    (CBn_FSR_MULTI | CBn_FSR_SS | \
                                             CBn_FSR_UUT | CBn_FSR_ASF | CBn_FSR_TLBLKF | \
                                             CBn_FSR_TLBLMCF | CBn_FSR_EF | CBn_FSR_PF | \
                                             CBn_FSR_AFF | CBn_FSR_TF)

/*SMMU_CBn_ACTLR defined in SMMU500*/
#define CBn_ACTLR_CPRE                       BIT(1)
#define CBn_ACTLR_CMTLB                      BIT(0)

/*mask for invalidate all TLB entries, used by GR0 registers*/
#define SMMU_TLB_INVALL_MASK                 0xffffffff

/*mask for init the TLB sync msg*/
#define SMMU_TLB_SYNC_MASK                   0xffffffff

/*TLB sync status used in SMMU_sTLBGSTATUS and SMMU_CBn_TLBSTATUS*/
#define TLBSTATUS_GSACTIVE                  BIT(0)
/*the kernel loops N times before declear a TLB invalidation failure*/
#define TLBSYNC_LOOP                         1000

/*SMMU_CBARn*/
#define CBARn_TYPE_SET(v)                     ((v) << 16)
#define CBARn_BPSHCFG_SET(v)                  ((v) << 8)
#define CBARn_VMID_SET(v)                     ((v) & 0xff)

#define CBARn_TYPE_STAGE2                      0
#define CBARn_TYPE_STAGE1                      1   /*stage 1 with stage 2 by pass*/

#define CBARn_BPSHCFG_OUTER                    1
#define CBARn_BPSHCFG_INNER                    2
#define CBARn_BPSHCFG_NONE                     3

#define CBARn_MemAttr_SET(v)                  ((v) << 12)
#define MemAttr_OWB_IWB                        0xf /*outer & inner write-back cacheable*/

/*SMMU_CBA2Rn*/
#define CBA2Rn_VMID_SET(v)                 (((v) & 0xffff) << 16)
#define CBA2Rn_VA64_SET                    1

/*SMMU_CBn_TCR stage1/2 when SMMU_CBn_CBA2R.VA64 is 1*/
#define CBn_TCR_TG1_SET(v)                 ((v) << 30)
#define CBn_TCR_SH1_SET(v)                 ((v) << 28)
#define CBn_TCR_ORGN1_SET(v)               ((v) << 26)
#define CBn_TCR_IRGN1_SET(v)               ((v) << 24)
#define CBn_TCR_EPD1_DIS                   (1 << 23)     /*translation disabled for TTBR1 region*/
#define CBn_TCR_A1_EN                      (1 << 22)
#define CBn_TCR_T1SZ_SET(v)                (((v) & 0x3f) << 16)
#define CBn_TCR_TG0_SET(v)                 ((v) << 14)
#define CBn_TCR_SH0_SET(v)                 ((v) << 12)
#define CBn_TCR_ORGN0_SET(v)               ((v) << 10)
#define CBn_TCR_IRGN0_SET(v)               ((v) << 8)
#define CBn_TCR_T0SZ_SET(v)                ((v) & 0x3f)

#define CBn_TCR_TG_4K                      0
#define CBn_TCR_TG_64K                     1
#define CBn_TCR_TG_16K                     2

#define CBn_TCR_SH_NONE                    0
#define CBn_TCR_SH_OUTER                   2
#define CBn_TCR_SH_INNER                   3

#define CBn_TCR_GN_NCACHE                  0
#define CBn_TCR_GN_WB_WA_CACHE             1
#define CBn_TCR_GN_WT_CACHE                2
#define CBn_TCR_GN_WB_NWA_CACHE            3

/*SMMU_CBn_TCR stage 2 when SMMU_CBn_CBA2R.VA64 is 1*/
#define CBn_TCR_PASize_SET(v)               ((v) << 16)
#define CBn_TCR_SL0_SET(v)                  ((v) << 6)
#define CBn_TCR_SL0_4KB_L2                  0
#define CBn_TCR_SL0_4KB_L1                  1
#define CBn_TCR_SL0_4KB_L0                  2

/*SMMU_CBn_TCR2*/
#define CBn_TCR2_SEP_SET(v)                 ((v) << 15)
#define CBn_TCR2_AS_SET(v)                  ((v) << 4)
#define CBn_TCR2_PASize_SET(v)              (v)

#define CBn_TCR2_SEP_UPSTREAM_SIZE           7
#define CBn_TCR2_AS_16                       1
#define CBn_TCR2_PASize_32                   0
#define CBn_TCR2_PASize_36                   1
#define CBn_TCR2_PASize_40                   2
#define CBn_TCR2_PASize_42                   3
#define CBn_TCR2_PASize_44                   4
#define CBn_TCR2_PASize_48                   5

/*SMMU_CBn_TTBRm*/
#define CBn_TTBRm_ASID_SET(v)                (((v) & 0xffffull) << 48)

/*SMMU_CBn_MAIRm,
this is the same as the MAIR in core*/

/*MAIR0*/
#define CBn_MAIRm_ATTR_DEVICE_nGnRnE         0x00
#define CBn_MAIRm_ATTR_ID_DEVICE_nGnRnE      0

#define CBn_MAIRm_ATTR_DEVICE_nGnRE          0x04
#define CBn_MAIRm_ATTR_ID_DEVICE_nGnRE       1

#define CBn_MAIRm_ATTR_DEVICE_GRE            0xc
#define CBn_MAIRm_ATTR_ID_DEVICE_GRE         2

#define CBn_MAIRm_ATTR_NC                    0x44   /*non-cacheable normal memory*/
#define CBn_MAIRm_ATTR_ID_NC                 3      /*index for non-cacheable attribute*/

/*MAIR1*/
/*R/W allocate, normal memory, outer/innner write back*/
#define CBn_MAIRm_ATTR_CACHE                0xff
#define CBn_MAIRm_ATTR_ID_CACHE              0

/*8 bit per attribute*/
#define CBn_MAIRm_ATTR_SHIFT(n)              ((n) << 3)

/*SMMU_CBn_SCTLR*/
#define CBn_SCTLR_CFIE                         (1 << 6)
#define CBn_SCTLR_CFRE                         (1 << 5)
#define CBn_SCTLR_AFE                          (1 << 2)
#define CBn_SCTLR_TRE                          (1 << 1)
#define CBn_SCTLR_M                            1
#define CBn_SCTLR_S1_ASIDPNE                   (1 << 12)

/*SMMU_CBn_TLBIASID*/
#define CBn_TLBIASID_SET(v)                    ((v) & 0xffff)

/*SMMU_TLBIVMID*/
#define TLBIVMID_SET(v)                        ((v) & 0xffff)

/*SMMU_CBn_TLBIVA*/
#define CBn_TLBIVA_SET(asid, vaddr)            (((asid) & 0xffff) << 48 | ((vaddr) >> 12 & 0xfffffffffff))

/*SMMU_CBn_TLBIIPAS2*/
#define CBn_TLBIIPAS2_SET(vaddr)               ((vaddr) >> 12 & 0xfffffffff)

/*supported stages of translations*/
#define STAGE1_TRANS           (1 << 0)
#define STAGE2_TRANS           (1 << 1)
#define NESTED_TRANS           (1 << 2)
/*supported translation table formats*/
#define AARCH32S_FMT           (1 << 0)
#define AARCH32L_FMT           (1 << 1)
#define NO_AARCH32_FMT         (1 << 2)
#define TRANS_PAGES_4KB        (1 << 3)
#define TRANS_PAGES_16KB       (1 << 4)
#define TRANS_PAGES_64KB       (1 << 5)

/*the default vritual address bits for partition TTBR0 and TTBR1*/
#define SMMU_VA_DEFAULT_BITS      48

#ifdef CONFIG_ARCH_AARCH32
uint32_t dma_mapping[] = {
    0x4081a000,
    0x4081b000,
    0x4081c000,
    0x4081d000,
    0x4081e000,
    0x4081f000,
    0x40820000,
    0x40821000,
    0x40822000,
    0x40823000,
    0x40824000,
    0x40825000,
    0x40826000,
    0x40827000,
    0x40828000,
    0x40829000,
    0x4082a000,
    0x4082b000,
    0x4082c000,
    0x4082d000,
    0x4082e000,
    0x4082f000,
    0x40830000,
    0x40831000,
    0x40832000,
    0x40833000,
    0x40834000,
    0x40835000,
    0x40836000,
    0x40837000,
    0x40838000,
    0x40839000,
    0x4083a000,
    0x4083b000,
    0x4083c000,
    0x4083d000,
    0x4083e000,
    0x4083f000,
    0x40840000,
    0x40841000,
    0x40842000,
    0x40843000,
    0x40844000,
    0x40845000,
    0x40846000,
    0x40847000,
    0x40848000,
    0x40849000,
    0x4084a000,
    0x4084b000,
    0x4084c000,
    0x4084d000,
    0x4084e000,
    0x4084f000,
    0x40850000,
    0x40851000,
    0x40852000,
    0x40853000,
    0x40854000,
    0x40855000,
    0x40856000,
    0x40857000,
    0x40858000,
    0x40859000,
    0x4085a000,
    0x4085b000,
    0x4085c000,
    0x4085d000,
    0x4085e000,
    0x4085f000,
    0x40860000,
    0x40861000,
    0x40862000,
    0x40863000,
    0x40864000,
    0x40865000,
    0x40866000,
    0x40867000,
    0x40868000,
    0x40869000,
    0x4086a000,
    0x4086b000,
    0x4086c000,
    0x4086d000,
    0x4086e000,
    0x4086f000,
    0x40870000,
    0x40871000,
    0x40872000,
    0x40873000,
    0x40874000,
    0x40875000,
    0x40876000,
    0x40877000,
    0x40878000,
    0x40879000,
    0x4087a000,
    0x4087b000,
    0x4087c000,
    0x4087d000,
    0x4087e000,
    0x4087f000,
    0x40880000,
    0x40881000,
    0x40882000,
    0x40883000,
    0x40884000,
    0x40885000,
    0x40886000,
    0x40887000,
    0x40888000,
    0x40889000,
    0x4088a000,
    0x4088b000,
    0x4088c000,
    0x4088d000,
    0x4088e000,
    0x4088f000,
    0x40890000,
    0x40891000,
    0x40892000,
    0x40893000,
    0x40894000,
    0x40895000,
    0x40896000,
    0x40897000,
    0x40898000,
    0x40899000,
    0x4089a000,
    0x4089b000,
    0x4089c000,
    0x4089d000,
    0x4089e000,
    0x4089f000,
    0x408a0000,
    0x408a1000,
    0x408a2000,
    0x408a3000,
    0x408a4000,
    0x408a5000,
    0x408a6000,
    0x408a7000,
    0x408a8000,
    0x408a9000,
    0x408aa000,
    0x408ab000,
    0x408ac000,
    0x408ad000,
    0x408ae000,
    0x408af000,
    0x408b0000,
    0x408b1000,
    0x408b2000,
    0x408b3000,
    0x408b4000,
    0x408b5000,
    0x408b6000,
    0x408b7000,
    0x408b8000,
    0x408b9000,
    0x408ba000,
    0x408bb000,
    0x408bc000,
    0x408bd000,
    0x408be000,
    0x408bf000,
    0x408c0000,
    0x408c1000,
    0x408c2000,
    0x408c3000,
    0x408c4000,
    0x408c5000,
    0x408c6000,
    0x408c7000,
    0x408c8000,
    0x408c9000,
    0x408ca000,
    0x408cb000,
    0x408cc000,
    0x408cd000,
    0x408ce000,
    0x408cf000,
    0x408d0000,
    0x408d1000,
    0x408d2000,
    0x408d3000,
    0x408d4000,
    0x408d5000,
    0x408d6000,
    0x408d7000,
    0x408d8000,
    0x408d9000,
    0x408da000,
    0x408db000,
    0x408dc000,
    0x408dd000,
    0x408de000,
    0x408df000,
    0x408e0000,
    0x408e1000,
    0x408e2000,
    0x408e3000,
    0x408e4000,
    0x408e5000,
    0x408e6000,
    0x408e7000,
    0x408e8000,
    0x408e9000,
    0x408ea000,
    0x408eb000,
    0x408ec000,
    0x408ed000,
    0x408ee000,
    0x408ef000,
    0x408f0000,
    0x408f1000,
    0x408f2000,
    0x408f3000,
    0x408f4000,
    0x408f5000,
    0x408f6000,
    0x408f7000,
    0x408f8000,
    0x408f9000,
    0x408fa000,
    0x408fb000,
    0x408fc000,
    0x408fd000,
    0x408fe000,
    0x408ff000,
    0x40900000,
    0x40901000,
    0x40902000,
    0x40903000,
    0x40904000,
    0x40905000,
    0x40906000,
    0x40907000,
    0x40908000,
    0x40909000,
    0x4090a000,
    0x4090b000,
    0x4090c000,
    0x4090d000,
    0x4090e000,
    0x4090f000,
    0x40910000,
    0x40911000,
    0x40912000,
    0x40913000,
    0x40914000,
    0x40915000,
    0x40916000,
    0x40917000,
    0x40918000,
    0x40919000,
    0x4091a000,
    0x4091b000,
    0x4091c000,
    0x4091d000,
    0x4091e000,
    0x4091f000,
    0x40920000,
    0x40921000,
    0x40922000,
    0x40923000,
    0x40924000,
    0x40925000,
    0x40926000,
    0x40927000,
    0x40928000,
    0x40929000,
    0x4092a000,
    0x4092b000,
    0x4092c000,
    0x4092d000,
    0x4092e000,
    0x4092f000,
    0x40930000,
    0x40931000,
    0x40932000,
    0x40933000,
    0x40934000,
    0x40935000,
    0x40936000,
    0x40937000,
    0x40938000,
    0x40939000,
    0x4093a000,
    0x4093b000,
    0x4093c000,
    0x4093d000,
    0x4093e000,
    0x4093f000,
    0x40940000,
    0x40941000,
    0x40942000,
    0x40943000,
    0x40944000,
    0x40945000,
    0x40946000,
    0x40947000,
    0x40948000,
    0x40949000,
    0x4094a000,
    0x4094b000,
    0x4094c000,
    0x4094d000,
    0x4094e000,
    0x4094f000,
    0x40950000,
    0x40951000,
    0x40952000,
    0x40953000,
    0x40954000,
    0x40955000,
    0x40956000,
    0x40957000,
    0x40958000,
    0x40959000,
    0x4095a000,
    0x4095b000,
    0x4095c000,
    0x4095d000,
    0x4095e000,
    0x4095f000,
    0x40960000,
    0x40961000,
    0x40962000,
    0x40963000,
    0x40964000,
    0x40965000,
    0x40966000,
    0x40967000,
    0x40968000,
    0x40969000,
    0x4096a000,
    0x4096b000,
    0x4096c000,
    0x4096d000,
    0x4096e000,
    0x4096f000,
    0x40970000,
    0x40971000,
    0x40972000,
    0x40973000,
    0x40974000,
    0x40975000,
    0x40976000,
    0x40977000,
    0x40978000,
    0x40979000,
    0x4097a000,
    0x4097b000,
    0x4097c000,
    0x4097d000,
    0x4097e000,
    0x4097f000,
    0x40980000,
    0x40981000,
    0x40982000,
    0x40983000,
    0x40984000,
    0x40985000,
    0x40986000,
    0x40987000,
    0x40988000,
    0x40989000,
    0x4098a000,
    0x4098b000,
    0x4098c000,
    0x4098d000,
    0x4098e000,
    0x4098f000,
    0x40990000,
    0x40991000,
    0x40992000,
    0x40993000,
    0x40994000,
    0x40995000,
    0x40996000,
    0x40997000,
    0x40998000,
    0x40999000,
    0x4099a000,
    0x4099b000,
    0x4099c000,
    0x4099d000,
    0x4099e000,
    0x4099f000,
    0x409a0000,
    0x409a1000,
    0x409a2000,
    0x409a3000,
    0x409a4000,
    0x409a5000,
    0x409a6000,
    0x409a7000,
    0x409a8000,
    0x409a9000,
    0x409aa000,
    0x409ab000,
    0x409ac000,
    0x409ad000,
    0x409ae000,
    0x409af000,
    0x409b0000,
    0x409b1000,
    0x409b2000,
    0x409b3000,
    0x409b4000,
    0x409b5000,
    0x409b6000,
    0x409b7000,
    0x409b8000,
    0x409b9000,
    0x409ba000,
    0x409bb000,
    0x409bc000,
    0x409bd000,
    0x409be000,
    0x409bf000,
    0x409c0000,
    0x409c1000,
    0x409c2000,
    0x409c3000,
    0x409c4000,
    0x409c5000,
    0x409c6000,
    0x409c7000,
    0x409c8000,
    0x409c9000,
    0x409ca000,
    0x409cb000,
    0x409cc000,
    0x409cd000,
    0x409ce000,
    0x409cf000,
    0x409d0000,
    0x409d1000,
    0x409d2000,
    0x409d3000,
    0x409d4000,
    0x409d5000,
    0x409d6000,
    0x409d7000,
    0x409d8000,
    0x409d9000,
    0x409da000,
    0x409db000,
    0x409dc000,
    0x409dd000,
    0x409de000,
    0x409df000,
    0x409e0000,
    0x409e1000,
    0x409e2000,
    0x409e3000,
    0x409e4000,
    0x409e5000,
    0x409e6000,
    0x409e7000,
    0x409e8000,
    0x409e9000,
    0x409ea000,
    0x409eb000,
    0x409ec000,
    0x409ed000,
    0x409ee000,
    0x409ef000,
    0x409f0000,
    0x409f1000,
    0x409f2000,
    0x409f3000,
    0x409f4000,
    0x409f5000,
    0x409f6000,
    0x409f7000,
    0x409f8000,
    0x409f9000,
    0x409fa000,
    0x409fb000,
    0x409fc000,
    0x409fd000,
    0x409fe000,
    0x409ff000,
    0x40a00000,
    0x40a01000,
    0x40a02000,
    0x40a03000,
    0x40a04000,
    0x40a05000,
    0x40a06000,
    0x40a07000,
    0x40a08000,
    0x40a09000,
    0x40a0a000,
    0x40a0b000,
    0x40a0c000,
    0x40a0d000,
    0x40a0e000,
    0x40a0f000,
    0x40a10000,
    0x40a11000,
    0x40a12000,
    0x40a13000,
    0x40a14000,
    0x40a15000,
    0x40a16000,
    0x40a17000,
    0x40a18000,
    0x40a19000,
    0x40a1a000,
    0x40a1b000,
};
#else
uint32_t dma_mapping[] = {
    0x4081a000,
    0x40889000,
    0x408f8000,
    0x40967000,
    0x409d6000,
    0x409ef000,
    0x409fa000,
    0x40a05000,
    0x40a10000,
    0x40a1b000,
    0x40825000,
    0x40830000,
    0x4083b000,
    0x40846000,
    0x40851000,
    0x4085c000,
    0x40867000,
    0x40872000,
    0x4087d000,
    0x40888000,
    0x40894000,
    0x4089f000,
    0x408aa000,
    0x408b5000,
    0x408c0000,
    0x408cb000,
    0x408d6000,
    0x408e1000,
    0x408ec000,
    0x408f7000,
    0x40903000,
    0x4090e000,
    0x40919000,
    0x40924000,
    0x4092f000,
    0x4093a000,
    0x40945000,
    0x40950000,
    0x4095b000,
    0x40966000,
    0x40972000,
    0x4097d000,
    0x40988000,
    0x40993000,
    0x4099e000,
    0x409a9000,
    0x409b4000,
    0x409bf000,
    0x409ca000,
    0x409d5000,
    0x409e1000,
    0x409e6000,
    0x409e7000,
    0x409e8000,
    0x409e9000,
    0x409ea000,
    0x409eb000,
    0x409ec000,
    0x409ed000,
    0x409ee000,
    0x409f0000,
    0x409f1000,
    0x409f2000,
    0x409f3000,
    0x409f4000,
    0x409f5000,
    0x409f6000,
    0x409f7000,
    0x409f8000,
    0x409f9000,
    0x409fb000,
    0x409fc000,
    0x409fd000,
    0x409fe000,
    0x409ff000,
    0x40a00000,
    0x40a01000,
    0x40a02000,
    0x40a03000,
    0x40a04000,
    0x40a06000,
    0x40a07000,
    0x40a08000,
    0x40a09000,
    0x40a0a000,
    0x40a0b000,
    0x40a0c000,
    0x40a0d000,
    0x40a0e000,
    0x40a0f000,
    0x40a11000,
    0x40a12000,
    0x40a13000,
    0x40a14000,
    0x40a15000,
    0x40a16000,
    0x40a17000,
    0x40a18000,
    0x40a19000,
    0x40a1a000,
    0x4081b000,
    0x4081c000,
    0x4081d000,
    0x4081e000,
    0x4081f000,
    0x40820000,
    0x40821000,
    0x40822000,
    0x40823000,
    0x40824000,
    0x40826000,
    0x40827000,
    0x40828000,
    0x40829000,
    0x4082a000,
    0x4082b000,
    0x4082c000,
    0x4082d000,
    0x4082e000,
    0x4082f000,
    0x40831000,
    0x40832000,
    0x40833000,
    0x40834000,
    0x40835000,
    0x40836000,
    0x40837000,
    0x40838000,
    0x40839000,
    0x4083a000,
    0x4083c000,
    0x4083d000,
    0x4083e000,
    0x4083f000,
    0x40840000,
    0x40841000,
    0x40842000,
    0x40843000,
    0x40844000,
    0x40845000,
    0x40847000,
    0x40848000,
    0x40849000,
    0x4084a000,
    0x4084b000,
    0x4084c000,
    0x4084d000,
    0x4084e000,
    0x4084f000,
    0x40850000,
    0x40852000,
    0x40853000,
    0x40854000,
    0x40855000,
    0x40856000,
    0x40857000,
    0x40858000,
    0x40859000,
    0x4085a000,
    0x4085b000,
    0x4085d000,
    0x4085e000,
    0x4085f000,
    0x40860000,
    0x40861000,
    0x40862000,
    0x40863000,
    0x40864000,
    0x40865000,
    0x40866000,
    0x40868000,
    0x40869000,
    0x4086a000,
    0x4086b000,
    0x4086c000,
    0x4086d000,
    0x4086e000,
    0x4086f000,
    0x40870000,
    0x40871000,
    0x40873000,
    0x40874000,
    0x40875000,
    0x40876000,
    0x40877000,
    0x40878000,
    0x40879000,
    0x4087a000,
    0x4087b000,
    0x4087c000,
    0x4087e000,
    0x4087f000,
    0x40880000,
    0x40881000,
    0x40882000,
    0x40883000,
    0x40884000,
    0x40885000,
    0x40886000,
    0x40887000,
    0x4088a000,
    0x4088b000,
    0x4088c000,
    0x4088d000,
    0x4088e000,
    0x4088f000,
    0x40890000,
    0x40891000,
    0x40892000,
    0x40893000,
    0x40895000,
    0x40896000,
    0x40897000,
    0x40898000,
    0x40899000,
    0x4089a000,
    0x4089b000,
    0x4089c000,
    0x4089d000,
    0x4089e000,
    0x408a0000,
    0x408a1000,
    0x408a2000,
    0x408a3000,
    0x408a4000,
    0x408a5000,
    0x408a6000,
    0x408a7000,
    0x408a8000,
    0x408a9000,
    0x408ab000,
    0x408ac000,
    0x408ad000,
    0x408ae000,
    0x408af000,
    0x408b0000,
    0x408b1000,
    0x408b2000,
    0x408b3000,
    0x408b4000,
    0x408b6000,
    0x408b7000,
    0x408b8000,
    0x408b9000,
    0x408ba000,
    0x408bb000,
    0x408bc000,
    0x408bd000,
    0x408be000,
    0x408bf000,
    0x408c1000,
    0x408c2000,
    0x408c3000,
    0x408c4000,
    0x408c5000,
    0x408c6000,
    0x408c7000,
    0x408c8000,
    0x408c9000,
    0x408ca000,
    0x408cc000,
    0x408cd000,
    0x408ce000,
    0x408cf000,
    0x408d0000,
    0x408d1000,
    0x408d2000,
    0x408d3000,
    0x408d4000,
    0x408d5000,
    0x408d7000,
    0x408d8000,
    0x408d9000,
    0x408da000,
    0x408db000,
    0x408dc000,
    0x408dd000,
    0x408de000,
    0x408df000,
    0x408e0000,
    0x408e2000,
    0x408e3000,
    0x408e4000,
    0x408e5000,
    0x408e6000,
    0x408e7000,
    0x408e8000,
    0x408e9000,
    0x408ea000,
    0x408eb000,
    0x408ed000,
    0x408ee000,
    0x408ef000,
    0x408f0000,
    0x408f1000,
    0x408f2000,
    0x408f3000,
    0x408f4000,
    0x408f5000,
    0x408f6000,
    0x408f9000,
    0x408fa000,
    0x408fb000,
    0x408fc000,
    0x408fd000,
    0x408fe000,
    0x408ff000,
    0x40900000,
    0x40901000,
    0x40902000,
    0x40904000,
    0x40905000,
    0x40906000,
    0x40907000,
    0x40908000,
    0x40909000,
    0x4090a000,
    0x4090b000,
    0x4090c000,
    0x4090d000,
    0x4090f000,
    0x40910000,
    0x40911000,
    0x40912000,
    0x40913000,
    0x40914000,
    0x40915000,
    0x40916000,
    0x40917000,
    0x40918000,
    0x4091a000,
    0x4091b000,
    0x4091c000,
    0x4091d000,
    0x4091e000,
    0x4091f000,
    0x40920000,
    0x40921000,
    0x40922000,
    0x40923000,
    0x40925000,
    0x40926000,
    0x40927000,
    0x40928000,
    0x40929000,
    0x4092a000,
    0x4092b000,
    0x4092c000,
    0x4092d000,
    0x4092e000,
    0x40930000,
    0x40931000,
    0x40932000,
    0x40933000,
    0x40934000,
    0x40935000,
    0x40936000,
    0x40937000,
    0x40938000,
    0x40939000,
    0x4093b000,
    0x4093c000,
    0x4093d000,
    0x4093e000,
    0x4093f000,
    0x40940000,
    0x40941000,
    0x40942000,
    0x40943000,
    0x40944000,
    0x40946000,
    0x40947000,
    0x40948000,
    0x40949000,
    0x4094a000,
    0x4094b000,
    0x4094c000,
    0x4094d000,
    0x4094e000,
    0x4094f000,
    0x40951000,
    0x40952000,
    0x40953000,
    0x40954000,
    0x40955000,
    0x40956000,
    0x40957000,
    0x40958000,
    0x40959000,
    0x4095a000,
    0x4095c000,
    0x4095d000,
    0x4095e000,
    0x4095f000,
    0x40960000,
    0x40961000,
    0x40962000,
    0x40963000,
    0x40964000,
    0x40965000,
    0x40968000,
    0x40969000,
    0x4096a000,
    0x4096b000,
    0x4096c000,
    0x4096d000,
    0x4096e000,
    0x4096f000,
    0x40970000,
    0x40971000,
    0x40973000,
    0x40974000,
    0x40975000,
    0x40976000,
    0x40977000,
    0x40978000,
    0x40979000,
    0x4097a000,
    0x4097b000,
    0x4097c000,
    0x4097e000,
    0x4097f000,
    0x40980000,
    0x40981000,
    0x40982000,
    0x40983000,
    0x40984000,
    0x40985000,
    0x40986000,
    0x40987000,
    0x40989000,
    0x4098a000,
    0x4098b000,
    0x4098c000,
    0x4098d000,
    0x4098e000,
    0x4098f000,
    0x40990000,
    0x40991000,
    0x40992000,
    0x40994000,
    0x40995000,
    0x40996000,
    0x40997000,
    0x40998000,
    0x40999000,
    0x4099a000,
    0x4099b000,
    0x4099c000,
    0x4099d000,
    0x4099f000,
    0x409a0000,
    0x409a1000,
    0x409a2000,
    0x409a3000,
    0x409a4000,
    0x409a5000,
    0x409a6000,
    0x409a7000,
    0x409a8000,
    0x409aa000,
    0x409ab000,
    0x409ac000,
    0x409ad000,
    0x409ae000,
    0x409af000,
    0x409b0000,
    0x409b1000,
    0x409b2000,
    0x409b3000,
    0x409b5000,
    0x409b6000,
    0x409b7000,
    0x409b8000,
    0x409b9000,
    0x409ba000,
    0x409bb000,
    0x409bc000,
    0x409bd000,
    0x409be000,
    0x409c0000,
    0x409c1000,
    0x409c2000,
    0x409c3000,
    0x409c4000,
    0x409c5000,
    0x409c6000,
    0x409c7000,
    0x409c8000,
    0x409c9000,
    0x409cb000,
    0x409cc000,
    0x409cd000,
    0x409ce000,
    0x409cf000,
    0x409d0000,
    0x409d1000,
    0x409d2000,
    0x409d3000,
    0x409d4000,
    0x409d7000,
    0x409d8000,
    0x409d9000,
    0x409da000,
    0x409db000,
    0x409dc000,
    0x409dd000,
    0x409de000,
    0x409df000,
    0x409e0000,
    0x409e2000,
    0x409e3000,
    0x409e4000,
    0x409e5000,
};
#endif /* CONFIG_ARCH_AARCH32 */

struct  smmu_feature {
    word_t stream_match;              /*stream match register funtionality included*/
    word_t trans_op;                  /*address translation operations supported*/
    word_t cotable_walk;              /*coherent translation table walk*/
    word_t broadcast_tlb;             /*broadcast TLB maintenance*/
    word_t vmid16;                    /*16 bits VMIDs are supported*/
    uint32_t supported_trans;         /*supported translation stages*/
    uint32_t supported_fmt;           /*supported translation formats*/
    uint32_t num_cfault_ints;         /*supported number of context fault interrupts*/
    uint32_t num_stream_ids;          /*number of stream IDs*/
    uint32_t num_stream_map_groups;   /*num stream mapping register groups*/
    uint32_t smmu_page_size;          /*page size in SMMU register address space*/
    uint32_t smmu_num_pages;          /*number of pages in global or context bank address space*/
    uint32_t num_s2_cbanks;           /*cbanks that support stage 2 only*/
    uint32_t num_cbanks;              /*total number of context banks*/
    uint32_t va_bits;                 /*upstream address size*/
    uint32_t pa_bits;                 /*PA address size*/
    uint32_t ipa_bits;                /*IPA address size*/
    word_t cb_base;                   /*base of context bank address space*/
};

struct smmu_table_config {
    uint32_t tcr[2];                    /*SMMU_CBn_TCRm*/
    uint32_t mair[2];                  /*SMMU_CBn_MAIRm*/
#ifdef CONFIG_ARCH_AARCH32
    uint32_t ttbr[2];                  /*SMMU_CBn_TTBRm*/
#else
    uint64_t ttbr[2];                  /*SMMU_CBn_TTBRm*/
#endif /* CONFIG_ARCH_AARCH32 */
};

static struct smmu_feature smmu_dev_knowledge;
static struct smmu_table_config smmu_stage_table_config;

static inline uint32_t smmu_read_reg32(word_t base, uint32_t index)
{
    return *(volatile uint32_t *)(base + index);
}

static inline void smmu_write_reg32(word_t base, uint32_t index, uint32_t val)
{
    *(volatile uint32_t *)(base + index) = val;
}

static inline uint64_t smmu_read_reg64(word_t base, uint32_t index)
{
    return *(volatile uint64_t *)(base + index);
}

static inline void smmu_write_reg64(word_t base, uint32_t index, uint64_t val)
{
    *(volatile uint64_t *)(base + index) = val;
}

static void smmu_tlb_sync(word_t base, uint32_t sync, uint32_t status)
{
    int count = 0;
    smmu_write_reg32(base, sync, SMMU_TLB_SYNC_MASK);
    while (count < TLBSYNC_LOOP) {
        /*pulling the active flag, reading the TLB command state.*/
        if (!(smmu_read_reg32(base, status) & TLBSTATUS_GSACTIVE)) {
            break;
        }
        count++;
    }
}

static inline uint32_t smmu_obs_size_to_bits(uint32_t size)
{
    /*coverting the output bus address size into address bit, defined in
    IDx registers*/
    switch (size) {
    case 0:
        return 32;
    case 1:
        return 36;
    case 2:
        return 40;
    case 3:
        return 42;
    case 4:
        return 44;
    default:
        return 48;
    }
}
static inline uint32_t smmu_ubs_size_to_bits(uint32_t size)
{
    /*coverting the upstream address size into address bit, defined in
    IDx registers*/
    switch (size) {
    case 0:
        return 32;
    case 1:
        return 36;
    case 2:
        return 40;
    case 3:
        return 42;
    case 4:
        return 44;
    case 5:
        return 49;
    default:
        return 64;
    }
}

enum _bool {
    false = 0,
    true  = 1
};
typedef word_t asid_t;

static void smmu_probe(void)
{
    uint32_t reg, field;
    /*ID0*/
    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_IDR0);
    /*stages supported*/
    if (reg & IDR0_S1TS) {
        smmu_dev_knowledge.supported_trans |= STAGE1_TRANS;
    }
    if (reg & IDR0_S2TS) {
        smmu_dev_knowledge.supported_trans |= STAGE2_TRANS;
    }
    if (reg & IDR0_NTS) {
        smmu_dev_knowledge.supported_trans |= NESTED_TRANS;
    }
    /*stream matching register*/
    if (reg & IDR0_SMS) {
        smmu_dev_knowledge.stream_match = true;
    }
    /*address translation operation*/
    if ((reg & IDR0_ATOSNS) == 0) {
        smmu_dev_knowledge.trans_op = true;
    }
    /*AARCH32 translation format support*/
    field = IDR0_PTFS_VAL(reg & IDR0_PTFS);
    if (field == PTFS_AARCH32S_AARCH32L) {
        smmu_dev_knowledge.supported_fmt |= AARCH32L_FMT;
        smmu_dev_knowledge.supported_fmt |= AARCH32S_FMT;
    } else if (field == PTFS_AARCH32L_ONLY) {
        smmu_dev_knowledge.supported_fmt |= AARCH32L_FMT;
    } else {
        smmu_dev_knowledge.supported_fmt |= NO_AARCH32_FMT;
    }
    /*number of context fault intrrupts
    * However, in smmuv2, each context bank has dedicated interrupt pin
    * hence no requirement to specify implemented interrupts here.*/
    smmu_dev_knowledge.num_cfault_ints = IDR0_NUMIRPT_VAL(reg & IDR0_NUMIRPT);
    /*coherent translation table walk*/
    if (reg & IDR0_CTTW) {
        smmu_dev_knowledge.cotable_walk = true;
    }
    /*broadcast TLB maintenance*/
    if (reg & IDR0_BTM) {
        smmu_dev_knowledge.broadcast_tlb = true;
    }
    /*number of stream IDs*/
    smmu_dev_knowledge.num_stream_ids = (1 << IDR0_NUMSIDB_VAL(reg & IDR0_NUMSIDB)) - 1;
    /*number of stream mapping register groups*/
    smmu_dev_knowledge.num_stream_map_groups = reg & IDR0_NUMSMRG;

    /*ID1*/
    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_IDR1);
    /*smmu page size*/
    if (reg & IDR1_PAGESIZE) {
        smmu_dev_knowledge.smmu_page_size = SMMU_PAGE_64KB;
    } else {
        smmu_dev_knowledge.smmu_page_size = SMMU_PAGE_4KB;
    }
    /*smmu num pages, 2^(numdxb + 1)*/
    field = IDR1_NUMPAGENDXB_VAL(reg & IDR1_NUMPAGENDXB);
    smmu_dev_knowledge.smmu_num_pages = 1 << (field + 1);
    /*num of stage 2 context banks*/
    smmu_dev_knowledge.num_s2_cbanks = IDR1_NUMS2CB_VAL(reg & IDR1_NUMS2CB);
    /*total num of context banks*/
    smmu_dev_knowledge.num_cbanks = reg & IDR1_NUMCB;
    /*calcuate the context bank base*/
    smmu_dev_knowledge.cb_base = SMMU_CB_BASE_PADDR(
                                     SMMU_GLOBAL_SIZE(smmu_dev_knowledge.smmu_num_pages, smmu_dev_knowledge.smmu_page_size));

    /*ID2*/
    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_IDR2);
    /*VNID16S*/
    if (reg & IDR2_VMID16S) {
        smmu_dev_knowledge.vmid16 = true;
    }
    /*PTFSV8_64KB*/
    if (reg & IDR2_PTFSV8_64) {
        smmu_dev_knowledge.supported_fmt |= TRANS_PAGES_64KB;
    }
    /*PTFSV8_16KB*/
    if (reg & IDR2_PTFSV8_16) {
        smmu_dev_knowledge.supported_fmt |= TRANS_PAGES_16KB;
    }
    /*PTFSV8_64KB*/

    if (reg & IDR2_PTFSV8_4) {
        smmu_dev_knowledge.supported_fmt |= TRANS_PAGES_4KB;
    }
    /*UBS virtual address size*/
    smmu_dev_knowledge.va_bits = smmu_ubs_size_to_bits(IDR2_UBS_VAL(reg & IDR2_UBS));
    /*OAS*/
    smmu_dev_knowledge.pa_bits = smmu_obs_size_to_bits(IDR2_OAS_VAL(reg & IDR2_OAS));
    /*IAS*/
    smmu_dev_knowledge.ipa_bits = smmu_obs_size_to_bits(reg & IDR2_IAS);
}


static inline void smmu_reset(){
    uint32_t reg = 0;
    word_t cb_bank_ptr;
    uint32_t major;

    /*clear the fault syndrom registers*/
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_sGFSYNR0, reg);
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_sGFSYNR1, reg);

    /*clear the global FSR by writing back the read value*/
    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_sGFSR);
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_sGFSR, reg);

    /*reset stream to context config as using context banks*/
    reg = S2CR_PRIVCFG_SET(S2CR_PRIVCFG_DEFAULT);
    reg |= S2CR_TYPE_SET(S2CR_TYPE_CB);

    /*the number of stream-to-context is related to the stream indexing method*/
    if (smmu_dev_knowledge.stream_match) {
        /*stream matching*/
        for (uint32_t i = 0; i < smmu_dev_knowledge.num_stream_map_groups; i++) {
            smmu_write_reg32(SMMU_GR0_PPTR, SMMU_S2CRn(i), reg);
        }
        /*reset the stream match registers as invalid*/
        reg = SMR_VALID_SET(SMR_VALID_DIS);
        for (uint32_t i = 0; i < smmu_dev_knowledge.num_stream_map_groups; i++) {
            smmu_write_reg32(SMMU_GR0_PPTR, SMMU_SMRn(i), reg);
        }
    } else {
         /*stream ID*/
         for (uint32_t i = 0; i < smmu_dev_knowledge.num_stream_ids; i++) {
             smmu_write_reg32(SMMU_GR0_PPTR, SMMU_S2CRn(i), reg);
         }
    }

    /*special init requested by the smmu-500: start*/
    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_IDR7);
    major = IDR7_MAJOR_VAL(reg & IDR7_MAJOR);
    /*init the auxiliary configuration register*/
    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_sACR);
    /*unlock the write access to SMMU_CBn_ACTLR,
    only provided in version 2 and above*/
    if (major >= 2) {
        reg &= ~ACR_CACHE_LOCK;
    }
    /*enable the TLB to cache bypassing*/
    reg |= ACR_S2CRB_TLBEN | ACR_SMTNMB_TLBEN;
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_sACR, reg);
    /*special init requested by the smmu-500: end*/

    for (uint32_t i = 0; i < smmu_dev_knowledge.num_cbanks; i++) {
        cb_bank_ptr = SMMU_CBn_BASE_PPTR(i);
        /*disable context banks and clear the context bank fault registers*/
        smmu_write_reg32(cb_bank_ptr, SMMU_CBn_SCTLR, 0);
        /*clear the syndrom register*/
#ifdef CONFIG_ARCH_AARCH32
        smmu_write_reg32(cb_bank_ptr, SMMU_CBn_FAR, 0ULL);
#else
        smmu_write_reg64(cb_bank_ptr, SMMU_CBn_FAR, 0ULL);
#endif
        smmu_write_reg32(cb_bank_ptr, SMMU_CBn_FSR, CBn_FSR_CLEAR_ALL);
        /*special init requested by the smmu-500: start*/
        /*disable MMU-500's next page prefetch due to errata 841119 and 826419*/
        reg = smmu_read_reg32(cb_bank_ptr, SMMU_CBn_ACTLR);
        reg &= ~CBn_ACTLR_CPRE;
        smmu_write_reg32(cb_bank_ptr, SMMU_CBn_ACTLR, reg);
        /*special init requested by the smmu-500: end*/
    }

    /*invalidate TLB */
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_TLBIALLH, SMMU_TLB_INVALL_MASK);
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_TLBIALLNSNH, SMMU_TLB_INVALL_MASK);

    reg = smmu_read_reg32(SMMU_GR0_PPTR, SMMU_sCR0);
    /*enable global fault reporting*/
    reg |= CR0_GFRE | CR0_GFIE | CR0_GCFGFRE | CR0_GCFGFIE;
    /*raise fault for any transaction that does not match to
    any stream mapping table entires*/
    reg |= CR0_USFCFG;
    /*raise fault for stream match conflict*/
    reg |= CR0_SMCFCFG;
    /*enable the VMID private name space*/
    reg |= CR0_VMIDPNE;
    /*TLB is maintained together with the rest of the system*/
    reg &= ~CR0_PTM;
    /*enable force TLB broadcast on bypassing transactions*/
    reg |= CR0_FB;
    /*enable client access, ie transaction enforced by SMMU*/
    reg &= ~CR0_CLIENTPD;
    /*upgrade barrier to full system*/
    reg &= ~CR0_BSU(CR0_BSU_ALL);
    /*syn above issued TLB operations*/
    smmu_tlb_sync(SMMU_GR0_PPTR, SMMU_sTLBGSYNC, SMMU_sTLBGSTATUS);
    /*enable the SMMU*/
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_sCR0, reg);
}

#ifdef CONFIG_ARCH_AARCH32
static void smmu_config_stage1(struct smmu_table_config *cfg,
                               UNUSED bool_t coherence, uint32_t pa_bits,
                               uint32_t _smmu_table[], UNUSED asid_t asid)
#else
static void smmu_config_stage1(struct smmu_table_config *cfg,
                               bool_t coherence, uint32_t pa_bits,
                               uint64_t _smmu_table[], UNUSED asid_t asid)
#endif
{
    uint32_t reg = 0;
#ifdef CONFIG_ARCH_AARCH64
    /*SMMU_CBn_TCR*/
    coherence = false;
    if (coherence) {
        reg |= CBn_TCR_SH0_SET(CBn_TCR_SH_INNER);
        reg |= CBn_TCR_ORGN0_SET(CBn_TCR_GN_WB_WA_CACHE);
        reg |= CBn_TCR_IRGN0_SET(CBn_TCR_GN_WB_WA_CACHE);
    } else {
        reg |= CBn_TCR_SH0_SET(CBn_TCR_SH_OUTER);
        reg |= CBn_TCR_ORGN0_SET(CBn_TCR_GN_NCACHE);
        reg |= CBn_TCR_IRGN0_SET(CBn_TCR_GN_NCACHE);
    }
    /*page size is configed as 4k*/
    reg |= CBn_TCR_TG0_SET(CBn_TCR_TG_4K);
    /*the TTBR0 size, caculated according to the aarch64 formula*/
    reg |= CBn_TCR_T0SZ_SET(64 - SMMU_VA_DEFAULT_BITS);
    /*disable (speculative) page table walks through TTBR1*/
    reg |= CBn_TCR_EPD1_DIS;
    cfg->tcr[0] = reg;
#else
    cfg->tcr[0] = 0;
#endif
    /*TCR2*/
    reg = 0;
#ifdef CONFIG_ARCH_AARCH32
    pa_bits = 32;
#endif
    switch (pa_bits) {
    case 32:
        reg |= CBn_TCR2_PASize_SET(CBn_TCR2_PASize_32);
        break;
    case 36:
        reg |= CBn_TCR2_PASize_SET(CBn_TCR2_PASize_36);
        break;
    case 40:
        reg |= CBn_TCR2_PASize_SET(CBn_TCR2_PASize_40);
        break;
    case 42:
        reg |= CBn_TCR2_PASize_SET(CBn_TCR2_PASize_42);
        break;
    case 44:
        reg |= CBn_TCR2_PASize_SET(CBn_TCR2_PASize_44);
        break;
    default:
        reg |= CBn_TCR2_PASize_SET(CBn_TCR2_PASize_48);
        break;
    }
#ifdef CONFIG_ARCH_AARCH64
    reg |= CBn_TCR2_SEP_SET(CBn_TCR2_SEP_UPSTREAM_SIZE) | CBn_TCR2_AS_SET(CBn_TCR2_AS_16);
#endif /* CONFIG_ARCH_AARCH32 */
    cfg->tcr[1] = reg;
#ifdef CONFIG_ARCH_AARCH64
    /*MAIR0, configured according to the MAIR values in cores*/
    reg = CBn_MAIRm_ATTR_DEVICE_nGnRnE << CBn_MAIRm_ATTR_SHIFT(CBn_MAIRm_ATTR_ID_DEVICE_nGnRnE);
    reg |= CBn_MAIRm_ATTR_DEVICE_nGnRE << CBn_MAIRm_ATTR_SHIFT(CBn_MAIRm_ATTR_ID_DEVICE_nGnRE);
    reg |= CBn_MAIRm_ATTR_DEVICE_GRE << CBn_MAIRm_ATTR_SHIFT(CBn_MAIRm_ATTR_ID_DEVICE_GRE);
    reg |= CBn_MAIRm_ATTR_NC << CBn_MAIRm_ATTR_SHIFT(CBn_MAIRm_ATTR_ID_NC);
    cfg->mair[0] = reg;
    /*MAIR1*/
    reg = CBn_MAIRm_ATTR_CACHE << CBn_MAIRm_ATTR_SHIFT(CBn_MAIRm_ATTR_ID_CACHE);
    cfg->mair[1] = reg;
#endif /* CONFIG_ARCH_AARCH32 */
    /*TTBRs*/
    /*The SMMU only uses user-level address space, TTBR0.*/
#ifdef CONFIG_ARCH_AARCH32
    cfg->ttbr[0] = (uint32_t) _smmu_table;
    cfg->ttbr[1] = (uint32_t) _smmu_table;
#else
    cfg->ttbr[0] = (uint64_t) _smmu_table;
    cfg->ttbr[1] = (uint64_t) _smmu_table;
#endif /* CONFIG_ARCH_AARCH32 */
}

#ifdef CONFIG_ARCH_AARCH32
static void smmu_cb_assign_vspace(word_t cb, uint32_t _smmu_table[], asid_t asid)
#else
static void smmu_cb_assign_vspace(word_t cb, uint64_t _smmu_table[], asid_t asid)
#endif /* CONFIG_ARCH_AARCH32 */
{
    uint32_t reg = 0;
    uint32_t vmid = cb;

    smmu_stage_table_config = (struct smmu_table_config) {0};

    smmu_config_stage1(&smmu_stage_table_config,
                       smmu_dev_knowledge.cotable_walk,
                       smmu_dev_knowledge.ipa_bits,
                       _smmu_table,
                       asid);
    /*CBA2R*/
    /*currently only support aarch64*/
#ifdef CONFIG_ARCH_AARCH32
    reg = 0;
#else
    reg = CBA2Rn_VA64_SET;
#endif /* CONFIG_ARCH_AARCH32 */
    if (smmu_dev_knowledge.vmid16) {
        reg |= CBA2Rn_VMID_SET(vmid);
    }
    smmu_write_reg32(SMMU_GR1_PPTR, SMMU_CBA2Rn(cb), reg);

    /*CBAR*/
    /*stage 1 translation only, CBAR_TYPE_S1_TRANS_S2_BYPASS*/
    reg = CBARn_TYPE_SET(CBARn_TYPE_STAGE1);
    /*configured as the weakest shareability/memory types,
     * so they can be overwritten by ttbcr or pte */
    reg |= CBARn_BPSHCFG_SET(CBARn_BPSHCFG_NONE);
    reg |= CBARn_MemAttr_SET(MemAttr_OWB_IWB);

    smmu_write_reg32(SMMU_GR1_PPTR, SMMU_CBARn(cb), reg);
    /*TCR*/
    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_TCR2, smmu_stage_table_config.tcr[1]);
    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_TCR, smmu_stage_table_config.tcr[0]);
    /* stage 1 transaltion requires both ttbr 1 and ttbr 0
     * stage 2 transaltion requires ttbr 0*/

    /*ttbr0 (user space), for both stage 1 and stage 2*/
#ifdef CONFIG_ARCH_AARCH32
    printf("ttbr0 = %p\n", smmu_stage_table_config.ttbr[0]);
    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_TTBR0, smmu_stage_table_config.ttbr[0]);
    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_TTBR1, smmu_stage_table_config.ttbr[1]);
#else
    smmu_write_reg64(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_TTBR0, smmu_stage_table_config.ttbr[0]);
    smmu_write_reg64(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_TTBR1, smmu_stage_table_config.ttbr[1]);
#endif

    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_MAIR0, smmu_stage_table_config.mair[0]);
    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_MAIR1, smmu_stage_table_config.mair[1]);

    /*SCTLR, */
    reg = CBn_SCTLR_CFIE | CBn_SCTLR_CFRE | CBn_SCTLR_AFE | CBn_SCTLR_TRE | CBn_SCTLR_M | CBn_SCTLR_S1_ASIDPNE;
    smmu_write_reg32(SMMU_CBn_BASE_PPTR(cb), SMMU_CBn_SCTLR, reg);
}

void smmu_sid_bind_cb(word_t sid, word_t cb, word_t table_id)
{
    uint32_t reg = 0;
    reg = S2CR_PRIVCFG_SET(S2CR_PRIVCFG_DEFAULT);
    reg |= S2CR_TYPE_SET(S2CR_TYPE_CB);
    reg |= S2CR_CBNDX_SET(cb);
    smmu_write_reg32(SMMU_GR0_PPTR, SMMU_S2CRn(table_id), reg);
    /* The number of stream-to-context mapping
     * is related to the stream indexing method.
     * We currently supports mapping one stream ID to one context bank.*/
    if (smmu_dev_knowledge.stream_match) {
        reg = SMR_VALID_SET(SMR_VALID_EN) | SMR_ID_SET(sid);
        smmu_write_reg32(SMMU_GR0_PPTR, SMMU_SMRn(table_id), reg);
    }
}

#ifdef CONFIG_ARCH_AARCH64
void init_smmu_pagetables_aarch64(void)
{
    vaddr_t first_vaddr = DMA_START;
    vaddr_t last_vaddr = DMA_END;

    _smmu_pgd[GET_PGD_INDEX(first_vaddr)]
        = ((uintptr_t) _smmu_pud) | BIT(1) | BIT(0); /* its a page table */

    _smmu_pud[GET_PUD_INDEX(first_vaddr)]
        = ((uintptr_t) _smmu_pmd) | BIT(1) | BIT(0); /* its a page table */

    /* Make sure that the first and last vaddr are within the same page table */
    if (GET_PUD_INDEX(first_vaddr) != GET_PUD_INDEX(last_vaddr - 1)) {
        printf("first_vaddr and last_vaddr are in different page tables!\n");
        abort();
    }

    /* Map in the entire DMA region using the DMA mapping table */
    vaddr_t curr_vaddr = first_vaddr;
    int num_pages = DMA_SIZE / 4096;
    for (int i = 0; i < num_pages; i++) {
        if (GET_PMD_INDEX(curr_vaddr) == 2) {
            _smmu_pmd[GET_PMD_INDEX(curr_vaddr)] = ((uintptr_t) _smmu_pte_lo) | BIT(1) | BIT(0);
            _smmu_pt_lo[GET_PTE_INDEX(curr_vaddr)] = dma_mapping[i]
                | BIT(11)
                | BIT(10)
                | BIT(6)
                | (0 << 2)
                | BIT(1)
                | BIT(0);
        } else {
            _smmu_pmd[GET_PMD_INDEX(curr_vaddr)] = ((uintptr_t) _smmu_pte_hi) | BIT(1) | BIT(0);
            _smmu_pt_hi[GET_PTE_INDEX(curr_vaddr)] = dma_mapping[i]
                | BIT(11)
                | BIT(10)
                | BIT(6)
                | (0 << 2)
                | BIT(1)
                | BIT(0);
        }

        curr_vaddr += 4096;
    }
}
#else
void init_smmu_pagetables_aarch32(void)
{
    printf("smmu_pd = %p, smmu_pt_lo = %p, smmu_pt_mid = %p, smmu_pt_hi = %p\n", _smmu_pd, _smmu_pt_lo, _smmu_pt_mid, _smmu_pt_hi);

    /* Map in the entire DMA region using the DMA mapping table */
    vaddr_t curr_vaddr = DMA_START;
    int num_pages = DMA_SIZE / 4096;
    for (int i = 0; i < num_pages; i++) {
        if (GET_PD_INDEX(curr_vaddr) == 8) {
            _smmu_pd[GET_PD_INDEX(curr_vaddr)]
                = ((uint32_t) _smmu_pt_lo) | BIT(9) | BIT(0); /* it's a page table */
            _smmu_pt_lo[GET_PT_INDEX(curr_vaddr)] = dma_mapping[i]
                | (3 << 4) /* Full access r/w */
                | BIT(1); /* 4K page */
        } else if (GET_PD_INDEX(curr_vaddr) == 9) {
            _smmu_pd[GET_PD_INDEX(curr_vaddr)]
                = ((uint32_t) _smmu_pt_mid) | BIT(9) | BIT(0); /* it's a page table */
            _smmu_pt_mid[GET_PT_INDEX(curr_vaddr)] = dma_mapping[i]
                | (3 << 4) /* Full access r/w */
                | BIT(1); /* 4K page */
        } else if (GET_PD_INDEX(curr_vaddr) == 10) {
            _smmu_pd[GET_PD_INDEX(curr_vaddr)]
                = ((uint32_t) _smmu_pt_hi) | BIT(9) | BIT(0); /* it's a page table */
            _smmu_pt_hi[GET_PT_INDEX(curr_vaddr)] = dma_mapping[i]
                | (3 << 4) /* Full access r/w */
                | BIT(1); /* 4K page */
        } else {
            printf("What?\n");
            abort();
        }
        curr_vaddr += 4096;
    }
}
#endif /* CONFIG_ARCH_AARCH64 */

void init_static_smmu(void)
{
#ifdef CONFIG_ARCH_AARCH32
    init_smmu_pagetables_aarch32();
#else
    init_smmu_pagetables_aarch64();
#endif /* CONFIG_ARCH_AARCH32 */
    smmu_probe();
    smmu_reset();
#ifdef CONFIG_ARCH_AARCH32
	smmu_cb_assign_vspace(1, _smmu_pd, 1);
#else
	smmu_cb_assign_vspace(1, _smmu_pgd, 1);
#endif /* CONFIG_ARCH_AARCH32 */
    smmu_sid_bind_cb(0x877, 1, 1); // Stream ID of GEM3, fourth Ethernet device on ZYNQMP
	printf("Static SMMU initialised\n");
}

/*
 * Doesn't work on all platforms! The ATS1PR_LO and HI registers are not
 * explicitly required to be implemented.
 */
/*
void test_static_smmu(struct image_info *kernel_info) {
    printf("Doing a quick SMMU test...\n");

    uintptr_t cb_base = SMMU_CBn_PADDR(smmu_dev_knowledge.cb_base, 1, SMMU_PAGE_4KB) & 0xFFFFFFFF;
    printf("cb_base = %lx\n", cb_base);

    volatile uint32_t *trans_lo = (uint32_t *) (cb_base + 0x800); // SMMU_CBn_ATS1PR_LO
    volatile uint32_t *trans_hi = (uint32_t *) (cb_base + 0x804); // SMMU_CBn_ATS1PR_HI
    printf("trans_lo = %lx\n", trans_lo);
    printf("trans_hi = %lx\n", trans_hi);

    *trans_lo = (kernel_info->virt_region_start + 0x16000) & 0xFFFFF000;
    *trans_hi = ((kernel_info->virt_region_start + 0x16000) >> 32) & 0xFFFFFFFF;

    printf("Waiting for translation...\n");
    volatile uint32_t *status = (uint32_t *) (cb_base + 0x8f0); // SMMU_CBn_ATSR
    printf("status = %lx\n", status);
    while (*status & 0x1) {
        uint64_t in_progress = *status & 0x1;
        if (!in_progress) {
            break;
        }
    }
    printf("Translation done!\n");

    volatile uint32_t *res_lo = (uint32_t *) (cb_base + 0x50); // SMMU_CBn_PAR_LO
    volatile uint32_t *res_hi = (uint32_t *) (cb_base + 0x54); // SMMU_CBn_PAR_HI

    uint64_t phys = *res_lo;
    phys |= ((uint64_t) *res_hi) << 32;

    if (phys & 0x1) {
        printf("Fault occurred during translation!\n");
        return;
    }
    printf("Translated successfully!\n");
    printf("phys is 0x%x\n", phys);
    printf("Kernel's first paddr is 0x%x\n", kernel_info->phys_region_start + 0x16000);
}
*/
