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
 * @file adf_acceldev_isr.c
 *
 * @description
 *    This file contains ADF code for ISR services to upper layers
 *
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "cpa.h"
#include "icp_adf_init.h"
#include "icp_accel_devices.h"
#include "adf_drv.h"
#include "icp_platform.h"
#include "adf_transport_ctrl.h"
#include "adf_ETring_mgr.h"
#include "adf_isr.h"
#include "adf_bh.h"
#include "icp_firml_interface.h"
#include "adf_ae.h"
#include "adf_cfg.h"
#include "adf_drv.h"
#include "adf_platform_dh89xxcc.h"

/*
 * SSM error type
 * List the various SSM based Uncorrectable Errors
 */
#define SHAREDMEM 0 /*Shared Memory UErrors*/
#define MMP       1 /*MMP ECC UErrors*/
#define PPERR     2 /*Push/Pull UErrors*/
#define SLICE0    0 /*MMP Slice 0*/
#define SLICE1    1 /*MMP Slice 1*/
/*
 * List all the AEs
 */
#define AE0       0
#define AE1       1
#define AE2       2
#define AE3       3
#define AE4       4
#define AE5       5
#define AE6       6
#define AE7       7
#define MAX_NB_AE 8

/*
 * Define reset Mode
 */
#define RESET_ASYNC 0

/*
 * adf_isr_logAE_Int
 * Log Me based UErrors
 */
STATIC inline void
adf_isr_LogAE_Int(uint8_t index, void *pmisc_bar_addr)
{
    uint32_t ustore = ICP_ADF_CSR_RD(pmisc_bar_addr,
            ICP_DH89xxCC_USTORE_ERROR_STATUS(index));
    uint32_t reg_error = ICP_ADF_CSR_RD(pmisc_bar_addr,
            ICP_DH89xxCC_REG_ERROR_STATUS(index));
    uint32_t eperrlog = ICP_ADF_CSR_RD(pmisc_bar_addr,
            ICP_DH89xxCC_EPERRLOG);
    uint32_t ctx_enables = ICP_ADF_CSR_RD(pmisc_bar_addr,
            PMISCBAR_CTX_ENABLES(index));

    ADF_DEBUG("ustore 0x%X, reg_error 0x%X, \n"
            "eperrlog 0x%X ctx_enables 0x%X\n",
            ustore, reg_error, eperrlog, ctx_enables);
    ADF_ERROR("AE #%x Error\n"
            "Uncorrectable Error: %s\n"
            "Control Store Error: %s\n"
            "Control Store information: Address 0x%X - syndrom 0x%X\n"
            "Register ECC Error %s - Parity Error: %s\n"
            "Register information: address 0x%X - type %s\n",
            index,
            ICP_DH89xxCC_EPERRLOG_ERR(
                ICP_DH89xxCC_GET_EPERRLOG(eperrlog, index)),
            ICP_DH89xxCC_USTORE_IS_UE(ustore),
            ICP_DH89xxCC_USTORE_UADDR(ustore),
            ICP_DH89xxCC_USTORE_GET_SYN(ustore),
            ICP_DH89xxCC_CTX_ECC_ERR(ctx_enables),
            ICP_DH89xxCC_CTX_PAR_ERR(ctx_enables),
            ICP_DH89xxCC_REG_GET_REG_AD(reg_error),
            ICP_DH89xxCC_REG_TYP_STR(ICP_DH89xxCC_REG_TYPE(reg_error)));
}

/*
 * adf_isr_logSSM_Int
 * Log SSM based Uerrors (shared Mem, MMP and Push/Pull)
 */
