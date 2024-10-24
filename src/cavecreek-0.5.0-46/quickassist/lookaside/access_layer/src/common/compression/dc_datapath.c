/****************************************************************************
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
 * @file dc_datapath.c
 *
 * @defgroup Dc_DataCompression DC Data Compression
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Implementation of the Data Compression datapath operations.
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_dc.h"
#include "cpa_dc_dp.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_qat_fw_comp.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "dc_session.h"
#include "dc_datapath.h"
#include "sal_statistics.h"
#include "lac_common.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "lac_log.h"
#include "sal_types_compression.h"
#include "dc_stats.h"
#include "lac_buffer_desc.h"
#include "lac_sal.h"
#include "lac_sync.h"
#include "sal_service_state.h"
#include "sal_qat_cmn_msg.h"
#include "dc_header_footer.h"

#ifdef ICP_B0_SILICON
/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Modify the parity bit in the state registers for compression (State
 *      Register 0 bit 55)
 *
 * @description
 *      The parity bit needs to be updated with the XOR of all the bits in the
 *      last byte of history and the register bit's value
 *
 * @param[out]   stateRegistersComp     State registers for compression
 * @param[in]    lastByteHistory        Last byte of history
 *
 *****************************************************************************/
STATIC void
dcModify_parity(Cpa8U *stateRegistersComp,
    Cpa8U lastByteHistory)
{
    Cpa8U bit = 0, bit_55 = 0, out = 0;
    Cpa32U numBit = 0;

    /* Modify the state */
    bit_55 = (stateRegistersComp[DC_STATE_REGISTER_PARITY_BIT_WORD] >>
                  DC_STATE_REGISTER_PARITY_BIT) & 0x1;

    for(numBit = 0; numBit < LAC_NUM_BITS_IN_BYTE; numBit++)
    {
        bit = (lastByteHistory >> numBit) & 0x1;
        out = out ^ bit;
    }

    /* Xor with bit 55 */
    out = out ^ bit_55;

    out = (out << DC_STATE_REGISTER_PARITY_BIT)
               & DC_STATE_REGISTER_KEEP_MSB_MASK;

    stateRegistersComp[DC_STATE_REGISTER_PARITY_BIT_WORD] =
        out | (stateRegistersComp[DC_STATE_REGISTER_PARITY_BIT_WORD] &
                   DC_STATE_REGISTER_ZERO_MSB_MASK);
}
#endif

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Get the post pended adler32 checksum from uncompressed data
 *
 * @description
 *      Get the post pended adler32 checksum from uncompressed data
 *      for statelesss session
 *
 * @param[in]   consumed              Octets consumed by the operation
 * @param[out]  pPostPendedAdler32    Pointer to the post-pended adler32
 * @param[in]   pSrcBuff              Pointer to buffer of compressed data
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully
 * @retval CPA_STATUS_FAIL          Function failed
 *
 *****************************************************************************/
