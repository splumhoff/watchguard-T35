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
 * @file sal_compression.c
 *
 * @ingroup SalCtrl
 *
 * @description
 *    This file contains the sal implementation for compression.
 *
 *****************************************************************************/

/* QAT-API includes */
#include "cpa.h"
#include "cpa_dc.h"

/* Osal includes */
#include "Osal.h"

/* ADF includes */
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_cfg.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_poll.h"
#include "icp_adf_debug.h"
#include "icp_adf_esram.h"

/* SAL includes */
#include "lac_log.h"
#include "lac_mem.h"
#include "lac_common.h"
#include "lac_mem_pools.h"
#include "sal_statistics.h"
#include "lac_list.h"
#include "icp_sal_poll.h"
#include "sal_types_compression.h"
#include "dc_session.h"
#include "dc_datapath.h"
#include "dc_stats.h"
#include "lac_sal.h"
#include "lac_sal_ctrl.h"
#include "sal_string_parse.h"
#include "sal_service_state.h"
#include "lac_buffer_desc.h"
#include "icp_qat_fw_comp.h"

/*
 * Prints statistics for a compresion instance
 */
STATIC int SalCtrl_CompresionDebug(void* private_data, char* data, int size)
{
    sal_compression_service_t* pCompressionService =
                           (sal_compression_service_t*)private_data;
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaDcStats dcStats = {0};
    Cpa32S len = 0;

    status = cpaDcGetStats(pCompressionService, &dcStats);
    if(status != CPA_STATUS_SUCCESS)
    {
        LAC_LOG_ERROR("cpaDcGetStats returned error\n");
        return 0;
    }

    /* Engine Info */
    len += snprintf(data+len, size-len,
         SEPARATOR
         BORDER " Statistics for Instance %24s | \n"
         SEPARATOR, pCompressionService->debug_file->name);

    /*Perform Info*/
    len += snprintf(data+len, size-len,
         BORDER " DC comp Requests:               %16llu " BORDER "\n"
         BORDER " DC comp Request Errors:         %16llu " BORDER "\n"
         BORDER " DC comp Completed:              %16llu " BORDER "\n"
         BORDER " DC comp Completed Errors:       %16llu " BORDER "\n"
         SEPARATOR,
         (long long unsigned int)dcStats.numCompRequests,
         (long long unsigned int)dcStats.numCompRequestsErrors,
         (long long unsigned int)dcStats.numCompCompleted,
         (long long unsigned int)dcStats.numCompCompletedErrors);

    /*Perform Info*/
    len += snprintf(data+len, size-len,
         BORDER " DC decomp Requests:             %16llu " BORDER "\n"
         BORDER " DC decomp Request Errors:       %16llu " BORDER "\n"
         BORDER " DC decomp Completed:            %16llu " BORDER "\n"
         BORDER " DC decomp Completed Errors:     %16llu " BORDER "\n"
         SEPARATOR,
         (long long unsigned int)dcStats.numDecompRequests,
         (long long unsigned int)dcStats.numDecompRequestsErrors,
         (long long unsigned int)dcStats.numDecompCompleted,
         (long long unsigned int)dcStats.numDecompCompletedErrors);
    return len;
}