STATIC inline void
adf_isr_LogSSM_Int(void *pmisc_bar_addr,
        uint8_t cpm, uint8_t extra, uint8_t type)
{
    uint32_t uerrssm, uerrssmad;
    uint32_t uerrssm_off, uerrssmad_off;

    if (PPERR == type) {
        uint32_t pperr, pperrid;
        if (0 == cpm) {
            pperr = ICP_ADF_CSR_RD(pmisc_bar_addr,
                    ICP_DH89xxCC_PPERR0);
            pperrid = ICP_ADF_CSR_RD(pmisc_bar_addr,
                    ICP_DH89xxCC_PPERRID0);
        }
        else {
            pperr = ICP_ADF_CSR_RD(pmisc_bar_addr,
                    ICP_DH89xxCC_PPERR1);
            pperrid = ICP_ADF_CSR_RD(pmisc_bar_addr,
                    ICP_DH89xxCC_PPERRID1);
        }
        ADF_DEBUG("pperr 0x%X pperrid 0x%X\n", pperr, pperrid);
        ADF_ERROR("Push/Pull Error on CPM %i\n"
                "%s - Error type: %s\n"
                "id 0x%X\n",
                cpm, ICP_DH89xxCC_PPERR_GET_STATUS(pperr),
                ICP_DH89xxCC_PPERR_TYPE_STR(
                    ICP_DH89xxCC_PPERR_TYPE(pperr)),
                pperrid);
        return;
    }
    else if (SHAREDMEM == type) {
        if (cpm == 0) {
            uerrssm_off = ICP_DH89xxCC_UERRSSMSH0;
            uerrssmad_off = ICP_DH89xxCC_UERRSSMSHAD0;
        }
        else {
            uerrssm_off = ICP_DH89xxCC_UERRSSMSH1;
            uerrssmad_off = ICP_DH89xxCC_UERRSSMSHAD1;
        }
    }
    else {
        /*MMP*/
        int cse = (cpm << 1) | extra;
        switch(cse) {
        case 3:
            {
                uerrssm_off = ICP_DH89xxCC_UERRSSMMMP11;
                uerrssmad_off = ICP_DH89xxCC_UERRSSMMMPAD11;
            }
        case 2:
            {
                uerrssm_off = ICP_DH89xxCC_UERRSSMMMP10;
                uerrssmad_off = ICP_DH89xxCC_UERRSSMMMPAD10;
            }
        case 1:
            {
                uerrssm_off = ICP_DH89xxCC_UERRSSMMMP01;
                uerrssmad_off = ICP_DH89xxCC_UERRSSMMMPAD01;
            }
        case 0:
        default:
            {
                uerrssm_off = ICP_DH89xxCC_UERRSSMMMP00;
                uerrssmad_off = ICP_DH89xxCC_UERRSSMMMPAD00;
            }
        }
    }

    uerrssm = ICP_ADF_CSR_RD(pmisc_bar_addr,
            uerrssm_off);
    uerrssmad = ICP_ADF_CSR_RD(pmisc_bar_addr,
            uerrssmad_off);

    if (MMP == type) {
        /*MMP*/
        ADF_DEBUG("uerrssmmmp 0x%X, uerrssmmmpad 0x%X\n",
                uerrssm, uerrssmad);
        ADF_ERROR("MMP %i CPM %i - Uncorrectable Error: %s\n"
                "Operation: %s -Address: 0x%X\n",
                extra, cpm,
                (((uerrssm & ICP_DH89xxCC_UERRSSMMMP_UERR))!= 0 ? "Yes": "No"),
                ICP_DH89xxCC_UERRSSMMMP_ERRTYPE_STR(
                    ICP_DH89xxCC_UERRSSMMMP_ERRTYPE(uerrssm)),
                (uerrssmad & ICP_DH89xxCC_UERRSSMMMPAD_ADDR));
    }
    else {
        ADF_DEBUG("uerrssmsh 0x%X, uerrssmshad 0x%X\n",
                uerrssm, uerrssmad);
        ADF_ERROR("SharedMem Error CPM %x - Uncorrectable Error: %s\n"
                "Operation: %s - Type: %s - Address: 0x%X\n",
                cpm,
                (uerrssm & ICP_DH89xxCC_UERRSSMSH_UERR) != 0? "Yes": "No",
                ICP_DH89xxCC_UERRSSMSH_GET_OP(uerrssm),
                ICP_DH89xxCC_UERRSSMSH_GET_ERRTYPE(
                    ICP_DH89xxCC_UERRSSMSH_ERRTYPE(uerrssm)),
                (uerrssmad & ICP_DH89xxCC_UERRSSMSHAD_ADDR));
    }
}

/*
 * adf_isr_handleAEInterrrupt
 * Handle Ae interrupt sources: Uncorrectable errors and firmware custom
 */
