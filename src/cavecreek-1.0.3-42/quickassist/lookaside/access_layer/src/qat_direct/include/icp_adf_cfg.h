/******************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
 *  version: QAT1.7.Upstream.L.1.0.3-42
 *
 *****************************************************************************/
/******************************************************************************
 * @file icp_adf_cfg.h
 *
 * @defgroup icp_AdfCfg Acceleration Driver Framework Configuration Interface.
 *
 * @ingroup icp_Adf
 *
 * @description
 *      This is the top level header file for the run-time system configuration
 *      parameters. This interface may be used by components of this API to
 *      access the supported run-time configuration parameters.
 *
 *****************************************************************************/

#ifndef ICP_ADF_CFG_H
#define ICP_ADF_CFG_H

#include "cpa.h"
#include "icp_accel_devices.h"

/******************************************************************************
* Section for #define's & typedef's
******************************************************************************/
/* MMP firmware version */
#define ICP_CFG_MMP_VER_KEY             ("Firmware_MmpVer")
/* UOF firmware version */
#define ICP_CFG_UOF_VER_KEY             ("Firmware_UofVer")
/* Hardware rev id */
#define ICP_CFG_HW_REV_ID_KEY           ("HW_RevId")
/* Lowest Compatible Driver Version */
#define ICP_CFG_LO_COMPATIBLE_DRV_KEY   ("Lowest_Compat_Drv_Ver")
/* Device node id, tells to which die the device is
 * connected to */
#define ADF_DEV_NODE_ID                 ("Device_NodeId")
/* Device package id, this is accel_dev id */
#define ADF_DEV_PKG_ID                  ("Device_PkgId")

/* String names for the exposed sections of config file. */
#define DYN_SEC                         "DYN"

/* This flag is to enable use of secure RAM for intermediate
 * buffers instead of DRAM for dynamic compression
 */
#define ICP_CFG_PKE_DISABLED "PkeDisabled"
/* logarithmic value of (Intermediate buffer size in bytes) on base 2
 * applicable only when PkeDisabled = 1
 * Valid values: 14 to 18
 * If other values are used, it will be treated as PkeDisabled = 0
 * Eg. If InterBuffLogVal = 14, Inter buff size will be 2^14 = 16KB
 */ 
#define ICP_CFG_INTER_BUF_LOG_VAL "InterBuffLogVal"

/*
 * icp_adf_cfgGetParamValue
 *
 * Description:
 * This function is used to determine the value for a given parameter name.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_cfgGetParamValue(icp_accel_dev_t *accel_dev,
                                   const char *section,
                                   const char * param_name,
                                   char * param_value);
/*
 * icp_adf_cfgGetRingNumber
 *
 * Description:
 * Function returns ring number configured for the service.
 * NOTE: this function will only be used by QATAL in kernelspace.
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_cfgGetRingNumber(icp_accel_dev_t *accel_dev,
                                   const char *section_name,
                                   const Cpa32U accel_num,
                                   const Cpa32U bank_num,
                                   const char *pServiceName,
                                   Cpa32U* pRingNum);

/*
 * icp_adf_get_busAddress
 * Gets the B.D.F. of the physical device
 */
Cpa16U icp_adf_get_busAddress(Cpa16U packageId);

/*
 * icp_adf_get_KptAcHandle
 * Gets the kpt achandle of the physical device
 */
Cpa32U icp_adf_get_kptAcHandle(Cpa16U packageId);

#endif /* ICP_ADF_CFG_H */