CpaStatus
SalCtrl_CompressionInit(icp_accel_dev_t* device, sal_service_t* service)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U numCompConcurrentReq = 0;
    Cpa32U dcTotalSRAMAvailable = 0;
    Cpa32U esramSize = 0;
    Cpa64U physAddr = 0, virtAddr = 0;
    Cpa32U request_ring_id = 0;
    Cpa32U ring_offset = 0;

    char adfGetParam[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
    char compMemPool[SAL_CFG_MAX_VAL_LEN_IN_BYTES];
    char temp_string[SAL_CFG_MAX_VAL_LEN_IN_BYTES]={0};
    char temp_string2[SAL_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
    char *instance_name = NULL;
    sal_statistics_collection_t *pStatsCollection =
        (sal_statistics_collection_t*)device->pQatStats;
    icp_resp_deliv_method rx_resp_type = ICP_RESP_TYPE_IRQ;
    sal_compression_service_t* pCompressionService =
            (sal_compression_service_t*) service;
    Cpa32U maxNumAccel = 0;

    SAL_SERVICE_GOOD_FOR_INIT(pCompressionService);

    pCompressionService->generic_service_info.state =
      SAL_SERVICE_STATE_INITIALIZING;

    /* Get Config Info: Accel Num, bank Num, packageID,
                                coreAffinity, nodeAffinity and response mode */

    status = Sal_StringParsing("Dc",
            pCompressionService->generic_service_info.instance,
            "AcceleratorNumber", temp_string);
    LAC_CHECK_STATUS(status);
    status = icp_adf_cfgGetParamValue(device,
            icpGetProcessName(),
            temp_string,
            adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            temp_string);
        return status;
    }
    pCompressionService->acceleratorNum = Sal_Strtoul(adfGetParam,
                                               NULL, SAL_CFG_BASE_DEC);

    /* Get eSRAM Info */
    status = icp_adf_esramGetAddress(device,
                 pCompressionService->acceleratorNum,
                 &physAddr, &virtAddr, &esramSize);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Could not get the eSRAM address");
        return CPA_STATUS_FAIL;
    }

    /* Get max accelerator */
    status = icp_adf_cfgGetParamValue(device,
                                      LAC_CFG_SECTION_GENERAL,
                                      ADF_DEV_MAX_ACCEL,
                                      adfGetParam);

    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Could not get the max accelerators");
        return CPA_STATUS_FAIL;
    }
    maxNumAccel = Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

    status = icp_adf_cfgGetParamValue(device,
                                      LAC_CFG_SECTION_GENERAL,
                                      "dcTotalSRAMAvailable",
                                      adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_ERROR("Failed to get dcTotalSRAMAvailable");
        return status;
    }
    dcTotalSRAMAvailable = Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);

    if((dcTotalSRAMAvailable / maxNumAccel) > esramSize)
    {
        LAC_LOG_ERROR("Total eSRAM available from configuration file out of "
                "range");
        return CPA_STATUS_FAIL;
    }

    /* Check if eSRAM memory is available */
    if(dcTotalSRAMAvailable > 0)
    {
        pCompressionService->minSRAMBuffSizeInBytes =
            ((dcTotalSRAMAvailable / maxNumAccel)
                    / DC_NUM_INTER_BUFFERS);

        /* Initialise the intermediate buffers. It assumes that the firmware
         * directly takes a pointer to the eSRAM memory and not a SGL like in
         * the DRAM case */
        pCompressionService->interBuff1eSRAMPhyAddr = physAddr;
        pCompressionService->interBuff2eSRAMPhyAddr =
            pCompressionService->interBuff1eSRAMPhyAddr +
            pCompressionService->minSRAMBuffSizeInBytes;
    }
    else
    {
        pCompressionService->minSRAMBuffSizeInBytes = 0;
        pCompressionService->interBuff1eSRAMPhyAddr = 0;
        pCompressionService->interBuff2eSRAMPhyAddr = 0;
    }

    status = Sal_StringParsing("Dc",
            pCompressionService->generic_service_info.instance,
            "BankNumber", temp_string);
    LAC_CHECK_STATUS(status);
    status = icp_adf_cfgGetParamValue(device,
            icpGetProcessName(),
            temp_string,
            adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            temp_string);
        return status;
    }
    pCompressionService->bankNum = Sal_Strtoul(adfGetParam,
                                               NULL, SAL_CFG_BASE_DEC);

    status = Sal_StringParsing("Dc",
            pCompressionService->generic_service_info.instance,
            "IsPolled", temp_string);
    LAC_CHECK_STATUS(status);
    status = icp_adf_cfgGetParamValue(device,
            icpGetProcessName(),
            temp_string,
            adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            temp_string);
        return status;
    }
    pCompressionService->isPolled = Sal_Strtoul(adfGetParam,
                                               NULL, SAL_CFG_BASE_DEC);

    if(SAL_RESP_POLL_CFG_FILE == pCompressionService->isPolled)
    {
         rx_resp_type = ICP_RESP_TYPE_POLL;
    }

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            ADF_DEV_PKG_ID,
            adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            ADF_DEV_PKG_ID);
        return status;
    }
    pCompressionService->pkgID = Sal_Strtoul(adfGetParam,
                                               NULL, SAL_CFG_BASE_DEC);

    status = icp_adf_cfgGetParamValue(device,
            LAC_CFG_SECTION_GENERAL,
            ADF_DEV_NODE_ID,
            adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            ADF_DEV_NODE_ID);
        return status;
    }
    pCompressionService->nodeAffinity = Sal_Strtoul(adfGetParam,
                                               NULL, SAL_CFG_BASE_DEC);

   /* Next need to read the [AcceleratorX] section of the config file */
    status = Sal_StringParsing("Accelerator",
            pCompressionService->acceleratorNum,
            "", temp_string2);
    LAC_CHECK_STATUS(status);
    status = Sal_StringParsing("Bank",
            pCompressionService->bankNum,
            "CoreIDAffinity", temp_string);
    LAC_CHECK_STATUS(status);

    status = icp_adf_cfgGetParamValue(device,
            temp_string2,
            temp_string,
            adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            temp_string);
        return status;
    }
    pCompressionService->coreAffinity = Sal_Strtoul(adfGetParam,
                                               NULL, SAL_CFG_BASE_DEC);

    status = Sal_StringParsing("Dc",
                    pCompressionService->generic_service_info.instance,
                    "NumConcurrentRequests", temp_string);
    LAC_CHECK_STATUS(status);
    status = icp_adf_cfgGetParamValue(device,
                                      icpGetProcessName(),
                                      temp_string,
                                      adfGetParam);
    if(CPA_STATUS_SUCCESS != status)
    {
        LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
            temp_string);
        return status;
    }

    numCompConcurrentReq = Sal_Strtoul(adfGetParam, NULL, SAL_CFG_BASE_DEC);
    if((SAL_64_CONCURR_REQUESTS != numCompConcurrentReq) &&
       (SAL_128_CONCURR_REQUESTS != numCompConcurrentReq) &&
       (SAL_256_CONCURR_REQUESTS != numCompConcurrentReq) &&
       (SAL_512_CONCURR_REQUESTS != numCompConcurrentReq) &&
       (SAL_1024_CONCURR_REQUESTS != numCompConcurrentReq) &&
       (SAL_2048_CONCURR_REQUESTS != numCompConcurrentReq) &&
       (SAL_4096_CONCURR_REQUESTS != numCompConcurrentReq))
    {
        LAC_LOG_ERROR("Invalid NumConcurrentRequests, valid "\
                     "values {64, 128, 256, 512, 1024, 2048, 4096}");
        return CPA_STATUS_FAIL;
    }

    /* ADF does not allow us to completely fill the ring for batch requests */
    pCompressionService->maxNumCompConcurrentReq =
        ((numCompConcurrentReq>>1) - SAL_BATCH_SUBMIT_FREE_SPACE);

    /* 1. Create transport handles */
    status = Sal_StringParsing("Dc",
            pCompressionService->generic_service_info.instance, "RingTx",
            temp_string);
    LAC_CHECK_STATUS(status);

    status = icp_adf_transCreateHandle(device,
                 ICP_TRANS_TYPE_ETR,
                 icpGetProcessName(),
                 pCompressionService->acceleratorNum,
                 pCompressionService->bankNum,
                 temp_string,
                 lac_getRingType(SAL_RING_TYPE_DC),
                 NULL,
                 ICP_RESP_TYPE_NONE,
                 (numCompConcurrentReq>>1),
                 (icp_comms_trans_handle *)
                     &(pCompressionService->trans_handle_compression_tx));
    LAC_CHECK_STATUS(status);

    /* Ensure that the request ring id is even */
    if(icp_adf_transGetRingNum(
            pCompressionService->trans_handle_compression_tx,
            &request_ring_id) != CPA_STATUS_SUCCESS ||
            (request_ring_id & 0x1))
    {
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);

       LAC_LOG_ERROR("Invalid ring number. Tx ring number has to be even");
       return CPA_STATUS_FAIL;
    }

    status = Sal_StringParsing("Dc",
            pCompressionService->generic_service_info.instance, "RingRx",
            temp_string);
    if(CPA_STATUS_SUCCESS != status)
    {
        icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_tx);
        return status;
    }

    status = icp_adf_transCreateHandle(device,
                 ICP_TRANS_TYPE_ETR,
                 icpGetProcessName(),
                 pCompressionService->acceleratorNum,
                 pCompressionService->bankNum,
                 temp_string,
                 lac_getRingType(SAL_RING_TYPE_NONE),
                 (icp_trans_callback)dcCompression_ProcessCallback,
                 rx_resp_type,
                 (numCompConcurrentReq>>1),
                 (icp_comms_trans_handle *)
                     &(pCompressionService->trans_handle_compression_rx));
    if(CPA_STATUS_SUCCESS != status)
    {
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);
       return status;
    }

    /* Get response ring, based on trans handle compression rx and ensure that
     * the response ring id is odd */
    if(icp_adf_transGetRingNum(
            pCompressionService->trans_handle_compression_rx,
            &pCompressionService->response_ring_id) != CPA_STATUS_SUCCESS ||
            (!(pCompressionService->response_ring_id & 0x1)) ||
            (request_ring_id + 1 != pCompressionService->response_ring_id))
    {
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);

       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_rx);

       LAC_LOG_ERROR("Invalid ring number. Rx ring number has to be odd");
       LAC_LOG_ERROR("Rx ring number has to be Tx + 1");
       return CPA_STATUS_FAIL;
    }

    status = icp_adf_cfgGetParamValue(device,
                                      LAC_CFG_SECTION_GENERAL,
                                      ADF_VF_RING_OFFSET_KEY,
                                      temp_string);
    if(CPA_STATUS_SUCCESS != status)
    {
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_rx);
       LAC_LOG_ERROR("Can not get VF ring offset");
       return status;
    }
    ring_offset = Sal_Strtoul(temp_string, NULL, SAL_CFG_BASE_DEC);
    pCompressionService->response_ring_id += ring_offset;

    /* 2. Allocates memory pools */

    /* Valid initialisation value for a pool ID */
    pCompressionService->compression_mem_pool = LAC_MEM_POOL_INIT_POOL_ID;

    status = Sal_StringParsing("Comp",
            pCompressionService->generic_service_info.instance, "_MemPool",
            compMemPool);
    if(CPA_STATUS_SUCCESS != status)
    {
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);

       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_rx);

       return status;
    }

    status = Lac_MemPoolCreate(&pCompressionService->compression_mem_pool,
                               compMemPool,
                               (numCompConcurrentReq+1),
                               sizeof(dc_compression_cookie_t),
                               LAC_64BYTE_ALIGNMENT,
                               CPA_FALSE,
                               pCompressionService->nodeAffinity);
    if(CPA_STATUS_SUCCESS != status)
    {
       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);

       icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_rx);

       return status;
    }

    /* For the compression pool fill out the physical address of data that
       will be send to QAT */
    Lac_MemPoolInitDcCookiePhyAddr(pCompressionService->compression_mem_pool);

    /* Init compression statistics */
    status = dcStatsInit(pCompressionService);
    if(CPA_STATUS_SUCCESS != status)
    {
        Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

        icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_tx);

        icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_rx);

        return status;
    }
    if(CPA_TRUE == pStatsCollection->bDcStatsEnabled)
    {
       /* Get instance name for stats */
        status = LAC_OS_MALLOC(&instance_name, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
        if(CPA_STATUS_SUCCESS != status)
        {
           Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

           icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);

           icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_rx);

           return status;
        }

        status = Sal_StringParsing("Dc",
                    pCompressionService->generic_service_info.instance,
                    "Name", temp_string);
        if(CPA_STATUS_SUCCESS != status)
        {
            Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

            icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_tx);

            icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_rx);
            LAC_OS_FREE(instance_name);
            return status;
        }
        status = icp_adf_cfgGetParamValue(device,
                icpGetProcessName(),
                temp_string,
                adfGetParam);
        if(CPA_STATUS_SUCCESS != status)
        {
            LAC_LOG_STRING_ERROR1("Failed to get %s from configuration file",
                temp_string);

            Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

            icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_tx);

            icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_rx);
            LAC_OS_FREE(instance_name);
            return status;
        }

        snprintf(instance_name,ADF_CFG_MAX_VAL_LEN_IN_BYTES,"%s",adfGetParam);

        status = LAC_OS_MALLOC(&pCompressionService->debug_file,
                              sizeof(debug_file_info_t));
        if(CPA_STATUS_SUCCESS != status)
        {
            Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

            icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_tx);

            icp_adf_transReleaseHandle(
                pCompressionService->trans_handle_compression_rx);
            LAC_OS_FREE(instance_name);
            return status;
        }

        osalMemSet(pCompressionService->debug_file,0,sizeof(debug_file_info_t));
        pCompressionService->debug_file->name = instance_name;
        pCompressionService->debug_file->type = adf_proc_simple;
        pCompressionService->debug_file->read = SalCtrl_CompresionDebug;
        pCompressionService->debug_file->private_data = pCompressionService;
        pCompressionService->debug_file->parent =
            pCompressionService->generic_service_info.debug_parent_dir;

        status = icp_adf_debugAddFile(device,
                      pCompressionService->debug_file, adf_proc_simple);
        if(CPA_STATUS_SUCCESS != status)
        {
           Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

           icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_tx);

           icp_adf_transReleaseHandle(
               pCompressionService->trans_handle_compression_rx);
           LAC_OS_FREE(instance_name);
           LAC_OS_FREE(pCompressionService->debug_file);
           return status;
        }
    }
    pCompressionService->generic_service_info.stats = pStatsCollection;
    pCompressionService->generic_service_info.state =
      SAL_SERVICE_STATE_INITIALIZED;

    return status;
}