STATIC CpaStatus
dcGet_PostPendedAdler32(Cpa32U consumed,
                        Cpa32U * pPostPendedAdler32,
                        CpaBufferList *pSrcBuff)
{
    CpaFlatBuffer *pEndBuf = NULL;
    Cpa32U numBuffers = 0;
    Cpa32U dataLen = 0;
    Cpa32U index = 0;
    Cpa32U leftShiftBits = 0;
    Cpa8U * pData = NULL;

    if(consumed <= DC_ZLIB_FOOTER_SIZE)
    { /* no post-pended adler32 */
        return CPA_STATUS_FAIL;
    }
#ifdef ICP_PARAM_CHECK
    LAC_ASSERT_NOT_NULL(pPostPendedAdler32);
    LAC_ASSERT_NOT_NULL(pSrcBuff);
#endif

    pEndBuf = pSrcBuff->pBuffers;
    numBuffers = pSrcBuff->numBuffers;

#ifdef DEBUG_PRINTS
    LAC_LOG1("numBuffers = %d\n", numBuffers);
    LAC_LOG1("consumed = %d\n", consumed);
    LAC_LOG1("pEndBuf->dataLenInBytes = %d\n", pEndBuf->dataLenInBytes);
#endif
    /* Find the last flat buffer pEndBuf in buffer list pSrcBuff,
     * and get the data length in bytes of the data
     * in the last flat buffer */
    while ((0 != numBuffers) && (0 != consumed)
           && (0 != pEndBuf->dataLenInBytes))
    {
        if(consumed <= pEndBuf->dataLenInBytes)
        {
            dataLen = consumed;
        }
        else
        {
            dataLen = pEndBuf->dataLenInBytes;
            pEndBuf ++;
            numBuffers --;
        }
        consumed -= dataLen;
    }
    if(((0 == numBuffers) && (0 != consumed))
       || ((0 != consumed) && (0 == pEndBuf->dataLenInBytes)))
    {
        return CPA_STATUS_FAIL;
    }
#ifdef DEBUG_PRINTS
    LAC_LOG1("numBuffers = %d\n", numBuffers);
    LAC_LOG1("consumed = %d\n", consumed);
    LAC_LOG1("dataLen = %d\n", dataLen);
#endif
    if(dataLen < DC_ZLIB_FOOTER_SIZE)
    {
        *pPostPendedAdler32 = 0;
        pData = pEndBuf->pData;
        index = dataLen;
#ifdef DEBUG_PRINTS
        LAC_LOG1("pEndBuf->pData = 0x%x\n", *((Cpa32U *)pData));
#endif
        while(index)
        {
            leftShiftBits = ((dataLen - index) *
                              LAC_NUM_BITS_IN_BYTE);
#ifdef DEBUG_PRINTS
            LAC_LOG1("left shift %d bytes\n",
                             (dataLen - index));
#endif
            index --;
            /* save to pPostPendedAdler32 in little endian order */
            *pPostPendedAdler32 += ((Cpa32U)(*(pData + index)) & 0xff ) <<
                                   leftShiftBits;
#ifdef DEBUG_PRINTS
            LAC_LOG1("index = %d\n", index);
#endif
        }
        pData = (pEndBuf-1)->pData +
                (pEndBuf-1)->dataLenInBytes -
                (DC_ZLIB_FOOTER_SIZE - dataLen);
        index = DC_ZLIB_FOOTER_SIZE - dataLen;
#ifdef DEBUG_PRINTS
        LAC_LOG1("the end of (pEndBuf-1)->pData = 0x%x\n", *((Cpa32U *)pData));
#endif
        while(index)
        {
            leftShiftBits = ((DC_ZLIB_FOOTER_SIZE - index) * LAC_NUM_BITS_IN_BYTE);
#ifdef DEBUG_PRINTS
            LAC_LOG1("left shift %d bytes\n", DC_ZLIB_FOOTER_SIZE - index);
#endif
            index --;
            /* save to pPostPendedAdler32 in little endian order */
            *pPostPendedAdler32 += ((Cpa32U)(*(pData + index)) & 0xff ) <<
                                   leftShiftBits;
#ifdef DEBUG_PRINTS
            LAC_LOG1("index = %d\n", index);
#endif
        }
    }
    else
    {
        pData = (Cpa8U *)pPostPendedAdler32;
        pData[0] = *(pEndBuf->pData +
                     dataLen - DC_ZLIB_FOOTER_SIZE + 3);
        pData[1] = *(pEndBuf->pData +
                     dataLen - DC_ZLIB_FOOTER_SIZE + 2);
        pData[2] = *(pEndBuf->pData +
                     dataLen - DC_ZLIB_FOOTER_SIZE + 1);
        pData[3] = *(pEndBuf->pData +
                     dataLen - DC_ZLIB_FOOTER_SIZE + 0);
    }
#ifdef DEBUG_PRINTS
    LAC_LOG1("*pPostPendedAdler32 = 0x%x\n", *pPostPendedAdler32);
#endif
    return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Update Adler32 Checksum as the workaround for Adler32 issue
 *      for decompression
 *
 * @description
 *      Update Adler32 Checksum as the workaround for Adler32 issue
 *      for decompression
 *
 * @param[in]     pSessionDesc        Pointer to the session descriptor
 * @param[in/out] pAdler32            Pointer to the adler32 checksum
 * @param[in]     pSrcBuff            Pointer to data buffer of decompressed
 * @param[in]     pDestBuff           Pointer to buffer space for data after
 * @param[in]     pSrcBuff            Pointer to data buffer for decompression
 * @param[in]     pDestBuff           Pointer to buffer space for data after
 *                                    decompression
 * @param[in]     flushFlag           Indicates the type of flush to be
 *                                    performed
 * @param[in]     produced            Octets produced by the operation
 * @param[in]     consumed            Octets consumed by the operation
 *
 *****************************************************************************/
STATIC void
dcUpdate_Checksum(dc_session_desc_t* pSessionDesc,
                  Cpa32U *pAdler32,
                  CpaBufferList *pSrcBuff,
                  CpaBufferList *pDestBuff,
                  CpaDcFlush flushFlag,
                  Cpa32U produced,
                  Cpa32U consumed)
{
    CpaFlatBuffer *pFlatBuf = NULL;
    Cpa32U numBuffers = 0;
    Cpa32U bufSizeInBytes = 0;
    Cpa32U dataLen = 0;
    Cpa32U postPendedAdler32 = 0;
    Cpa8U *pData = NULL;
    Cpa32U isUpdate = 1;
    Cpa32U tempAdler32 = 0;
    Cpa32U tempPreviousAdler32 = 0;

#ifdef ICP_PARAM_CHECK
    LAC_ASSERT_NOT_NULL(pSessionDesc);
    LAC_ASSERT_NOT_NULL(pAdler32);
    LAC_ASSERT_NOT_NULL(pSrcBuff);
    LAC_ASSERT_NOT_NULL(pDestBuff);
#endif
    if((0 == consumed) || (0 == produced)
       || (0 == pSrcBuff->numBuffers)
       || (0 == pDestBuff->numBuffers))
    {
        return;
    }
    pFlatBuf = pDestBuff->pBuffers;
    numBuffers = pDestBuff->numBuffers;
    bufSizeInBytes = pFlatBuf->dataLenInBytes;
    tempPreviousAdler32 = pSessionDesc->previousChecksum;

    if(CPA_DC_STATELESS == pSessionDesc->sessState)
    {
        CpaStatus status = CPA_STATUS_SUCCESS;
        status = dcGet_PostPendedAdler32(consumed,
                                         &postPendedAdler32,
                                         pSrcBuff);

        if((CPA_STATUS_SUCCESS == status)
            && (*pAdler32 == postPendedAdler32))
        {
            /* For stateless session, not update Adler32
             * if the post-pended Adler32
             * is the same as the Adler32
             * calculated by the slice */
            isUpdate = 0;
        }
    }
#ifdef DEBUG_PRINTS
    LAC_LOG1("numBuffers = %d\n", numBuffers);
    LAC_LOG1("bufSizeInBytes = %d\n", bufSizeInBytes);
    LAC_LOG1("consumed = %d\n", consumed);
    LAC_LOG1("produced = %d\n", produced);
#endif
    while (isUpdate && (0 != numBuffers) && (0 != produced)
           && (0 != bufSizeInBytes))
    {
        pData = pFlatBuf->pData;
        if(produced < bufSizeInBytes)
        {
            dataLen = produced;
        }
        else
        {
            dataLen = bufSizeInBytes;
            pFlatBuf++;
            numBuffers--;
            bufSizeInBytes = pFlatBuf->dataLenInBytes;
        }
        produced -= dataLen;
#ifdef DEBUG_PRINTS
        LAC_LOG1("tempPreviousAdler32 = 0x%x\n",
                                tempPreviousAdler32);
#endif
        /* calculate adler32 checksum for
         * each byte in pDestBuff->pBuffers->pData */
        tempAdler32 = tempPreviousAdler32;
        osalAdler32(&tempAdler32, pData, dataLen);

        /* Save the checksum for the next request */
        tempPreviousAdler32 = tempAdler32;
#ifdef DEBUG_PRINTS
        LAC_LOG1("numBuffers = %d\n", numBuffers);
        LAC_LOG1("bufSizeInBytes = %d\n", bufSizeInBytes);
        LAC_LOG1("consumed = %d\n", consumed);
        LAC_LOG1("produced = %d\n", produced);
        LAC_LOG1("dataLen = %d\n", dataLen);
#endif
    }
    if((!isUpdate) || ((0 == numBuffers) && (0 != produced))
       || ((0 != produced) && (0 == bufSizeInBytes)))
    { /* not update adler32 */
        return;
    }
    *pAdler32 = tempAdler32;
    pSessionDesc->previousChecksum = tempPreviousAdler32;
#ifdef DEBUG_PRINTS
    LAC_LOG1("after update: checksum = 0x%x\n", *pAdler32);
#endif
}

void
dcCompression_ProcessCallback(icp_comms_trans_handle transHandle,
                              void *pRespMsg)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    icp_qat_fw_comp_resp_t *pCompRespMsg = NULL;
    void *callbackTag = NULL;
    Cpa64U *pReqData = NULL;
    CpaDcDpOpData *pResponse = NULL;
    CpaDcRqResults *pResults = NULL;
    CpaDcCallbackFn pCbFunc = NULL;
    dc_session_desc_t *pSessionDesc = NULL;
    z_stream stream = {0};

    sal_compression_service_t* pService = NULL;
    dc_compression_cookie_t* pCookie = NULL;
    CpaBoolean cmpPass = CPA_TRUE, xlatPass = CPA_TRUE;
    Cpa8U cmpErr = ERR_CODE_NO_ERROR, xlatErr = ERR_CODE_NO_ERROR;
    dc_request_dir_t compDecomp = DC_COMPRESSION_REQUEST;
    Cpa8U opStatus = ICP_QAT_FW_COMN_STATUS_FLAG_OK;

    /* Cast response message to compression response message type */
    pCompRespMsg = (icp_qat_fw_comp_resp_t *)pRespMsg;
#ifdef ICP_PARAM_CHECK
    LAC_ASSERT_NOT_NULL(pCompRespMsg);
#endif

    /* Extract request data pointer from the opaque data */
    LAC_MEM_SHARED_READ_TO_PTR(pCompRespMsg->comn_resp.opaque_data, pReqData);
#ifdef ICP_PARAM_CHECK
    LAC_ASSERT_NOT_NULL(pReqData);
#endif

    /* Extract fields from the request data structure */
    pCookie = (dc_compression_cookie_t*) pReqData;
    pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pCookie->pSessionHandle);

    if(CPA_TRUE == pSessionDesc->isDcDp)
    {
        pResponse = (CpaDcDpOpData *) pReqData;
        pResults = &(pResponse->results);
    }
    else
    {
        pSessionDesc = pCookie->pSessionDesc;
        pResults = pCookie->pResults;
        callbackTag = pCookie->callbackTag;
        pCbFunc = pCookie->pSessionDesc->pCompressionCb;
        compDecomp = pCookie->compDecomp;
    }

    pService = (sal_compression_service_t*) (pCookie->dcInstance);

    opStatus = pCompRespMsg->comn_resp.comn_status;

    /* Check compression response status */
    cmpPass = (ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
            ICP_QAT_FW_COMN_RESP_CMP_STAT_GET(opStatus));

    /* Get the cmp error code */
    cmpErr = pCompRespMsg->comn_resp.comn_error.s1.cmp_err_code;

    /* We return the compression error code for now. We would need to update
     * the API if we decide to return both error codes */
    pResults->status = (Cpa8S)cmpErr;

    /* Check the translator status */
    if((CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection)
     &&(CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType))
    {
        /* Check translator response status */
        xlatPass = (ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
                ICP_QAT_FW_COMN_RESP_XLAT_STAT_GET(opStatus));

        /* Get the translator error code */
        xlatErr = pCompRespMsg->comn_resp.comn_error.s1.xlat_err_code;

        /* Return a fatal error or a potential error in the translator slice
         * if the compression slice did not return any error */
        if((CPA_DC_OK == pResults->status) ||
           (CPA_DC_FATALERR == (Cpa8S)xlatErr))
        {
            pResults->status = (Cpa8S)xlatErr;
        }
    }

    /* In case of any error for an end of packet request, we need to update
     * the request type for the following request */
    if((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
      ((CPA_DC_FLUSH_FINAL == pCookie->flushFlag) ||
       (CPA_DC_FLUSH_FULL == pCookie->flushFlag)) &&
       ((CPA_TRUE != cmpPass)||(CPA_TRUE != xlatPass)))
    {
        pSessionDesc->requestType = DC_REQUEST_SUBSEQUENT;
    }

    /* Stateful overflow is a valid use case. The previous update on the
     * request type is still required */
    if((CPA_DC_STATEFUL == pSessionDesc->sessState)
     &&(CPA_DC_OVERFLOW == pResults->status))
    {
        cmpPass = CPA_TRUE;

        if(DC_COMPRESSION_REQUEST == compDecomp)
        {
            LAC_LOG("Recoverable error: stateful compression overflow. You may "
                "need to increase the size of your destination buffer and "
                "resubmit this request");
        }
    }

    if((CPA_TRUE == cmpPass)&&(CPA_TRUE == xlatPass))
    {
        /* Extract the response from the firmware */
        pResults->consumed = pCompRespMsg->comn_resp_params.input_byte_counter;
        pResults->produced = pCompRespMsg->comn_resp_params.output_byte_counter;

        if(CPA_DC_STATEFUL == pSessionDesc->sessState)
        {
            pSessionDesc->cumulativeConsumedBytes += pResults->consumed;
        }
        else
        {
            /* In the stateless case all requests have both SOP and EOP set */
            pSessionDesc->cumulativeConsumedBytes = pResults->consumed;
        }

        if(CPA_DC_CRC32 == pSessionDesc->checksumType)
        {
            pResults->checksum = pCompRespMsg->comn_resp_params.current_crc32;
        }
        else if(CPA_DC_ADLER32 == pSessionDesc->checksumType)
        {
            pResults->checksum = pCompRespMsg->comn_resp_params.current_adler;
        }

#ifdef DEBUG_PRINTS
        LAC_LOG1("checksum = 0x%x\n", pResults->checksum);
#endif
        if((CPA_TRUE != pSessionDesc->isDcDp)
            && (CPA_DC_ADLER32 == pSessionDesc->checksumType)
            && (DC_DECOMPRESSION_REQUEST == compDecomp)
            && (NULL != pCookie->pSourceBuffer)
            && (NULL != pCookie->pDestinationBuffer))
        {
            dcUpdate_Checksum(pSessionDesc,
                              &pResults->checksum,
                              pCookie->pSourceBuffer,
                              pCookie->pDestinationBuffer,
                              pCookie->flushFlag,
                              pResults->produced,
                              pResults->consumed);
        }
        /* Save the checksum for the next request */
        pSessionDesc->previousChecksum = pResults->checksum;

        /* reset pSessionDesc->previousChecksum */
        if((CPA_TRUE != pSessionDesc->isDcDp)
            && ((CPA_DC_FLUSH_FINAL == pCookie->flushFlag)
            || (CPA_DC_FLUSH_FULL == pCookie->flushFlag)))
        {
#ifdef DEBUG_PRINTS
            LAC_LOG1("pResults->consumed = 0x%x\n", pResults->consumed);
            LAC_LOG1("pCookie->srcTotalDataLenInBytes = 0x%x\n",
                     pCookie->srcTotalDataLenInBytes);
#endif
            if(pResults->consumed
               == pCookie->srcTotalDataLenInBytes)
            {
                if(CPA_DC_ADLER32 == pSessionDesc->checksumType)
                {
                    pSessionDesc->previousChecksum = 1;
                }
                else
                {
                    pSessionDesc->previousChecksum = 0;
                }
#ifdef DEBUG_PRINTS
                LAC_LOG1("pSessionDesc->previousChecksum = 0x%x\n",
                          pSessionDesc->previousChecksum);
#endif
            }
        }

        if(CPA_TRUE == pSessionDesc->isDcDp)
        {
            pResponse->responseStatus = CPA_STATUS_SUCCESS;
        }
        else
        {
#ifdef ICP_B0_SILICON
            if((CPA_DC_STATEFUL == pSessionDesc->sessState)
             &&(DC_COMPRESSION_REQUEST == compDecomp)
             &&(0 != pSessionDesc->minContextSize)
             &&(pResults->consumed > 0))
            {
                Cpa32U compressContextSize = pSessionDesc->minContextSize;
                Cpa32U compressHistorySize = pSessionDesc->historyBuffSize;

                if(CPA_DC_DIR_COMBINED == pSessionDesc->sessDirection)
                {
                    /* The context and history size were defined at
                     * initialisation time to support both the compression and
                     * the decompression case. We need to use the right values
                     * here */
                    if((CPA_DC_L1 == pSessionDesc->compLevel)
                     ||((CPA_DC_L3 == pSessionDesc->compLevel)
                     &&(DC_32K_WINDOW_SIZE == pSessionDesc->deflateWindowSize))
                     ||(CPA_DC_L4 == pSessionDesc->compLevel)
                     ||(CPA_DC_L7 == pSessionDesc->compLevel))
                    {
                        compressContextSize = 0;
                    }
                    else if(!((CPA_DC_L2 == pSessionDesc->compLevel) &&
                      (DC_32K_WINDOW_SIZE == pSessionDesc->deflateWindowSize)))
                    {

                        compressHistorySize = DC_MIN_HISTORY_SIZE;
                    }
                }

                if(0 != compressContextSize)
                {
                    /* Need to overwrite the last byte of history for stateful
                     * compression SOP or MOP if its value is wrong */
                    if(CPA_TRUE == pCookie->updateHistory)
                    {
                        Cpa32U currentBuffer = 0;
                        Cpa64U offset = 0;

                        /* Keep track of the byte position to overwrite */
                        pSessionDesc->byteToUpdateOffset += pResults->consumed;

                        /* The context buffer list is a circular list */
                        while(pSessionDesc->byteToUpdateOffset >
                                  compressHistorySize)
                        {
                            pSessionDesc->byteToUpdateOffset -=
                                compressHistorySize;
                        }

                        /* Go through the context buffer list to find the byte
                         * to overwrite */
                        offset = pSessionDesc->byteToUpdateOffset;
                        while(offset > pSessionDesc->pContextBuffer->
                                pBuffers[currentBuffer].dataLenInBytes)
                        {
                            offset -= pSessionDesc->pContextBuffer->
                                    pBuffers[currentBuffer].dataLenInBytes;
                            currentBuffer++;
                        }

                        /* Compare the value of the last byte of history with
                         * the expected value */
                        if(pSessionDesc->pContextBuffer->
                               pBuffers[currentBuffer].pData[offset-1] !=
                                   pCookie->srcBuffLastByte)
                        {
                            /* Overwrite the last byte of history */
                            pSessionDesc->pContextBuffer->
                                pBuffers[currentBuffer].pData[offset-1] =
                                    pCookie->srcBuffLastByte;

                            /* Update the parity bit in the state */
                            dcModify_parity(pSessionDesc->stateRegistersComp,
                                pSessionDesc->pContextBuffer->
                                    pBuffers[currentBuffer].pData[offset-1]);
                        }
                    }
                    else
                    {
                        /* Reset the position of the byte to overwrite for an
                         * EOP request */
                        pSessionDesc->byteToUpdateOffset = 0;
                    }
                }
            }
#endif
            if(DC_COMPRESSION_REQUEST == compDecomp)
            {
                COMPRESSION_STAT_INC(numCompCompleted, pService);
            }
            else
            {
                COMPRESSION_STAT_INC(numDecompCompleted, pService);
            }
        }
    }
    else
    {
        if ((CPA_DC_BAD_LITLEN_CODES == pResults->status) &&
            (CPA_DC_STATELESS == pSessionDesc->sessState) &&
            (CPA_DC_DIR_DECOMPRESS == pSessionDesc->sessDirection))
        {
            /* Decompress data using ZLIB inflate */
            status = osalZlibInflate((void*)pCookie->pSourceBuffer,
                                     (void*)pCookie->pDestinationBuffer,
                                     &pResults->produced,
                                     pSessionDesc->checksumType,
                                     &pResults->checksum,
                                     &stream);
            if(CPA_STATUS_SUCCESS == status)
            {
                pResults->status = CPA_DC_OK;
                pResults->consumed = stream.total_in;
            }
            else if (CPA_DC_OVERFLOW == status)
            {
                pResults->status = CPA_DC_OVERFLOW;
                pResults->checksum = 0;
                pResults->consumed = 0;
                pResults->produced = 0;
            }
            else
            {
                pResults->status = CPA_DC_BAD_LITLEN_CODES;
                pResults->checksum = 0;
                pResults->consumed = 0;
                pResults->produced = 0;
            }
        }
        else
        {
            LAC_LOG_ERROR1("Unexpected response status = %d",
                pResults->status);
        }

        if(CPA_DC_OVERFLOW == pResults->status &&
           CPA_DC_STATELESS == pSessionDesc->sessState)
        {
            LAC_LOG_ERROR("Unrecoverable error: stateless overflow. You may "
                "need to increase the size of your destination buffer");
        }

        if(CPA_TRUE == pSessionDesc->isDcDp)
        {
            pResponse->responseStatus = CPA_STATUS_FAIL;
        }
        else
        {
            if(CPA_DC_OK != pResults->status)
            {
                status = CPA_STATUS_FAIL;
            }

            if(DC_COMPRESSION_REQUEST == compDecomp)
            {
                COMPRESSION_STAT_INC(numCompCompletedErrors, pService);
            }
            else
            {
                COMPRESSION_STAT_INC(numDecompCompletedErrors, pService);
            }
        }
    }

    if(CPA_TRUE == pSessionDesc->isDcDp)
    {
        /* Decrement number of stateless pending callbacks for session */
        pSessionDesc->pendingDpStatelessCbCount--;

        (pService->pDcDpCb)(pResponse);
    }
    else
    {
        /* Decrement number of pending callbacks for session */
        if(CPA_DC_STATELESS == pSessionDesc->sessState)
        {
            osalAtomicDec(&(pCookie->pSessionDesc->pendingStatelessCbCount));
        }
        else
        {
            osalAtomicDec(&(pCookie->pSessionDesc->pendingStatefulCbCount));
        }

        /* Free the memory pool */
        if (NULL != pCookie)
        {
            Lac_MemPoolEntryFree(pCookie);
            pCookie = NULL;
        }

        if(NULL != pCbFunc)
        {
            pCbFunc(callbackTag, status);
        }
    }
}

