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
 ***************************************************************************
 * @file lac_sym_dp.c
 *    Implementation of the symmetric data plane API
 *
* @ingroup cpaCySymDp
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "cpa_cy_sym_dp.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_transport_dp.h"
#include "icp_adf_debug.h"

#include "lac_mem.h"
#include "lac_log.h"
#include "lac_sym.h"
#include "lac_sym_qat_cipher.h"
#include "lac_list.h"
#include "lac_sal_types_crypto.h"
#include "sal_service_state.h"
#include "lac_sym_auth_enc.h"


#ifdef ICP_PARAM_CHECK
/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Check that the operation data is valid
 *
 * @description
 *      Check that all the parameters defined in the operation data are valid
 *
 * @param[in]       pRequest         Pointer to an operation data for crypto
 *                                   data plane API
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in
 *
 *****************************************************************************/
STATIC CpaStatus
LacDp_EnqueueParamCheck(const CpaCySymDpOpData *pRequest)
{
    lac_session_desc_t *pSessionDesc = NULL;

    LAC_CHECK_NULL_PARAM(pRequest);
    LAC_CHECK_NULL_PARAM(pRequest->instanceHandle);
    LAC_CHECK_NULL_PARAM(pRequest->sessionCtx);

    /* Ensure this is a crypto instance */
    SAL_CHECK_INSTANCE_TYPE(pRequest->instanceHandle, SAL_SERVICE_TYPE_CRYPTO);

    pSessionDesc = LAC_SYM_SESSION_DESC_FROM_CTX_GET(pRequest->sessionCtx);
    if(NULL == pSessionDesc)
    {
        LAC_INVALID_PARAM_LOG("Session context not as expected");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(CPA_FALSE == pSessionDesc->isDPSession)
    {
        LAC_INVALID_PARAM_LOG("Session not initialised for data plane API");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(0 == pRequest->srcBuffer)
    {
        LAC_INVALID_PARAM_LOG("Invalid srcBuffer");
        return CPA_STATUS_INVALID_PARAM;
    }
    if(0 == pRequest->dstBuffer)
    {
        LAC_INVALID_PARAM_LOG("Invalid destBuffer");
        return CPA_STATUS_INVALID_PARAM;
    }
    if(0 == pRequest->thisPhys)
    {
        LAC_INVALID_PARAM_LOG("Invalid thisPhys");
        return CPA_STATUS_INVALID_PARAM;
    }

    /* Check that src buffer Len = dst buffer Len
    Note this also checks that they are of the same type */
    if (pRequest->srcBufferLen != pRequest->dstBufferLen)
    {
        LAC_INVALID_PARAM_LOG(
            "Source and Destination buffer lengths need to be equal");
        return CPA_STATUS_INVALID_PARAM;
    }

    /* Cipher specific tests */
    if(CPA_CY_SYM_OP_HASH != pSessionDesc->symOperation)
    {
        /* Perform IV check */
        if ((LAC_CIPHER_IS_CTR_MODE(pSessionDesc->cipherAlgorithm)
         || LAC_CIPHER_IS_CBC_MODE(pSessionDesc->cipherAlgorithm)
         ||LAC_CIPHER_IS_AES_F8(pSessionDesc->cipherAlgorithm))
         && (!(LAC_CIPHER_IS_CCM(pSessionDesc->cipherAlgorithm))) )
        {
            Cpa32U ivLenInBytes =
                LacSymQat_CipherIvSizeBytesGet(pSessionDesc->cipherAlgorithm);
            if(pRequest->ivLenInBytes != ivLenInBytes)
            {
                if(!( /* GCM with 12 byte IV is OK */
                      ( LAC_CIPHER_IS_GCM(pSessionDesc->cipherAlgorithm) &&
                        pRequest->ivLenInBytes == LAC_CIPHER_IV_SIZE_GCM_12)))
                {
                    LAC_INVALID_PARAM_LOG("invalid cipher IV size");
                    return CPA_STATUS_INVALID_PARAM;
                }
            }
            if(0 == pRequest->iv)
            {
               LAC_INVALID_PARAM_LOG("invalid iv of 0");
               return CPA_STATUS_INVALID_PARAM;
            }

            /* pRequest->pIv is only used for CCM so is not checked here */
        }
        else if(LAC_CIPHER_IS_KASUMI(pSessionDesc->cipherAlgorithm))
        {
            if(LAC_CIPHER_KASUMI_F8_IV_LENGTH != pRequest->ivLenInBytes)
            {
                LAC_INVALID_PARAM_LOG("invalid cipher IV size");
                return CPA_STATUS_INVALID_PARAM;
            }
            if(0 == pRequest->iv)
            {
               LAC_INVALID_PARAM_LOG("invalid iv of 0");
               return CPA_STATUS_INVALID_PARAM;
            }
        }
        else if(LAC_CIPHER_IS_SNOW3G_UEA2(pSessionDesc->cipherAlgorithm))
        {
            if(ICP_QAT_HW_SNOW_3G_UEA2_IV_SZ != pRequest->ivLenInBytes)
            {
                LAC_INVALID_PARAM_LOG("invalid cipher IV size");
                return CPA_STATUS_INVALID_PARAM;
            }
            if(0 == pRequest->iv)
            {
               LAC_INVALID_PARAM_LOG("invalid iv of 0");
               return CPA_STATUS_INVALID_PARAM;
            }
        }
        else if(LAC_CIPHER_IS_CCM(pSessionDesc->cipherAlgorithm))
        {
            if(CPA_STATUS_SUCCESS !=
                 LacSymAlgChain_CheckCCMData(pRequest->pAdditionalAuthData,
                            pRequest->pIv,
                            pRequest->messageLenToCipherInBytes,
                            pRequest->ivLenInBytes))
            {
                return CPA_STATUS_INVALID_PARAM;
            }
        }

        /* Perform algorithm-specific checks */
        if (!(LAC_CIPHER_IS_ARC4(pSessionDesc->cipherAlgorithm) ||
              LAC_CIPHER_IS_CTR_MODE(pSessionDesc->cipherAlgorithm) ||
              LAC_CIPHER_IS_F8_MODE(pSessionDesc->cipherAlgorithm) ||
              LAC_CIPHER_IS_SNOW3G_UEA2(pSessionDesc->cipherAlgorithm)))
        {
            /* Mask & check below is based on assumption that block size is
             * a power of 2. If data size is not a multiple of the block size,
             * the "remainder" bits selected by the mask be non-zero */
            if (pRequest->messageLenToCipherInBytes &
                (LacSymQat_CipherBlockSizeBytesGet(
                    pSessionDesc->cipherAlgorithm) - 1))
            {
                LAC_INVALID_PARAM_LOG("Data size must be block size multiple");
                return CPA_STATUS_INVALID_PARAM;
            }
        }
    }

    /* Hash specific tests */
    if(CPA_CY_SYM_OP_CIPHER != pSessionDesc->symOperation)
    {
        /* For CCM and snow3G there is always an AAD buffer
           For GCM there is an AAD buffer if aadLenInBytes is
           nonzero */
        if((CPA_CY_SYM_HASH_AES_CCM == pSessionDesc->hashAlgorithm) ||
           (CPA_CY_SYM_HASH_AES_GCM == pSessionDesc->hashAlgorithm &&
             (0 != pSessionDesc->aadLenInBytes)))
        {
             LAC_CHECK_NULL_PARAM(pRequest->pAdditionalAuthData);
             if(0 == pRequest->additionalAuthData)
             {
                 LAC_INVALID_PARAM_LOG("Invalid additionalAuthData");
                 return CPA_STATUS_INVALID_PARAM;
             }
        }
        else if(CPA_CY_SYM_HASH_SNOW3G_UIA2 == pSessionDesc->hashAlgorithm)
        {
            if(0 == pRequest->additionalAuthData)
            {
                LAC_INVALID_PARAM_LOG("Invalid additionalAuthData");
                return CPA_STATUS_INVALID_PARAM;
            }
        }

        if((CPA_CY_SYM_HASH_AES_CCM != pSessionDesc->hashAlgorithm) &&
           (!pSessionDesc->digestIsAppended) &&
           (0 == pRequest->digestResult))
        {
            LAC_INVALID_PARAM_LOG("Invalid digestResult");
            return CPA_STATUS_INVALID_PARAM;
        }
        if (CPA_CY_SYM_HASH_AES_CCM == pSessionDesc->hashAlgorithm)
        {
            if((pRequest->cryptoStartSrcOffsetInBytes +
                pRequest->messageLenToCipherInBytes +
                pSessionDesc->hashResultSize) > pRequest->dstBufferLen)
            {
                LAC_INVALID_PARAM_LOG ("CCM - not enough room for digest in"
                                       " destination buffer");
                return CPA_STATUS_INVALID_PARAM;
            }
        }
        if (CPA_CY_SYM_HASH_AES_GMAC == pSessionDesc->hashAlgorithm)
        {
            if (pSessionDesc->aadLenInBytes != 0 ||
                pRequest->pAdditionalAuthData != NULL)
            {
                LAC_INVALID_PARAM_LOG("For AES_GMAC, aadLenInBytes must be zero"
                                      " and pAdditionalAuthData must be NULL");
                return CPA_STATUS_INVALID_PARAM;
            }
        }
        if ((!pSessionDesc->digestIsAppended) &&
            (CPA_CY_SYM_OP_ALGORITHM_CHAINING == pSessionDesc->symOperation))
        {
            /* Check that digestResult is not at end of Hash region */
            if (pRequest->dstBufferLen != CPA_DP_BUFLIST)
            {
                if((pRequest->digestResult == pRequest->dstBuffer +
                    pRequest->hashStartSrcOffsetInBytes +
                    pRequest->messageLenToHashInBytes))
                {
                    LAC_INVALID_PARAM_LOG("To append the Digest Result, "
                                      "digestIsAppended flag must be set ");
                    return CPA_STATUS_INVALID_PARAM;
                }
            }
        }
    }

    if(CPA_DP_BUFLIST != pRequest->srcBufferLen)
    {
        if((CPA_CY_SYM_OP_HASH != pSessionDesc->symOperation) &&
           ((pRequest->messageLenToCipherInBytes +
             pRequest->cryptoStartSrcOffsetInBytes) > pRequest->srcBufferLen))
        {
            LAC_INVALID_PARAM_LOG("cipher len + offset greater than "
                "srcBufferLen");
            return CPA_STATUS_INVALID_PARAM;
        }
        else if((CPA_CY_SYM_OP_CIPHER != pSessionDesc->symOperation) &&
                (CPA_CY_SYM_HASH_AES_CCM != pSessionDesc->hashAlgorithm) &&
                (CPA_CY_SYM_HASH_AES_GCM != pSessionDesc->hashAlgorithm) &&
                (CPA_CY_SYM_HASH_AES_GMAC != pSessionDesc->hashAlgorithm) &&
                ((pRequest->messageLenToHashInBytes +
              pRequest->hashStartSrcOffsetInBytes) > pRequest->srcBufferLen))
        {
           LAC_INVALID_PARAM_LOG("hash len + offset greater than srcBufferLen");
           return CPA_STATUS_INVALID_PARAM;
        }
    }
    else
    {
        LAC_CHECK_8_BYTE_ALIGNMENT(pRequest->srcBuffer);
        LAC_CHECK_8_BYTE_ALIGNMENT(pRequest->dstBuffer);
    }

    LAC_CHECK_8_BYTE_ALIGNMENT(pRequest->thisPhys);

    return CPA_STATUS_SUCCESS;
}
#endif


/**
 *****************************************************************************
 * @ingroup cpaCySymDp
 *      Write Message on the ring and write request params
 *
 * @description
 *      Write Message on the ring and write request params
 *
 * @param[in/out]    pRequest       Pointer to operation data for crypto
 *                                  data plane API
 * @param[in/out]    pCurrentQatMsg Pointer to ring memory where msg will
 *                                  be written
 *
 * @retval none
 *
 *****************************************************************************/

STATIC void
LacDp_WriteRingMsg(CpaCySymDpOpData *pRequest,icp_qat_fw_la_bulk_req_t
                                                  *pCurrentQatMsg)
{
    lac_session_desc_t *pSessionDesc =
              LAC_SYM_SESSION_DESC_FROM_CTX_GET(pRequest->sessionCtx);
    lac_sym_qat_hash_state_buffer_info_t *pHashStateBufferInfo = NULL;

    /* Write Request */
    /*
     * Fill in the initial bytes of the ET ring message - cached from the
     * session descriptor.
     */
    osalMemCopy((void*)pCurrentQatMsg,(void*) &(pSessionDesc->reqCache),
        SAL_SESSION_REQUEST_CACHE_SIZE_IN_BYTES);

    LAC_MEM_SHARED_WRITE_FROM_PTR(pCurrentQatMsg->comn_mid.opaque_data,
        pRequest);

    if(pSessionDesc->laCmdId == ICP_QAT_FW_LA_CMD_AUTH)
    {
        /* Skip the cipher request params */
        pCurrentQatMsg->req_params_addr = pRequest->thisPhys
                     + sizeof(icp_qat_fw_la_cipher_req_params_t);
    }
    else
    {
        pCurrentQatMsg->req_params_addr = pRequest->thisPhys;
    }

    if(CPA_DP_BUFLIST == pRequest->srcBufferLen)
    {
        /* Use buffer lists */
        ICP_QAT_FW_COMN_PTR_TYPE_SET(
            pCurrentQatMsg->comn_hdr.comn_req_flags,
            QAT_COMN_PTR_TYPE_SGL);

        pCurrentQatMsg->comn_ftr.next_request_addr = 0;
    }
    else
    {
        pCurrentQatMsg->comn_ftr.s.src_length = pRequest->srcBufferLen;
        pCurrentQatMsg->comn_ftr.s.dst_length = pRequest->dstBufferLen;
    }

    pCurrentQatMsg->comn_mid.src_data_addr = pRequest->srcBuffer;
    pCurrentQatMsg->comn_mid.dest_data_addr = pRequest->dstBuffer;

    if(CPA_CY_SYM_HASH_AES_CCM == pSessionDesc->hashAlgorithm)
    {
       /* prepare IV and AAD for CCM */
       LacSymAlgChain_PrepareCCMData(pSessionDesc,
                    pRequest->pAdditionalAuthData,
                    pRequest->pIv,
                    pRequest->messageLenToCipherInBytes,
                    pRequest->ivLenInBytes);

       /* overwrite hash parameters */
       pRequest->hashStartSrcOffsetInBytes =
                    pRequest->cryptoStartSrcOffsetInBytes;
       pRequest->messageLenToHashInBytes =
                    pRequest->messageLenToCipherInBytes;
   }
   else if (CPA_CY_SYM_HASH_AES_GCM == pSessionDesc->hashAlgorithm ||
            CPA_CY_SYM_HASH_AES_GMAC == pSessionDesc->hashAlgorithm)
   {
       /* GCM case */
       if (CPA_CY_SYM_HASH_AES_GMAC != pSessionDesc->hashAlgorithm)
       {
           LacSymAlgChain_PrepareGCMData(pSessionDesc,
                           pRequest->pAdditionalAuthData);
       }

       if(LAC_CIPHER_IV_SIZE_GCM_12 == pRequest->ivLenInBytes)
       {
           ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(
                        pCurrentQatMsg->comn_la_req.u.la_flags,
                        ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);
       }
   }

    /* Write Request Params */

    /* CIPHER */
    pRequest->reserved0 = pSessionDesc->cipherRequestParamsQATCache;

    if(CPA_CY_SYM_CIPHER_ARC4 == pSessionDesc->cipherAlgorithm)
    {
        /* ARC4 does not have an IV but the field is used to store the
         * initial state */
        pRequest->iv = pSessionDesc->cipherARC4InitialStatePhysAddr;
    }

    /* HASH */
    pHashStateBufferInfo = &(pSessionDesc->hashStateBufferInfo);
    pRequest->reserved1 = pSessionDesc->hashRequestParamsQATCache;

    if ((CPA_CY_SYM_HASH_SNOW3G_UIA2 != pSessionDesc->hashAlgorithm &&
        CPA_CY_SYM_HASH_AES_CCM != pSessionDesc->hashAlgorithm &&
        CPA_CY_SYM_HASH_AES_GCM != pSessionDesc->hashAlgorithm &&
        CPA_CY_SYM_HASH_AES_GMAC != pSessionDesc->hashAlgorithm) &&
        (pHashStateBufferInfo->prefixAadSzQuadWords > 0))
    {
        /* prefixAadSzQuadWords > 0 when there is prefix data
           - i.e. nested hash or HMAC no precompute cases
           Note partials not supported on DP api so we do not need
           dynamic hash state in this case */
        pRequest->additionalAuthData = pHashStateBufferInfo->pDataPhys
                    + LAC_QUADWORDS_TO_BYTES(
                    pHashStateBufferInfo->stateStorageSzQuadWords);
    }
}

CpaStatus
cpaCySymDpSessionCtxGetSize(const CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        Cpa32U *pSessionCtxSizeInBytes)
{
#ifdef ICP_PARAM_CHECK
     /* CPA_INSTANCE_HANDLE_SINGLE is not supported on DP apis */
    LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
    /* All other param checks are common with trad api */
#endif

    return cpaCySymSessionCtxGetSize(instanceHandle,
        pSessionSetupData,
        pSessionCtxSizeInBytes);
}

/** @ingroup cpaCySymDp */
CpaStatus
cpaCySymDpInitSession(CpaInstanceHandle instanceHandle,
        const CpaCySymSessionSetupData *pSessionSetupData,
        CpaCySymDpSessionCtx sessionCtx)
{
    CpaStatus status = CPA_STATUS_FAIL;
    sal_service_t * pService = NULL;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
    SAL_CHECK_ADDR_TRANS_SETUP(instanceHandle);
    SAL_CHECK_INSTANCE_TYPE(instanceHandle, SAL_SERVICE_TYPE_CRYPTO);
    LAC_CHECK_NULL_PARAM(pSessionSetupData);
    if(CPA_CY_PRIORITY_HIGH != pSessionSetupData->sessionPriority)
    {
        LAC_INVALID_PARAM_LOG("Low priority sessions are not supported by "
            "data plane API");
        return CPA_STATUS_INVALID_PARAM;
    }
#endif /*ICP_PARAM_CHECK*/
    pService = (sal_service_t *) instanceHandle;

    /* Check crypto service is running otherwise return an error */
    SAL_RUNNING_CHECK(pService);

    status = LacSym_InitSession(instanceHandle,
                           NULL, /* Callback */
                           pSessionSetupData,
                           CPA_TRUE, /* isDPSession */
                           sessionCtx);
    return status;
}

CpaStatus
cpaCySymDpRemoveSession(const CpaInstanceHandle instanceHandle,
        CpaCySymDpSessionCtx sessionCtx)
{
#ifdef ICP_PARAM_CHECK
     /* CPA_INSTANCE_HANDLE_SINGLE is not supported on DP apis */
    LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
    /* All other param checks are common with trad api */
#endif

    return cpaCySymRemoveSession(instanceHandle, sessionCtx);
}

CpaStatus cpaCySymDpRegCbFunc(const CpaInstanceHandle instanceHandle,
                              const CpaCySymDpCbFunc pSymDpCb)
{
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
    SAL_CHECK_INSTANCE_TYPE(instanceHandle, SAL_SERVICE_TYPE_CRYPTO);
    LAC_CHECK_NULL_PARAM(pSymDpCb);
#endif
    SAL_RUNNING_CHECK(instanceHandle);
    pService->pSymDpCb = pSymDpCb;

    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaCySymDpEnqueueOp(CpaCySymDpOpData *pRequest, const CpaBoolean performOpNow)
{
    icp_qat_fw_la_bulk_req_t *pCurrentQatMsg = NULL;
    icp_comms_trans_handle trans_handle = NULL;
    lac_session_desc_t *pSessionDesc = NULL;

#ifdef ICP_PARAM_CHECK
    CpaStatus status = CPA_STATUS_SUCCESS;
    LAC_CHECK_NULL_PARAM(pRequest);
    status = LacDp_EnqueueParamCheck(pRequest);
    if(CPA_STATUS_SUCCESS != status)
    {
        return status;
    }
#endif

    trans_handle = ((sal_crypto_service_t*)pRequest->instanceHandle)->
        trans_handle_sym_tx_hi;

    pSessionDesc = LAC_SYM_SESSION_DESC_FROM_CTX_GET(pRequest->sessionCtx);

    icp_adf_getSingleQueueAddr(trans_handle, (void **)&pCurrentQatMsg);
    if (NULL == pCurrentQatMsg )
    {
        /*
         * No space is available on the queue.
         */
        return CPA_STATUS_RETRY;
    }

    LacDp_WriteRingMsg(pRequest, pCurrentQatMsg);

    (pSessionDesc->pendingDpCbCount)++;

    if (CPA_TRUE == performOpNow)
    {
        icp_adf_updateQueueTail(trans_handle);
    }

    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaCySymDpPerformOpNow(const CpaInstanceHandle instanceHandle)
{
    icp_comms_trans_handle trans_handle = NULL;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
    SAL_CHECK_INSTANCE_TYPE(instanceHandle, SAL_SERVICE_TYPE_CRYPTO);
#endif

    /* Check if SAL is initialised otherwise return an error */
    SAL_RUNNING_CHECK(instanceHandle);

    trans_handle =
       ((sal_crypto_service_t*)instanceHandle)->trans_handle_sym_tx_hi;

    if(CPA_TRUE == icp_adf_queueDataToSend(trans_handle))
    {
        icp_adf_updateQueueTail(trans_handle);
    }

    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaCySymDpEnqueueOpBatch(
        const Cpa32U numberRequests,
        CpaCySymDpOpData *pRequests[],
        const CpaBoolean performOpNow)
{
    icp_qat_fw_la_bulk_req_t *pCurrentQatMsg = NULL;
    icp_comms_trans_handle trans_handle = NULL;
    lac_session_desc_t *pSessionDesc = NULL;
    Cpa32U i = 0;

#ifdef ICP_PARAM_CHECK
    CpaStatus status = CPA_STATUS_SUCCESS;
    sal_crypto_service_t *pService = NULL;

    LAC_CHECK_NULL_PARAM(pRequests);
    LAC_CHECK_NULL_PARAM(pRequests[0]);
    LAC_CHECK_NULL_PARAM(pRequests[0]->instanceHandle);

    pService = (sal_crypto_service_t*)(pRequests[0]->instanceHandle);

    if((0 == numberRequests) ||
            (numberRequests > pService->maxNumSymReqBatch))
    {
        LAC_INVALID_PARAM_LOG1("The number of requests needs to be between 1 "
            "and %d", pService->maxNumSymReqBatch);
        return CPA_STATUS_INVALID_PARAM;
    }

    for(i=0; i < numberRequests; i++)
    {
        status = LacDp_EnqueueParamCheck(pRequests[i]);
        if(CPA_STATUS_SUCCESS != status)
        {
            return status;
        }

        /* Check that all instance handles and session contexts are the same */
        if(pRequests[i]->instanceHandle != pRequests[0]->instanceHandle)
        {
            LAC_INVALID_PARAM_LOG("All instance handles should be the same "
                "in the requests");
            return CPA_STATUS_INVALID_PARAM;
        }
        if(pRequests[i]->sessionCtx != pRequests[0]->sessionCtx)
        {
            LAC_INVALID_PARAM_LOG("All session contexts should be the same "
                "in the requests");
            return CPA_STATUS_INVALID_PARAM;
        }
    }
#endif

    trans_handle = ((sal_crypto_service_t*)pRequests[0]->instanceHandle)->
            trans_handle_sym_tx_hi;
    pSessionDesc = LAC_SYM_SESSION_DESC_FROM_CTX_GET(pRequests[0]->sessionCtx);

    icp_adf_getQueueMemory(trans_handle, numberRequests,
                          (void **)&pCurrentQatMsg);
    if (NULL == pCurrentQatMsg)
    {
        /*
         * No space is available on the queue.
         */
        return CPA_STATUS_RETRY;
    }

    for (i=0; i < numberRequests; i++)
    {
        LacDp_WriteRingMsg(pRequests[i], pCurrentQatMsg);
        icp_adf_getQueueNext(trans_handle, (void **)&pCurrentQatMsg);
    }

    pSessionDesc->pendingDpCbCount += numberRequests;
    if (CPA_TRUE == performOpNow)
    {
        icp_adf_updateQueueTail(trans_handle);
    }

    return CPA_STATUS_SUCCESS;
}

CpaStatus
icp_sal_CyPollDpInstance(const CpaInstanceHandle instanceHandle,
                         const Cpa8U responseQuota)
{
    icp_comms_trans_handle trans_handle = NULL;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_INSTANCE_HANDLE(instanceHandle);
    SAL_CHECK_INSTANCE_TYPE(instanceHandle, SAL_SERVICE_TYPE_CRYPTO);
#endif

    /* Check if SAL is initialised otherwise return an error */
    SAL_RUNNING_CHECK(instanceHandle);

    trans_handle =
        ((sal_crypto_service_t*)instanceHandle)->trans_handle_sym_rx_hi;

    return icp_adf_pollQueue(trans_handle, responseQuota);
}