CpaStatus
SalCtrl_CompressionStart(icp_accel_dev_t* device, sal_service_t* service)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    sal_compression_service_t* pCompressionService =
            (sal_compression_service_t*) service;

    if (SAL_SERVICE_STATE_INITIALIZED !=
            pCompressionService->generic_service_info.state)
    {
        LAC_LOG_ERROR("Not in the correct state to call start\n");
        return CPA_STATUS_FAIL;
    }

    pCompressionService->generic_service_info.state = SAL_SERVICE_STATE_RUNNING;

    return status;
}

CpaStatus
SalCtrl_CompressionStop(icp_accel_dev_t* device, sal_service_t* service)
{
    sal_compression_service_t* pCompressionService =
            (sal_compression_service_t*) service;

    if (SAL_SERVICE_STATE_RUNNING !=
        pCompressionService->generic_service_info.state)
    {
        LAC_LOG_ERROR("Not in the correct state to call stop");
        return CPA_STATUS_FAIL;
    }

    pCompressionService->generic_service_info.state =
      SAL_SERVICE_STATE_SHUTTING_DOWN;

    return CPA_STATUS_RETRY;
}

CpaStatus
SalCtrl_CompressionShutdown(icp_accel_dev_t* device,
                            sal_service_t* service)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    sal_compression_service_t* pCompressionService =
            (sal_compression_service_t*) service;
    sal_statistics_collection_t *pStatsCollection =
        (sal_statistics_collection_t*)device->pQatStats;


    if ((SAL_SERVICE_STATE_INITIALIZED !=
          pCompressionService->generic_service_info.state) &&
        (SAL_SERVICE_STATE_SHUTTING_DOWN !=
          pCompressionService->generic_service_info.state))
    {
        LAC_LOG_ERROR("Not in the correct state to call shutdown");
        return CPA_STATUS_FAIL;
    }

    Lac_MemPoolDestroy(pCompressionService->compression_mem_pool);

    status = icp_adf_transReleaseHandle(
            pCompressionService->trans_handle_compression_tx);
    LAC_CHECK_STATUS(status);

    status = icp_adf_transReleaseHandle(
            pCompressionService->trans_handle_compression_rx);
    LAC_CHECK_STATUS(status);

    if(CPA_TRUE == pStatsCollection->bDcStatsEnabled)
    {
       /* Clean stats */
        icp_adf_debugRemoveFile(pCompressionService->debug_file);
        LAC_OS_FREE(pCompressionService->debug_file->name);
        LAC_OS_FREE(pCompressionService->debug_file);
        pCompressionService->debug_file = NULL;
    }
    pCompressionService->generic_service_info.stats = NULL;
    dcStatsFree(pCompressionService);

    pCompressionService->generic_service_info.state =
      SAL_SERVICE_STATE_SHUTDOWN;
    return status;
}

