/*****************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or 
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
 * 
 *   This program is free software; you can redistribute it and/or modify 
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 * 
 *   This program is distributed in the hope that it will be useful, but 
 *   WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *   General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program; if not, write to the Free Software 
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution 
 *   in the file called LICENSE.GPL.
 * 
 *   Contact Information:
 *   Intel Corporation
 * 
 *   BSD LICENSE 
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 *  version: SXXXX.L.0.5.0-46
 *
 *****************************************************************************/

/*****************************************************************************
 * @file adf_platform_dh89xxcc.h
 *
 * @description
 *      This file contains the platform specific macros for DH89xxCC processor
 *
 *****************************************************************************/
#ifndef ADF_PLATFORM_DH89xxCC_H
#define ADF_PLATFORM_DH89xxCC_H

/*****************************************************************************
 * Define Constants and Macros
 *****************************************************************************/

/* PCIe configuration space */
#define ICP_DH89xxCC_PESRAM_BAR              0
#define ICP_DH89xxCC_PESRAM_BAR_SIZE         0x80000 /* (512KB) */
#define ICP_DH89xxCC_PMISC_BAR               1
#define ICP_DH89xxCC_PMISC_BAR_SIZE          0x20000 /* (128KB) */
#define ICP_DH89xxCC_PETRINGCSR_BAR          2
#define ICP_DH89xxCC_PETRINGCSR_BAR_SIZE     0x4000  /* (16KB) */
#define ICP_DH89xxCC_MAX_PCI_BARS            3

/* eSRAM */
#define ICP_DH89xxCC_SRAM_AE_ADDR            0x2000000000000ull
#define ICP_DH89xxCC_SRAM_AE_SIZE            0x80000

/* Clock */
#define ICP_DH89xxCC_AE_CLOCK_IN_MHZ         1066

/* ETR */
#define ICP_DH89xxCC_ETR_MAX_BANKS           16
#define ICP_DH89xxCC_ETR_MAX_RINGS_PER_BANK  16
#define ICP_DH89xxCC_BANKS_PER_ACCELERATOR   8
#define ICP_DH89xxCC_MAX_ET_RINGS   \
            (ICP_DH89xxCC_ETR_MAX_BANKS * ICP_DH89xxCC_ETR_MAX_RINGS_PER_BANK)
#define ICP_DH89xxCC_ETR_MAX_AP_BANKS 8

/* msix vector */
#define ICP_DH89xxCC_MSIX_BANK_VECTOR_START 0
#define ICP_DH89xxCC_MSIX_BANK_VECTOR_NUM   16
#define ICP_DH89xxCC_MSIX_AE_VECTOR_START   16
#define ICP_DH89xxCC_MSIX_AE_VECTOR_NUM     1

/* Fuse Control */
#define ICP_DH89xxCC_FUSECTL_OFFSET         0x40
#define ICP_DH89xxCC_CLKCTL_OFFSET          0x4C
#define ICP_DH89xxCC_CLKCTL_MASK            0x380000
#define ICP_DH89xxCC_CLKCTL_SHIFT           0x13
#define ICP_DH89xxCC_CLKCTL_DIV_SKU3        0x6
#define ICP_DH89xxCC_CLKCTL_DIV_SKU4        0x7

#define ICP_DH89xxCC_MAX_ACCELERATORS       2
#define ICP_DH89xxCC_MAX_ACCELENGINES       8
#define ICP_DH89xxCC_ACCELERATORS_MASK      0x03
#define ICP_DH89xxCC_ACCELENGINES_MASK      0xFF

/* Accelerator Functions */
#define ICP_DH89xxCC_LEGFUSE_OFFSET         0x4C
#define ICP_DH89xxCC_LEGFUSE_MASK           0x1F
#define ICP_DH89xxCC_LEGFUSES_FN0BIT4       0x04
#define ICP_DH89xxCC_LEGFUSES_FN0BIT3       0x03
#define ICP_DH89xxCC_LEGFUSES_FN0BIT2       0x02
#define ICP_DH89xxCC_LEGFUSES_FN0BIT1       0x01
#define ICP_DH89xxCC_LEGFUSES_FN0BIT0       0x00


/* PMISC BAR offsets */
#define PMISCBAROFFSET    0x1A000

/* Interrupt */
#define ICP_DH89xxCC_SINTPF                PMISCBAROFFSET + 0x24
#define ICP_DH89xxCC_SMIAOF                PMISCBAROFFSET + 0x28
#define ICP_DH89xxCC_BUNDLES_IRQ_MASK      0xFFFF
#define ICP_DH89xxCC_AE_IRQ_MASK           0x10000
#define ICP_DH89xxCC_SMIA_MASK             \
    (ICP_DH89xxCC_BUNDLES_IRQ_MASK | ICP_DH89xxCC_AE_IRQ_MASK)