STATIC inline irqreturn_t
adf_isr_handleAEInterrupt(icp_accel_dev_t *accel_dev)
{
        void *pmisc_bar_addr = adf_isr_getMiscBarAd(accel_dev);

        adf_fw_loader_handle_t* ldr_handle = NULL;

        uint32_t errsou0 = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_ERRSOU0);
        uint32_t errsou1 = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_ERRSOU1);
        uint32_t errsou3 = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_ERRSOU3);

        int status = ICP_FIRMLOADER_FAIL;

        bool reset_needed = false;

        /*UERR interrupts ?*/
        if ((0 != errsou0) || (0 != errsou1) || (0 != errsou3)) {
            ADF_DEBUG("UERR interrupt occurred");
            /*Identify the interrupt Sources and log them*/
            /*AE0-3 Errors*/
            if (0 != errsou0) {
                if (errsou0 & ICP_DH89xxCC_M0UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE0, pmisc_bar_addr);
                }
                if (errsou0 & ICP_DH89xxCC_M1UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE1, pmisc_bar_addr);
                }
                if (errsou0 & ICP_DH89xxCC_M2UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE2, pmisc_bar_addr);
                }
                if (errsou0 & ICP_DH89xxCC_M3UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE3, pmisc_bar_addr);
                }
            }
            /*AE4-7 Errors*/
            if (0 != errsou1) {
                if (errsou1 & ICP_DH89xxCC_M4UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE4, pmisc_bar_addr);
                }
                if (errsou1 & ICP_DH89xxCC_M5UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE5, pmisc_bar_addr);
                }
                if (errsou1 & ICP_DH89xxCC_M6UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE6, pmisc_bar_addr);
                }
                if (errsou1 & ICP_DH89xxCC_M7UNCOR_MASK) {
                   reset_needed = true;
                   adf_isr_LogAE_Int(AE7, pmisc_bar_addr);
                }
            }
            if (0 != errsou3) {
                /*eSRAM errors*/
                if (errsou3 & ICP_DH89xxCC_UERR_MASK) {
                    uint32_t  esramuerr = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_ESRAMUERR);
                    uint32_t  esramuerrad = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_ESRAMUERRAD);
                    reset_needed = true;
                    ADF_DEBUG("esramuerr 0x%X - esramuerrad 0x%X\n",
                            esramuerr, esramuerrad);
                    ADF_ERROR("Uncorrectable Error Occurred on eSram\n"
                            "Interrupt triggered: %s\n"
                            "Operation type %s - Error Type %s\n"
                            "Address (quad words) 0x%X\n",
                             ICP_DH89xxCC_ESRAMUERR_GET_UERR(esramuerr),
                             ICP_DH89xxCC_ESRAMUERR_GET_OP(esramuerr),
                             ICP_DH89xxCC_ESRAMUERR_GET_ERRSTR(
                                 ICP_DH89xxCC_ESRAMUERR_GET_ERRTYPE(esramuerr)),
                             esramuerrad);
                }
                /*CPM: MMP, SH, PUSH/PULL errors*/
                if ((errsou3 & ICP_DH89xxCC_EMSK3_CPM0_MASK) ||
                        (errsou3 & ICP_DH89xxCC_EMSK3_CPM1_MASK)) {
                    uint8_t cpm;
                    uint32_t instatssm;
                    if ((errsou3 & ICP_DH89xxCC_EMSK3_CPM0_MASK)) {
                        cpm = 0;
                        instatssm = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_INTSTATSSM0);
                    }
                    else {
                        cpm = 1;
                        instatssm = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_INTSTATSSM1);
                    }
                    ADF_ERROR("Uncorrectable Error Occurred on CPM %i\n", cpm);
                    ADF_DEBUG("instatssm 0x%X\n", instatssm);
                    if (instatssm & ICP_DH89xxCC_INTSTATSSM_MMP1) {
                        reset_needed = true;
                        adf_isr_LogSSM_Int(pmisc_bar_addr, cpm, SLICE1,
                                MMP);
                    }
                    if (instatssm & ICP_DH89xxCC_INTSTATSSM_MMP0) {
                        reset_needed = true;
                        adf_isr_LogSSM_Int(pmisc_bar_addr, cpm, SLICE0,
                                MMP);
                    }
                    if (instatssm & ICP_DH89xxCC_INTSTATSSM_SH) {
                        reset_needed = true;
                        adf_isr_LogSSM_Int(pmisc_bar_addr, cpm,
                                SLICE0/*not used*/, SHAREDMEM);
                    }
                    if (instatssm & ICP_DH89xxCC_INTSTATSSM_PPERR) {
                        reset_needed = true;
                        adf_isr_LogSSM_Int(pmisc_bar_addr, cpm,
                                SLICE0/*not used*/, PPERR);
                    }
                }
                /*Or'ed Shac Data Error*/
                if (errsou3 & ICP_DH89xxCC_EMSK3_SHaC0_MASK) {
                    uint32_t err_status = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_CPP_SHAC_ERR_STATUS);
                    uint32_t err_ppid = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_CPP_SHAC_ERR_PPID);
                    reset_needed = true;
                    /*Log error*/
                    ADF_DEBUG("ccp_shac_err_status 0x%X -"
                            "cpp_shac_err_ppid 0x%X\n", err_status, err_ppid);
                    ADF_ERROR("Uncorrectable Data Error Occurred in SHaC\n"
                            "Operation type: %s, Error target %s\n"
                            "Error ppid 0x%X\nInterrupt Triggered %s\n",
                            ICP_DH89xxCC_CPP_SHAC_GET_TYP(err_status) == 0 ?
                            "Push": "Pull",
                            ICP_DH89xxCC_CPP_SHAC_ERTYP_STR(
                            ICP_DH89xxCC_CPP_SHAC_GET_ERTYP(err_status)),
                            err_ppid,
                            ICP_DH89xxCC_CPP_SHAC_GET_INT(err_status) == 1 ?
                            "Yes": "No");
                }
                /*Push/Pull Misc*/
                if (errsou3 & ICP_DH89xxCC_PPMISCERR_MASK) {
                    uint32_t cpptgterr = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_CPPMEMTGTERR);
                    uint32_t errppid = ICP_ADF_CSR_RD(pmisc_bar_addr,
                        ICP_DH89xxCC_ERRPPID);
                    uint32_t ticppintsts = ICP_ADF_CSR_RD(pmisc_bar_addr,
                            ICP_DH89xxCC_TICPPINTSTS);
                    uint32_t tierrid;
                    if(ticppintsts & ICP_DH89xxCC_TICPP_PUSH) {
                        tierrid = ICP_ADF_CSR_RD(pmisc_bar_addr,
                                ICP_DH89xxCC_TIERRPUSHID);
                    }
                    else {
                        tierrid = ICP_ADF_CSR_RD(pmisc_bar_addr,
                                ICP_DH89xxCC_TIERRPULLID);
                    }
                    reset_needed = true;
                    ADF_DEBUG("cpptgterr 0x%X, errppid 0x%X, ticppintsts 0%X,"
                            "tierrxxxid 0x%X\n", cpptgterr, errppid,
                            ticppintsts, tierrid);
                    ADF_ERROR("Uncorrectable Push/Pull Misc Error\n"
                            "memory status: %s - Transaction Id 0x%X - "
                            "Error type %s\n"
                            "Bus Operation Type %s - Id 0x%X\n",
                            ICP_DH89xxCC_CPPMEMTGTERR_GET_STATUS(cpptgterr),
                            errppid,
                            ICP_DH89xxCC_CPPMEMTGTERR_ERRTYP_STR(
                                ICP_DH89xxCC_CPPMEMTGTERR_ERRTYP(cpptgterr)),
                            ICP_DH89xxCC_TICPP_GETOP(ticppintsts), tierrid);
                }
            } /* ERRSOU3*/

            /*Reset the device - async mode*/
            if(reset_needed)
            {
                ADF_DEBUG("Reset Scheduled\n");
                adf_aer_schedule_reset_dev(accel_dev,
                        RESET_ASYNC);
                return IRQ_HANDLED;
            }
        }
        /*Non UERR source, Call tools handle */
        ldr_handle = (adf_fw_loader_handle_t*) accel_dev->pUcodePrivData;

        if (NULL != ldr_handle->firmLoaderHandle) {
            status =
                icp_FirmLoader_ISR((void *)ldr_handle->firmLoaderHandle);
        }
        if (ICP_FIRMLOADER_SUCCESS == status) {
            return IRQ_HANDLED;
        }
        return IRQ_NONE;
}

