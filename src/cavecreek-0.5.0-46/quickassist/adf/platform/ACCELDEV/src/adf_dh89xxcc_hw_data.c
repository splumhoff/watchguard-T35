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
 * @file adf_dh89xxcc_hw_data.c
 *
 * @ingroup
 *
 * @description
 *    This file contains the hw device data for DH89xxcc
 *****************************************************************************/
#include "cpa.h"
#include "icp_accel_devices.h"
#include "icp_platform.h"
#include "adf_platform_common.h"
#include "adf_platform_dh89xxcc.h"

/*****************************************************************************
 * External symbols
 *****************************************************************************/
extern void adf_bank0_handler(void* handle);
extern void adf_bank1_handler(void* handle);
extern void adf_bank2_handler(void* handle);
extern void adf_bank3_handler(void* handle);
extern void adf_bank4_handler(void* handle);
extern void adf_bank5_handler(void* handle);
extern void adf_bank6_handler(void* handle);
extern void adf_bank7_handler(void* handle);
extern void adf_bank8_handler(void* handle);
extern void adf_bank9_handler(void* handle);
extern void adf_bank10_handler(void* handle);
extern void adf_bank11_handler(void* handle);
extern void adf_bank12_handler(void* handle);
extern void adf_bank13_handler(void* handle);
extern void adf_bank14_handler(void* handle);
extern void adf_bank15_handler(void* handle);

extern void adf_bank0_polling_handler(void* handle);
extern void adf_bank1_polling_handler(void* handle);
extern void adf_bank2_polling_handler(void* handle);
extern void adf_bank3_polling_handler(void* handle);
extern void adf_bank4_polling_handler(void* handle);
extern void adf_bank5_polling_handler(void* handle);
extern void adf_bank6_polling_handler(void* handle);
extern void adf_bank7_polling_handler(void* handle);
extern void adf_bank8_polling_handler(void* handle);
extern void adf_bank9_polling_handler(void* handle);
extern void adf_bank10_polling_handler(void* handle);
extern void adf_bank11_polling_handler(void* handle);
extern void adf_bank12_polling_handler(void* handle);
extern void adf_bank13_polling_handler(void* handle);
extern void adf_bank14_polling_handler(void* handle);
extern void adf_bank15_polling_handler(void* handle);

/*******************************************************************************
* Static Variables
*******************************************************************************/
STATIC adf_hw_device_class_t dh89xxcc_class = {
    .name         = "dh89xxcc"    ,
    .type         = DEV_DH89XXCC  ,
    .numInstances = 0             ,
    .currentInstanceId = 0
};

STATIC bank_handler bh_bank_handlers[] = {
    adf_bank0_handler,
    adf_bank1_handler,
    adf_bank2_handler,
    adf_bank3_handler,
    adf_bank4_handler,
    adf_bank5_handler,
    adf_bank6_handler,
    adf_bank7_handler,
    adf_bank8_handler,
    adf_bank9_handler,
    adf_bank10_handler,
    adf_bank11_handler,
    adf_bank12_handler,
    adf_bank13_handler,
    adf_bank14_handler,
    adf_bank15_handler,
};

STATIC bank_handler bh_polling_bank_handlers[] = {
    adf_bank0_polling_handler,
    adf_bank1_polling_handler,
    adf_bank2_polling_handler,
    adf_bank3_polling_handler,
    adf_bank4_polling_handler,
    adf_bank5_polling_handler,
    adf_bank6_polling_handler,
    adf_bank7_polling_handler,
    adf_bank8_polling_handler,
    adf_bank9_polling_handler,
    adf_bank10_polling_handler,
    adf_bank11_polling_handler,
    adf_bank12_polling_handler,
    adf_bank13_polling_handler,
    adf_bank14_polling_handler,
    adf_bank15_polling_handler,
};

#define NUM_ELEM(array)                   \
    sizeof (array) / sizeof (*array)