CpaStatus
cpaDcGetStatusText(const CpaInstanceHandle dcInstance,
        const CpaStatus errStatus,
        Cpa8S *pStatusText)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(pStatusText);
#endif

    switch(errStatus)
    {
        case CPA_STATUS_SUCCESS:
            LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_SUCCESS);
            break;
        case CPA_STATUS_FAIL:
            LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_FAIL);
            break;
        case CPA_STATUS_RETRY:
            LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_RETRY);
            break;
        case CPA_STATUS_RESOURCE:
            LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_RESOURCE);
            break;
        case CPA_STATUS_INVALID_PARAM:
            LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_INVALID_PARAM);
            break;
        case CPA_STATUS_FATAL:
            LAC_COPY_STRING(pStatusText, CPA_STATUS_STR_FATAL);
            break;
        default:
            status = CPA_STATUS_INVALID_PARAM;
            break;
    }

    return status;
}

/* This function is splitting one buffer list in two distinct scatter gather
 * lists to be used by the firmware. */
CpaStatus
cpaDcStartInstance(CpaInstanceHandle instanceHandle,
    CpaBufferList *buffer)
{
    icp_qat_addr_width_t bufListDescPhyAddr = 0;
    icp_qat_addr_width_t bufListAlignedPhyAddr = 0;
    CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
    icp_buffer_list_desc_t *pBufferListDesc = NULL;
    icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;
    Cpa64U offsetSplitBuffer = 0;
    CpaInstanceInfo2 info = {0};
    icp_accel_dev_t *dev = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;
    sal_compression_service_t* pService = NULL;
    CpaInstanceHandle insHandle = NULL;
    Cpa64U clientListSize = 0, currentDataSize = 0;
    Cpa32U numBuffers = 0, num = 0;
    CpaBoolean secondBuff = CPA_FALSE;
    CpaBoolean exactCut = CPA_FALSE;
#ifdef ICP_PARAM_CHECK
    Cpa64U buffSize = 0;
#endif

    if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle) {
        insHandle = dcGetFirstHandle();
    }
    else {
        insHandle = instanceHandle;
    }
    LAC_CHECK_NULL_PARAM(insHandle);

    status = cpaDcInstanceGetInfo2(insHandle, &info);
    if (CPA_STATUS_SUCCESS != status) {
        LAC_LOG_ERROR("Can not get instance info\n");
        return status;
    }
    dev = icp_adf_getAccelDevByAccelId(info.physInstId.packageId);
    if (NULL == dev) {
        LAC_LOG_ERROR("Can not find device for the instance\n");
        return CPA_STATUS_FAIL;
    }

    if (NULL == buffer) {
        /* Increment dev ref counter and return - DRAM is not used */
        icp_qa_dev_get(dev);
        return CPA_STATUS_SUCCESS;
    }

    pService = (sal_compression_service_t*) insHandle;
    /* Check parameters */