/* VF-to-PF Messaging Interrupt */
#define ICP_DH89xxCC_MAX_NUM_VF            16
#define ICP_PMISCBAR_TIMISCINTCTL          PMISCBAROFFSET + 0x548
#define ICP_PMISCBAR_VFTOPFINTSRC          PMISCBAROFFSET + 0x0C
#define ICP_PMISCBAR_VFTOPFINTMSK          PMISCBAROFFSET + 0x1C
#define ICP_VFTOPF_INTSOURCEMASK           0x1FFFE00
#define ICP_VFTOPF_INTSOURCEOFFSET         0x09

#define ICP_PMISCBAR_PF2VFDBINT_OFFSET     PMISCBAROFFSET + 0xD0
#define ICP_PMISCBAR_VINTSOU_OFFSET        PMISCBAROFFSET + 0x180
#define ICP_PMISCBAR_VINTMSK_OFFSET        PMISCBAROFFSET + 0x1C0
#define ICP_PMISCBAR_VINTMSK_DEFAULT       0x02
#define ICP_PMISCBAR_VFDEVOFFSET           0x04

/* PETRING BAR offsets */
#define ICP_DH89xxCC_ETRING_CSR_OFFSET      0x00

/* Ring configuration space size
 * This chunk of memory will be mapped up to userspace
 * It is 0x200 times 16 bundles
 */
#define ICP_DH89xxCC_ETRING_CSR_SIZE        0x2000

/* Autopush CSR Offsets */
#define ICP_DH89xxCC_AP_REGS_OFFSET         0x2000
#define ICP_DH89xxCC_AP_NF_MASK             ICP_DH89xxCC_AP_REGS_OFFSET + 0x00
#define ICP_DH89xxCC_AP_NF_DEST             ICP_DH89xxCC_AP_REGS_OFFSET + 0x20
#define ICP_DH89xxCC_AP_NE_MASK             ICP_DH89xxCC_AP_REGS_OFFSET + 0x40
#define ICP_DH89xxCC_AP_NE_DEST             ICP_DH89xxCC_AP_REGS_OFFSET + 0x60
#define ICP_DH89xxCC_AP_DELAY               ICP_DH89xxCC_AP_REGS_OFFSET + 0x80
#define ICP_DH89xxCC_AP_BANK_BYTE_OFFSET    4

/* User space */
#define ICP_DH89xxCC_USER_ETRING_CSR_SIZE       0x2000

#define SYS_PAGE_SIZE_4k                   0x0
#define SYS_PAGE_SIZE_8k                   0x1
#define SYS_PAGE_SIZE_64k                  0x2
#define SYS_PAGE_SIZE_256k                 0x4
#define SYS_PAGE_SIZE_1M                   0x8
#define SYS_PAGE_SIZE_4M                   0x10

/* PMISC BAR region offsets and sizes */
#define ICP_DH89xxCC_REGION_CAP_OFFSET           0x0
#define ICP_DH89xxCC_REGION_CAP_SIZE             0x4000  /* (16KB) */
#define ICP_DH89xxCC_REGION_SCRATCH_RAM_OFFSET   0x4000
#define ICP_DH89xxCC_REGION_SCRATCH_RAM_SIZE     0x4000  /* (16KB) */
#define ICP_DH89xxCC_REGION_CPMS_OFFSET
#define ICP_DH89xxCC_REGION_CPMS_SIZE            0x8000  /* (32KB) */
#define ICP_DH89xxCC_REGION_MES_OFFSET
#define ICP_DH89xxCC_REGION_MES_SIZE             0x8000  /* (32KB) */
#define ICP_DH89xxCC_REGION_CHAP_PMU_OFFSET
#define ICP_DH89xxCC_REGION_CHAP_PMU_SIZE        0x2000  /* (8KB) */
#define ICP_DH89xxCC_REGION_EP_MMIO_CSRS_OFFSET
#define ICP_DH89xxCC_REGION_EP_MMIO_CSRS_SIZE    0x1000  /* (4KB) */
#define ICP_DH89xxCC_REGION_MSIX_TABS_OFFSET
#define ICP_DH89xxCC_REGION_MSIX_TABS_SIZE       0x1000  /* (4KB) */

/*
 * Offsets
 */

#define PMISCBAR_AE_OFFSET               (0x10000)
#define PMISCBAR_CPM_OFFSET              (0x8000)
#define PMISCBAR_RING_CLUSTER_OFFSET     (0x1A800)
#define PMISCBAR_CPP_SHAC_OFFSET         (0xC00)

