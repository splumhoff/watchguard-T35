/*******************************************************************************
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
*******************************************************************************/

/*******************************************************************************
* @file adf_sxxxx_hw_data.c
*
* @ingroup
*
* @description
*   This file contains the hw device data for a SXXXX device
*
*******************************************************************************/
#include "cpa.h"
#include "icp_accel_devices.h"
#include "icp_platform.h"
#include "adf_platform_common.h"
#include "adf_platform_sxxxx.h"

/*******************************************************************************
* Include public/global header files
*******************************************************************************/

/*******************************************************************************
* Include private header files
*******************************************************************************/

/******************************************************************************
* External symbols
*******************************************************************************/

extern void adf_bank0_handler(void* handle);
extern void adf_bank1_handler(void* handle);
extern void adf_bank2_handler(void* handle);
extern void adf_bank3_handler(void* handle);
extern void adf_bank4_handler(void* handle);
extern void adf_bank5_handler(void* handle);
extern void adf_bank6_handler(void* handle);
extern void adf_bank7_handler(void* handle);

extern void adf_bank0_polling_handler(void* handle);
extern void adf_bank1_polling_handler(void* handle);
extern void adf_bank2_polling_handler(void* handle);
extern void adf_bank3_polling_handler(void* handle);
extern void adf_bank4_polling_handler(void* handle);
extern void adf_bank5_polling_handler(void* handle);
extern void adf_bank6_polling_handler(void* handle);
extern void adf_bank7_polling_handler(void* handle);

/*******************************************************************************
* STATIC Variables
*******************************************************************************/

STATIC adf_hw_device_class_t sxxxx_class = {
    .name         = "sxxxx"    ,
    .type         = DEV_SXXXX  ,
    .numInstances = 0             ,
    .currentInstanceId = 0
};

STATIC bank_handler adf_sxxxx_bh_bank_handlers[] = {
    adf_bank0_handler,
    adf_bank1_handler,
    adf_bank2_handler,
    adf_bank3_handler,
    adf_bank4_handler,
    adf_bank5_handler,
    adf_bank6_handler,
    adf_bank7_handler,
};

STATIC bank_handler adf_sxxxx_bh_polling_bank_handlers[] = {
    adf_bank0_polling_handler,
    adf_bank1_polling_handler,
    adf_bank2_polling_handler,
    adf_bank3_polling_handler,
    adf_bank4_polling_handler,
    adf_bank5_polling_handler,
    adf_bank6_polling_handler,
    adf_bank7_polling_handler,
};

#define NUM_ELEM(array)                   \
    sizeof (array) / sizeof (*array)

/*******************************************************************************
* Functions
*******************************************************************************/

/*
 * getAcceleratorsMask
 * Gets the acceleration mask based on the fuse information
 */
STATIC Cpa32U
adf_sxxxx_getAcceleratorsMask(Cpa32U fuse)
{
    return ((~fuse) & ICP_SXXXX_ACCELERATORS_MASK);
}

/*
 * getAccelEnginesMask
 * Gets the accelerator engine mask based on the fuse information
 */
STATIC Cpa32U
adf_sxxxx_getAccelEnginesMask(Cpa32U fuse)
{
    /* return zero if both slices of any service are disabled */
    if( ((fuse & (1 << ICP_SXXXX_PKE_DISABLE_BIT))) ||
        ((fuse & (1 << ICP_SXXXX_ATH_DISABLE_BIT))) ||
        ((fuse & (1 << ICP_SXXXX_CPH_DISABLE_BIT))) )
        return 0;
    else
        return ( (~((fuse >> ICP_SXXXX_AE1_DISABLE_BIT) <<
                                 ICP_SXXXX_AE1_DISABLE_BIT)) &
                                     ICP_SXXXX_ACCELENGINES_MASK);
}

/*
 * getDevFuseOffset
 * Function returns the device fuse control offset
 */
STATIC Cpa32U
adf_sxxxx_getDevFuseOffset(void)
{
    return ICP_SXXXX_FUSECTL_OFFSET;
}

/*
 * getDevSKU
 * Function returns device SKU info
 */