#ifdef ICP_PARAM_CHECK
    SAL_CHECK_ADDR_TRANS_SETUP(insHandle);

    /* buffSize may overflow in the case size is greater than 2^32-1 giving an
     * incorrect value. But this value is not used here. Therefore we can use
     * this function to verify the buffer list */
    if(LacBuffDesc_BufferListVerify(buffer, &buffSize,
                              LAC_NO_ALIGNMENT_SHIFT) != CPA_STATUS_SUCCESS) {
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    pCurrClientFlatBuffer = buffer->pBuffers;
    numBuffers = buffer->numBuffers;

    /* Get the full size of the buffer list */
    for(num = 0; num < buffer->numBuffers; num++)
    {
        clientListSize += buffer->pBuffers[num].dataLenInBytes;
    }

    pService->minInterBuffSizeInBytes = clientListSize/DC_NUM_INTER_BUFFERS;

    /*
     * Get the physical address of this descriptor - need to offset by the
     * alignment restrictions on the buffer descriptors
     */
    bufListDescPhyAddr = LAC_OS_VIRT_TO_PHYS_EXTERNAL(
                             (*pService), buffer->pPrivateMetaData);

    if (bufListDescPhyAddr == 0) {
        LAC_LOG_ERROR("Unable to get the physical address of the metadata\n");
        return CPA_STATUS_FAIL;
    }

    bufListAlignedPhyAddr = LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
                                    ICP_DESCRIPTOR_ALIGNMENT_BYTES);

    pBufferListDesc = (icp_buffer_list_desc_t *)(
            (LAC_ARCH_UINT)buffer->pPrivateMetaData +
            (LAC_ARCH_UINT)(bufListAlignedPhyAddr - bufListDescPhyAddr));

    /* Go past the Buffer List descriptor to the list of buffer descriptors */
    pCurrFlatBufDesc = (icp_flat_buffer_desc_t *)(
                            (pBufferListDesc->phyBuffers));

    /* Initialise the first intermediate buffer */
    pService->pInterBuff1 = pBufferListDesc;
    pService->interBuff1PhyAddr = LAC_MEM_CAST_PTR_TO_UINT64(
        LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                pService->pInterBuff1));

    if (pService->interBuff1PhyAddr == 0) {
        LAC_LOG_ERROR("Unable to get the physical address of the intermediate"
            "buffer\n");
        return CPA_STATUS_FAIL;
    }

    while (0 != numBuffers)
    {
        /* Get the current data size */
        currentDataSize += pCurrClientFlatBuffer->dataLenInBytes;

        /* Ensure that we are copying to the first intermediate buffer only or
         * the second intermediate buffer only */
        if((currentDataSize <= pService->minInterBuffSizeInBytes)
         ||(CPA_TRUE == secondBuff))
        {
            pCurrFlatBufDesc->dataLenInBytes =
                pCurrClientFlatBuffer->dataLenInBytes;

            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                    pCurrClientFlatBuffer->pData));

            if (pCurrFlatBufDesc->phyBuffer == 0)
            {
                LAC_LOG_ERROR("Unable to get the physical address of the flat"
                 " buffer\n");
                return CPA_STATUS_FAIL;
            }

            if(currentDataSize == pService->minInterBuffSizeInBytes)
            {
                exactCut = CPA_TRUE;
            }
        }

        /* The length of the buffer is bigger than the intermediate buffer.
         * The first intermediate buffer will be filled and the remaining data
         * copied to the second intermediate buffer */
        else
        {
            secondBuff = CPA_TRUE;

            /* Set the number of buffers for the first intermediate buffer */
            pBufferListDesc->numBuffers = buffer->numBuffers - numBuffers;

            /* Check if we need to add part of a buffer to the first
             * intermediate buffer list */
            if(CPA_FALSE == exactCut)
            {
                /* Adding one more buffer to intermediate1 buffer list */
                pBufferListDesc->numBuffers++;

                /* Fill the end of the first intermediate buffer */
                pCurrFlatBufDesc->dataLenInBytes =
                    pService->minInterBuffSizeInBytes -
                    (currentDataSize - pCurrClientFlatBuffer->dataLenInBytes);

                /* Save the data length to add as an offset to phyBuffer */
                offsetSplitBuffer = pCurrFlatBufDesc->dataLenInBytes;

                pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                    LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                        pCurrClientFlatBuffer->pData));

                if (pCurrFlatBufDesc->phyBuffer == 0)
                {
                    LAC_LOG_ERROR("Unable to get the physical address of the "
                        "flat buffer\n");
                    return CPA_STATUS_FAIL;
                }
            }

            /* Add an icp_buffer_list_desc_t block at the start of the second
             * intermediate buffer */
            pCurrFlatBufDesc++;
            pBufferListDesc = (icp_buffer_list_desc_t *)pCurrFlatBufDesc;
            pCurrFlatBufDesc = pBufferListDesc->phyBuffers;

            /* Initialise the second intermediate buffer */
            pService->pInterBuff2 = pBufferListDesc;
            pService->interBuff2PhyAddr = LAC_MEM_CAST_PTR_TO_UINT64(
                LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                        pService->pInterBuff2));

            if (pService->interBuff2PhyAddr == 0)
            {
                LAC_LOG_ERROR("Unable to get the physical address of the "
                    "intermediate buffer\n");
                return CPA_STATUS_FAIL;
            }

            /* Set the number of buffers for the second intermediate buffer */
            pBufferListDesc->numBuffers = numBuffers;

            /* Copy the remaining data in the second intermediate buffer */
            pCurrFlatBufDesc->dataLenInBytes = currentDataSize
                - pService->minInterBuffSizeInBytes;

            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                    pCurrClientFlatBuffer->pData + offsetSplitBuffer));

            if (pCurrFlatBufDesc->phyBuffer == 0)
            {
                LAC_LOG_ERROR("Unable to get he physical address of the flat"
                    " buffer\n");
                return CPA_STATUS_FAIL;
            }
        }

        pCurrFlatBufDesc++;
        pCurrClientFlatBuffer++;

        numBuffers--;
    }
    /* Increment dev ref counter */
    icp_qa_dev_get(dev);
    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcStopInstance(CpaInstanceHandle instanceHandle)
{
    sal_compression_service_t* pService = NULL;
    CpaInstanceHandle insHandle = NULL;
    CpaInstanceInfo2 info = {0};
    icp_accel_dev_t *dev = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;

    if(CPA_INSTANCE_HANDLE_SINGLE == instanceHandle)
    {
        insHandle = dcGetFirstHandle();
    }
    else
    {
        insHandle = instanceHandle;
    }

    LAC_CHECK_NULL_PARAM(insHandle);
    pService = (sal_compression_service_t*) insHandle;
    /* Set intermediate buffers 1 and 2 to NULL */
    pService->pInterBuff1 = NULL;
    pService->pInterBuff2 = NULL;

    status = cpaDcInstanceGetInfo2(insHandle, &info);
    if (CPA_STATUS_SUCCESS != status) {
        LAC_LOG_ERROR("Can not get instance info\n");
        return status;
    }
    dev = icp_adf_getAccelDevByAccelId(info.physInstId.packageId);
    if (NULL == dev) {
        LAC_LOG_ERROR("Can not find device for the instance\n");
        return CPA_STATUS_FAIL;
    }
    /* Decrement dev ref counter */
    icp_qa_dev_put(dev);
    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcGetNumInstances(Cpa16U *pNumInstances)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    icp_accel_dev_t **pAdfInsts = NULL;
    icp_accel_dev_t* dev_addr = NULL;
    sal_t* base_addr = NULL;
    sal_list_t* list_temp = NULL;
    Cpa16U num_accel_dev = 0;
    Cpa16U num = 0;
    Cpa16U i = 0;

    LAC_CHECK_NULL_PARAM(pNumInstances);

    /* Get the number of accel_dev in the system */
    status = icp_amgr_getNumInstances(&num_accel_dev);
    LAC_CHECK_STATUS(status);

    /* Allocate memory to store addr of accel_devs */
    pAdfInsts = osalMemAlloc(num_accel_dev*sizeof(icp_accel_dev_t*));
    if(NULL == pAdfInsts)
    {
        LAC_LOG_ERROR("Failed to allocate dev instance memory");
        return CPA_STATUS_RESOURCE;
    }
    num_accel_dev = 0;

    /* Get ADF to return accel_devs with dc enabled */
    status = icp_amgr_getAllAccelDevByCapabilities(
                                        ICP_ACCEL_CAPABILITIES_COMPRESSION,
                                                pAdfInsts, &num_accel_dev);
    if(CPA_STATUS_SUCCESS == status)
    {
        for(i = 0; i < num_accel_dev; i++)
        {
            dev_addr = (icp_accel_dev_t*) pAdfInsts[i];
            if(NULL != dev_addr)
            {
                base_addr = dev_addr->pSalHandle;
                if(NULL != base_addr)
                {
                    list_temp = base_addr->compression_services;
                    while(NULL != list_temp)
                    {
                        num++;
                        list_temp=SalList_next(list_temp);
                    }
                }
            }
        }

        *pNumInstances = num;
    }

    osalMemFree(pAdfInsts);

    return status;
}