/*
 * Uncorrectable errors Mask registers
 */

/*PMISCBAR Uncorrectable Error Mask*/
#define ICP_DH89xxCC_ERRMSK0            (PMISCBAROFFSET+0x10)
#define ICP_DH89xxCC_ERRMSK1            (PMISCBAROFFSET+0x14)
#define ICP_DH89xxCC_ERRMSK2            (PMISCBAROFFSET+0x18)
#define ICP_DH89xxCC_ERRMSK3            (PMISCBAROFFSET+0x1C)
/*PMISCBAR Uncorrectable Error Mask value*/
#define ICP_DH89xxCC_M3UNCOR            (0<<25) /*AE3 uncorrectable errors*/
#define ICP_DH89xxCC_M3COR              (1<<24) /*!AE3 correctable errors*/
#define ICP_DH89xxCC_M2UNCOR            (0<<17) /*AE2 uncorrectable errors*/
#define ICP_DH89xxCC_M2COR              (1<<16) /*!AE2 correctable errors*/
#define ICP_DH89xxCC_M1UNCOR            (0<<9)  /*AE1 uncorrectable errors*/
#define ICP_DH89xxCC_M1COR              (1<<8)  /*!AE1 correctable errors*/
#define ICP_DH89xxCC_M0UNCOR            (0<<1)  /*AE0 uncorrectable errors*/
#define ICP_DH89xxCC_M0COR              (1<<0)  /*!AE0 correctable errors*/
#define ICP_DH89xxCC_ERRMSK0_UERR                  \
    (ICP_DH89xxCC_M3UNCOR | ICP_DH89xxCC_M2UNCOR | \
    ICP_DH89xxCC_M1UNCOR | ICP_DH89xxCC_M0UNCOR  | \
    ICP_DH89xxCC_M3COR | ICP_DH89xxCC_M2COR | \
    ICP_DH89xxCC_M1COR | ICP_DH89xxCC_M0COR)
#define ICP_DH89xxCC_M7UNCOR            (0<<25) /*AE7 uncorrectable errors*/
#define ICP_DH89xxCC_M7COR              (1<<24) /*!AE7 correctable errors*/
#define ICP_DH89xxCC_M6UNCOR            (0<<17) /*AE6 uncorrectable errors*/
#define ICP_DH89xxCC_M6COR              (1<<16) /*!AE6 correctable errors*/
#define ICP_DH89xxCC_M5UNCOR            (0<<9)  /*AE5 uncorrectable errors*/
#define ICP_DH89xxCC_M5COR              (1<<8)  /*!AE5 correctable errors*/
#define ICP_DH89xxCC_M4UNCOR            (0<<1)  /*AE4 uncorrectable errors*/
#define ICP_DH89xxCC_M4COR              (1<<0)  /*!AE4 correctable errors*/
#define ICP_DH89xxCC_ERRMSK1_UERR                  \
    (ICP_DH89xxCC_M7UNCOR | ICP_DH89xxCC_M6UNCOR | \
    ICP_DH89xxCC_M5UNCOR | ICP_DH89xxCC_M4UNCOR  | \
    ICP_DH89xxCC_M7COR | ICP_DH89xxCC_M6COR | \
    ICP_DH89xxCC_M5COR | ICP_DH89xxCC_M4COR)
#define ICP_DH89xxCC_UERR                (0<<8) /*eSRam uncorrectable errors*/
#define ICP_DH89xxCC_PPMISCERR           (0<<6) /*Push Pull Misc uncorrectable
                                                   errors*/
#define ICP_DH89xxCC_EMSK3_CPM1          (0<<3) /*CPM1 interrupts*/
#define ICP_DH89xxCC_EMSK3_CPM0          (0<<2) /*CPM0 interrupts*/
#define ICP_DH89xxCC_EMSK3_SHaC1         (0<<1) /*OR'ed ShaC Attention Signal needed
                                                  by fw debug interrupt*/
#define ICP_DH89xxCC_EMSK3_SHaC0         (0<<0) /*OR'ed ShaC data errors*/
#define ICP_DH89xxCC_TIMISC              (1<<4) /*!FLR/BME Errors*/
#define ICP_DH89xxCC_RIMISC              (1<<5) /*!Push Pull error
                                                  detected by RI*/
#define ICP_EMSK3_DISABLE_OTHER          \
    ((1<<27) | (1<<26) | (1<<25) |(1<<7) |(0xFFFF<<9)) /* !PMU, !CPPCMD,
                             !VFTOPF (will be activated later if needed) */
