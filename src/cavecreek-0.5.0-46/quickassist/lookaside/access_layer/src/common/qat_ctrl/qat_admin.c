/*
 ***************************************************************************
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
 * @file  qat_admin.c
 *
 * @ingroup qat_ctrl
 *
 * @description
 *        This file contains the functions to be used in the admin of the
 *        qat's. The admin msg is used to debug the AEs.
 *
 *****************************************************************************/
#include "cpa.h"

/* Log and Osal includes. */
#include "lac_log.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "Osal.h"

/* ADF includes */
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_cfg.h"
#include "icp_adf_debug.h"

/* FW includes */
#include "icp_qat_fw_init.h"
#include "icp_qat_fw_admin.h"
#include "icp_qat_fw_la.h"
#include "icp_qat_fw.h"
/* SAL includes */
#include "lac_sym.h"
#include "lac_sal_types.h"
#include "lac_sal_types_qat_ctrl.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "sal_string_parse.h"
/* QAT includes */
#include "qat_ctrl.h"
#include "qat_admin.h"
#include "qat_init_defs.h"

/*
 * QatCtrl_AdminSyncCb
 * Admin Sync Callback function.
 */
void
QatCtrl_AdminSyncCb(void *pCallbackTag, CpaStatus status)
{
    LacSync_GenWakeupSyncCaller(pCallbackTag, status);
}


/*
 * QatCtrl_FWCountGet
 * Function called by the debug function.
 * The functions sends the admin msg down to the AEs
 * and returns the number of messages sent to that AE
 * and the number of responses sent back.
 */

CpaStatus
QatCtrl_FWCountGet(CpaInstanceHandle instanceHandle,
                   void* qatInstanceHandle,
                   qat_admin_stats_t *pQatStats)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    sal_qat_service_t *qatInstance = (sal_qat_service_t *)qatInstanceHandle;

    LAC_CHECK_NULL_PARAM(instanceHandle);
    LAC_CHECK_NULL_PARAM(qatInstanceHandle);
    LAC_CHECK_NULL_PARAM(pQatStats);

    /*
     * Send the Get Status msgs to all AEs on this instance.
     * The qat_admin_stats_t structure will be set as a result
     * of this function call - assuming it is successfull.
     */

    status = QatCtrl_SendAdminMsg(instanceHandle,
                                  qatInstance,
                                  ICP_QAT_FW_ADMIN_CMD_GET,
                                  pQatStats);

    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR
            ("QatCtrl_FWCountGet Get number of messages "
             "from firmware - Failed\n");
    }
    return status;
}


/*
 * This function takes care of building the STATUS_GET
 * msg for all active AEs in the system.
 */
CpaStatus
QatCtrl_SendAdminMsg(CpaInstanceHandle instanceHandle,
                     sal_qat_service_t* qat_instance,
                     icp_qat_fw_admin_cmd_id_t adminCmdId,
                     qat_admin_stats_t* pQatStats)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U i = 0;

    /*
     * Loop over all AEs associated with this QAT
     */
    for(i = 0; i < qat_instance->num_aes_per_qat; i++)
    {
         /*
          * Only send the admin msgs to Aes with associated
          * services.
          */
         if(!(qat_instance->serviceMask[i]))
         {
             continue;
         }
         status = QatCtrl_AdminMsgSendSync(instanceHandle,
                                           qat_instance,
                                           adminCmdId,
                                           i, pQatStats);
         if(CPA_STATUS_SUCCESS != status)
         {
             LAC_LOG_ERROR1("Failed to send admin msg to AE %d\n",
                            i);
             return status;
         }
    }

    return status;
}

/*
 * Create the Sync Cookie and then build and send the
 * admin message. Once the callback is received the
 * cookie is destroyed.
 */