/*
 * getAcceleratorsMask
 * Gets the acceleration mask based on the fuse information
 */
STATIC Cpa32U
getAcceleratorsMask(Cpa32U fuse)
{
    return (((~fuse) >> ICP_DH89xxCC_MAX_ACCELENGINES) &
                                ICP_DH89xxCC_ACCELERATORS_MASK);
}

/*
 * getAccelEnginesMask
 * Gets the accelerator engine mask based on the fuse information
 */
STATIC Cpa32U
getAccelEnginesMask(Cpa32U fuse)
{
    return ((~fuse) & ICP_DH89xxCC_ACCELENGINES_MASK);
}

/*
 * getDevFuseOffset
 * Function returns the device fuse control offset
 */
STATIC Cpa32U
getDevFuseOffset(void)
{
    return ICP_DH89xxCC_FUSECTL_OFFSET;
}

/*
 * getDevClkOffset
 * Function returns the device clock control offset
 */
STATIC Cpa32U
getDevClkOffset(void)
{
    return ICP_DH89xxCC_CLKCTL_OFFSET;
}

/*
 * getDevSKU
 * Function returns device SKU info
 */
STATIC dev_sku_info_t
getDevSKU(Cpa32U NumAccelerators, Cpa32U NumAccelEngines, Cpa32U ClkMask,
          Cpa32U fuse)
{
    Cpa32U clock = (ClkMask & ICP_DH89xxCC_CLKCTL_MASK)
                                           >> ICP_DH89xxCC_CLKCTL_SHIFT;

    switch (NumAccelerators)
    {
        case 1:
            return DEV_SKU_2;
        case ICP_DH89xxCC_MAX_ACCELERATORS:
        switch(clock)
        {
            case ICP_DH89xxCC_CLKCTL_DIV_SKU3:
                return DEV_SKU_3;
            case ICP_DH89xxCC_CLKCTL_DIV_SKU4:
                return DEV_SKU_4;
            default:
                return DEV_SKU_UNKNOWN;
        }
        default:
            return DEV_SKU_UNKNOWN;
    }
    return DEV_SKU_UNKNOWN;
}

/*
 * getNumAccelerators
 * Function returns the number of accelerators
 *
 */
STATIC Cpa32U
getNumAccelerators(adf_hw_device_data_t *self, Cpa32U mask)
{
    Cpa32U i = 0, ctr = 0;

    if (!mask)
    {
        return 0;
    }

    for (i = 0; i < ICP_DH89xxCC_MAX_ACCELERATORS; i++)
    {
        if (mask & (1 << i))
        {
            ctr++;
        }
    }

    return ctr;
}

/*
 * getNumAccelEngines
 * Function returns the number of accelerator engines
 */
STATIC Cpa32U
getNumAccelEngines(adf_hw_device_data_t *self, Cpa32U mask)
{
    Cpa32U i, ctr = 0;

    if (!mask)
    {
        return 0;
    }

    for (i = 0; i < ICP_DH89xxCC_MAX_ACCELENGINES; i++)
    {
        if (mask & (1 << i))
        {
            ctr++;
        }
    }

    return ctr;
}

/*
 * getAccelMaskList
 * Function returns the accelerator mask for both
 * accelerators in the system
 */