#define ICP_DH89xxCC_ERRMSK3_UERR \
    (ICP_DH89xxCC_UERR | ICP_DH89xxCC_PPMISCERR |\
     ICP_DH89xxCC_EMSK3_CPM1 | ICP_DH89xxCC_EMSK3_CPM0 | \
     ICP_DH89xxCC_EMSK3_SHaC0 | ICP_DH89xxCC_RIMISC | ICP_DH89xxCC_TIMISC | \
     ICP_DH89xxCC_EMSK3_SHaC1 | ICP_EMSK3_DISABLE_OTHER)

/*AE ECC*/
#define PMISCBAR_CTX_ENABLES(idx)        (PMISCBAR_AE_OFFSET+(idx*0x1000)+0x818)
#define ICP_DH89xxCC_ENABLE_ECC_ERR      (1<<28) /*AEx Ecc error on*/
#define ICP_DH89xxCC_CTX_ECC_ERR(val)    ((val & (1<<29)) == 0 ? "No": "Yes")
#define ICP_DH89xxCC_CTX_PAR_ERR(val)    ((val & (1<<25)) == 0 ? "No": "Yes")

/*CPM Uncorrectable Errors*/
#define ICP_DH89xxCC_INTMASKSSM0         (PMISCBAR_CPM_OFFSET+0x0) /*32bits reg
                                                                     for CPM0*/
#define ICP_DH89xxCC_INTMASKSSM1         (ICP_DH89xxCC_INTMASKSSM0+0x4000)
/*32bits reg for CPM1*/
#define ICP_DH89xxCC_PPERR_MASK          (0<<6) /*Push/Pull Uncorrectable
                                                  interrupt*/
#define ICP_DH89xxCC_MMP1_UIMASK         (0<<4) /*MMP1 Uncorrectable interrupt*/
#define ICP_DH89xxCC_MMP0_UIMASK         (0<<2) /*MMP0 Uncorrectable interrupt*/
#define ICP_DH89xxCC_SH_UIMASK           (0<<0) /*Shared Memory Uncorrectable
                                                  interrupt*/
#define ICP_DH89xxCC_MMP1_CIMASK         (1<<5) /*!MMP1 correctable interrupt*/
#define ICP_DH89xxCC_MMP0_CIMASK         (1<<3) /*!MMP0 correctable interrupt*/
#define ICP_DH89xxCC_SH_CIMASK           (1<<1) /*!Shared Memory correctable
                                                  interrupt*/
#define ICP_DH89xxCC_TLB_PARITY_MASK     (0<<7) /*Push/Pull Uncorrectable
                                                  interrupt*/
#define ICP_DH89xxCC_INTMASKSSM_UERR                   \
(ICP_DH89xxCC_MMP0_UIMASK | ICP_DH89xxCC_MMP1_UIMASK | \
ICP_DH89xxCC_MMP0_CIMASK | ICP_DH89xxCC_MMP1_CIMASK | ICP_DH89xxCC_SH_CIMASK | \
ICP_DH89xxCC_SH_UIMASK | ICP_DH89xxCC_PPERR_MASK | ICP_DH89xxCC_TLB_PARITY_MASK)

/*SHAC Uncorrectable Errors*/
#define ICP_DH89xxCC_CPP_SHAC_ERR_CTRL   (PMISCBAR_CPP_SHAC_OFFSET+0x0)

/*Push/Pull control reg*/
#define ICP_DH89xxCC_SHAC_INTEN          (1<<1) /*Enable interrupt*/
#define ICP_DH89xxCC_SHAC_PPERREN        (1<<0) /*Enable push/pull errors
                                                  detection*/
#define ICP_DH89xxCC_CPP_SHAC_UE         (ICP_DH89xxCC_SHAC_INTEN | \
        ICP_DH89xxCC_SHAC_PPERREN)

/*eSRAM Uncorrectable Errors*/
#define ICP_DH89xxCC_ESRAMUERR           (PMISCBAR_RING_CLUSTER_OFFSET+0x4)
/*eSRAM UERROR Reg*/
#define ICP_DH89xxCC_ESRAM_INTEN         (1<<17) /*Enable interrupts*/
#define ICP_DH89xxCC_ESRAM_EN            (1<<3)  /*Enable Error Detection*/
#define ICP_DH89xxCC_ESRAM_UERR          (ICP_DH89xxCC_ESRAM_INTEN | \
        ICP_DH89xxCC_ESRAM_EN)

/*Push/Pull/Misc Errors*/
#define ICP_DH89xxCC_CPPMEMTGTERR        (PMISCBAR_RING_CLUSTER_OFFSET+0x10)
#define ICP_DH89xxCC_TGT_PPEREN          (1<<3) /*Enable Error Detection*/
#define ICP_DH89xxCC_TGT_INTEN           (1<<2) /*Enable Interrupts*/
#define ICP_DH89xxCC_TGT_UERR            (ICP_DH89xxCC_TGT_PPEREN | \
        ICP_DH89xxCC_TGT_INTEN)