#ifdef ICP_PARAM_CHECK
/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Check the compression or decompression function parameters
 *
 * @description
 *      Check that all the parameters used for a compression or decompression
 *      request are valid
 *
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pSessionHandle        Session handle
 * @param[in]   pSrcBuff              Pointer to data buffer for compression
 * @param[in]   pDestBuff             Pointer to buffer space for data after
 *                                    compression
 * @param[in]   pResults              Pointer to results structure
 * @param[in]   flushFlag             Indicates the type of flush to be
 *                                    performed
 * @param[in]   srcBuffSize           Size of the source buffer
 * @param[in]   compDecomp            Direction of the operation
 *
 * @retval CPA_STATUS_SUCCESS         Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 *
 *****************************************************************************/
STATIC CpaStatus
dcCheckCompressData(sal_compression_service_t *pService,
        CpaDcSessionHandle pSessionHandle,
        CpaBufferList *pSrcBuff,
        CpaBufferList *pDestBuff,
        CpaDcRqResults *pResults,
        CpaDcFlush flushFlag,
        Cpa64U srcBuffSize,
        dc_request_dir_t compDecomp)
{
    dc_session_desc_t *pSessionDesc = NULL;
    Cpa64U destBuffSize = 0;

    LAC_CHECK_NULL_PARAM(pSessionHandle);
    LAC_CHECK_NULL_PARAM(pSrcBuff);
    LAC_CHECK_NULL_PARAM(pDestBuff);
    LAC_CHECK_NULL_PARAM(pResults);

    pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);
    if(NULL == pSessionDesc)
    {
        LAC_INVALID_PARAM_LOG("Session handle not as expected");
        return CPA_STATUS_INVALID_PARAM;
    }

    if((flushFlag < CPA_DC_FLUSH_NONE) ||
       (flushFlag > CPA_DC_FLUSH_FULL))
    {
        LAC_INVALID_PARAM_LOG("Invalid flushFlag value");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(pSrcBuff == pDestBuff)
    {
        LAC_INVALID_PARAM_LOG("In place operation not supported");
        return CPA_STATUS_INVALID_PARAM;
    }

    /* Compressing or decompressing zero bytes is not supported for stateless
     * sessions */
    if((CPA_DC_STATELESS == pSessionDesc->sessState) && (0 == srcBuffSize))
    {
        LAC_INVALID_PARAM_LOG("The source buffer size need to be greater than "
            "zero byte for stateless sessions");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(srcBuffSize > DC_BUFFER_MAX_SIZE)
    {
        LAC_INVALID_PARAM_LOG("The source buffer size need to be less than or "
            "equal to 2^32-1 bytes");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(LacBuffDesc_BufferListVerify(pDestBuff, &destBuffSize,
            LAC_NO_ALIGNMENT_SHIFT)
                != CPA_STATUS_SUCCESS)
    {
        LAC_INVALID_PARAM_LOG("Invalid destination buffer list parameter");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(destBuffSize > DC_BUFFER_MAX_SIZE)
    {
        LAC_INVALID_PARAM_LOG("The destination buffer size need to be less "
            "than or equal to 2^32-1 bytes");
        return CPA_STATUS_INVALID_PARAM;
    }

    if(CPA_TRUE == pSessionDesc->isDcDp)
    {
        LAC_INVALID_PARAM_LOG("The session type should not be data plane");
        return CPA_STATUS_INVALID_PARAM;
    }

#ifdef ICP_B0_SILICON
    if((DC_DECOMPRESSION_REQUEST == compDecomp)
     &&(CPA_DC_STATEFUL == pSessionDesc->sessState))
    {
        LAC_INVALID_PARAM_LOG("Stateful decompression is not supported");
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    if(DC_COMPRESSION_REQUEST == compDecomp)
    {
        if(CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType)
        {
            /* Check if eSRAM or DRAM is supported */
            if((0 == pService->interBuff1eSRAMPhyAddr) &&
               (NULL == pService->pInterBuff1))
            {
                LAC_LOG_ERROR("No buffer defined for this instance - see "
                    "cpaDcStartInstance");
                return CPA_STATUS_INVALID_PARAM;
            }

            /* Ensure that the destination buffer size is greater or equal
             * to 128B */
            if(destBuffSize < DC_DEST_BUFFER_DYN_MIN_SIZE)
            {
                LAC_INVALID_PARAM_LOG("Destination buffer size should be "
                    "greater or equal to 128B");
                return CPA_STATUS_INVALID_PARAM;
            }
        }
        else
        {
            /* Ensure that the destination buffer size is greater or equal
             * to 64B */
            if(destBuffSize < DC_DEST_BUFFER_STA_MIN_SIZE)
            {
                LAC_INVALID_PARAM_LOG("Destination buffer size should be "
                    "greater or equal to 64B");
                return CPA_STATUS_INVALID_PARAM;
            }
        }
    }
    else
    {
        /* Ensure that the destination buffer size is greater or equal
         * to 16B */
        if(destBuffSize < DC_DEST_BUFFER_DEC_MIN_SIZE)
        {
            LAC_INVALID_PARAM_LOG("Destination buffer size should be "
                "greater or equal to 16B");
            return CPA_STATUS_INVALID_PARAM;
        }
    }

    return CPA_STATUS_SUCCESS;
}
#endif

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Populate the compression request parameters
 *
 * @description
 *      This function will populate the compression request parameters
 *
 * @param[out]  pCompReqParams   Pointer to the compression request parameters
 * @param[in]   nextSlice        Next slice
 * @param[in]   pCookie          Pointer to the compression cookie
 * @param[in]   pService         Pointer to the compression service
 *
 *****************************************************************************/
STATIC void
dcCompRequestParamsPopulate(
    icp_qat_fw_comp_req_params_t *pCompReqParams,
    icp_qat_fw_slice_t nextSlice,
    dc_compression_cookie_t *pCookie,
    sal_compression_service_t *pService)
{
    LAC_ENSURE_NOT_NULL(pCompReqParams);

    pCompReqParams->next_id = nextSlice;
    pCompReqParams->curr_id = ICP_QAT_FW_SLICE_COMP;
    pCompReqParams->comp_len = pCookie->srcTotalDataLenInBytes;
    pCompReqParams->out_buffer_sz = pCookie->dstTotalDataLenInBytes;

    pCompReqParams->resrvd = 0;
    pCompReqParams->resrvd1 = 0;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Populate the translator request parameters
 *
 * @description
 *      This function will populate the translator request parameters
 *
 * @param[out]  pTransReqParams     Pointer to the translator request parameters
 * @param[in]   nextSlice           Next slice
 * @param[in]   interBuff1PhyAddr   Physical address of the first intermediate
 *                                  buffer
 * @param[in]   interBuff2PhyAddr   Physical address of the second intermediate
 *                                  buffer
 * @param[in]   interBufferType     Type of intermediate buffer
 *
 *****************************************************************************/
STATIC void
dcTransRequestParamsPopulate(
    icp_qat_fw_trans_req_params_t *pTransReqParams,
    icp_qat_fw_slice_t nextSlice,
    CpaPhysicalAddr interBuff1PhyAddr,
    CpaPhysicalAddr interBuff2PhyAddr,
    CpaBoolean interBufferType)
{
    LAC_ENSURE_NOT_NULL(pTransReqParams);

    pTransReqParams->next_id = nextSlice;
    pTransReqParams->curr_id = ICP_QAT_FW_SLICE_XLAT;
    pTransReqParams->inter_buffer_1 = interBuff1PhyAddr;
    pTransReqParams->inter_buffer_2 = interBuff2PhyAddr;
    pTransReqParams->inter_buffer_type = interBufferType;

    pTransReqParams->resrvd = 0;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Create the requests for compression or decompression
 *
 * @description
 *      Create the requests for compression or decompression. This function
 *      will update the cookie will all required information.
 *
 * @param{out]  pCookie             Pointer to the compression cookie
 * @param[in]   pService            Pointer to the compression service
 * @param[in]   pSessionDesc        Pointer to the session descriptor
 * @param[in    pSessionHandle      Session handle
 * @param[in]   pSrcBuff            Pointer to data buffer for compression
 * @param[in]   pDestBuff           Pointer to buffer space for data after
 *                                  compression
 * @param[in]   pResults            Pointer to results structure
 * @param[in]   flushFlag           Indicates the type of flush to be
 *                                  performed
 * @param[in]   callbackTag         Pointer to the callback tag
 * @param[in]   compDecomp          Direction of the operation
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in
 *
 *****************************************************************************/
STATIC CpaStatus
dcCreateRequest(
    dc_compression_cookie_t *pCookie,
    sal_compression_service_t *pService,
    dc_session_desc_t *pSessionDesc,
    CpaDcSessionHandle pSessionHandle,
    CpaBufferList *pSrcBuff,
    CpaBufferList *pDestBuff,
    CpaDcRqResults *pResults,
    CpaDcFlush flushFlag,
    void *callbackTag,
    dc_request_dir_t compDecomp)
{
    Cpa8U *pReqParams = NULL;
    Cpa8U reqParamsOffset = 0;
    icp_qat_fw_la_bulk_req_t* pMsg = NULL;
    Cpa64U srcAddrPhys = 0, dstAddrPhys = 0;
    Cpa64U srcTotalDataLenInBytes = 0, dstTotalDataLenInBytes = 0;

    Cpa16U cmdFlags = 0;
    Cpa8U sop = ICP_QAT_FW_COMP_SOP;
    Cpa8U eop = ICP_QAT_FW_COMP_EOP;
    Cpa8U sessType = ICP_QAT_FW_COMP_STATELESS_SESSION;
    Cpa8U autoSelectBest = ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST;
    Cpa8U enhancedAutoSelectBest = ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST;
    Cpa8U disableType0EnhancedAutoSelectBest =
            ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST;
    Cpa32U i = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaBoolean useDRAM = CPA_FALSE;

    icp_qat_fw_comp_req_t *pReqCache = NULL;

    /* Write the buffer descriptors */
    status = LacBuffDesc_BufferListDescWriteAndGetSize(pSrcBuff, &srcAddrPhys,
            CPA_FALSE, &srcTotalDataLenInBytes,
            &(pService->generic_service_info));

    if (status != CPA_STATUS_SUCCESS)
    {
        return status;
    }

    status = LacBuffDesc_BufferListDescWriteAndGetSize(pDestBuff, &dstAddrPhys,
            CPA_FALSE, &dstTotalDataLenInBytes,
            &(pService->generic_service_info));

    if (status != CPA_STATUS_SUCCESS)
    {
        return status;
    }

    /* Populate the compression cookie */
    pCookie->dcInstance = pService;
    pCookie->pSessionHandle = pSessionHandle;
    pCookie->callbackTag = callbackTag;
    pCookie->pSessionDesc = pSessionDesc;
    pCookie->flushFlag = flushFlag;
    pCookie->pResults = pResults;
    pCookie->compDecomp = compDecomp;

    /* The firmware expects the length in bytes for source and destination to be
     * Cpa32U parameters. However the total data length could be bigger as
     * allocated by the user. We ensure that this is not the case in
     * dcCheckCompressData and cast the values to Cpa32U here */
    pCookie->srcTotalDataLenInBytes = (Cpa32U)srcTotalDataLenInBytes;

    if((DC_COMPRESSION_REQUEST == compDecomp) &&
       (CPA_DC_HT_FULL_DYNAMIC == pSessionDesc->huffType))
    {
        if(NULL != pService->pInterBuff1)
        {
            useDRAM = CPA_TRUE;
        }

        if(0 != pService->interBuff1eSRAMPhyAddr)
        {
            /* The destination length passed to the firmware shall be the
             * minimum of the length of the intermediate buffer and the
             * destination buffer */
            if(pService->minSRAMBuffSizeInBytes <
                (Cpa32U)dstTotalDataLenInBytes)
            {
                if(CPA_TRUE == useDRAM)
                {
                    /* In this case we need to check if the DRAM has a bigger
                     * intermediate buffer size than eSRAM to try to limit the
                     * probability of getting an overflow */
                    if(pService->minSRAMBuffSizeInBytes >=
                            pService->minInterBuffSizeInBytes)
                    {
                        pCookie->dstTotalDataLenInBytes =
                            pService->minSRAMBuffSizeInBytes;
                        useDRAM = CPA_FALSE;
                    }
                }
                else
                {
                    pCookie->dstTotalDataLenInBytes =
                        pService->minSRAMBuffSizeInBytes;
                }
            }
            else
            {
                pCookie->dstTotalDataLenInBytes =
                    (Cpa32U)dstTotalDataLenInBytes;
                useDRAM = CPA_FALSE;
            }
        }

        if(CPA_TRUE == useDRAM)
        {
            if(pService->minInterBuffSizeInBytes <
                (Cpa32U)dstTotalDataLenInBytes)
            {
                pCookie->dstTotalDataLenInBytes =
                    (Cpa32U)(pService->minInterBuffSizeInBytes);
            }
            else
            {
                pCookie->dstTotalDataLenInBytes =
                    (Cpa32U)dstTotalDataLenInBytes;
            }
        }
    }
    else
    {
        pCookie->dstTotalDataLenInBytes = (Cpa32U)dstTotalDataLenInBytes;
    }

#ifdef ICP_B0_SILICON
    if((CPA_DC_STATEFUL == pSessionDesc->sessState)
     &&(DC_COMPRESSION_REQUEST == compDecomp))
    {
        /* Save the last byte of the source buffer */
        Cpa32U numBuffers = pSrcBuff->numBuffers;
        Cpa32U srcLenInBytes = pSrcBuff->pBuffers[numBuffers-1].dataLenInBytes;

        while((0 == srcLenInBytes) && (numBuffers > 1))
        {
            numBuffers--;
            srcLenInBytes = pSrcBuff->pBuffers[numBuffers-1].dataLenInBytes;
        }

        if(srcLenInBytes > 0)
        {
            pCookie->srcBuffLastByte = pSrcBuff->pBuffers[numBuffers-1].
                pData[srcLenInBytes-1];
        }
        else
        {
            pCookie->srcBuffLastByte = 0;
        }

        /* Initialise the updateHistory parameter */
        pCookie->updateHistory = CPA_FALSE;
    }
#endif

    if((CPA_DC_STATEFUL == pSessionDesc->sessState)
     &&(CPA_DC_FLUSH_FINAL != flushFlag)
     &&(CPA_DC_FLUSH_FULL != flushFlag)
     &&(DC_DECOMPRESSION_REQUEST == compDecomp)
     &&(1 == (pCookie->srcTotalDataLenInBytes & 0x00000001)))
    {
        /* If the total length of the source buffer is odd then update it to
         * an even number */
        pCookie->srcTotalDataLenInBytes--;
    }

    pReqParams = pCookie->dcReqParamsBuffer;

    pMsg = (icp_qat_fw_la_bulk_req_t*) &pCookie->request;

    /* Populate the cmdFlags */
    if(CPA_DC_STATEFUL == pSessionDesc->sessState)
    {
        pSessionDesc->previousRequestType = pSessionDesc->requestType;

        sessType = ICP_QAT_FW_COMP_STATEFUL_SESSION;

        if(DC_REQUEST_FIRST == pSessionDesc->requestType)
        {
            /* Update the request type for following requests */
            pSessionDesc->requestType = DC_REQUEST_SUBSEQUENT;

            /* Reinitialise the cumulative amount of consumed bytes */
            pSessionDesc->cumulativeConsumedBytes = 0;
        }
        else
        {
            sop = ICP_QAT_FW_COMP_NOT_SOP;
        }

        if((CPA_DC_FLUSH_FINAL == flushFlag)||(CPA_DC_FLUSH_FULL == flushFlag))
        {
            /* Update the request type for following requests */
            pSessionDesc->requestType = DC_REQUEST_FIRST;
        }
        else
        {
            eop = ICP_QAT_FW_COMP_NOT_EOP;

#ifdef ICP_B0_SILICON
            pCookie->updateHistory = CPA_TRUE;
#endif
        }
    }

    switch(pSessionDesc->autoSelectBestHuffmanTree)
    {
        case CPA_DC_ASB_DISABLED:
            break;
        case CPA_DC_ASB_STATIC_DYNAMIC:
            autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
            break;
        case CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_STORED_HDRS:
            autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
            enhancedAutoSelectBest = ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST;
            break;
        case CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_NO_HDRS:
            autoSelectBest = ICP_QAT_FW_COMP_AUTO_SELECT_BEST;
            enhancedAutoSelectBest = ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST;
            disableType0EnhancedAutoSelectBest = 
                ICP_QAT_FW_COMP_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST;
            break;
        default:
            break;
    }

    cmdFlags = ICP_QAT_FW_COMP_FLAGS_BUILD(sop,
                       eop,
                       sessType,
                       autoSelectBest,
                       enhancedAutoSelectBest,
                       disableType0EnhancedAutoSelectBest);

    /* Walk the QAT slice chain for compression, excluding last (terminating)
     * entry */
    if(DC_COMPRESSION_REQUEST == compDecomp)
    {
        for (i = 0; (i < DC_MAX_NUM_QAT_SLICES_COMP - 1); i++)
        {
            if (ICP_QAT_FW_SLICE_COMP == pSessionDesc->qatSlicesComp[i])
            {
                icp_qat_fw_comp_req_params_t *pCompReqParams =
                    (icp_qat_fw_comp_req_params_t *)
                    (pReqParams + reqParamsOffset);

                LAC_ENSURE_NOT_NULL(pCompReqParams);

                dcCompRequestParamsPopulate(
                    pCompReqParams,
                    pSessionDesc->qatSlicesComp[i + 1],
                    pCookie,
                    pService);

                /* Update offset */
                reqParamsOffset += sizeof(*pCompReqParams);
            }
            else if (ICP_QAT_FW_SLICE_XLAT ==
                         pSessionDesc->qatSlicesComp[i])
            {
                CpaPhysicalAddr interBuff1PhyAddr = 0;
                CpaPhysicalAddr interBuff2PhyAddr = 0;
                CpaBoolean interBufferType = 0;

                icp_qat_fw_trans_req_params_t *pTransReqParams =
                    (icp_qat_fw_trans_req_params_t *)
                    (pReqParams + reqParamsOffset);

                LAC_ENSURE_NOT_NULL(pTransReqParams);

                if(CPA_FALSE == useDRAM)
                {
                    interBuff1PhyAddr = pService->interBuff1eSRAMPhyAddr;
                    interBuff2PhyAddr = pService->interBuff2eSRAMPhyAddr;
                    interBufferType = ICP_QAT_FW_INTER_USE_FLAT;
                }
                else
                {
                    interBuff1PhyAddr = pService->interBuff1PhyAddr;
                    interBuff2PhyAddr = pService->interBuff2PhyAddr;
                    interBufferType = ICP_QAT_FW_INTER_USE_SGL;
                }

                dcTransRequestParamsPopulate(
                    pTransReqParams,
                    pSessionDesc->qatSlicesComp[i + 1],
                    interBuff1PhyAddr,
                    interBuff2PhyAddr,
                    interBufferType);

                /* Update offset */
                reqParamsOffset += sizeof(*pTransReqParams);
            }
            /* Chain terminators */
            else if (ICP_QAT_FW_SLICE_DRAM_WR ==
                         pSessionDesc->qatSlicesComp[i])
            {
                /* End of chain */
                break;
            }
        } /* for (i = 0; ... */
    }
    else
    {
        /* Walk the QAT slice chain for decompression, excluding last
         * (terminating) entry */
        for (i = 0; (i < DC_MAX_NUM_QAT_SLICES_DECOMP - 1); i++)
        {
            if (ICP_QAT_FW_SLICE_COMP == pSessionDesc->qatSlicesDecomp[i])
            {
                icp_qat_fw_comp_req_params_t *pCompReqParams =
                    (icp_qat_fw_comp_req_params_t *)
                    (pReqParams + reqParamsOffset);

                LAC_ENSURE_NOT_NULL(pCompReqParams);

                dcCompRequestParamsPopulate(
                    pCompReqParams,
                    pSessionDesc->qatSlicesDecomp[i + 1],
                    pCookie,
                    pService);

                /* Update offset */
                reqParamsOffset += sizeof(*pCompReqParams);
            }
            /* Chain terminators */
            else if (ICP_QAT_FW_SLICE_DRAM_WR ==
                         pSessionDesc->qatSlicesDecomp[i])
            {
                /* End of chain */
                break;
            }
        } /* for (i = 0; ... */
    }

    if(DC_COMPRESSION_REQUEST == compDecomp)
    {
        pReqCache = &(pSessionDesc->reqCacheComp);
    }
    else
    {
        pReqCache = &(pSessionDesc->reqCacheDecomp);
    }

    pReqCache->flow_id = pSessionDesc->flowId;

    /* Fills in the initial 20 bytes of the ET ring message - cached from the
     * session descriptor */
    osalMemCopy((void*)pMsg, (void*)(pReqCache),
        SAL_SESSION_REQUEST_CACHE_SIZE_IN_BYTES);

    pMsg->comn_la_req.u.la_flags = cmdFlags;

    /* Populates the QAT common request middle part of the message
     * (LW 6 to 11) */
    LAC_MEM_SHARED_WRITE_FROM_PTR(
        pMsg->comn_mid.opaque_data, pCookie);

    pMsg->comn_mid.src_data_addr = srcAddrPhys;
    pMsg->comn_mid.dest_data_addr = dstAddrPhys;
    pMsg->req_params_addr = pCookie->dcReqParamsBufferPhyAddr;
    pMsg->comn_ftr.next_request_addr = 0;

    return CPA_STATUS_SUCCESS;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Send a compression request to QAT
 *
 * @description
 *      Send the requests for compression or decompression to QAT
 *
 * @param{in]   pCookie               Pointer to the compression cookie
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pSessionDesc          Pointer to the session descriptor
 * @param[in]   compDecomp            Direction of the operation
 *
 * @retval CPA_STATUS_SUCCESS         Function executed successfully
 * @retval CPA_STATUS_INVALID_PARAM   Invalid parameter passed in
 *
 *****************************************************************************/
STATIC CpaStatus
dcSendRequest(dc_compression_cookie_t* pCookie,
              sal_compression_service_t* pService,
              dc_session_desc_t* pSessionDesc,
              dc_request_dir_t compDecomp)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    /* Send the request */
    status = icp_adf_transPutMsg(pService->trans_handle_compression_tx,
                            (void*)&(pCookie->request),
                            LAC_QAT_MSG_SZ_LW);

    if(CPA_STATUS_SUCCESS != status)
    {
        if((CPA_DC_STATEFUL == pSessionDesc->sessState) &&
          (CPA_STATUS_RETRY == status))
        {
            /* reset requestType after recieving an retry on
             * the stateful request */
            pSessionDesc->requestType = pSessionDesc->previousRequestType;
        }

        /* Free the memory pool */
        if (NULL != pCookie)
        {
            Lac_MemPoolEntryFree(pCookie);
            pCookie = NULL;
        }

        /* The pending requests counter will be decremented in the main calling
         * function */
    }

    return status;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Process the synchronous and asynchronous case for compression or
 *      decompression
 *
 * @description
 *      Process the synchronous and asynchronous case for compression or
 *      decompression. This function will then create and send the request to
 *      the firmware.
 *
 * @param[in]   pService            Pointer to the compression service
 * @param[in]   pSessionDesc        Pointer to the session descriptor
 * @param[in]   dcInstance          Instance handle derived from discovery
 *                                  functions
 * @param[in]   pSessionHandle      Session handle
 * @param[in]   pSrcBuff            Pointer to data buffer for compression
 * @param[in]   pDestBuff           Pointer to buffer space for data after
 *                                  compression
 * @param[in]   pResults            Pointer to results structure
 * @param[in]   flushFlag           Indicates the type of flush to be
 *                                  performed
 * @param[in]   callbackTag         Pointer to the callback tag
 * @param[in]   compDecomp          Direction of the operation
 * @param[in]   isAsyncMode         Used to know if synchronous or asynchronous
 *                                  mode
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully
 * @retval CPA_STATUS_FAIL          Function failed
 * @retval CPA_STATUS_RESOURCE      Resource error
 *
 *****************************************************************************/
STATIC CpaStatus
dcCompDecompData(sal_compression_service_t* pService,
        dc_session_desc_t* pSessionDesc,
        CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle,
        CpaBufferList *pSrcBuff,
        CpaBufferList *pDestBuff,
        CpaDcRqResults *pResults,
        CpaDcFlush flushFlag,
        void *callbackTag,
        dc_request_dir_t compDecomp,
        CpaBoolean isAsyncMode)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    dc_compression_cookie_t *pCookie = NULL;

    if((LacSync_GenWakeupSyncCaller == pSessionDesc->pCompressionCb)
        && isAsyncMode == CPA_TRUE)
    {
        lac_sync_op_data_t *pSyncCallbackData = NULL;

        status = LacSync_CreateSyncCookie(&pSyncCallbackData);

        if(CPA_STATUS_SUCCESS == status)
        {
            status = dcCompDecompData(pService, pSessionDesc, dcInstance,
                         pSessionHandle, pSrcBuff, pDestBuff, pResults,
                         flushFlag, pSyncCallbackData, compDecomp, CPA_FALSE);
        }

        if(CPA_STATUS_SUCCESS == status)
        {
            CpaStatus syncStatus = CPA_STATUS_SUCCESS;

            syncStatus = LacSync_WaitForCallback(pSyncCallbackData,
                            DC_SYNC_CALLBACK_TIMEOUT,
                            &status, NULL);

            /* If callback doesn't come back */
            if(CPA_STATUS_SUCCESS != syncStatus)
            {
                if(DC_COMPRESSION_REQUEST == compDecomp)
                {
                    COMPRESSION_STAT_INC(numCompCompletedErrors, pService);
                }
                else
                {
                    COMPRESSION_STAT_INC(numDecompCompletedErrors, pService);
                }

                LAC_LOG_ERROR("Callback timed out");
                status = syncStatus;
            }
        }

        LacSync_DestroySyncCookie(&pSyncCallbackData);
        return status;
    }

    /* Allocate the compression cookie
     * The memory is freed in callback or in sendRequest if an error occurs
     */
    do {
        pCookie = (dc_compression_cookie_t*)
                                  Lac_MemPoolEntryAlloc(pService->compression_mem_pool);
        if (NULL == pCookie)
        {
            LAC_LOG_ERROR("Cannot get mem pool entry for compression");
            status = CPA_STATUS_RESOURCE;
        }
        else if ((void*)CPA_STATUS_RETRY == pCookie)
        {
            /* Give back the control to the OS */
            osalYield();
        }
    }while ((void*)CPA_STATUS_RETRY == pCookie);

    /* Initialise the addresses of the source and destination buffers
     * in the pCookie.
     */
    pCookie->pSourceBuffer = pSrcBuff;
    pCookie->pDestinationBuffer = pDestBuff;

    if(CPA_STATUS_SUCCESS == status)
    {
        status = dcCreateRequest(pCookie, pService,
                pSessionDesc, pSessionHandle, pSrcBuff, pDestBuff,
                pResults, flushFlag, callbackTag, compDecomp);
    }
    if(CPA_STATUS_SUCCESS == status)
    {
        status = dcSendRequest(pCookie, pService, pSessionDesc, compDecomp);
    }

    if(CPA_STATUS_SUCCESS == status)
    {
        if(DC_COMPRESSION_REQUEST == compDecomp)
        {
            COMPRESSION_STAT_INC(numCompRequests, pService);
        }
        else
        {
            COMPRESSION_STAT_INC(numDecompRequests, pService);
        }
    }
    else
    {
        if(DC_COMPRESSION_REQUEST == compDecomp)
        {
            COMPRESSION_STAT_INC(numCompRequestsErrors, pService);
        }
        else
        {
            COMPRESSION_STAT_INC(numDecompRequestsErrors, pService);
        }

        /* Decrement number of pending callbacks for session */
        if(CPA_DC_STATELESS == pSessionDesc->sessState)
        {
            osalAtomicDec(&(pSessionDesc->pendingStatelessCbCount));
        }
        else
        {
            osalAtomicDec(&(pSessionDesc->pendingStatefulCbCount));
        }
    }
    return status;
}

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Handle zero length compression or decompression requests
 *
 * @description
 *      Handle zero length compression or decompression requests
 *
 * @param[in]   pService              Pointer to the compression service
 * @param[in]   pSessionDesc          Pointer to the session descriptor
 * @param[in]   pResults              Pointer to results structure
 * @param[in]   flushFlag             Indicates the type of flush to be
 *                                    performed
 * @param[in]   callbackTag           User supplied value to help correlate
 *                                    the callback with its associated request
 * @param[in]   compDecomp            Direction of the operation
 *
 * @retval CPA_TRUE                   Zero length SOP or MOP processed
 * @retval CPA_FALSE                  Zero length EOP
 *
 *****************************************************************************/
STATIC CpaBoolean
dcZeroLengthRequests(sal_compression_service_t *pService,
        dc_session_desc_t *pSessionDesc,
        CpaDcRqResults *pResults,
        CpaDcFlush flushFlag,
        void *callbackTag,
        dc_request_dir_t compDecomp)
{
    CpaBoolean status = CPA_FALSE;
    CpaDcCallbackFn pCbFunc = pSessionDesc->pCompressionCb;

    if(DC_REQUEST_FIRST == pSessionDesc->requestType)
    {
        /* Update the request type for following requests */
        pSessionDesc->requestType = DC_REQUEST_SUBSEQUENT;

        /* Reinitialise the cumulative amount of consumed bytes */
        pSessionDesc->cumulativeConsumedBytes = 0;

        /* Zero length SOP */
        if(CPA_DC_ADLER32 == pSessionDesc->checksumType)
        {
            pResults->checksum = 1;
        }
        else
        {
            pResults->checksum = 0;
        }

        status = CPA_TRUE;
    }
    else if((CPA_DC_FLUSH_NONE == flushFlag) ||
            (CPA_DC_FLUSH_SYNC == flushFlag))
    {
        /* Zero length MOP */
        pResults->checksum = pSessionDesc->previousChecksum;
        status = CPA_TRUE;
    }

    if((CPA_DC_FLUSH_FINAL == flushFlag)||(CPA_DC_FLUSH_FULL == flushFlag))
    {
        /* Update the request type for following requests */
        pSessionDesc->requestType = DC_REQUEST_FIRST;
    }

    if(CPA_TRUE == status)
    {
        pResults->status = CPA_DC_OK;
        pResults->produced = 0;
        pResults->consumed = 0;

        /* Increment statistics */
        if(DC_COMPRESSION_REQUEST == compDecomp)
        {
            COMPRESSION_STAT_INC(numCompRequests, pService);
            COMPRESSION_STAT_INC(numCompCompleted, pService);
        }
        else
        {
            COMPRESSION_STAT_INC(numDecompRequests, pService);
            COMPRESSION_STAT_INC(numDecompCompleted, pService);
        }

        LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));

        if((NULL != pCbFunc) && (LacSync_GenWakeupSyncCaller != pCbFunc))
        {
            pCbFunc(callbackTag, CPA_STATUS_SUCCESS);
        }

        return CPA_TRUE;
    }

    return CPA_FALSE;
}

CpaStatus
cpaDcCompressData(CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle,
        CpaBufferList *pSrcBuff,
        CpaBufferList *pDestBuff,
        CpaDcRqResults *pResults,
        CpaDcFlush flushFlag,
        void *callbackTag)
{
    sal_compression_service_t* pService = NULL;
    dc_session_desc_t* pSessionDesc = NULL;
    CpaInstanceHandle insHandle = NULL;
    Cpa64U srcBuffSize = 0;

    if(CPA_INSTANCE_HANDLE_SINGLE == dcInstance)
    {
         insHandle = dcGetFirstHandle();
    }
    else
    {
         insHandle = dcInstance;
    }

    pService = (sal_compression_service_t*) insHandle;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(insHandle);
    SAL_CHECK_ADDR_TRANS_SETUP(insHandle);
#endif

    /* Check if SAL is initialised otherwise return an error */
    SAL_RUNNING_CHECK(insHandle);

    /* This check is outside the parameter checking as it is needed to manage
     * zero length requests */
    if(LacBuffDesc_BufferListVerifyNull(pSrcBuff, &srcBuffSize,
            LAC_NO_ALIGNMENT_SHIFT)
               != CPA_STATUS_SUCCESS)
    {
        LAC_INVALID_PARAM_LOG("Invalid source buffer list parameter");
        return CPA_STATUS_INVALID_PARAM;
    }

#ifdef ICP_PARAM_CHECK
    /* Ensure this is a compression instance */
    SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

    if(dcCheckCompressData(pService, pSessionHandle, pSrcBuff, pDestBuff,
        pResults, flushFlag, srcBuffSize, DC_COMPRESSION_REQUEST)
        != CPA_STATUS_SUCCESS)
    {
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

#ifdef ICP_PARAM_CHECK
    if(CPA_DC_DIR_DECOMPRESS == pSessionDesc->sessDirection)
    {
        LAC_INVALID_PARAM_LOG("Invalid sessDirection value");
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    if(CPA_DC_STATEFUL == pSessionDesc->sessState)
    {
        /* Lock the session to check if there are in-flight stateful requests */
        LAC_SPINLOCK(&(pSessionDesc->sessionLock));

        /* Check if there is already one in-flight stateful request */
        if(0 != osalAtomicGet(&(pSessionDesc->pendingStatefulCbCount)))
        {
            LAC_LOG_ERROR("Only one in-flight stateful request supported");
            LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
            return CPA_STATUS_RETRY;
        }

        if(0 == srcBuffSize)
        {
            if(CPA_TRUE == dcZeroLengthRequests(pService, pSessionDesc,
                    pResults, flushFlag, callbackTag, DC_COMPRESSION_REQUEST))
            {
                return CPA_STATUS_SUCCESS;
            }
        }

        osalAtomicInc(&(pSessionDesc->pendingStatefulCbCount));
        LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
    }
    else
    {
        osalAtomicInc(&(pSessionDesc->pendingStatelessCbCount));
    }

    return dcCompDecompData(pService, pSessionDesc, dcInstance,
               pSessionHandle, pSrcBuff, pDestBuff, pResults,
               flushFlag, callbackTag, DC_COMPRESSION_REQUEST, CPA_TRUE);
}

CpaStatus
cpaDcDecompressData(CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle,
        CpaBufferList *pSrcBuff,
        CpaBufferList *pDestBuff,
        CpaDcRqResults *pResults,
        CpaDcFlush flushFlag,
        void *callbackTag)
{
    sal_compression_service_t* pService = NULL;
    dc_session_desc_t* pSessionDesc = NULL;
    CpaInstanceHandle insHandle = NULL;
    Cpa64U srcBuffSize = 0;

    if(CPA_INSTANCE_HANDLE_SINGLE == dcInstance)
    {
         insHandle = dcGetFirstHandle();
    }
    else
    {
         insHandle = dcInstance;
    }

    pService = (sal_compression_service_t*) insHandle;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(insHandle);
    SAL_CHECK_ADDR_TRANS_SETUP(insHandle);
#endif

    /* Check if SAL is initialised otherwise return an error */
    SAL_RUNNING_CHECK(insHandle);

    /* This check is outside the parameter checking as it is needed to manage
     * zero length requests */
    if(LacBuffDesc_BufferListVerifyNull(pSrcBuff, &srcBuffSize,
            LAC_NO_ALIGNMENT_SHIFT)
               != CPA_STATUS_SUCCESS)
    {
        LAC_INVALID_PARAM_LOG("Invalid source buffer list parameter");
        return CPA_STATUS_INVALID_PARAM;
    }

#ifdef ICP_PARAM_CHECK
    /* Ensure this is a compression instance */
    SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

    if(dcCheckCompressData(pService, pSessionHandle, pSrcBuff, pDestBuff,
           pResults, flushFlag, srcBuffSize, DC_DECOMPRESSION_REQUEST)
           != CPA_STATUS_SUCCESS)
    {
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    pSessionDesc = DC_SESSION_DESC_FROM_CTX_GET(pSessionHandle);

#ifdef ICP_PARAM_CHECK
    if(CPA_DC_DIR_COMPRESS == pSessionDesc->sessDirection)
    {
        LAC_INVALID_PARAM_LOG("Invalid sessDirection value");
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    if(CPA_DC_STATEFUL == pSessionDesc->sessState)
    {
        /* Lock the session to check if there are in-flight stateful requests */
         LAC_SPINLOCK(&(pSessionDesc->sessionLock));

        /* Check if there is already one in-flight stateful request */
        if(0 != osalAtomicGet(&(pSessionDesc->pendingStatefulCbCount)))
        {
            LAC_LOG_ERROR("Only one in-flight stateful request supported");
            LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
            return CPA_STATUS_RETRY;
        }

        if(0 == srcBuffSize)
        {
            if(CPA_TRUE == dcZeroLengthRequests(pService, pSessionDesc,
                    pResults, flushFlag, callbackTag, DC_DECOMPRESSION_REQUEST))
            {
                return CPA_STATUS_SUCCESS;
            }
        }

        osalAtomicInc(&(pSessionDesc->pendingStatefulCbCount));
        LAC_SPINUNLOCK(&(pSessionDesc->sessionLock));
    }
    else
    {
        osalAtomicInc(&(pSessionDesc->pendingStatelessCbCount));
    }

    return dcCompDecompData(pService, pSessionDesc, dcInstance,
               pSessionHandle, pSrcBuff, pDestBuff, pResults,
               flushFlag, callbackTag, DC_DECOMPRESSION_REQUEST, CPA_TRUE);
}

CpaStatus
cpaDcBufferListGetMetaSize(const CpaInstanceHandle instanceHandle,
        Cpa32U numBuffers,
        Cpa32U *pSizeInBytes)
{
#ifdef ICP_PARAM_CHECK
    CpaInstanceHandle insHandle = NULL;

    if(CPA_INSTANCE_HANDLE_SINGLE == instanceHandle)
    {
         insHandle = dcGetFirstHandle();
    }
    else
    {
         insHandle = instanceHandle;
    }

    LAC_CHECK_INSTANCE_HANDLE(insHandle);
    LAC_CHECK_NULL_PARAM(pSizeInBytes);

    /* Ensure this is a compression instance */
    SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

    if(0 == numBuffers)
    {
        LAC_INVALID_PARAM_LOG("Number of Buffers");
        return CPA_STATUS_INVALID_PARAM;
    }
#endif

    /* icp_buffer_list_desc_t is 8 bytes in size and icp_flat_buffer_desc_t
     * is 16 bytes in size. Therefore if icp_buffer_list_desc_t is aligned
     * so will each icp_flat_buffer_desc_t structure */
    /* The size of an icp_buffer_list_desc_t is added as well as the size of the
     * appropriate number of icp_flat_buffer_desc_t. This is to allow the
     * processing of the intermediate buffers */

    *pSizeInBytes =  (DC_NUM_INTER_BUFFERS * sizeof(icp_buffer_list_desc_t)) +
                     (sizeof(icp_flat_buffer_desc_t) *
                         (numBuffers + DC_NUM_INTER_BUFFERS - 1)) +
                     ICP_DESCRIPTOR_ALIGNMENT_BYTES;

    return CPA_STATUS_SUCCESS;
}
