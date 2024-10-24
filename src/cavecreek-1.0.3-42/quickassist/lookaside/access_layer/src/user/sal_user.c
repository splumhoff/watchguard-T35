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
 * @file sal_user.c
 *
 * @defgroup SalUser
 *
 * @description
 *    This file contains implementation of functions to start/stop user process
 *
 *****************************************************************************/

/* QAT-API includes */
#include "cpa.h"

/* ADF includes */
#include "icp_adf_init.h"
#include "icp_accel_devices.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_user_proxy.h"
#include "icp_adf_transport.h"
#include "icp_adf_cfg.h"
#include "icp_adf_debug.h"

/* FW includes */
#include "icp_qat_fw_la.h"

/* SAL includes */
#include "icp_sal_user.h"
#include "lac_log.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "lac_list.h"
#ifndef ICP_DC_ONLY
#include "lac_sal_types_crypto.h"
#endif
#include "sal_types_compression.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"

static pthread_mutex_t sync_lock = PTHREAD_MUTEX_INITIALIZER;
static int start_ref_count = 0;

static CpaStatus do_userStart(const char *process_name)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    status = icpSetProcessName(process_name);
    LAC_CHECK_STATUS(status);
    status = SalCtrl_AdfServicesRegister();
    LAC_CHECK_STATUS(status);

    status = icp_adf_userProxyInit(process_name);
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to initialize proxy\n");
        SalCtrl_AdfServicesUnregister();
        return status;
    }
    status = SalCtrl_AdfServicesStartedCheck();
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to start services\n");
        SalCtrl_AdfServicesUnregister();
    }
    return status;
}

CpaStatus icp_sal_userStart(const char *process_name)
{
    char name[ADF_CFG_MAX_SECTION_LEN_IN_BYTES+1] = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;

    if(pthread_mutex_lock(&sync_lock))
    {
        LAC_LOG_ERROR("Mutex lock failed\n");
        return CPA_STATUS_FAIL;
    }

    if(0 == start_ref_count)
    {
        status = icp_adf_userProcessToStart(process_name, name);

	if(CPA_STATUS_SUCCESS != status)
        {
                LAC_LOG_ERROR("icp_adf_userProcessToStart failed\n");
                pthread_mutex_unlock(&sync_lock);
                return CPA_STATUS_FAIL;
        }
        status = do_userStart(name);
    }
    if(CPA_STATUS_SUCCESS == status)
    {
        start_ref_count += 1;
    }
    if(pthread_mutex_unlock(&sync_lock))
    {
        LAC_LOG_ERROR("Mutex unlock failed\n");
        return CPA_STATUS_FAIL;
    }
    return status;
}

CpaStatus icp_sal_userStartMultiProcess(const char *pProcessName,
                               CpaBoolean limitDevAccess)
{
        return  icp_sal_userStart(pProcessName);
}

static CpaStatus do_userStop()
{
    CpaStatus status = SalCtrl_AdfServicesUnregister();

    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to unregister\n");
        return status;
    }

    status = icp_adf_userProxyShutdown();
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to shutdown proxy\n");
        return status;
    }
    icp_adf_userProcessStop();
    return status;
}

CpaStatus icp_sal_userStop()
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    if(pthread_mutex_lock(&sync_lock))
    {
        LAC_LOG_ERROR("Mutex lock failed\n");
        return CPA_STATUS_FAIL;
    }
    start_ref_count -= 1;
    if(0 == start_ref_count)
    {
        status = do_userStop();
    }
    if(0 > start_ref_count)
    {
        start_ref_count = 0;
    }
    if(pthread_mutex_unlock(&sync_lock))
    {
        LAC_LOG_ERROR("Mutex unlock failed\n");
        return CPA_STATUS_FAIL;
    }
    return status;
}

CpaStatus icp_sal_find_new_devices(void)
{
    return icp_adf_find_new_devices();
}

CpaStatus icp_sal_poll_device_events(void)
{
    return icp_adf_poll_device_events();
}

CpaStatus  icp_sal_check_device(Cpa32U accelId)
{
    return icp_adf_check_device(accelId);
}

CpaStatus  icp_sal_check_all_devices(void)
{
    return icp_adf_check_all_devices();
}

CpaStatus icp_sal_reset_device(Cpa32U accelId)
{
    return icp_adf_reset_device(accelId);
}