/*
 * adf_isr_get_value
 * Function to retrieve the value of a configuration parameter key
 */
STATIC int adf_isr_get_value(icp_accel_dev_t *accel_dev,
                                char *section, char *key, int *value)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};

    status = icp_adf_cfgGetParamValue(accel_dev, section, key, val);
    if (CPA_STATUS_SUCCESS != status) {
            return FAIL;
    }
    *value = ICP_STRTOUL(val, NULL, ADF_CFG_BASE_DEC);
    return SUCCESS;
}

/*
 * adf_isr_get_intr_mode
 * Function to retrieve interrupt mode from the configuration file
 *
 * If no interrupt mode is set in the configuration, MSI-X will be
 * used as a default interrupt mode.
 */
STATIC void adf_isr_get_intr_mode(icp_accel_dev_t *accel_dev,
                                    adf_isr_mode_t *mode)
{
    int status = SUCCESS;
    int isr_value = 0;

    /* MSIX has the highest priority */
    status = adf_isr_get_value(accel_dev,
                    GENERAL_SEC, ADF_ISR_MSIX_KEY, &isr_value);
    if ((SUCCESS == status) && (isr_value == 1)) {
            ADF_DEBUG("%s is set\n", ADF_ISR_MSIX_KEY);
            *mode = adf_isr_mode_msix;
            return;
    }
    status = adf_isr_get_value(accel_dev,
                    GENERAL_SEC, ADF_ISR_MSI_KEY, &isr_value);
    if ((SUCCESS == status) && (isr_value == 1)) {
            ADF_DEBUG("%s is set\n", ADF_ISR_MSI_KEY);
            *mode = adf_isr_mode_msi;
            return;
    }
    status = adf_isr_get_value(accel_dev,
                    GENERAL_SEC, ADF_ISR_INTX_KEY, &isr_value);
    if ((SUCCESS == status) && (isr_value == 1)) {
            ADF_DEBUG("%s is set\n", ADF_ISR_INTX_KEY);
            *mode = adf_isr_mode_intx;
            return;
    }
    /* In case no mode is set */
    ADF_DEBUG("Interrupt Mode undefined, defaulting to MSIX.\n");
    *mode = adf_isr_mode_msix;
}


/*
 * adf_enable_msix
 * Function enables MSIX capability
 */
STATIC int adf_enable_msix(icp_accel_dev_t *accel_dev)
{
        adf_hw_device_data_t *hw_data = NULL;
        icp_accel_pci_info_t *pci_dev_info = NULL;
        int stat = SUCCESS;
        int index = 0;
        int vector = 0;
        Cpa32U msix_num_entries = 0;
        Cpa32U lastVector = 0;

        if (NULL == accel_dev) {
                ADF_ERROR("invalid param accel_dev\n");
                return FAIL;
        }

        if (NULL == accel_dev->pHwDeviceData) {
                ADF_ERROR("pHwDeviceData is null\n");
                return FAIL;
        }
        hw_data = accel_dev->pHwDeviceData;
        pci_dev_info = &accel_dev->pciAccelDev;
        if (adf_isr_mode_msix != pci_dev_info->isr_mode) {
            return FAIL;
        }

        msix_num_entries = hw_data->msix.banksVectorNum +
            hw_data->msix.aeVectorNum;
        /* create bundle entries */
        lastVector =
            hw_data->msix.banksVectorStart + hw_data->msix.banksVectorNum;

        for (vector = hw_data->msix.banksVectorStart, index = 0;
             vector < lastVector; ++vector, ++index) {
                pci_dev_info->msixEntries.value[index].entry = index;
        }

        /* create AE Cluster entry */
        lastVector =
            hw_data->msix.aeVectorStart + hw_data->msix.aeVectorNum;

        for (vector = hw_data->msix.aeVectorStart;
             vector < lastVector; ++vector, ++index) {
                pci_dev_info->msixEntries.value[index].entry = index;
        }

        stat = pci_enable_msix(pci_dev_info->pDev,
                               pci_dev_info->msixEntries.value,
                               msix_num_entries);
        if (SUCCESS != stat){
                ADF_ERROR("Unable to enable MSIX\n");
                pci_dev_info->isr_mode = adf_isr_mode_msi;
        }
        else {
                ADF_DEBUG("MSIX capability enabled\n");
                pci_dev_info->isr_mode = adf_isr_mode_msix;
        }
        return stat;
}