STATIC void
getAccelMaskList(adf_hw_device_data_t *self,
                Cpa32U accelMask, Cpa32U aeMask, Cpa32U *pMaskList)
{
    /* This function sets the accelerator mask for both accelerators */
    Cpa32U i = 0, accelId = 0;
    Cpa32U ae_ctr = 0, tmpMask = 0;
    Cpa32U numAe = 0, numAccel = 0;
    Cpa32U num_aes_per_accel = 0;

    for(i = 0; i < self->maxNumAccel; i++)
    {
        pMaskList[i] = 0;
    }
    /*
     * For the case where we only have 1 accelerator
     * the all the AEs must be associated with that
     * accelerator.
     */
    numAccel = self->getNumAccelerators(self, accelMask);
    if(self->maxNumAccel != numAccel)
    {
        for(accelId = 0; accelId < self->maxNumAccel; accelId++)
        {
            /*
             * The accelMask is equal to 1 or 2 in decimal units.
             * Once we discover if we are 1 or 2 then we are done.
             */
            if(accelMask == (accelId+1))
            {
                pMaskList[accelId] = aeMask;
                /*In this case we are done.*/
                return;
            }
            else
            {
                pMaskList[accelId] = 0;
            }
        }
    }
    /*
     * For the other cases we must have 2 accelerator
     * active. Calculate the mask for the first accelerator
     * and the second one is clear then
     */
    accelId = 0;
    numAe = self->getNumAccelEngines(self, aeMask);
    num_aes_per_accel = numAe/numAccel;
    for(i = 0; i < self->maxNumAccelEngines; i++)
    {
        if (aeMask & (1 << i))
        {
            ae_ctr++;
            pMaskList[accelId] |= (1 << i);
        }
        if(ae_ctr == num_aes_per_accel)
        {
            /* We are done with this accelerator. */
            break;
        }
    }
    /* Now to find the mask of the other accelerator is simple. */
    tmpMask = ~pMaskList[accelId] & aeMask;
    pMaskList[++accelId] = tmpMask;

    return;
}

/*
 * getAccFuncOffset
 * Function returns the accelerator capabilities offset
 */
STATIC Cpa32U
getAccFuncOffset(void)
{
    return ICP_DH89xxCC_LEGFUSE_OFFSET;
}

/*
 * getAccFuncMask
 * Gets the accelerator functionality mask based on the accelerator capability
 */
STATIC Cpa32U
getAccFuncMask(Cpa32U func)
{
    Cpa32U mask = 0;
    Cpa32U accFunc = 0;

    /* func is the value of the register */
    accFunc = func;

    accFunc |= ICP_DH89xxCC_LEGFUSE_MASK;

    if( accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT0 ))
        mask |= ICP_ACCEL_CAPABILITIES_CIPHER;

    if( accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT1 ))
        mask |= ICP_ACCEL_CAPABILITIES_AUTHENTICATION;

    if( accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT2 ))
        mask |= ICP_ACCEL_CAPABILITIES_REGEX;

    if( accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT3 ))
        mask |= ICP_ACCEL_CAPABILITIES_COMPRESSION;

    if( accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT4 ))
        mask |= ICP_ACCEL_CAPABILITIES_LZS_COMPRESSION;

    if( (accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT0 )) &&
                (accFunc & ( 1 << ICP_DH89xxCC_LEGFUSES_FN0BIT1 )) )
    {
        mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
        mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
        mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_0;
        mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_1;
    }

    /* add random number capability */
    mask |= ICP_ACCEL_CAPABILITIES_RANDOM_NUMBER;

    return mask;
}

/*
 * getMiscBarId
 * Returns the MISC BAR id of the device
 */
STATIC Cpa32U
getMiscBarId(adf_hw_device_data_t *self)
{
    return ICP_DH89xxCC_PMISC_BAR;
}

/*
 * getSmiapfOffsetMask
 * Function sets the SMIAPF offset and default mask
 */
STATIC void
getSmiapfOffsetMask(adf_hw_device_data_t *self, Cpa32U *offset, Cpa32U *mask)
{
    if (NULL != offset)
    {
        *offset = ICP_DH89xxCC_SMIAOF;
    }
    if (NULL != mask)
    {
        *mask   = ICP_DH89xxCC_SMIA_MASK;
    }
}

/*
 * getSramBarId
 * Returns the SRAM BAR id of the device
 */
STATIC Cpa32U
getSramBarId(adf_hw_device_data_t *self)
{
    return ICP_DH89xxCC_PESRAM_BAR;
}

/*
 * getEtrBarId
 * Returns the ETR BAR id of the device
 */
