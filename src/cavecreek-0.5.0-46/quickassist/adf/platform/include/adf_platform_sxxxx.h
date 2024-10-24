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
 * @file adf_platform_sxxxx.h
 *
 * @description
 *      This file contains the platform specific macros for SXXXX processor
 *
 *****************************************************************************/
#ifndef ADF_PLATFORM_SXXXX_H
#define ADF_PLATFORM_SXXXX_H

/*****************************************************************************
 * Define Constants and Macros
 *****************************************************************************/

/* PCIe configuration space */
#define ICP_SXXXX_PMISC_BAR               1
#define ICP_SXXXX_PMISC_BAR_SIZE          0x20000 /* (128KB) */
#define ICP_SXXXX_PETRINGCSR_BAR          2
#define ICP_SXXXX_PETRINGCSR_BAR_SIZE     0x4000  /* (16KB) */
#define ICP_SXXXX_MAX_PCI_BARS            3

/* Clock */
#define ICP_SXXXX_AE_CLOCK_IN_MHZ         800

/* ETR */
#define ICP_SXXXX_ETR_MAX_BANKS           8
#define ICP_SXXXX_BANKS_PER_ACCELERATOR   8
#define ICP_SXXXX_ETR_MAX_RINGS_PER_BANK  16
#define ICP_MAX_ET_RINGS                    \
    (ICP_SXXXX_ETR_MAX_BANKS * ICP_SXXXX_ETR_MAX_RINGS_PER_BANK)
#define ICP_SXXXX_ETR_MAX_AP_BANKS        4

/* msix vector */
#define ICP_SXXXX_MSIX_BANK_VECTOR_START 0
#define ICP_SXXXX_MSIX_BANK_VECTOR_NUM   8
#define ICP_SXXXX_MSIX_AE_VECTOR_START   16
#define ICP_SXXXX_MSIX_AE_VECTOR_NUM     1

/* Fuse Control */
#define ICP_SXXXX_FUSECTL_OFFSET         0x40
#define ICP_SXXXX_MAX_ACCELERATORS       1
#define ICP_SXXXX_MAX_ACCELENGINES       2
#define ICP_SXXXX_ACCELERATORS_MASK      0x1
#define ICP_SXXXX_ACCELENGINES_MASK      0x3

/* Accelerator Functions */
#define ICP_SXXXX_PKE_DISABLE_BIT        0x6
#define ICP_SXXXX_ATH_DISABLE_BIT        0x5
#define ICP_SXXXX_CPH_DISABLE_BIT        0x4
#define ICP_SXXXX_LOW_SKU_BIT            0x3
#define ICP_SXXXX_MID_SKU_BIT            0x2
#define ICP_SXXXX_AE1_DISABLE_BIT        0x1

/* PMISC BAR offsets */
#define PMISCBAROFFSET    0x1A000

/* Interrupt */
#define ICP_SXXXX_SINTPF                PMISCBAROFFSET + 0x24
#define ICP_SXXXX_SMIAOF                PMISCBAROFFSET + 0x28
#define ICP_SXXXX_BUNDLES_IRQ_MASK      0xFF
#define ICP_SXXXX_AE_IRQ_MASK           0x10000
#define ICP_SXXXX_SMIA_MASK             \
    (ICP_SXXXX_BUNDLES_IRQ_MASK | ICP_SXXXX_AE_IRQ_MASK)

/* VF-to-PF Messaging Interrupt */
#define ICP_SXXXX_MAX_NUM_VF           7
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
#define ICP_SXXXX_ETRING_CSR_OFFSET     0x00

/* Ring configuration space size
 * This chunk of memory will be mapped up to userspace
 * It is 0x200 times 16 bundles
 */
#define ICP_SXXXX_ETRING_CSR_SIZE       0x2000

/* Autopush CSR Offsets */
#define ICP_RING_CSR_AP_NF_MASK            0x2000
#define ICP_RING_CSR_AP_NF_DEST            0x2020
#define ICP_RING_CSR_AP_NE_MASK            0x2040
#define ICP_RING_CSR_AP_NE_DEST            0x2060
#define ICP_RING_CSR_AP_DELAY              0x2080

/* Autopush CSR Byte Offset */
#define AP_BANK_CSR_BYTE_OFFSET            4

/* user space */
#define ICP_SXXXX_USER_ETRING_CSR_SIZE   0x2000

#define SYS_PAGE_SIZE_4k                   0x0
#define SYS_PAGE_SIZE_8k                   0x1
#define SYS_PAGE_SIZE_64k                  0x2
#define SYS_PAGE_SIZE_256k                 0x4
#define SYS_PAGE_SIZE_1M                   0x8
#define SYS_PAGE_SIZE_4M                   0x10

/*
 * irq_get_bank_number
 * Function returns the interrupt source's bank number
 */
static inline Cpa32U irq_get_bank_number(Cpa32U sint)
{
    Cpa32U bank_number = 0;
    bank_number = sint & ICP_SXXXX_BUNDLES_IRQ_MASK;
    return bank_number;
}

/*
 * irq_ae_source
 * Function returns the value of AE interrupt source
 */
static inline Cpa32U irq_ae_source(Cpa32U sint)
{
    Cpa32U ae_irq = 0;
    ae_irq = sint & ICP_SXXXX_AE_IRQ_MASK;
    return ae_irq;
}

#endif /* ADF_PLATFORM_SXXXX_H */