/*
 * adf_disable_msix
 * Function disables MSIX capability
 */
STATIC void adf_disable_msix(icp_accel_pci_info_t *pci_dev_info)
{
        pci_dev_info->isr_mode = adf_isr_mode_intx;
        ADF_DEBUG("Disabling MSIX capability\n");
        pci_disable_msix(pci_dev_info->pDev);
}

/*
 * adf_enable_msi
 * Function enables MSI capability
 */
STATIC int adf_enable_msi(icp_accel_pci_info_t *pci_dev_info)
{
        int stat = SUCCESS;

        if (adf_isr_mode_msi != pci_dev_info->isr_mode) {
            return FAIL;
        }

        stat = pci_enable_msi(pci_dev_info->pDev);
        if (SUCCESS != stat){
                ADF_ERROR("Unable to enable MSI\n");
                pci_dev_info->isr_mode = adf_isr_mode_intx;
        }
        else {
                ADF_DEBUG("MSI capability enabled\n");
                pci_dev_info->isr_mode = adf_isr_mode_msi;
                pci_dev_info->irq = pci_dev_info->pDev->irq;
        }
        return stat;
}

/*
 * adf_disable_msi
 * Function disables MSI capability
 */
STATIC void adf_disable_msi(icp_accel_pci_info_t *pci_dev_info)
{
        pci_dev_info->isr_mode = adf_isr_mode_intx;
        ADF_DEBUG("Disabling MSI capability\n");
        pci_disable_msi(pci_dev_info->pDev);
        pci_dev_info->irq = pci_dev_info->pDev->irq;
}

/*
 * adf_msix_isr_bundle
 * Top Half ISR for MSIX
 */
STATIC irqreturn_t adf_msix_isr_bundle(int irq, void *privdata)
{
        icp_accel_dev_t *accel_dev = NULL;
        icp_etr_priv_data_t* priv_ring_data = NULL;
        int status = 0, i = 0;
        unsigned long* csr_base_addr = 0;
        icp_et_ring_bank_data_t *bank_data = NULL;

        /* Read the ring status CSR to determine which rings have data */
        accel_dev = (icp_accel_dev_t *) privdata;
        priv_ring_data = (icp_etr_priv_data_t*) accel_dev->pCommsHandle;
        csr_base_addr = (unsigned long*)(UARCH_INT)
                                 priv_ring_data->csrBaseAddress;

        for (i = 0; i < accel_dev->pHwDeviceData->msix.banksVectorNum; i++) {
                if (accel_dev->pciAccelDev.msixEntries.value[i].vector == irq) {
                        bank_data = &(priv_ring_data->banks[i]);
                        if (!bank_data->timedCoalescEnabled ||
                                           bank_data->numberMsgCoalescEnabled) {
                                WRITE_CSR_INT_EN(i, 0);
                        }
                        if (bank_data->timedCoalescEnabled) {
                                WRITE_CSR_INT_COL_CTL_CLR(i);
                        }
                        tasklet_hi_schedule(bank_data->ETR_bank_tasklet);
                        status = 1;
                }
        }
        if (status == TRUE) {
                return IRQ_HANDLED;
        }
        return IRQ_NONE;
}

/*
 *  adf_msix_isr_ae
 *   Top Half ISR
 *  Handles VF doorbell and AE Cluster IRQs
 *  Calls the Firmware Loader TopHalf handler.
 */
STATIC irqreturn_t adf_msix_isr_ae(int irq, void *privdata)
{
        icp_accel_dev_t *accel_dev = NULL;
        int status = ICP_FIRMLOADER_FAIL, i = 0;

        accel_dev = (icp_accel_dev_t *) privdata;

        /*
         * If virtualization is enabled then need to check the
         * source of the IRQ and in case it is VF2PF handle it
         */
        if (accel_dev->virtualization.enabled) {
                uint32_t mask = 0, reg = 0;
                adf_hw_device_data_t  *hw_data = NULL;
                uint32_t *pmisc_bar_addr = NULL;
                Cpa32U pmisc_bar_id = 0;
                Cpa32U vf2pfDefMask = 0;
                Cpa32U vf2pfBitOffset = 0;

                hw_data = accel_dev->pHwDeviceData;

                pmisc_bar_id = hw_data->getMiscBarId(hw_data);

                pmisc_bar_addr = (void*)
                    accel_dev->pciAccelDev.pciBars[pmisc_bar_id].virtAddr;

                reg = ICP_ADF_CSR_RD(pmisc_bar_addr,
                        hw_data->getVf2PfIntSourceOffset());

                /* write to PF2VF register to clean IRQ */
                vf2pfBitOffset = hw_data->getVf2PfBitOffset();
                vf2pfDefMask = hw_data->getVf2PfIntMaskDefault();
                ICP_ADF_CSR_WR(pmisc_bar_addr,
                        hw_data->getVf2PfIntMaskOffset(), vf2pfDefMask);
                mask = reg & vf2pfDefMask;
                mask >>= vf2pfBitOffset;

                if (mask) {
                        icp_accel_pci_vf_info_t *vf;
                        /* Handle IRQ from VF */
                        for (i = 0; i < hw_data->maxNumVf; i++) {
                                if (mask & (1 << i)) {
                                        vf = accel_dev->pci_vfs[i];
                                        tasklet_hi_schedule(vf->tasklet_vf);
                                        status = SUCCESS;
                                }
                        }
                        if (SUCCESS == status) {
                                return IRQ_HANDLED;
                        }
                }
        }

        /* If it wasn't VF2PF then it is AE.*/
        return adf_isr_handleAEInterrupt(accel_dev);
}