STATIC Cpa32U
getEtrBarId(adf_hw_device_data_t *self, Cpa32U *offset)
{
    if (NULL != offset)
    {
        *offset = ICP_DH89xxCC_ETRING_CSR_OFFSET;
    }
    return ICP_DH89xxCC_PETRINGCSR_BAR;
}

/*
 * isBarAvailable
 * Returns whether the barId is available on the device
 */
STATIC CpaBoolean
isBarAvailable(adf_hw_device_data_t *self, Cpa32U barId)
{
    CpaBoolean bars[] = {CPA_TRUE, CPA_TRUE, CPA_TRUE};

    return (barId < (sizeof (bars) / sizeof (*bars))) ?
        bars[barId]                                   :
        CPA_FALSE;
}

/*
 * getEsramInfo
 * Function sets the eSRAM information of the device
 */
STATIC void
getEsramInfo(adf_hw_device_data_t *self, adf_esram_data_t *esramInfo)
{
    if (NULL != esramInfo)
    {
        esramInfo->sramAeAddr = ICP_DH89xxCC_SRAM_AE_ADDR;
        esramInfo->sramAeSize = ICP_DH89xxCC_SRAM_AE_SIZE;
    }
}

/*
 * getScratchRamInfo
 * Function returns the Scratch RAM information of the device
 *
 */
STATIC void
getScratchRamInfo(adf_hw_device_data_t *self,
            Cpa32U *offset, Cpa32U *size)
{
    if (NULL != offset)
    {
        *offset = ICP_DH89xxCC_REGION_SCRATCH_RAM_OFFSET;
    }

    if (NULL != size)
    {
        *size = ICP_DH89xxCC_REGION_SCRATCH_RAM_SIZE;
    }
}

/*
 * irqGetBankNumber
 * Function returns the bank number based on interrupt source(sintpf)
 */
STATIC Cpa32U
irqGetBankNumber(adf_hw_device_data_t *self,
            Cpa32U sintpf)
{
    return irq_get_bank_number(sintpf);
}

/*
 * irqGetAeSource
 * Function returns the AE source based on interrupt source(sintpf)
 */
STATIC Cpa32U
irqGetAeSource(adf_hw_device_data_t *self, Cpa32U sintpf)
{
    return irq_ae_source(sintpf);
}

/*
 * init
 * Function initialise internal hw data
 */
STATIC CpaStatus
init(adf_hw_device_data_t *self, Cpa8U node_id)
{
    ICP_CHECK_FOR_NULL_PARAM(self);

    self->instanceId = self->dev_class->currentInstanceId++;
    ++self->dev_class->numInstances;

    return CPA_STATUS_SUCCESS;
}

/*
 * cleanup
 * Function cleanup internal hw data
 */
STATIC void
cleanup(adf_hw_device_data_t *self)
{
    ICP_CHECK_FOR_NULL_PARAM_VOID(self);

    --self->dev_class->numInstances;
    self->instanceId = self->dev_class->currentInstanceId--;

    return;
}

/*
 * getBankBhHandler
 * Function returns a bank handler based on bankId
 */
STATIC bank_handler
getBankBhHandler(Cpa32U bankId)
{
    return (bankId < NUM_ELEM(bh_bank_handlers)) ?
        bh_bank_handlers[bankId]                 :
        NULL;
}

/*
 * getBankBhPollingHandler
 * Function returns a polling bank handler based on bankId
 */
STATIC bank_handler
getBankBhPollingHandler(Cpa32U bankId)
{
    return (bankId < NUM_ELEM(bh_polling_bank_handlers)) ?
        bh_polling_bank_handlers[bankId]                 :
        NULL;
}

/*
 * getApCsrOffsets
 * This function set the auto-push register offset based on
 * the bank id.
 */
