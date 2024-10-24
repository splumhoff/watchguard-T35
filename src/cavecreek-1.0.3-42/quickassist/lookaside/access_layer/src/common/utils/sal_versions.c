/***************************************************************************
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
 ***************************************************************************/

/**
 *****************************************************************************
 * @file sal_versions.c
 *
 * @ingroup SalVersions
 *
 * @description
 *    This file contains implementation of functions used to obtain version
 *    information
 *
 *****************************************************************************/

#include "cpa.h"
#include "Osal.h"

#include "icp_accel_devices.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_cfg.h"

#include "lac_common.h"

#include "icp_sal_versions.h"

#define ICP_SAL_VERSIONS_ALL_CAP_MASK       0xFFFFFFFF
/**< Mask used to get all devices from ADF */


/**
*******************************************************************************
 * @ingroup SalVersions
 *      Fills in the version info structure
 * @description
 *      This function obtains hardware and software information associated with
 *      a given device and fills in the version info structure
 *
 * @param[in]   device      Pointer to the device for which version information
 *                          is to be obtained.
 * @param[out]  pVerInfo    Pointer to a structure that will hold version
 *                          information
 *
 * @context
 *      This function might sleep. It cannot be executed in a context that
 *      does not permit sleeping.
 * @assumptions
 *      The system has been started
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @return CPA_STATUS_SUCCESS       Operation finished successfully
 * @return CPA_STATUS_FAIL          Operation failed
 *
 *****************************************************************************/
STATIC CpaStatus
SalVersions_FillVersionInfo(icp_accel_dev_t *device,
        icp_sal_dev_version_info_t *pVerInfo)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    osalMemSet(pVerInfo, 0, sizeof(icp_sal_dev_version_info_t));
    pVerInfo->devId = device->accelId;

    snprintf((char *)pVerInfo->softwareVersion,
                ICP_SAL_VERSIONS_SW_VERSION_SIZE,
                "%d.%d.%d",
                SAL_INFO2_DRIVER_SW_VERSION_MAJ_NUMBER,
                SAL_INFO2_DRIVER_SW_VERSION_MIN_NUMBER,
                SAL_INFO2_DRIVER_SW_VERSION_PATCH_NUMBER );

    return status;
}

CpaStatus
icp_sal_getDevVersionInfo(Cpa32U devId, icp_sal_dev_version_info_t *pVerInfo)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa16U numInstances = 0;
    icp_accel_dev_t **pAccel_dev = NULL;
    Cpa16U num_accel_dev = 0, index = 0;
    icp_accel_dev_t *pDevice = NULL;


#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(pVerInfo);
#endif

    status = icp_amgr_getNumInstances(&numInstances);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Error while getting number of devices\n");
        return CPA_STATUS_FAIL;
    }

#ifdef ICP_PARAM_CHECK
    if(devId >= ADF_MAX_DEVICES)
    {
        LAC_LOG_ERROR("Invalid devId\n");
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    pAccel_dev = osalMemAlloc(numInstances * sizeof(icp_accel_dev_t *));
    if(NULL ==  pAccel_dev)
    {
        LAC_LOG_ERROR("Failed to allocate dev instance memory\n");
        return CPA_STATUS_RESOURCE;
    }

    /* Get ADF to return all accel_devs */
    status = icp_amgr_getAllAccelDevByCapabilities(
                ICP_SAL_VERSIONS_ALL_CAP_MASK,
                pAccel_dev, &num_accel_dev);

    if(CPA_STATUS_SUCCESS == status)
    {
        for(index = 0; index < num_accel_dev; index++)
        {
            pDevice = (icp_accel_dev_t *)pAccel_dev[index];

            if(pDevice->accelId == devId)
            {
                status = SalVersions_FillVersionInfo(pDevice, pVerInfo);
                if(CPA_STATUS_SUCCESS != status)
                {
                    LAC_LOG_ERROR("Error while filling in version info\n");
                }
                break;
            }
        }

        if(index == num_accel_dev)
        {
            LAC_LOG_ERROR1("Device %d not found or not started\n", devId);
            status = CPA_STATUS_FAIL;
        }
    }
    else
    {
        LAC_LOG_ERROR("Error while getting devices");
    }

    osalMemFree(pAccel_dev);
    return status;
}