/*
 * Status/Log Register
 */

/*PMISCBAR Uncorrectable Error*/
#define ICP_DH89xxCC_ERRSOU0              (PMISCBAROFFSET)
#define ICP_DH89xxCC_ERRSOU1              (PMISCBAROFFSET+0x04)
#define ICP_DH89xxCC_ERRSOU2              (PMISCBAROFFSET+0x08)
#define ICP_DH89xxCC_ERRSOU3              (PMISCBAROFFSET+0x0C)
/*errsou 1*/
#define ICP_DH89xxCC_M7UNCOR_MASK         (1<<25) /*AE7 uncorrectable errors*/
#define ICP_DH89xxCC_M6UNCOR_MASK         (1<<17) /*AE6 uncorrectable errors*/
#define ICP_DH89xxCC_M5UNCOR_MASK         (1<<9)  /*AE5 uncorrectable errors*/
#define ICP_DH89xxCC_M4UNCOR_MASK         (1<<1)  /*AE4 uncorrectable errors*/
/*errsou 0*/
#define ICP_DH89xxCC_M3UNCOR_MASK         (1<<25) /*AE3 uncorrectable errors*/
#define ICP_DH89xxCC_M2UNCOR_MASK         (1<<17) /*AE2 uncorrectable errors*/
#define ICP_DH89xxCC_M1UNCOR_MASK         (1<<9)  /*AE1 uncorrectable errors*/
#define ICP_DH89xxCC_M0UNCOR_MASK         (1<<1)  /*AE0 uncorrectable errors*/
/*errsou 3*/
#define ICP_DH89xxCC_UERR_MASK            (1<<8)  /*eSRam uncorrectable errors*/
#define ICP_DH89xxCC_PPMISCERR_MASK       (1<<6)  /*Push Pull Misc
                                                   uncorrectable errors*/
#define ICP_DH89xxCC_EMSK3_CPM1_MASK      (1<<3)  /*CPM1 interrupts*/
#define ICP_DH89xxCC_EMSK3_CPM0_MASK      (1<<2)  /*CPM0 interrupts*/
#define ICP_DH89xxCC_EMSK3_SHaC0_MASK     (1<<0)  /*OR'ed ShaC data errors*/

/*AE Status*/
#define ICP_DH89xxCC_EPERRLOG             (PMISCBAROFFSET+0x20)
#define ICP_DH89xxCC_GET_EPERRLOG(val, me) \
    (val & (1 <<(1+(2*me))))
#define ICP_DH89xxCC_EPERRLOG_ERR(val)    ((val  == 0) ? "No": "Yes")

/*Me Control*/
#define ICP_DH89xxCC_USTORE_ERROR_STATUS(idx) \
    (PMISCBAR_AE_OFFSET+(idx*0x1000)+0x80C)
#define ICP_DH89xxCC_USTORE_IS_UE(val)    ((val & (1<<31)) == 0 ? "No": "Yes")
#define ICP_DH89xxCC_USTORE_GET_UE(val)   ((val & (1<<31)) >> 31)
#define ICP_DH89xxCC_USTORE_UADDR(val)    (val & 0x3FFF) /* Faulty address*/
#define ICP_DH89xxCC_USTORE_GET_SYN(val)  ((val & (0x7F << 20)) >> 20)
/*Syndrom*/

/*AE Parity Error*/
#define ICP_DH89xxCC_REG_ERROR_STATUS(idx) \
        (PMISCBAR_AE_OFFSET+(idx*0x1000)+0x830)
#define ICP_DH89xxCC_REG_GET_REG_AD(val)  (val & 0xFFFF) /*Faulty Register
                                                           or local mem*/
#define ICP_DH89xxCC_REG_TYPE(val)        ((val & (0x3<<15)) >> 15)
/*Mem type mask (00-GPR 01-transfer 10-NextNeighbor 11-Local Mem)*/
#define ICP_DH89xxCC_REG_TYP_STR(val)     ((val == 0)? "GPR": \
        ((val == 1)? "Transfer": ((val == 2)?                \
        "Next Neighbour": "Local Memory")))

/*CPM*/
#define ICP_DH89xxCC_INTSTATSSM0          (PMISCBAR_CPM_OFFSET+0x4) /*32bits reg
                                                                      for CPM0*/