STATIC void
getApCsrOffsets(Cpa32U bankId, Cpa32U *nfMask, Cpa32U *nfDest,
                Cpa32U *neMask, Cpa32U *neDest, Cpa32U *apDelay)
{
    if (NULL != nfMask)
    {
        *nfMask = ICP_DH89xxCC_AP_NF_MASK +
                    (bankId * ICP_DH89xxCC_AP_BANK_BYTE_OFFSET);
    }

    if (NULL != nfDest)
    {
        *nfDest = ICP_DH89xxCC_AP_NF_DEST +
                    (bankId * ICP_DH89xxCC_AP_BANK_BYTE_OFFSET);
    }

    if (NULL != neMask)
    {
        *neMask = ICP_DH89xxCC_AP_NE_MASK +
                    (bankId * ICP_DH89xxCC_AP_BANK_BYTE_OFFSET);
    }

    if (NULL != neDest)
    {
        *neDest = ICP_DH89xxCC_AP_NE_DEST +
                    (bankId * ICP_DH89xxCC_AP_BANK_BYTE_OFFSET);
    }

    if (NULL != apDelay)
    {
        *apDelay = ICP_DH89xxCC_AP_DELAY;
    }
}

/*
 * getVf2PfIntSourceOffset
 * This function returns byte offset of the VF-to-PF interrupt source CSR.
 */
STATIC Cpa32U
getVf2PfIntSourceOffset(void)
{
    return ICP_PMISCBAR_VFTOPFINTSRC;
}

/*
 * getVf2PfBitOffset
 * This function returns the VF-to-PF bit offset in the interrupt source CSR.
 */
STATIC Cpa32U
getVf2PfBitOffset(void)
{
    return ICP_VFTOPF_INTSOURCEOFFSET;
}

/*
 * getVf2PfIntMaskOffset
 * This function returns byte offset of the VF-to-PF interrupt mask CSR.
 */
STATIC Cpa32U
getVf2PfIntMaskOffset(void)
{
    return ICP_PMISCBAR_VFTOPFINTMSK;
}

/*
 * getVf2PfIntMaskDefault
 * This function returns the default VF-to-PF interrupt mask.
 */
STATIC Cpa32U
getVf2PfIntMaskDefault(void)
{
    return ICP_VFTOPF_INTSOURCEMASK;
}

/*
 * getTiMiscIntCtlOffset
 * This function returns byte offset of the miscellaneous interrupt
 * control register.
 */
STATIC Cpa32U
getTiMiscIntCtlOffset(void)
{
    return ICP_PMISCBAR_TIMISCINTCTL;
}

/*
 * getVIntMskOffset
 * This function returns byte offset of the VF interrupt mask CSR.
 */
STATIC Cpa32U
getVIntMskOffset(Cpa32U id)
{
    return (ICP_PMISCBAR_VINTMSK_OFFSET + (id * ICP_PMISCBAR_VFDEVOFFSET));
}

/*
 * getVIntMskDefault
 * This function returns the default VF interrupt mask.
 */
STATIC Cpa32U
getVIntMskDefault(void)
{
    return ICP_PMISCBAR_VINTMSK_DEFAULT;
}

/*
 * getPf2VfDbIntOffset
 * This function returns byte offset of the doorbell and interrupt CSR
 */
STATIC Cpa32U
getPf2VfDbIntOffset(Cpa32U id)
{
    return (ICP_PMISCBAR_PF2VFDBINT_OFFSET + (id * ICP_PMISCBAR_VFDEVOFFSET));
}

/*
 * adf_set_hw_data_dh89xxcc
 * Initialise the hw data structure
 */
