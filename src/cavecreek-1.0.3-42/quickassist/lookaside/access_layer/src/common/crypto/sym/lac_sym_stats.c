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

/**
 ***************************************************************************
 * @file lac_sym_stats.c   Implementation of symmetric stats
 *
 * @ingroup LacSym
 *
 ***************************************************************************/


/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_cy_sym.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "lac_mem_pools.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "icp_qat_fw_la.h"
#include "lac_sym_qat.h"
#include "lac_sal_types_crypto.h"
#include "sal_statistics.h"

/* Number of Symmetric Crypto statistics */
#define LAC_SYM_NUM_STATS (sizeof(CpaCySymStats64) / sizeof(Cpa64U))

CpaStatus
LacSym_StatsInit(CpaInstanceHandle instanceHandle)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;

    status = LAC_OS_MALLOC(&(pService->pLacSymStatsArr),
                LAC_SYM_NUM_STATS * sizeof(OsalAtomic));

    if(CPA_STATUS_SUCCESS == status)
    {
        LAC_OS_BZERO(
            (void *) LAC_CONST_VOLATILE_PTR_CAST(pService->pLacSymStatsArr),
            LAC_SYM_NUM_STATS * sizeof(OsalAtomic));
    }
    return status;
}

void
LacSym_StatsFree(CpaInstanceHandle instanceHandle)
{
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;
    if(NULL != pService->pLacSymStatsArr)
    {
        LAC_OS_FREE(pService->pLacSymStatsArr);
    }
}

void
LacSym_StatsInc(Cpa32U offset, CpaInstanceHandle instanceHandle)
{
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;
    if(CPA_TRUE == pService->generic_service_info.stats->bSymStatsEnabled)
    {
        osalAtomicInc(&pService->pLacSymStatsArr[ offset / sizeof(Cpa64U) ]);
    }
}

void
LacSym_Stats32CopyGet(CpaInstanceHandle instanceHandle,
                      struct _CpaCySymStats * const pSymStats)
{
    int i = 0;
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;
    LAC_ENSURE(NULL != instanceHandle,
                                            "invalid handle\n");
    LAC_ENSURE_NOT_NULL(pSymStats);

    for (i = 0; i < LAC_SYM_NUM_STATS; i ++)
    {
        ((Cpa32U *)pSymStats)[i]=
            (Cpa32U)osalAtomicGet(&pService->pLacSymStatsArr[i]);
    }
}

void
LacSym_Stats64CopyGet(CpaInstanceHandle instanceHandle,
                      CpaCySymStats64 * const pSymStats)
{
    int i = 0;
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;
    LAC_ENSURE(NULL != instanceHandle,
                                            "invalid handle\n");
    LAC_ENSURE_NOT_NULL(pSymStats);

    for (i = 0; i < LAC_SYM_NUM_STATS; i ++)
    {
        ((Cpa64U *)pSymStats)[i]=
            osalAtomicGet(&pService->pLacSymStatsArr[i]);
    }
}

void
LacSym_StatsShow(CpaInstanceHandle instanceHandle)
{
    CpaCySymStats64 symStats = {0};

    LacSym_Stats64CopyGet(instanceHandle, &symStats);

    osalLog64 (OSAL_LOG_LVL_USER,
               OSAL_LOG_DEV_STDOUT,
               SEPARATOR
               BORDER "              Symmetric Stats               " BORDER"\n"
               SEPARATOR,
               0, 0, 0, 0, 0, 0, 0, 0);

    /* Session Info */
    osalLog64 (OSAL_LOG_LVL_USER,
               OSAL_LOG_DEV_STDOUT,
               BORDER " Sessions Initialized:           %16llu " BORDER "\n"
               BORDER " Sessions Removed:               %16llu " BORDER "\n"
               BORDER " Session Errors:                 %16llu " BORDER "\n"
               SEPARATOR,
               symStats.numSessionsInitialized,
               symStats.numSessionsRemoved,
               symStats.numSessionErrors,
               0, 0, 0, 0, 0);

    /* Session info */
    osalLog64 (OSAL_LOG_LVL_USER,
               OSAL_LOG_DEV_STDOUT,
               BORDER " Symmetric Requests:             %16llu " BORDER "\n"
               BORDER " Symmetric Request Errors:       %16llu " BORDER "\n"
               BORDER " Symmetric Completed:            %16llu " BORDER "\n"
               BORDER " Symmetric Completed Errors:     %16llu " BORDER "\n"
               BORDER " Symmetric Verify Failures:      %16llu " BORDER "\n"
               SEPARATOR,
               symStats.numSymOpRequests,
               symStats.numSymOpRequestErrors,
               symStats.numSymOpCompleted,
               symStats.numSymOpCompletedErrors,
               symStats.numSymOpVerifyFailures,
               0, 0, 0);
}