#define ICP_DH89xxCC_INTSTATSSM1          (ICP_DH89xxCC_INTSTATSSM0+0x4000)
/*32bits reg for CPM1*/
#define ICP_DH89xxCC_INTSTATSSM_PPERR     (1<<6) /*Push Pull Error
                                                   -> log in PPERRSSM*/
#define ICP_DH89xxCC_INTSTATSSM_MMP1      (1<<4) /*MMP UERR -> log in
                                                   UERRSSMMMPx0*/
#define ICP_DH89xxCC_INTSTATSSM_MMP0      (1<<2) /*MMP UERR -> log in
                                                   UERRSSMMMPx1*/
#define ICP_DH89xxCC_INTSTATSSM_SH        (1<<0) /*Shared Memory UERR ->
                                                   log in UERRSSMSHx*/

/*MMP*/
#define ICP_DH89xxCC_UERRSSMMMP00         (PMISCBAR_CPM_OFFSET+0x388)  /*CPM0 -
                                                                         MMP0*/
#define ICP_DH89xxCC_UERRSSMMMP01         (PMISCBAR_CPM_OFFSET+0x1388) /*CPM0 -
                                                                         MMP1*/
#define ICP_DH89xxCC_UERRSSMMMP10         (ICP_DH89xxCC_UERRSSMMMP00+0x4000)
/*CPM0 - MMP0*/
#define ICP_DH89xxCC_UERRSSMMMP11         (ICP_DH89xxCC_UERRSSMMMP01+0x4000)
/*CPM1 - MMP1*/
#define ICP_DH89xxCC_UERRSSMMMP_EN        (1<<3)   /*1 = Enable logging*/
#define ICP_DH89xxCC_UERRSSMMMP_UERR      (1<<0)   /*1 = Uerr*/
#define ICP_DH89xxCC_UERRSSMMMP_ERRTYPE(val) \
    ((val & (0xF<<4)) >> 4)
/*MMP ECC Err TypeMask  0 - MMP program mem 1 - MMP OPA Ram, 2 MMP OPB Ram
 *(3-F rsd)*/
#define ICP_DH89xxCC_UERRSSMMMP_ERRTYPE_STR(val) \
    ((val == 0)? "MMP program Mem": \
     ((val == 1)? "MMP OPA Ram": ((val == 2)? "MMP OPB Ram": "Reserved")))
#define ICP_DH89xxCC_UERRSSMMMPAD00       (PMISCBAR_CPM_OFFSET+0x38C)  /*CPM0 -
                                                                  MMP0 address*/
#define ICP_DH89xxCC_UERRSSMMMPAD01       (PMISCBAR_CPM_OFFSET+0x138C) /*CPM0 -
                                                                  MMP1 address*/
#define ICP_DH89xxCC_UERRSSMMMPAD10       (ICP_DH89xxCC_UERRSSMMMPAD00+0x4000)
/*CPM1 - MMP0 address*/
#define ICP_DH89xxCC_UERRSSMMMPAD11       (ICP_DH89xxCC_UERRSSMMMPAD01+0x4000)
/*CPM1 - MMP1 address*/
#define ICP_DH89xxCC_UERRSSMMMPAD_ADDR    (0xFFFF) /*Address Mask*/

/*Shared memory*/
#define ICP_DH89xxCC_UERRSSMSH0           (PMISCBAR_CPM_OFFSET+0x18)
#define ICP_DH89xxCC_UERRSSMSH1           (ICP_DH89xxCC_UERRSSMSH0+0x4000)
#define ICP_DH89xxCC_UERRSSMSH_EN         (1<<3) /*Enable logging*/
#define ICP_DH89xxCC_UERRSSMSH_UERR       (1<<0) /*Uncorrectable error*/
#define ICP_DH89xxCC_UERRSSMSH_R          (1<<1) /*On read operation*/
#define ICP_DH89xxCC_UERRSSMSH_W          (1<<2) /*On Write operation*/
#define ICP_DH89xxCC_UERRSSMSH_GET_OP(val) \
        ((val& ICP_DH89xxCC_UERRSSMSH_R) != 0? "Read": \
        ((val& ICP_DH89xxCC_UERRSSMSH_W) != 0? "Write": "Error"))
#define ICP_DH89xxCC_UERRSSMSH_ERRTYPE(val) \
    ((val & (0xF<<4)) >> 4)
/*Error type 1,4,8-F reserved ECC 0-slice push 2-AE Push command
 * 3-Me Pull Command 5-DRAM Pull 6-CPM Memory read (RMW DRAM  push)
 * 7-CPM bus master read*/
#define ICP_DH89xxCC_UERRSSMSH_GET_ERRTYPE(val) \
((val == 0)? "Slice Push": ((val == 2)? "AE Push command": (                   \
 (val == 3)? "AE Pull command": ((val == 5)? "DRAM pull": (                    \
 (val == 6)? "CPM Memory read (DRAM  push)": ((val == 7)?                      \
         "CPM bus master read": "Reserved"))))))