CpaStatus
QatCtrl_AdminMsgSendSync(CpaInstanceHandle instanceHandle,
                         sal_qat_service_t* qat_instance,
                         icp_qat_fw_admin_cmd_id_t adminCmdId,
                         Cpa32U aeIndex, qat_admin_stats_t* pQatStats)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    lac_sync_op_data_t *pSyncCallbackData = NULL;
    CpaStatus waitCbStatus = CPA_STATUS_FAIL;

    status = LacSync_CreateSyncCookie(&pSyncCallbackData);
    if (CPA_STATUS_SUCCESS != status)
    {
        /* Failure allocating sync cookie */
        LAC_LOG_ERROR("Failed to create sync cookie\n");
        return status;
    }

    status = QatCtrl_BuildAndSendAdminMsg(instanceHandle,
                                          qat_instance,
                                          adminCmdId,
                                          aeIndex,
                                          pQatStats,
                                          QatCtrl_AdminSyncCb,
                                          pSyncCallbackData);
    if (CPA_STATUS_SUCCESS == status)
    {
        waitCbStatus = LacSync_WaitForCallback(pSyncCallbackData,
                                               LAC_SYM_SYNC_CALLBACK_TIMEOUT,
                                               &status,
                                               NULL);
        if (CPA_STATUS_SUCCESS != waitCbStatus)
        {
            LAC_LOG_ERROR("Callback timed out");
            status = waitCbStatus;
        }
    }

    LacSync_DestroySyncCookie(&pSyncCallbackData);
    return status;
}


/*
 * Build and send the admin msg down to the Admin Ring.
 */
CpaStatus
QatCtrl_BuildAndSendAdminMsg(CpaInstanceHandle instanceHandle,
                             sal_qat_service_t* qatInstanceHandle,
                             icp_qat_fw_admin_cmd_id_t adminCmdId,
                             Cpa32U aeIndex,
                             qat_admin_stats_t* pQatStats,
                             QatCtrl_GenCbFunc pCb,
                             void *pCallbackTag)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    icp_qat_fw_admin_req_t adminMsg;
    qatal_admin_cb_info_t* pCbInfo;

    status = LAC_OS_CAMALLOC(&pCbInfo,
                             sizeof(qatal_admin_cb_info_t),
                             LAC_64BYTE_ALIGNMENT,
                             0);
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Error in allocation QATAL ADMIN callback structure\n");
        return CPA_STATUS_RESOURCE;

    }
    pCbInfo->instance = qatInstanceHandle;
    pCbInfo->aeIndex = aeIndex;
    pCbInfo->pStats = pQatStats;
    pCbInfo->pCb = (LAC_ARCH_UINT)pCb;
    pCbInfo->pCallbackTag = (LAC_ARCH_UINT)pCallbackTag;

    /* Build the ICP_QAT_FW_ADMIN_CMD_GET request message */
    status = QatCtrl_AdminMsgBuild(instanceHandle,
                                   &adminMsg,
                                   adminCmdId,
                                   aeIndex,
                                   qatInstanceHandle,
                                   pCbInfo);

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Send message by putting message on Ring */
        status = icp_adf_transPutMsg(
                         qatInstanceHandle->trans_handle_qat_admin_tx,
                         (Cpa32U *)&adminMsg,
                         QAT_MSG_SZ_WORDS);
        if(CPA_STATUS_SUCCESS != status)
        {
            LAC_LOG_ERROR("RETRY on putting admin msg onto ring\n");
            LAC_OS_CAFREE(pCbInfo);
        }
        else
        {
            /* Increment the inflight message counter. */
            osalAtomicInc(&qatInstanceHandle->adminCallbackPending);
        }
    }
    else
    {
        LAC_LOG_ERROR("Failed to send QAT admin message\n");
        LAC_OS_CAFREE(pCbInfo);
    }

    return status;
}

/*
 * Build the Admin Msg.
 */
