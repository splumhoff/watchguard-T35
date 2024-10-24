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
 *****************************************************************************
 * @file lac_log_message.c  Utility functions for logging contents of Msgs on
 *                      IA-FW interface
 *
 * @ingroup LacLog
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/


#include "cpa.h"
#include "lac_common.h"
#include "lac_mem.h"
#include "icp_adf_cfg.h"
#include "icp_adf_transport.h"
#include "icp_adf_transport_dp.h"
#include "icp_accel_devices.h"
#include "sal_statistics.h"
#include "sal_string_parse.h"
#include "icp_adf_debug.h"
#include "lac_sal_types.h"



/*
*******************************************************************************
* Define public/global function definitions
*******************************************************************************
*/

/**
 *****************************************************************************
 * @ingroup LacLog
 *****************************************************************************/
#ifdef MSG_DEBUG
CpaBoolean log_msg_enabled = FALSE;
CpaBoolean log_msg_to_dmesg = FALSE;
CpaBoolean log_msg_put_on_ring = TRUE;
CpaBoolean log_msg_replace_with_null_msg = FALSE;
#endif

/*
*******************************************************************************
* Local functions
*******************************************************************************
*/


/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_writeToDmesg()
 *
 * @description
 *      Log the message to dmesg
 *
 * @param[in/out]    pBlock
 * @param[in/out]    size_in_lws
 * @param[in/out]    block_type
 *
 * @retval CpaStatus
 *
 *****************************************************************************/

#ifdef MSG_DEBUG
static CpaStatus LacLogMsg_writeToDmesg(void * pBlock,
                                Cpa32U size_in_lws,
                                Cpa8U block_type)
{
    int lw;
    Cpa32U * pcurrent_lw = pBlock;


    switch (block_type)
    {
    case LAC_LOG_BLOCK:
        /*LAC_LOG_DEBUG2("Block dump: %d LWs @ %llu", size_in_lws,
                       (LAC_ARCH_UINT)pBlock);break;*/
    case LAC_LOG_PARTIAL_REQUEST:
        LAC_LOG_DEBUG1("Partially constructed Request from cache: %d LWs", size_in_lws);break;
    case LAC_LOG_REQUEST:
        LAC_LOG_DEBUG1("Request: %d LWs", size_in_lws);break;
    case LAC_LOG_RESPONSE:
        LAC_LOG_DEBUG1("Response: %d LWs", size_in_lws);break;
    default: break;
    }

    pcurrent_lw = pBlock;
    for(lw = 0; lw < size_in_lws; lw++ )
    {
        LAC_LOG_DEBUG2("LW%02d 0x%08X", lw, *pcurrent_lw);
        pcurrent_lw++;
    }

    return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_replaceWithNull()
 *
 * @description
 *      replace the request message with a NULL message to put onto the ring
 *
 * @param[in/out]    pqat_msg
 * @param[in/out]    size_in_lws
 *
 * @retval None
 *
 *****************************************************************************/
static void LacLogMsg_replaceWithNull(Cpa32U * pqat_msg, Cpa32U size_in_lws)
{

    /* just overwrite the header with the NULL message header */
    pqat_msg[0] = 0x93000000;
    pqat_msg[1] = 0x00000000;
    LacLogMsg_writeToDmesg(pqat_msg, size_in_lws, LAC_LOG_REQUEST);
}

Cpa8U skipSendingInitMsgsToFw;
void skipInitMsgs_SetConfig(icp_accel_dev_t* device)
{
    CpaStatus status;
    const char* paramName = "skipDh895InitMessagesToFW";
    char    paramValue[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};

    skipSendingInitMsgsToFw = 0;

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            paramName,
            paramValue);

    if (status == CPA_STATUS_SUCCESS &&
            (Sal_Strtoul(paramValue, NULL, SAL_CFG_BASE_DEC)> 0 ))
    {
        LAC_LOG("Skip sending init/admin commands.");
        skipSendingInitMsgsToFw = Sal_Strtoul(paramValue,NULL,SAL_CFG_BASE_DEC);
    }
}

Cpa8U skipInitMsgs(void)
{
    return skipSendingInitMsgsToFw;
}