/*
 * adf_isr
 * Top Half ISR for MSI and legacy INTX mode
 */
STATIC irqreturn_t adf_isr(int irq, void *privdata)
{
        icp_accel_dev_t *accel_dev = NULL;
        icp_etr_priv_data_t* priv_ring_data = NULL;
        uint32_t irq_source = 0;
        int status = 0, i = 0;
        unsigned long* csr_base_addr = 0;
        uint32_t s_int = 0;
        uint32_t *pmisc_addr = NULL;
        icp_et_ring_bank_data_t *bank_data = NULL;
        adf_hw_device_data_t   *hw_data = NULL;

        accel_dev = (icp_accel_dev_t *) privdata;
        hw_data = accel_dev->pHwDeviceData;
        priv_ring_data = (icp_etr_priv_data_t*) accel_dev->pCommsHandle;
        csr_base_addr = (unsigned long*)(UARCH_INT)
                                 priv_ring_data->csrBaseAddress;

        /*
         * Perform top-half processing.
         * Check SINT register
         */
        /* If one of our response rings has data to be processed */
        pmisc_addr = (void*)accel_dev->pciAccelDev.
                            pciBars[hw_data->getMiscBarId(hw_data)].virtAddr;
        /* Read the ring status CSR to determine which rings have data */
        s_int = ICP_ADF_CSR_RD(pmisc_addr, hw_data->sintpfOffset);

        /* Check bundle interrupt */
        irq_source = hw_data->irqGetBankNumber(hw_data, s_int);
        if (0 != irq_source)
        {
                for (i = 0; i < hw_data->maxNumBanks; i++) {
                        if (irq_source & (1 << i)) {
                                bank_data = &(priv_ring_data->banks[i]);

                                if (!bank_data->timedCoalescEnabled ||
                                           bank_data->numberMsgCoalescEnabled) {
                                        WRITE_CSR_INT_EN(i, 0);
                                }
                                if (bank_data->timedCoalescEnabled) {
                                        WRITE_CSR_INT_COL_CTL_CLR(i);
                                }
                                tasklet_hi_schedule(
                                        bank_data->ETR_bank_tasklet);
                                status = TRUE;
                        }
                }
        }
        /* Check miscellaneous interrupt */
        irq_source = hw_data->irqGetAeSource(hw_data, s_int);
        if (irq_source) {
            return adf_msix_isr_ae(irq, privdata);
        }

        if (status == TRUE) {
                return IRQ_HANDLED;
        }

        return IRQ_NONE;
}

/*
 * adf_isr_free_msix_entry_table
 * Free buffer for MSIX entry value and name
 */
STATIC void adf_isr_free_msix_entry_table(icp_accel_dev_t *accel_dev)
{
        if (NULL == accel_dev) {
                ADF_ERROR("invalid param accel_dev\n");
                return;
        }
        /* release memory allocates for msixEntries */
        if (NULL != accel_dev->pciAccelDev.msixEntries.value) {
            ICP_FREE_NUMA(accel_dev->pciAccelDev.msixEntries.value);
        }
        if (NULL != accel_dev->pciAccelDev.msixEntries.name) {
            ICP_FREE_NUMA(accel_dev->pciAccelDev.msixEntries.name);
        }
}

/*
 * adf_isr_resource_free
 * Free up the required resources
 */
int adf_isr_resource_free(icp_accel_dev_t *accel_dev)
{
        int status = SUCCESS, i = 0;
        adf_hw_device_data_t *hw_data = NULL;
        Cpa32U msixEntries = 0;

        hw_data = accel_dev->pHwDeviceData;
        msixEntries = hw_data->msix.banksVectorNum +
            hw_data->msix.aeVectorNum;

        if (accel_dev->pciAccelDev.isr_mode == adf_isr_mode_msix) {
                struct msix_entry *msixe =
                           accel_dev->pciAccelDev.msixEntries.value;

                for(i = 0; i < msixEntries; i++) {
                        free_irq(msixe[i].vector, (void*) accel_dev);
                }

                ssleep(1);
                adf_disable_msix(&accel_dev->pciAccelDev);
        }
        else {
                free_irq(accel_dev->pciAccelDev.irq, (void*) accel_dev);
                if (accel_dev->pciAccelDev.isr_mode == adf_isr_mode_msi) {
                        adf_disable_msi(&accel_dev->pciAccelDev);
                }
        }

        /* Clean bottom half handlers */
        adf_cleanup_bh(accel_dev);
        /* Free MSIX entry table */
        adf_isr_free_msix_entry_table(accel_dev);
        return status;
}