CpaStatus
cpaDcGetInstances(Cpa16U numInstances, CpaInstanceHandle* dcInstances)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    icp_accel_dev_t **pAdfInsts = NULL;
    icp_accel_dev_t* dev_addr = NULL;
    sal_t* base_addr = NULL;
    sal_list_t* list_temp = NULL;
    Cpa16U num_accel_dev = 0;
    Cpa16U index = 0;
    Cpa16U i = 0;

    LAC_CHECK_NULL_PARAM(dcInstances);
    if (0 == numInstances)
    {
        LAC_INVALID_PARAM_LOG("numInstances is 0");
        return CPA_STATUS_INVALID_PARAM;
    }

    /* Get the number of accel_dev in the system */
    status = icp_amgr_getNumInstances(&num_accel_dev);
    LAC_CHECK_STATUS(status);

    /* Allocate memory to store addr of accel_devs */
    pAdfInsts = osalMemAlloc(num_accel_dev*sizeof(icp_accel_dev_t*));
    if(pAdfInsts == NULL)
    {
        LAC_LOG_ERROR("Failed to allocate dev instance memory");
        return CPA_STATUS_RESOURCE;
    }

    num_accel_dev=0;
    /* Get ADF to return accel_devs with dc enabled */
    status = icp_amgr_getAllAccelDevByCapabilities(
                            ICP_ACCEL_CAPABILITIES_COMPRESSION,
                                     pAdfInsts, &num_accel_dev);

    if(CPA_STATUS_SUCCESS == status)
    {
        /* First check the number of instances in the system */
        for(i=0; i<num_accel_dev; i++)
        {
            dev_addr = (icp_accel_dev_t*) pAdfInsts[i];
            if(NULL != dev_addr)
            {
                base_addr = dev_addr->pSalHandle;
                if(NULL != base_addr)
                {
                    list_temp = base_addr->compression_services;
                    while(NULL != list_temp)
                    {
                        if(index > (numInstances-1))
                        {
                           break;
                        }
                        dcInstances[index++] = SalList_getObject(list_temp);
                        list_temp=SalList_next(list_temp);
                    }
                }
            }
        }

        if(numInstances > index)
        {
            LAC_LOG_ERROR1("Only %d dc instances available", index);
            status = CPA_STATUS_RESOURCE;
        }
    }
    osalMemFree(pAdfInsts);

    return status;
}