/*
******************************************************************************
*
* API functions
*
******************************************************************************
*/

/**
 *****************************************************************************
 * @ingroup LacLogMsg_isEnabled
 *      LacLogMsg_writeToDmesg()
 *
 * @description
 *
 * @retval CpaBoolean
 *
 *****************************************************************************/
CpaBoolean
LacLogMsg_isEnabled(void)
{
    return log_msg_enabled;
}

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_isDisabled()
 *
 * @description
 *
 * @retval CpaBoolean
 *
 *****************************************************************************/
CpaBoolean
LacLogMsg_isDisabled(void)
{
    return !log_msg_enabled;
}

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_startLogging()
 *
 * @description
 *
 *
 * @param[in/out]    log_to_dmesg
 * @param[in/out]    put_msg
 * @param[in/out]    replace_with_null_msg
 *
 * @retval CpaStatus
 *
 *****************************************************************************/
CpaStatus
LacLogMsg_startLogging(CpaBoolean log_to_dmesg,
                            CpaBoolean put_msg,
                            CpaBoolean replace_with_null_msg)
{

    if (log_msg_enabled)
    {
        return CPA_STATUS_SUCCESS;
    }

    log_msg_replace_with_null_msg = replace_with_null_msg;
    log_msg_to_dmesg = log_to_dmesg;
    log_msg_put_on_ring = put_msg;
    log_msg_enabled = TRUE;

    return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_logBlock()
 *
 * @description
 *      Logs a blcok of the message to dmesg
 *
 * @param[in/out]    size_in_lws 
 * @param[in/out]    service
 * @param[in/out]    block_type
 *
 * @retval None
 *
 *****************************************************************************/
void LacLogMsg_logBlock(void *pBlock,
                            Cpa32U size_in_lws,
                            Cpa8U service,
                            Cpa8U block_type)
{
    if (LacLogMsg_isDisabled())
        return;

    /* logging enabled */
    if (log_msg_to_dmesg)
    {
        LacLogMsg_writeToDmesg(pBlock, size_in_lws, block_type);
    }

    return;
}

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_transPutMsg()
 *
 * @description
 *
 *
 * @param[in/out]    trans_handle 
 * @param[in/out]    pqat_msg
 * @param[in/out]    size_in_lws
 * @param[in/out]    service
 *
 * @retval CpaStatus
 *
 *****************************************************************************/
CpaStatus LacLogMsg_transPutMsg(icp_comms_trans_handle trans_handle,
                            void *pqat_msg,
                            Cpa32U size_in_lws,
                            Cpa8U service)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    /* Should never come in here in this case, but just in case */
    if (!log_msg_enabled)
    {
        return icp_adf_transPutMsg((icp_comms_trans_handle)trans_handle,
                                       pqat_msg,
                                       size_in_lws);
    }

    /* logging enabled */
    if ((status == CPA_STATUS_SUCCESS) && log_msg_to_dmesg)
    {
        status = LacLogMsg_writeToDmesg(pqat_msg, size_in_lws, LAC_LOG_REQUEST);
    }

    if ((status == CPA_STATUS_SUCCESS) && log_msg_put_on_ring)
    {
        if (log_msg_replace_with_null_msg)
        {
            LacLogMsg_replaceWithNull(pqat_msg, size_in_lws);
        }

        LAC_LOG_DEBUG("Putting Request on the ring");
        status = icp_adf_transPutMsg((icp_comms_trans_handle)trans_handle,
                                            pqat_msg,
                                            size_in_lws);
    }
    else
    {
        LAC_LOG("NOT putting Request on the ring !! ");
    }
    return status;
}

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_updateQueueTail()
 *
 * @description
 *
 *
 * @param[in/out]    trans_handle 
 *
 * @retval CpaStatus
 *
 *****************************************************************************/
void LacLogMsg_updateQueueTail(icp_comms_trans_handle trans_handle)
{
    /* Should never come in here in this case, but just in case */
    if (!log_msg_enabled)
    {
        icp_adf_updateQueueTail((icp_comms_trans_handle)trans_handle);
    }

    if (log_msg_put_on_ring)
    {
        LAC_LOG_DEBUG("Updating Tail Ptr, i.e. putting Request to FW");
        icp_adf_updateQueueTail((icp_comms_trans_handle)trans_handle);

    }
    else
    {
        LAC_LOG("NOT updating Tail Ptr. Req(s) on ring but FW won't see it !! ");
    }
}
#endif