/*
 * adf_request_msix_irq
 * Request IRQ resources for all the MSIX_ENTRIES
 */
STATIC int adf_request_msix_irq(icp_accel_dev_t *accel_dev)
{
        int status = SUCCESS, i = 0;
        char *name = NULL;
        Cpa32U msixBundleEntries = 0;
        adf_hw_device_data_t *hw_data = NULL;
        icp_accel_pci_info_t *pci_dev_info = &accel_dev->pciAccelDev;
        struct msix_entry *msixe = pci_dev_info->msixEntries.value;

        hw_data = accel_dev->pHwDeviceData;
        msixBundleEntries = hw_data->msix.banksVectorNum;


        /* Request msix irq for bundles                  */
        for(i = 0; i < msixBundleEntries; i++)
        {
                name = pci_dev_info->msixEntries.name[i];

                snprintf(name, MAX_MSIX_DEVICE_NAME, "dev%dBundle%d",
                        accel_dev->accelId, i);
                status = request_irq(msixe[i].vector,
                        adf_msix_isr_bundle, 0, name,
                        (void *)accel_dev);

                if (status) {
                    ADF_ERROR(
                            "failed request_irq IRQ %d for %s status=%d\n",
                            msixe[i].vector, name, status);
                    return status;
                }
        }

        /* Request msix irq for AE Cluster */
        name = pci_dev_info->msixEntries.name[i];
        snprintf(name, MAX_MSIX_DEVICE_NAME, "dev%dAECluster",
                accel_dev->accelId);
        status = request_irq(msixe[i].vector,
                adf_msix_isr_ae, 0, name,
                (void *)accel_dev);

        if (status) {
            ADF_ERROR(
                    "failed IRQ %d, for %s status=%d\n",
                    msixe[i].vector, name, status);
            return status;
        }
        return status;
}

/*
 * adf_isr_alloc_msix_entry_table
 * Allocate buffer for MSIX entry value and name
 */
int adf_isr_alloc_msix_entry_table(icp_accel_dev_t *accel_dev)
{
        adf_hw_device_data_t *hw_data = NULL;
        Cpa32U msix_num_entries = 0;
        struct msix_entry *entries = NULL;
        void *name = NULL;

        if (NULL == accel_dev) {
                ADF_ERROR("invalid param accel_dev\n");
                return FAIL;
        }

        if (NULL == accel_dev->pHwDeviceData) {
                ADF_ERROR("accel_dev->pHwDeviceData is null\n");
                return FAIL;
        }

        hw_data = accel_dev->pHwDeviceData;
        /* Allocate memory for msix entries in pci info structure */
        msix_num_entries = hw_data->msix.banksVectorNum +
            hw_data->msix.aeVectorNum;

        entries = ICP_MALLOC_GEN_NUMA(
                msix_num_entries * sizeof (struct msix_entry),
                accel_dev->pkg_id);
        if (NULL == entries) {
            ADF_ERROR("Failed to allocate memory\n");
            return FAIL;
        }
        ICP_MEMSET(entries, '\0',
                msix_num_entries * sizeof (struct msix_entry));

        accel_dev->pciAccelDev.msixEntries.value = entries;

        name = ICP_MALLOC_GEN_NUMA(
                msix_num_entries * sizeof (Cpa8U) * MAX_MSIX_DEVICE_NAME,
                accel_dev->pkg_id);
        ICP_MEMSET(name, '\0',
                msix_num_entries * sizeof (Cpa8U) * MAX_MSIX_DEVICE_NAME);

        accel_dev->pciAccelDev.msixEntries.name= name;

        return SUCCESS;
}

/*
 * adf_isr_resource_alloc
 * Allocate the required ISR services.
 */
int adf_isr_resource_alloc(icp_accel_dev_t *accel_dev)
{
        int status = SUCCESS, flags = 0;

        status = adf_isr_alloc_msix_entry_table(accel_dev);
        if (status) {
            ADF_ERROR("adf_isr_alloc_msix_entry_table failed\n");
            return status;
        }
        /* setup bottom half handlers */
        adf_setup_bh(accel_dev);
        /* get the interrupt mode */
        adf_isr_get_intr_mode(accel_dev, &(accel_dev->pciAccelDev.isr_mode));
        /*
         * Setup the structures in accel_dev
         */
        if (accel_dev->pciAccelDev.irq) {
                /* Try to enable MSIX or MSI mode */
                status = adf_enable_msix(accel_dev);
                if (SUCCESS != status) {
                        status = adf_enable_msi(&accel_dev->pciAccelDev);
                }
                /* We only need to set the SHARED flag for INTX mode */
                flags =
                  (accel_dev->pciAccelDev.isr_mode == adf_isr_mode_intx)
                  ? IRQF_SHARED : 0;

                if (accel_dev->pciAccelDev.isr_mode == adf_isr_mode_msix) {
                        /* Managed to enable MSIX.
                         * Now request an IRQ for each entry */
                        status = adf_request_msix_irq(accel_dev);
                        if (status) {
                                goto err_failure;
                        }
                }
                else {
                    /* Need to handle IRQ in the old way */
                    status = request_irq(accel_dev->pciAccelDev.irq, adf_isr,
                                    flags, adf_driver_name, (void *)accel_dev);
                }
                if (status) {
                        ADF_ERROR("failed request_irq %d, status=%d\n",
                                  accel_dev->pciAccelDev.irq, status);
                        goto err_failure;
                }
        }
        return SUCCESS;

err_failure:
        adf_isr_resource_free(accel_dev);
        return status;
}