#define ICP_DH89xxCC_UERRSSMSHAD0        (PMISCBAR_CPM_OFFSET+0x1C) /*Shared Ram
                                                                   status CPM0*/
#define ICP_DH89xxCC_UERRSSMSHAD1        (ICP_DH89xxCC_UERRSSMSHAD0+0x4000+0x1C)
/*Shared Ram status CPM1*/
#define ICP_DH89xxCC_UERRSSMSHAD_ADDR    (0xFFFF) /*Address Mask*/

/*Push/Pull*/
#define ICP_DH89xxCC_PPERR0              (PMISCBAR_CPM_OFFSET+0x8) /*push/pull
                                                                 status cpm 0*/
#define ICP_DH89xxCC_PPERR1              (ICP_DH89xxCC_PPERR0+0x4000)
/*push/pull status cpm 1*/
#define ICP_DH89xxCC_PPERR_EN            (1<<2) /*Enable Err logging*/

#define ICP_DH89xxCC_PPERR_MERR          (1<<1) /*Multiple Errors*/
#define ICP_DH89xxCC_PPERR_PPERR         (1<<0) /*Uncorrectable Error*/
#define ICP_DH89xxCC_PPERR_GET_STATUS(val) \
   (((val & ICP_DH89xxCC_PPERR_PPERR)) != 0?    \
   (((val & ICP_DH89xxCC_PPERR_MERR) != 0)? "multiple errors occurred":\
     "one error occured"): "no errors occurred")
#define ICP_DH89xxCC_PPERR_TYPE(val)     ((val & (0xF<<4)) >> 4)
/*Error type 0-data asserted on AE Pull Data 1- data asserted on  Dram Push
 * 2-Master bus error 3-ECC error on Shared mem access to Dram pull data bus*/
#define ICP_DH89xxCC_PPERR_TYPE_STR(val) \
    ((val == 0)? "Data asserted on AE Pull": ((val == 1)? \
        "Data asserted on Dram Push": \
        ((val == 2)? "Master Bus Error":\
         "ECC error on Shared mem access to Dram pull data bus")))
#define ICP_DH89xxCC_PPERRID0            (PMISCBAR_CPM_OFFSET+0xC) /*push/pull
                                                               faulty ID cpm 0*/
#define ICP_DH89xxCC_PPERRID1            (ICP_DH89xxCC_PPERRID0+0x4000) /*push
                                                        /pull faulty ID cpm 1*/

#define ICP_DH89xxCC_CPP_SHAC_ERR_STATUS  \
    (PMISCBAR_CPP_SHAC_OFFSET+0x4) /*Push/Pull status reg*/
#define ICP_DH89xxCC_CPP_SHAC_GET_ERR(val) \
    (val & 1) /*Push/Pull status Error(s) occured*/
#define ICP_DH89xxCC_CPP_SHAC_GET_TYP(val) \
    ((val & (1<<3)) >> 3) /*Error type 0-push(read) 1-pull(write)*/
#define ICP_DH89xxCC_CPP_SHAC_GET_ERTYP(val) \
    ((val & (0x3<<4)) >> 4)
#define ICP_DH89xxCC_CPP_SHAC_ERTYP_STR(val) \
    ((val == 0)? "Scratch": ((val == 1)? "Hash": ((val == 2)? \
            "CAP - impossible for push": "reserved")))
/*Error type operation 00-scratch 01-hash 10-CAP(not possible for push)
 * 11-reserved*/
#define ICP_DH89xxCC_CPP_SHAC_GET_INT(val) ((val & (1<<2)) >> 2)
/*Push/Pull status interrupt occured*/
#define ICP_DH89xxCC_CPP_SHAC_ERR_PPID     (PMISCBAR_CPP_SHAC_OFFSET+0xC)
/*Push/Pull faulty ID(32b) reg*/

/*eSRAM*/
#define ICP_DH89xxCC_ESRAMUERR_UERR         (1<<0) /*eSRAM UERR*/
#define ICP_DH89xxCC_ESRAMUERR_GET_UERR(val) \
    ((val & ICP_DH89xxCC_ESRAMUERR_UERR) != 0? "Yes": "No")
#define ICP_DH89xxCC_ESRAMUERR_R            (1<<1) /*eSRAM UERR on read*/
#define ICP_DH89xxCC_ESRAMUERR_W            (1<<2) /*eSRAM UERR on write*/
#define ICP_DH89xxCC_ESRAMUERR_GET_ERRTYPE(val) \
    ((val & (0xF<<4)) >> 4)