/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      set_osal_log_debug_level()
 *
 * @description
 *      
 *
 * @retval None
 *
 *****************************************************************************/
Cpa64U conf_osal_log_level_debug = 0;
void set_osal_log_debug_level(void)
{
    Cpa64U previous_level=0;

    if (conf_osal_log_level_debug > 0 )
    {
        /* enable output from LAC_LOG_DEBUG */
        /* LAC_LOG_LVL_DEBUG1 = 6, DEBUG2=7, DEBUG3=8 */
        previous_level = osalLogLevelSet(conf_osal_log_level_debug);
    }
    else
    {
        /*don't change the conf level, just print it. */
        previous_level = osalLogLevelSet(0);
        osalLogLevelSet(previous_level);
    }
}


/**
 *****************************************************************************
 * @ingroup lac_log_message
 *      LacLogMsg_SetConfig()
 *
 * @description
 *
 * @param[in/out]    device       
 *
 * @retval None
 *
 *****************************************************************************/
void LacLogMsg_SetConfig(icp_accel_dev_t* device)
{

    char paramValue[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
#ifdef MSG_DEBUG
    CpaBoolean conf_log_msg_enabled = FALSE;
    CpaBoolean conf_log_msg_to_dmesg = FALSE;
    CpaBoolean conf_log_msg_put_msg = TRUE;
    CpaBoolean conf_log_msg_replace_with_null_msg = FALSE;
#endif
    CpaStatus status = CPA_STATUS_SUCCESS;

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            "osal_log_level_debug",
            paramValue);

    if (status == CPA_STATUS_SUCCESS)
    {
        Cpa64U new_level =0;

        new_level = Sal_Strtoul(paramValue, NULL, SAL_CFG_BASE_DEC);
        if ((new_level >= 1 ) && (new_level <= 3 ))
        {
            /* enable output from LAC_LOG_DEBUG  - filtered out by default*/
            /* LAC_LOG_LVL_DEBUG1 = 6, DEBUG2=7, DEBUG3=8 */
            conf_osal_log_level_debug = new_level + 5;
        }

    }
    set_osal_log_debug_level();
#ifdef MSG_DEBUG
    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            "log_msg_enabled",
            paramValue);

    if (status == CPA_STATUS_SUCCESS &&
            (Sal_Strtoul(paramValue, NULL, SAL_CFG_BASE_DEC) == 1 ))
    {
        conf_log_msg_enabled = TRUE;
    }

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            "log_msg_to_dmesg",
            paramValue);


    if (status == CPA_STATUS_SUCCESS &&
            (Sal_Strtoul(paramValue, NULL, SAL_CFG_BASE_DEC) == 1 ))
    {
        conf_log_msg_to_dmesg = TRUE;
        LAC_LOG_DEBUG("128B workaround. Logging msgs to dmesg");
    }

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            "log_msg_put_msg",
            paramValue);

    if (status == CPA_STATUS_SUCCESS &&
            (Sal_Strtoul(paramValue, NULL, SAL_CFG_BASE_DEC)== 0 ))
    {
        conf_log_msg_put_msg = FALSE;
        LAC_LOG("128B workaround. NOT putting msgs on ring");
    }

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            "log_msg_replace_with_null_msg",
            paramValue);

    if (status == CPA_STATUS_SUCCESS && conf_log_msg_put_msg &&
            (Sal_Strtoul(paramValue, NULL, SAL_CFG_BASE_DEC)== 1 ))
    {
        conf_log_msg_replace_with_null_msg = TRUE;
        LAC_LOG("128B workaround. Replacing msg with NULL msg and putting on ring");
    }

    if (conf_log_msg_enabled)
    {
        LAC_LOG("128B workaround. Logging msgs enabled");
        LacLogMsg_startLogging(conf_log_msg_to_dmesg,
                conf_log_msg_put_msg,
                conf_log_msg_replace_with_null_msg);
    }

    skipInitMsgs_SetConfig(device);
#endif
}