/*
 * adf_enable_bundle_msix
 * Enable single MSIX for a bundle while VF goes down.
 */
int adf_enable_bundle_msix(icp_accel_dev_t *accel_dev, int vf_number)
{
        int status = SUCCESS;
        icp_accel_pci_info_t *pci_dev_info = &accel_dev->pciAccelDev;
        struct msix_entry *msixe = &pci_dev_info->msixEntries.value[vf_number];
        char *name = pci_dev_info->msixEntries.name[vf_number];
        snprintf(name, MAX_MSIX_DEVICE_NAME, "dev%dBundle%d",
                                       accel_dev->accelId, vf_number);
        status = request_irq(msixe->vector,
                             adf_msix_isr_bundle, 0, name,
                             (void *)accel_dev);
        if (status) {
                ADF_ERROR("failed request_irq IRQ %d for %s status=%d\n",
                                            msixe->vector, name, status);
        }
        return status;
}

/*
 * adf_disable_bundle_msix
 * Disable single MSIX for a bundle that becomes VF.
 */
int adf_disable_bundle_msix(icp_accel_dev_t *accel_dev, int vf_number)
{
        int status = SUCCESS;
        icp_accel_pci_info_t *pci_dev_info = &accel_dev->pciAccelDev;
        struct msix_entry *msixe = &pci_dev_info->msixEntries.value[vf_number];
        free_irq(msixe->vector, (void*)accel_dev);
        return status;
}

/*
 * adf_isr_enable_unco_err_interrupts
 * Enable All the supported Uncorrectable Error sources and
 * their logging/debug abilities
 */
int adf_isr_enable_unco_err_interrupts(icp_accel_dev_t *accel_dev)
{
    uint8_t i = 0;
    void *pmisc_bar_addr = NULL;
    volatile uint32_t temp_reg = 0;

    /*retrieve the pmiscbar address*/
    pmisc_bar_addr = adf_isr_getMiscBarAd(accel_dev);

    /*Setup interrupt mask*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_ERRMSK0,
            ICP_DH89xxCC_ERRMSK0_UERR); /*AE3-AE0*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_ERRMSK1,
            ICP_DH89xxCC_ERRMSK1_UERR); /*AE7-AE4*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_ERRMSK3,
            ICP_DH89xxCC_ERRMSK3_UERR); /*MISC*/

    /*
     * Setup Error Sources
     */
    for(i = 0; i < MAX_NB_AE; i++)
    {
        ICP_ADF_CSR_WR(pmisc_bar_addr, PMISCBAR_CTX_ENABLES(i),
                ICP_DH89xxCC_ENABLE_ECC_ERR); /*AEi*/
    }

    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_INTMASKSSM0,
            ICP_DH89xxCC_INTMASKSSM_UERR);/*CPM0*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_INTMASKSSM1,
            ICP_DH89xxCC_INTMASKSSM_UERR);/*CPM1*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_CPP_SHAC_ERR_CTRL,
            ICP_DH89xxCC_CPP_SHAC_UE); /*SHaC*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_ESRAMUERR,
            ICP_DH89xxCC_ESRAM_UERR); /*eSRAM*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_CPPMEMTGTERR,
            ICP_DH89xxCC_TGT_UERR); /*Push/Pull Misc Error*/

    /*Check if signal misc errors are set and interrupts routed to IA*/
    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_SMIAOF);
    if (!(temp_reg & ICP_DH89xxCC_AE_IRQ_MASK)) {
        ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_SMIAOF,
                ICP_DH89xxCC_AE_IRQ_MASK);
    }

    /*
     * Enable Logging
     */
    /*Shared Memory*/
    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMSH0) |
        ICP_DH89xxCC_UERRSSMSH_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMSH0, temp_reg);
    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMSH1) |
        ICP_DH89xxCC_UERRSSMSH_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMSH1, temp_reg);
    /*MMP*/
#ifdef MMP_UNCORRECTABLE_ERR
    /*
     * Disable mmp uncorrectable error
     */
    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP00) |
        ICP_DH89xxCC_UERRSSMMMP_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP00, temp_reg);

    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP01) |
        ICP_DH89xxCC_UERRSSMMMP_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP01, temp_reg);

    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP10) |
        ICP_DH89xxCC_UERRSSMMMP_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP10, temp_reg);

    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP11) |
        ICP_DH89xxCC_UERRSSMMMP_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_UERRSSMMMP11, temp_reg);
#endif
    /*Push/Pull CPM*/
    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_PPERR0) |
        ICP_DH89xxCC_PPERR_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_PPERR0, temp_reg);
    temp_reg = ICP_ADF_CSR_RD(pmisc_bar_addr, ICP_DH89xxCC_PPERR1) |
        ICP_DH89xxCC_PPERR_EN;
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_PPERR1, temp_reg);
    /*Push/Pull Misc*/
    ICP_ADF_CSR_WR(pmisc_bar_addr, ICP_DH89xxCC_TICPPINTCTL,
            ICP_DH89xxCC_TICPP_EN);

    return SUCCESS;
}