CpaStatus
cpaDcInstanceGetInfo2(const CpaInstanceHandle instanceHandle,
                     CpaInstanceInfo2 *pInstanceInfo2)
{
    sal_compression_service_t* pCompressionService = NULL;
    CpaInstanceHandle insHandle = NULL;
    icp_accel_dev_t *dev = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;
    char keyStr[ADF_CFG_MAX_KEY_LEN_IN_BYTES] = {0};
    char valStr[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};

    if(CPA_INSTANCE_HANDLE_SINGLE == instanceHandle)
    {
         insHandle = dcGetFirstHandle();
    }
    else
    {
         insHandle = instanceHandle;
    }

    LAC_CHECK_NULL_PARAM(insHandle);
    SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
    LAC_CHECK_NULL_PARAM(pInstanceInfo2);

    memset(pInstanceInfo2, '\0', sizeof(CpaInstanceInfo2));
    pInstanceInfo2->accelerationServiceType =
        CPA_INSTANCE_TYPE_DATA_COMPRESSION;

    snprintf((char *) pInstanceInfo2->vendorName,
               CPA_INST_VENDOR_NAME_SIZE,
               "%s", SAL_INFO2_VENDOR_NAME);
    pInstanceInfo2->vendorName[CPA_INST_VENDOR_NAME_SIZE - 1] = '\0';

    snprintf((char *) pInstanceInfo2->partName,
               CPA_INST_PART_NAME_SIZE,
               SAL_INFO2_PART_NAME, SAL_DEVICE_DH89XXCC);
    pInstanceInfo2->partName[CPA_INST_PART_NAME_SIZE - 1] = '\0';

    snprintf((char *) pInstanceInfo2->swVersion,
                   CPA_INST_SW_VERSION_SIZE,
                   "Version %d.%d", SAL_INFO2_DRIVER_SW_VERSION_MAJ_NUMBER,
                                    SAL_INFO2_DRIVER_SW_VERSION_MIN_NUMBER);
    pInstanceInfo2->swVersion[CPA_INST_SW_VERSION_SIZE - 1] = '\0';

    /* Note we can safely read the contents of the compression service instance
       here because icp_amgr_getAccelDevByCapabilities() only returns devs
       that have started */
    pCompressionService = (sal_compression_service_t*) insHandle;
    pInstanceInfo2->physInstId.packageId = pCompressionService->pkgID;
    pInstanceInfo2->physInstId.acceleratorId =
        pCompressionService->acceleratorNum;
    pInstanceInfo2->physInstId.executionEngineId = 0;
    pInstanceInfo2->physInstId.busAddress =
                 icp_adf_get_busAddress(pInstanceInfo2->physInstId.packageId);

    /*set coreAffinity to zero before use */
    LAC_OS_BZERO(pInstanceInfo2->coreAffinity,
                                          sizeof(pInstanceInfo2->coreAffinity));
    CPA_BITMAP_BIT_SET(pInstanceInfo2->coreAffinity,
        pCompressionService->coreAffinity);

    pInstanceInfo2->nodeAffinity = pCompressionService->nodeAffinity;

    if(SAL_SERVICE_STATE_RUNNING ==
       pCompressionService->generic_service_info.state)
    {
        pInstanceInfo2->operState = CPA_OPER_STATE_UP;
    }
    else
    {
        pInstanceInfo2->operState = CPA_OPER_STATE_DOWN;
    }

    pInstanceInfo2->requiresPhysicallyContiguousMemory = CPA_TRUE;

    if(SAL_RESP_POLL_CFG_FILE == pCompressionService->isPolled)
    {
        pInstanceInfo2->isPolled = CPA_TRUE;
    }
    else
    {
        pInstanceInfo2->isPolled = CPA_FALSE;
    }

    pInstanceInfo2->isOffloaded = CPA_TRUE;
    /* Get the instance name from the config file */
    dev = icp_adf_getAccelDevByAccelId(pCompressionService->pkgID);
    if (NULL == dev) {
        LAC_LOG_ERROR("Can not find device for the instance\n");
        return CPA_STATUS_FAIL;
    }
    status = Sal_StringParsing("Dc",
                    pCompressionService->generic_service_info.instance,
                    "Name", keyStr);
    LAC_CHECK_STATUS(status);
    status = icp_adf_cfgGetParamValue(dev, icpGetProcessName(),
                                      keyStr, valStr);
    LAC_CHECK_STATUS(status);
    strncpy((char*)pInstanceInfo2->instName, valStr, CPA_INST_NAME_SIZE);
    strncpy((char*)pInstanceInfo2->instID, icpGetProcessName(),
                                                     CPA_INST_NAME_SIZE);
    strncat((char*)pInstanceInfo2->instID, "_", 1);
    strncat((char*)pInstanceInfo2->instID, valStr, CPA_INST_NAME_SIZE);
    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcQueryCapabilities(CpaInstanceHandle dcInstance,
        CpaDcInstanceCapabilities *pInstanceCapabilities)
{
    sal_compression_service_t* pCompressionService = NULL;
    CpaInstanceHandle insHandle = NULL;

    if(CPA_INSTANCE_HANDLE_SINGLE == dcInstance)
    {
         insHandle = dcGetFirstHandle();
    }
    else
    {
         insHandle = dcInstance;
    }

    /* Check parameters */
#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(insHandle);
    SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
    LAC_CHECK_NULL_PARAM(pInstanceCapabilities);
#endif

    pCompressionService = (sal_compression_service_t*) insHandle;

    /* Set compression capabilities */
    pInstanceCapabilities->statefulLZSCompression = CPA_FALSE;
    pInstanceCapabilities->statefulLZSDecompression = CPA_FALSE;
    pInstanceCapabilities->statelessLZSCompression = CPA_TRUE;
    pInstanceCapabilities->statelessLZSDecompression = CPA_TRUE;
    pInstanceCapabilities->statefulLZSSCompression = CPA_FALSE;
    pInstanceCapabilities->statefulLZSSDecompression = CPA_FALSE;
    pInstanceCapabilities->statelessLZSSCompression = CPA_FALSE;
    pInstanceCapabilities->statelessLZSSDecompression = CPA_FALSE;
    pInstanceCapabilities->statefulELZSCompression = CPA_FALSE;
    pInstanceCapabilities->statefulELZSDecompression = CPA_FALSE;
    pInstanceCapabilities->statelessELZSCompression = CPA_FALSE;
    pInstanceCapabilities->statelessELZSDecompression = CPA_FALSE;
    pInstanceCapabilities->statefulDeflateCompression = CPA_TRUE;
    pInstanceCapabilities->statefulDeflateDecompression = CPA_TRUE;
    pInstanceCapabilities->statelessDeflateCompression = CPA_TRUE;
    pInstanceCapabilities->statelessDeflateDecompression = CPA_TRUE;
    pInstanceCapabilities->checksumCRC32 = CPA_TRUE;
    pInstanceCapabilities->checksumAdler32 = CPA_TRUE;
    pInstanceCapabilities->dynamicHuffman = CPA_TRUE;
    pInstanceCapabilities->precompiledHuffman = CPA_FALSE;
    pInstanceCapabilities->dynamicHuffmanBufferReq = CPA_TRUE;
    pInstanceCapabilities->autoSelectBestHuffmanTree = CPA_TRUE;
    pInstanceCapabilities->validWindowSizeMaskCompression = 0xA0;
    pInstanceCapabilities->validWindowSizeMaskDecompression = 0xA0;
    pInstanceCapabilities->internalHuffmanMem =
            pCompressionService->minSRAMBuffSizeInBytes;

    return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcSetAddressTranslation(const CpaInstanceHandle instanceHandle,
        CpaVirtualToPhysical virtual2Physical)
{
    sal_service_t *pService = NULL;
    CpaInstanceHandle insHandle = NULL;

    if(CPA_INSTANCE_HANDLE_SINGLE == instanceHandle)
    {
        insHandle = dcGetFirstHandle();
    }
    else
    {
        insHandle = instanceHandle;
    }

    /* Check parameters */
#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(insHandle);
    SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
    LAC_CHECK_NULL_PARAM(virtual2Physical);
#endif

    pService = (sal_service_t *) insHandle;

    pService->virt2PhysClient = virtual2Physical;

    return CPA_STATUS_SUCCESS;
}

/**
 ******************************************************************************
 * @ingroup cpaDcCommon
 * Data compression specific polling function which polls a DC instance.
 *****************************************************************************/

CpaStatus
icp_sal_DcPollInstance(CpaInstanceHandle instanceHandle_in,
                  Cpa32U response_quota)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    sal_compression_service_t *dc_handle = NULL;
    sal_service_t *gen_handle = NULL;
    icp_comms_trans_handle trans_hndTable[DC_NUM_RX_RINGS];

    if(CPA_INSTANCE_HANDLE_SINGLE == instanceHandle_in)
    {
        dc_handle = (sal_compression_service_t *)dcGetFirstHandle();
    }
    else
    {
        dc_handle = (sal_compression_service_t *)instanceHandle_in;
    }

    LAC_CHECK_NULL_PARAM(dc_handle);
    SAL_RUNNING_CHECK(dc_handle);

    gen_handle = &(dc_handle->generic_service_info);
    if(SAL_SERVICE_TYPE_COMPRESSION != gen_handle->type)
    {
        LAC_LOG_ERROR("The instance handle is the wrong type");
        return CPA_STATUS_FAIL;
    }

    /*
     * From the instanceHandle we must get the trans_handle and send
     * down to adf for polling.
     * Populate our trans handle table with the appropriate handles.
     */
    trans_hndTable[0] = dc_handle->trans_handle_compression_rx;

    /* Call adf to do the polling. */
    status = icp_adf_pollInstance(trans_hndTable, DC_NUM_RX_RINGS,
                                  response_quota);
    return status;
}

/**
 ******************************************************************************
 * @ingroup cpaDcCommon
 *****************************************************************************/
CpaStatus
cpaDcInstanceSetNotificationCb(
    const CpaInstanceHandle instanceHandle,
    const CpaCyInstanceNotificationCbFunc pInstanceNotificationCb,
    void *pCallbackTag)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    sal_service_t *gen_handle = instanceHandle;

    LAC_CHECK_NULL_PARAM(gen_handle);
    gen_handle->notification_cb = pInstanceNotificationCb;
    gen_handle->cb_tag = pCallbackTag;
    return status;
}

CpaInstanceHandle
dcGetFirstHandle(void)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    static icp_accel_dev_t *adfInsts[ADF_MAX_DEVICES] = {0};
    CpaInstanceHandle dcInst = NULL;
    icp_accel_dev_t* dev_addr = NULL;
    sal_t* base_addr = NULL;
    sal_list_t* list_temp = NULL;
    Cpa16U i, num_dc = 0;


    /* Only need 1 dev with compression enabled - so check all devices */
    status = icp_amgr_getAllAccelDevByCapabilities(
                             ICP_ACCEL_CAPABILITIES_COMPRESSION,
                             adfInsts, &num_dc);
    if((0 == num_dc) || (CPA_STATUS_SUCCESS != status))
    {
         LAC_LOG("No compression devices enabled in the system\n");
         return dcInst;
    }

    for(i=0; i<num_dc; i++)
    {
        dev_addr = (icp_accel_dev_t*) adfInsts[i];
        if(NULL != dev_addr)
        {
            base_addr = dev_addr->pSalHandle;
            if(NULL != base_addr)
            {
                list_temp = base_addr->compression_services;
                if(NULL != list_temp)
                {
                    dcInst = SalList_getObject(list_temp);
                    break;
                }
            }
        }
    }
    return dcInst;
}