STATIC dev_sku_info_t
adf_sxxxx_getDevSKU(Cpa32U NumAccelerators, Cpa32U NumAccelEngines, Cpa32U ClkMask,
                    Cpa32U fuse)
{

    switch (NumAccelEngines)
    {
        case 1:
            if(fuse & (1 << ICP_SXXXX_LOW_SKU_BIT))
            {
                return DEV_SKU_3;
            }
               if(fuse & (1 << ICP_SXXXX_MID_SKU_BIT))
            {
                return DEV_SKU_2;
            }
        case ICP_SXXXX_MAX_ACCELENGINES:
            return DEV_SKU_1;
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
adf_sxxxx_getNumAccelerators(adf_hw_device_data_t *self, Cpa32U mask)
{
    Cpa32U i = 0, ctr = 0;

    if (!mask)
    {
        return 0;
    }

    for (i = 0; i < ICP_SXXXX_MAX_ACCELERATORS; i++)
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
adf_sxxxx_getNumAccelEngines(adf_hw_device_data_t *self, Cpa32U mask)
{
    Cpa32U i, ctr = 0;

    if (!mask)
    {
        return 0;
    }

    for (i = 0; i < ICP_SXXXX_MAX_ACCELENGINES; i++)
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
 * Function returns the accelerator mask for the
 * accelerator in the system
 */
STATIC void
adf_sxxxx_getAccelMaskList(struct adf_hw_device_data_s *self,
                Cpa32U devAccelMask, Cpa32U devAeMask, Cpa32U *pMaskList)
{
    pMaskList[0] = devAeMask;
}

/*
 * getAccFuncOffset
 * Function returns the accelerator capabilities offset
 */
STATIC Cpa32U
adf_sxxxx_getAccFuncOffset(void)
{
    return ICP_SXXXX_FUSECTL_OFFSET;
}

/*
 * getAccFuncMask
 * Gets the accelerator functionality mask based on the accelerator capability
 */
STATIC Cpa32U
adf_sxxxx_getAccFuncMask(Cpa32U func)
{
    Cpa32U mask = 0;
    Cpa32U accFunc = 0;

    /* func is the value of the FUSECTL register */
    accFunc = func;

    mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
    mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
    mask |= ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
    mask |= ICP_ACCEL_CAPABILITIES_CIPHER;
    mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_0;

    if( (!(accFunc & (1 << ICP_SXXXX_LOW_SKU_BIT))) &&
        (!(accFunc & (1 << ICP_SXXXX_MID_SKU_BIT)))  )
        mask |= ICP_ACCEL_CAPABILITIES_CRYPTO_1;

    return mask;
}

/*
 * getMiscBarId
 * Returns the MISC BAR id of the device
 */
STATIC Cpa32U
adf_sxxxx_getMiscBarId(adf_hw_device_data_t *self)
{
    return ICP_SXXXX_PMISC_BAR;
}

/*
 * getSmiapfOffsetMask
 * Function sets the SMIAPF offset and default mask
 */
STATIC void
adf_sxxxx_getSmiapfOffsetMask(adf_hw_device_data_t *self, Cpa32U *offset, Cpa32U *mask)
{
    if(NULL != offset)
    {
        *offset = ICP_SXXXX_SMIAOF;
    }
    if(NULL != mask)
    {
        *mask   = ICP_SXXXX_SMIA_MASK;
    }
}

/*
 * getEtrBarId
 * Returns the ETR BAR id of the device
 */
STATIC Cpa32U
adf_sxxxx_getEtrBarId(adf_hw_device_data_t *self, Cpa32U *offset)
{
    if (offset)
    {
        *offset = ICP_SXXXX_ETRING_CSR_OFFSET;
    }
    return ICP_SXXXX_PETRINGCSR_BAR;
}

/*
 * isBarAvailable
 * Returns whether the barId is available on the device
 */
STATIC CpaBoolean
adf_sxxxx_isBarAvailable(adf_hw_device_data_t *self, Cpa32U barId)
{
    /* isBarAvailable shall return FALSE
    for barId 0 */
    STATIC CpaBoolean bars[] = {CPA_FALSE, CPA_TRUE, CPA_TRUE};

    return (barId < (sizeof (bars) / sizeof (*bars))) ?
        bars[barId]                                   :
        CPA_FALSE;
}

/*
 * irqGetBankNumber
 * Function returns the bank number based on interrupt source(sintpf)
 */
STATIC Cpa32U
adf_sxxxx_irqGetBankNumber(adf_hw_device_data_t *self,
            Cpa32U sintpf)
{
    return irq_get_bank_number(sintpf);
}

/*
 * irqGetAeSource
 * Function returns the AE source based on interrupt source(sintpf)
 */
STATIC Cpa32U
adf_sxxxx_irqGetAeSource(adf_hw_device_data_t *self, Cpa32U sintpf)
{
    return irq_ae_source(sintpf);
}

/*
 * init
 * Function initialise internal hw data
 */
STATIC CpaStatus
adf_sxxxx_init(adf_hw_device_data_t *self, Cpa8U node_id)
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
adf_sxxxx_cleanup(adf_hw_device_data_t *self)
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
adf_sxxxx_getBankBhHandler(Cpa32U bankId)
{
    return (bankId < NUM_ELEM(adf_sxxxx_bh_bank_handlers)) ?
        adf_sxxxx_bh_bank_handlers[bankId]                 :
        NULL;
}

/*
 * getBankBhPollingHandler
 * Function returns a polling bank handler based on bankId
 */
STATIC bank_handler
adf_sxxxx_getBankBhPollingHandler(Cpa32U bankId)
{
    return (bankId < NUM_ELEM(adf_sxxxx_bh_polling_bank_handlers)) ?
        adf_sxxxx_bh_polling_bank_handlers[bankId]                 :
        NULL;
}

/*
 * getApCsrOffsets
 * This function set the auto-push register offset based on
 * the bank id.
 */
STATIC void
adf_sxxxx_getApCsrOffsets(Cpa32U bankId, Cpa32U *nfMask, Cpa32U *nfDest,
                Cpa32U *neMask, Cpa32U *neDest, Cpa32U *apDelay)
{
    if (NULL != nfMask)
    {
        *nfMask = ICP_RING_CSR_AP_NF_MASK +
                    (bankId * AP_BANK_CSR_BYTE_OFFSET);
    }

    if (NULL != nfDest)
    {
        *nfDest = ICP_RING_CSR_AP_NF_DEST +
                    (bankId * AP_BANK_CSR_BYTE_OFFSET);
    }

    if (NULL != neMask)
    {
        *neMask = ICP_RING_CSR_AP_NE_MASK +
                    (bankId * AP_BANK_CSR_BYTE_OFFSET);
    }

    if (NULL != neDest)
    {
        *neDest = ICP_RING_CSR_AP_NE_DEST +
                    (bankId * AP_BANK_CSR_BYTE_OFFSET);
    }

    if (NULL != apDelay)
    {
        *apDelay = ICP_RING_CSR_AP_DELAY;
    }
}

/*
 * getVf2PfIntSourceOffset
 * This function returns byte offset of the VF-to-PF interrupt source CSR.
 */
STATIC Cpa32U
adf_sxxxx_getVf2PfIntSourceOffset(void)
{
    return ICP_PMISCBAR_VFTOPFINTSRC;
}

/*
 * getVf2PfBitOffset
 * This function returns the VF-to-PF bit offset in the interrupt source CSR.
 */
STATIC Cpa32U
adf_sxxxx_getVf2PfBitOffset(void)
{
    return ICP_VFTOPF_INTSOURCEOFFSET;
}

/*
 * getVf2PfIntMaskOffset
 * This function returns byte offset of the VF-to-PF interrupt mask CSR.
 */
STATIC Cpa32U
adf_sxxxx_getVf2PfIntMaskOffset(void)
{
    return ICP_PMISCBAR_VFTOPFINTMSK;
}

/*
 * getVf2PfIntMaskDefault
 * This function returns the default VF-to-PF interrupt mask.
 */
STATIC Cpa32U
adf_sxxxx_getVf2PfIntMaskDefault(void)
{
    return ICP_VFTOPF_INTSOURCEMASK;
}

/*
 * getTiMiscIntCtlOffset
 * This function returns byte offset of the miscellaneous interrupt
 * control register.
 */
STATIC Cpa32U
adf_sxxxx_getTiMiscIntCtlOffset(void)
{
    return ICP_PMISCBAR_TIMISCINTCTL;
}

/*
 * getVIntMskOffset
 * This function returns byte offset of the VF interrupt mask CSR.
 */
STATIC Cpa32U
adf_sxxxx_getVIntMskOffset(Cpa32U id)
{
    return (ICP_PMISCBAR_VINTMSK_OFFSET + (id * ICP_PMISCBAR_VFDEVOFFSET));
}

/*
 * getVIntMskDefault
 * This function returns the default VF interrupt mask.
 */
STATIC Cpa32U
adf_sxxxx_getVIntMskDefault(void)
{
    return ICP_PMISCBAR_VINTMSK_DEFAULT;
}

/*
 * getPf2VfDbIntOffset
 * This function returns byte offset of the doorbell and interrupt CSR
 */
STATIC Cpa32U
adf_sxxxx_getPf2VfDbIntOffset(Cpa32U id)
{
    return (ICP_PMISCBAR_PF2VFDBINT_OFFSET + (id * ICP_PMISCBAR_VFDEVOFFSET));
}

void
adf_sxxxx_set_hw_data(void *hw_data)
{
    register adf_hw_device_data_t *hw = NULL;

    ICP_CHECK_FOR_NULL_PARAM_VOID(hw_data);

    hw = (adf_hw_device_data_t *)hw_data;

    hw->init      = adf_sxxxx_init;
    hw->cleanup   = adf_sxxxx_cleanup;

    hw->dev_class = &sxxxx_class;

    /* populate bank/ac information */
    hw->maxBars                 = ICP_SXXXX_MAX_PCI_BARS;
    hw->maxNumBanks             = ICP_SXXXX_ETR_MAX_BANKS;
    hw->maxNumApBanks           = ICP_SXXXX_ETR_MAX_AP_BANKS;
    hw->numRingsPerBank         = ICP_SXXXX_ETR_MAX_RINGS_PER_BANK;
    hw->numBanksPerAccel        = ICP_SXXXX_BANKS_PER_ACCELERATOR;
    hw->maxNumAccel             = ICP_SXXXX_MAX_ACCELERATORS;
    hw->maxNumAccelEngines      = ICP_SXXXX_MAX_ACCELENGINES;
    hw->aeClockInMhz            = ICP_SXXXX_AE_CLOCK_IN_MHZ;
    hw->sintpfOffset            = ICP_SXXXX_SINTPF;
    hw->userEtringCsrSize       = ICP_SXXXX_USER_ETRING_CSR_SIZE;
    hw->maxNumVf                = ICP_SXXXX_MAX_NUM_VF;

    /* populate msix information */
    hw->msix.banksVectorStart   = ICP_SXXXX_MSIX_BANK_VECTOR_START;
    hw->msix.banksVectorNum     = ICP_SXXXX_MSIX_BANK_VECTOR_NUM;
    hw->msix.aeVectorStart      = ICP_SXXXX_MSIX_AE_VECTOR_START;
    hw->msix.aeVectorNum        = ICP_SXXXX_MSIX_AE_VECTOR_NUM;

    /* device fuse */
    hw->getDevFuseOffset        = adf_sxxxx_getDevFuseOffset;
    hw->getDevClkOffset         = NULL;
    hw->getDevSKU               = adf_sxxxx_getDevSKU;
    hw->getAcceleratorsMask     = adf_sxxxx_getAcceleratorsMask;
    hw->getAccelEnginesMask     = adf_sxxxx_getAccelEnginesMask;
    hw->getNumAccelerators      = adf_sxxxx_getNumAccelerators;
    hw->getNumAccelEngines      = adf_sxxxx_getNumAccelEngines;
    hw->getAccelMaskList        = adf_sxxxx_getAccelMaskList;

    /* accelerator functions */
    hw->getAccFuncOffset        = adf_sxxxx_getAccFuncOffset;
    hw->getAccFuncMask          = adf_sxxxx_getAccFuncMask;

    /* populate bars callbacks  */
    hw->getEtrBarId             = adf_sxxxx_getEtrBarId;
    hw->getSramBarId            = NULL;
    hw->getMiscBarId            = adf_sxxxx_getMiscBarId;
    hw->isBarAvailable          = adf_sxxxx_isBarAvailable;

    /* populate esram callbacks    */
    hw->getEsramInfo            = NULL;

    /* populate SHAC callback */
    hw->getScratchRamInfo       = NULL;

    /* populate PMiscBar callback  */
    hw->getSmiapfOffsetMask     = adf_sxxxx_getSmiapfOffsetMask;

    hw->irqGetBankNumber        = adf_sxxxx_irqGetBankNumber;
    hw->irqGetAeSource          = adf_sxxxx_irqGetAeSource;

    hw->getBankBhHandler        = adf_sxxxx_getBankBhHandler;
    hw->getBankBhPollingHandler = adf_sxxxx_getBankBhPollingHandler;

    /* Auto-push feature callback */
    hw->isAutoPushSupported     = CPA_TRUE;
    hw->getApCsrOffsets         = adf_sxxxx_getApCsrOffsets;

    hw->getVf2PfIntSourceOffset = adf_sxxxx_getVf2PfIntSourceOffset;
    hw->getVf2PfBitOffset       = adf_sxxxx_getVf2PfBitOffset;
    hw->getVf2PfIntMaskOffset   = adf_sxxxx_getVf2PfIntMaskOffset;
    hw->getVf2PfIntMaskDefault  = adf_sxxxx_getVf2PfIntMaskDefault;

    hw->getTiMiscIntCtlOffset   = adf_sxxxx_getTiMiscIntCtlOffset;
    hw->getVIntMskOffset        = adf_sxxxx_getVIntMskOffset;
    hw->getVIntMskDefault       = adf_sxxxx_getVIntMskDefault;
    hw->getPf2VfDbIntOffset     = adf_sxxxx_getPf2VfDbIntOffset;
    return;
}
