/***************************************************************************
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

#include "cpa.h"
#include "lac_log.h"
#include "lac_mem.h"
#include "Osal.h"
#include "lac_sal_ctrl.h"
#include "icp_accel_devices.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_user_proxy.h"
#include "icp_sal_user.h"
#include "icp_adf_cfg.h"

static pthread_mutex_t sync_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sync_multi_lock = PTHREAD_MUTEX_INITIALIZER;
static char multi_section_name[ADF_CFG_MAX_SECTION_LEN_IN_BYTES] = {0};
static int start_ref_count = 0;

extern int lacSymDrbgLock_init(void);
extern void lacSymDrbgLock_exit(void);

CpaStatus icp_sal_userStartMultiProcess(const char *pProcessName,
                                        CpaBoolean limitDevAccess)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    char sectionName[ADF_CFG_MAX_SECTION_LEN_IN_BYTES] = {0};
    char tmpName[ADF_CFG_MAX_SECTION_LEN_IN_BYTES] = {0};

    if (ADF_CFG_MAX_SECTION_LEN_IN_BYTES <= strlen(pProcessName))
    {
        LAC_LOG_ERROR("Process name too long\n");
        return CPA_STATUS_FAIL;
    }
    if (CPA_TRUE == limitDevAccess)
    {
        if (ADF_CFG_MAX_SECTION_LEN_IN_BYTES <= strlen(pProcessName) +
                                        strlen(DEV_LIMIT_CFG_ACCESS_TMPL))
        {
            LAC_LOG_ERROR("Process name too long\n");
            return CPA_STATUS_FAIL;
        }
        strncpy(tmpName, DEV_LIMIT_CFG_ACCESS_TMPL,
                                       ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
        strncat(tmpName, pProcessName, ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
    }
    else
    {
        strncpy(tmpName, pProcessName, ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
    }

    if(pthread_mutex_lock(&sync_multi_lock))
    {
        LAC_LOG_ERROR("Mutex lock failed\n");
        return CPA_STATUS_FAIL;
    }

    if(0 == start_ref_count)
    {
            status = icp_adf_userProcessToStart(tmpName, sectionName);
            if(CPA_STATUS_SUCCESS != status)
            {
                LAC_LOG_ERROR("icp_adf_userProcessToStart failed\n");
                pthread_mutex_unlock(&sync_multi_lock);
                return CPA_STATUS_FAIL;
            }
            strncpy(multi_section_name, sectionName,
                    ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
    }

    status = icp_sal_userStart(multi_section_name);

    if(pthread_mutex_unlock(&sync_multi_lock))
    {
        LAC_LOG_ERROR("Mutex unlock failed\n");
        return CPA_STATUS_FAIL;
    }
    return status;
}

static CpaStatus do_userStart(const char *pProcessName)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    OSAL_STATUS osal_status = OSAL_SUCCESS;

    status = icpSetProcessName(pProcessName);
    LAC_CHECK_STATUS(status);

    osal_status = lacSymDrbgLock_init();
    LAC_CHECK_STATUS(osal_status);

    status = SalCtrl_AdfServicesRegister();
    LAC_CHECK_STATUS(status);

    osal_status = osalMemInit();
    if (OSAL_SUCCESS != osal_status)
    {
       LAC_LOG_ERROR("Failed to initialize memory\n");
       SalCtrl_AdfServicesUnregister();
       return CPA_STATUS_FAIL;
    }

    status = icp_adf_userProxyInit(pProcessName);
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to initialize proxy\n");
        SalCtrl_AdfServicesUnregister();
        osalMemDestroy();
        return status;
    }

    status = SalCtrl_AdfServicesStartedCheck();
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to start services\n");
        SalCtrl_AdfServicesUnregister();
        osalMemDestroy();
    }
    return status;
}

CpaStatus icp_sal_userStart(const char *pProcessName)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    if(pthread_mutex_lock(&sync_lock))
    {
        LAC_LOG_ERROR("Mutex lock failed\n");
        return CPA_STATUS_FAIL;
    }

    if(0 == start_ref_count)
    {
        status = do_userStart(pProcessName);
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

    lacSymDrbgLock_exit();
    icp_adf_userProcessStop();
    osalMemDestroy();
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
    memset(multi_section_name, '\0',
                    ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
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