CpaStatus QatCtrl_AdminMsgBuild(CpaInstanceHandle instanceHandle,
                             icp_qat_fw_admin_req_t *pAdminReq,
                             icp_qat_fw_admin_cmd_id_t adminCmdId,
                             Cpa32U aeIndex,
                             sal_qat_service_t* qatInstance,
                             qatal_admin_cb_info_t *pCbInfo)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    LAC_CHECK_NULL_PARAM(pAdminReq);
    if(ICP_QAT_FW_ADMIN_CMD_GET != adminCmdId)
    {
        LAC_LOG_ERROR("Admin command not supported\n");
        return CPA_STATUS_INVALID_PARAM;
    }

    /* Clear the message contents */
    osalMemSet(pAdminReq, 0, sizeof(icp_qat_fw_admin_req_t));

    /* Populate the Common Request Message Definition. */

    /* LW0 */
    SalQatMsg_ReqTypePopulate(&(pAdminReq->comn_hdr.arch_if),
                              ICP_ARCH_IF_REQ_QAT_FW_ADMIN,
                              qatInstance->ringNumRx);

    /* LW1-9 Common middle part */
    SalQatMsg_CmnMsgMidPopulate(&(pAdminReq->comn_mid),
                             pCbInfo,0,0);

    /* set the Admin command ID */
    pAdminReq->admin_cmd_id = adminCmdId;
    /* set the AE target ID */
    pAdminReq->target_ae_id =
           (qatInstance->aeTargetIds[
           (qatInstance->generic_service_info.instance
          * qatInstance->num_aes_per_qat) + aeIndex]);
    return status;
}

/*
 * The response handler function for the admin message.
 * Information is extracted from the admin message and
 * the global variables are populated.
 */
void QatCtrl_AdminRespHandler(void *pRespMsg,
                              icp_arch_if_request_t msgType)
{
    CpaStatus status = CPA_STATUS_FAIL;
    Cpa8U opStatus = ICP_QAT_FW_COMN_STATUS_FLAG_OK;
    Cpa8U qatalCmdId = 0;
    icp_qat_fw_admin_resp_t *pAdminRespMsg = NULL;
    qatal_admin_cb_info_t *pCbInfo = NULL;
    QatCtrl_GenCbFunc pCb = NULL;
    void *pCallbackTag = NULL;
    sal_qat_service_t* qatInstance = NULL;
    Cpa32U aeIndex = 0;
    qat_admin_stats_t* pQatStats = NULL;

    if (NULL == pRespMsg)
    {
        LAC_LOG_ERROR("QatCtrl_AdminRespHandler..Response message is NULL\n");
        return;
    }
    /* Cast pRespMsg to admin struct */
    pAdminRespMsg = (icp_qat_fw_admin_resp_t *) pRespMsg;

    qatalCmdId = pAdminRespMsg->comn_resp.serv_cmd_id;

    if (qatalCmdId >= ICP_QAT_FW_ADMIN_CMD_DELIMITER)
    {
        LAC_LOG_ERROR("QatCtrl_AdminRespHandler..Invalid ADMIN command ID\n");
        return;
    }

    /* Get cb info */
    LAC_MEM_SHARED_READ_TO_PTR(pAdminRespMsg->comn_resp.opaque_data, pCbInfo);
    qatInstance = pCbInfo->instance;
    aeIndex = pCbInfo->aeIndex;
    pQatStats = pCbInfo->pStats;
    pCb = (QatCtrl_GenCbFunc)pCbInfo->pCb;
    pCallbackTag = (void *)pCbInfo->pCallbackTag;

    /* Check the status of the response message */
    opStatus = pAdminRespMsg->comn_resp.comn_status;

    if (ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
        ICP_QAT_FW_COMN_RESP_INIT_ADMIN_STAT_GET(opStatus) )
    {
        pQatStats->numRec[aeIndex] = pAdminRespMsg->request_recvd_cnt;
        pQatStats->numSent[aeIndex] = pAdminRespMsg->response_sent_cnt;

        status = CPA_STATUS_SUCCESS;
    }
    if (CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("QatCtrl_AdminRespHandler ..."
                      "Response status not valid\n");
    }

    /*
     * Decrement the callback counter acknowledging the
     * admin response message.
     */
    osalAtomicDec(&qatInstance->adminCallbackPending);
    pCb(pCallbackTag, status);
    LAC_OS_CAFREE(pCbInfo);
}