/*eSRAM UERR Type mask 0 -ECC data error 1-F reserved*/
#define ICP_DH89xxCC_ESRAMUERR_GET_ERRSTR(val) \
    (val == 0? "Ecc data Error": "Reserved")
#define ICP_DH89xxCC_ESRAMUERRAD            (PMISCBAR_RING_CLUSTER_OFFSET+0xC)
/*eSRAM UERR Addr in Qword->address|000*/
#define ICP_DH89xxCC_ESRAMUERR_GET_OP(val) \
        ((val& ICP_DH89xxCC_ESRAMUERR_R) != 0? "Read": \
        ((val& ICP_DH89xxCC_ESRAMUERR_W) != 0? "Write": "Error"))

/*Miscellaneous Memory Target Error*/
#define ICP_DH89xxCC_CPPMEMTGTERR_ERRTYP(val) \
    ((val & (0xF<<16)) >> 16)
/*Error Type 3-P2S data path error 4-SRAM memory read transaction 5-
 * Pull bus during memory write - see EAS 13-221 - 0 to 2- 6 to F - reserved*/
#define ICP_DH89xxCC_CPPMEMTGTERR_ERRTYP_STR(val) \
    ((val == 4)? "Memory Read Transaction": ((val == 5)? "Memory write": \
        ((val == 3)? "P2S Data Path": "reserved")))
#define ICP_DH89xxCC_CPPMEMTGTERR_ERR      (1<<0)  /*Misc Memory Target Error
                                                     occured*/
#define ICP_DH89xxCC_CPPMEMTGTERR_MERR     (1<<1)  /*Multiple Errors occurred*/
#define ICP_DH89xxCC_CPPMEMTGTERR_GET_STATUS(val) \
   (((val & ICP_DH89xxCC_CPPMEMTGTERR_ERR)) != 0?    \
    (((val & ICP_DH89xxCC_CPPMEMTGTERR_MERR) != 0)? "multiple errors occurred":\
     "one error occured"): "no errors occurred")
#define ICP_DH89xxCC_ERRPPID               (PMISCBAR_RING_CLUSTER_OFFSET+0x14)
/*32 bits push/pull UERR id*/
#define ICP_DH89xxCC_TICPPINTCTL           (0x1A400+0x138) /*TI CPP control*/
#define ICP_DH89xxCC_TICPP_PUSH            (1<<0) /*push error log*/
#define ICP_DH89xxCC_TICPP_PULL            (1<<1) /*pull error log*/
#define ICP_DH89xxCC_TICPP_GETOP(val) \
    ((val&ICP_DH89xxCC_TICPP_PUSH) != 0?"Push": "Pull")
#define ICP_DH89xxCC_TICPP_EN              (ICP_DH89xxCC_TICPP_PUSH | \
        ICP_DH89xxCC_TICPP_PULL)           /*Enable Error log*/
#define ICP_DH89xxCC_TICPPINTSTS            (0x1A400+0x13C) /*captures the
                                                           CPP error detected on
                                                           the TI CPP bus*/

#define ICP_DH89xxCC_TIERRPUSHID            (0x1A400+0x140) /*log the id of the
                                                           push error*/
#define ICP_DH89xxCC_TIERRPULLID            (0x1A400+0x144) /*log the id of the
                                                           pull error*/

/*
 * adf_isr_getMiscBarAd
 * Returns the MISC BAR address of the device
 */
static inline void *
adf_isr_getMiscBarAd(icp_accel_dev_t *accel_dev)
{
    Cpa32U pmisc_bar_id = 0;
    adf_hw_device_data_t  *hw_data = NULL;
    /*Param check handle...?*/
    hw_data = accel_dev->pHwDeviceData;
    pmisc_bar_id = hw_data->getMiscBarId(hw_data);
    return (void*)(UARCH_INT)accel_dev->pciAccelDev.pciBars[pmisc_bar_id].virtAddr;
}

/*
 * irq_get_bank_number
 * Function returns the interrupt source's bank number
 */
static inline Cpa32U irq_get_bank_number(Cpa32U sint)
{
    Cpa32U bank_number = 0;
    bank_number = sint & ICP_DH89xxCC_BUNDLES_IRQ_MASK;
    return bank_number;
}

/*
 * irq_ae_source
 * Function returns the value of AE interrupt source
 */
static inline Cpa32U irq_ae_source(Cpa32U sint)
{
    Cpa32U ae_irq = 0;
    ae_irq = sint & ICP_DH89xxCC_AE_IRQ_MASK;
    return ae_irq;
}

#endif /* ADF_PLATFORM_DH89xxCC_H */