void
adf_set_hw_data_dh89xxcc(void *hw_data)
{
    adf_hw_device_data_t *hw = NULL;

    ICP_CHECK_FOR_NULL_PARAM_VOID(hw_data);

    hw = (adf_hw_device_data_t *)hw_data;

    hw->init    = init;
    hw->cleanup = cleanup;

    hw->dev_class = &dh89xxcc_class;

    /* populate bank/ac information */
    hw->maxBars               = ICP_DH89xxCC_MAX_PCI_BARS;
    hw->maxNumBanks           = ICP_DH89xxCC_ETR_MAX_BANKS;
    hw->maxNumApBanks         = ICP_DH89xxCC_ETR_MAX_AP_BANKS;
    hw->numRingsPerBank       = ICP_DH89xxCC_ETR_MAX_RINGS_PER_BANK;
    hw->numBanksPerAccel      = ICP_DH89xxCC_BANKS_PER_ACCELERATOR;
    hw->maxNumAccel           = ICP_DH89xxCC_MAX_ACCELERATORS;
    hw->maxNumAccelEngines    = ICP_DH89xxCC_MAX_ACCELENGINES;
    hw->aeClockInMhz          = ICP_DH89xxCC_AE_CLOCK_IN_MHZ;
    hw->sintpfOffset          = ICP_DH89xxCC_SINTPF;
    hw->userEtringCsrSize     = ICP_DH89xxCC_USER_ETRING_CSR_SIZE;
    hw->maxNumVf              = ICP_DH89xxCC_MAX_NUM_VF;

    /* populate msix information */
    hw->msix.banksVectorStart = ICP_DH89xxCC_MSIX_BANK_VECTOR_START;
    hw->msix.banksVectorNum   = ICP_DH89xxCC_MSIX_BANK_VECTOR_NUM;
    hw->msix.aeVectorStart    = ICP_DH89xxCC_MSIX_AE_VECTOR_START;
    hw->msix.aeVectorNum      = ICP_DH89xxCC_MSIX_AE_VECTOR_NUM;

    /* device fuse */
    hw->getDevFuseOffset      = getDevFuseOffset;
    hw->getDevClkOffset       = getDevClkOffset;
    hw->getDevSKU             = getDevSKU;
    hw->getAcceleratorsMask   = getAcceleratorsMask;
    hw->getAccelEnginesMask   = getAccelEnginesMask;
    hw->getNumAccelerators    = getNumAccelerators;
    hw->getNumAccelEngines    = getNumAccelEngines;
    hw->getAccelMaskList      = getAccelMaskList;

    /* accelerator functions */
    hw->getAccFuncOffset      = getAccFuncOffset;
    hw->getAccFuncMask        = getAccFuncMask;

    /* populate bars callbacks  */
    hw->getEtrBarId           = getEtrBarId;
    hw->getSramBarId          = getSramBarId;
    hw->getMiscBarId          = getMiscBarId;
    hw->isBarAvailable        = isBarAvailable;

    /* populate esram callbacks    */
    hw->getEsramInfo          = getEsramInfo;

    /* populate SHAC callback */
    hw->getScratchRamInfo     = getScratchRamInfo;

    /* populate PMiscBar callback  */
    hw->getSmiapfOffsetMask   = getSmiapfOffsetMask;

    hw->irqGetBankNumber      = irqGetBankNumber;
    hw->irqGetAeSource        = irqGetAeSource;

    hw->getBankBhHandler        = getBankBhHandler;
    hw->getBankBhPollingHandler = getBankBhPollingHandler;

    /* Auto-push feature callback */
    hw->isAutoPushSupported     = CPA_TRUE;
    hw->getApCsrOffsets         = getApCsrOffsets;

    hw->getVf2PfIntSourceOffset = getVf2PfIntSourceOffset;
    hw->getVf2PfBitOffset       = getVf2PfBitOffset;
    hw->getVf2PfIntMaskOffset   = getVf2PfIntMaskOffset;
    hw->getVf2PfIntMaskDefault  = getVf2PfIntMaskDefault;

    hw->getTiMiscIntCtlOffset   = getTiMiscIntCtlOffset;
    hw->getVIntMskOffset        = getVIntMskOffset;
    hw->getVIntMskDefault       = getVIntMskDefault;
    hw->getPf2VfDbIntOffset     = getPf2VfDbIntOffset;

}
