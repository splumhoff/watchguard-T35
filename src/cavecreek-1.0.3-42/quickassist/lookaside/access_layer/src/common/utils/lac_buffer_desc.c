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
 * @file lac_buffer_desc.c  Utility functions for setting buffer descriptors
 *
 * @ingroup LacBufferDesc
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include header files
*******************************************************************************
*/
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "icp_adf_init.h"
#include "lac_list.h"
#include "lac_sal_types.h"
#include "icp_bnp_buffer_desc.h"
#include "cpa_dc_bp.h"
#include "lac_mem.h"
#include "cpa_cy_common.h"
#include "dc_session.h"

/*
*******************************************************************************
* Define public/global function definitions
*******************************************************************************
*/
/* Invalid physical address value */
#define INVALID_PHYSICAL_ADDRESS       0


/* This function implements the buffer description writes for the BnP APIs */
CpaStatus
LacBuffDesc_BnpOpListDescWrite(const CpaDcBatchOpData *pBatchOpData,
                               const Cpa32U numRequests,
                               Cpa64U *pBatchOpDataPhyAddr,
                               CpaDcRqResults *pResults,
                               const CpaDcChecksum checksumType,
                               sal_service_t* pService)
{
    Cpa32U i = 0;
    Cpa32U numFlatBuffers = 0;
    Cpa64U totalDataLenInBytes = 0;
    CpaBufferList *pUserBufferList = NULL;
    icp_qat_addr_width_t bnpOpDataListAlignedPhyAddr = 0;
    icp_qat_addr_width_t firstBufListAlignedPhyAddr = 0;
    icp_qat_addr_width_t bufListDescPhyAddr = 0;
    icp_bnp_buffer_list_desc_t *pBnpBufferListDesc = NULL;
    icp_buffer_list_desc_t *pBufferListDesc = NULL;
    icp_qat_fw_comp_bnp_op_header_t *pCurrDescHeader = NULL;
    icp_qat_fw_comp_bnp_op_header_t *pPrevDescHeader = NULL;
    icp_qat_fw_comp_bnp_skip_info_t *pBnpSkipInfo = NULL;
    CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
    icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;
    icp_qat_fw_comp_bnp_out_tbl_entry_t *bnpResultTable = NULL;

    /* Check input parameters */
    if (0 == numRequests)
    {
        LAC_LOG_ERROR("Number of Batch request must be > 0");
        return CPA_STATUS_INVALID_PARAM;
    }
    LAC_ENSURE_NOT_NULL(pBatchOpDataPhyAddr);

    for (i=0; i<numRequests; i++)
    {
        /* Parameter checking on all the Op data in the batch */
        LAC_ENSURE_NOT_NULL(pBatchOpData[i]);
        pUserBufferList = pBatchOpData[i].pSrcBuff;
        LAC_ENSURE_NOT_NULL(pUserBufferList);
        LAC_ENSURE_NOT_NULL(pUserBufferList->pBuffers);
        LAC_ENSURE_NOT_NULL(pUserBufferList->pPrivateMetaData);
        LAC_ENSURE_NOT_NULL(pResults[i]);

        /* Retrieve descriptor
         * Get the physical address of this descriptor - need to offset by the
         * alignment restrictions on the buffer descriptors
         */
        bufListDescPhyAddr = (icp_qat_addr_width_t)
                              LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                                      pUserBufferList->pPrivateMetaData);
        if (INVALID_PHYSICAL_ADDRESS == bufListDescPhyAddr)
        {
            LAC_LOG_ERROR("Unable to get the physical address of the metadata\n");
            return CPA_STATUS_FAIL;
        }

        bnpOpDataListAlignedPhyAddr =
                LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
                                       ICP_DESCRIPTOR_ALIGNMENT_BYTES);

        pBnpBufferListDesc = (icp_bnp_buffer_list_desc_t *)(LAC_ARCH_UINT)
                            ((LAC_ARCH_UINT)pUserBufferList->pPrivateMetaData +
                             ((LAC_ARCH_UINT)bnpOpDataListAlignedPhyAddr
                               - (LAC_ARCH_UINT)bufListDescPhyAddr));

        pBufferListDesc = &pBnpBufferListDesc->bufferListDesc;
        /* Go past the Buffer List descriptor to the list of buffer descriptors */
        pCurrFlatBufDesc = (icp_flat_buffer_desc_t *)(
                                (pBufferListDesc->phyBuffers));

        pCurrDescHeader = &pBnpBufferListDesc->bnpOpData.bnp_op_header;

        /* Record address of first job */
        if (0 == i)
        {
            firstBufListAlignedPhyAddr = bnpOpDataListAlignedPhyAddr;
            /* Initialize previous descriptor header */
            pPrevDescHeader = pCurrDescHeader;
        }

        /* Initialize next_op_data_addr of previous job in the batch */
        pPrevDescHeader->next_opdata_addr = bnpOpDataListAlignedPhyAddr;

        /* This is the last request, therefore next address is NULL */
        if ((numRequests-1) == i)
        {
            pCurrDescHeader->next_opdata_addr = 0;
        }

        /* Go through all the flat buffers and extract
         * corresponding physical address */
        numFlatBuffers = pUserBufferList->numBuffers;
        /* Defining zero buffers is useful for example if running zero length
         * hash */
        if(0 == numFlatBuffers)
        {
            /* In the case where there are zero buffers within the buffer list
             * it is required by firmware that the number is set to 1
             * but the phyBuffer and dataLenInBytes are set to NULL.*/
            pBufferListDesc->numBuffers = 1;
            pCurrFlatBufDesc->dataLenInBytes = 0;
            pCurrFlatBufDesc->phyBuffer = 0;
        }
        /* Update number of buffers in SGL descriptor */
        pBufferListDesc->numBuffers = numFlatBuffers;

        pCurrClientFlatBuffer = pUserBufferList->pBuffers;
        totalDataLenInBytes = 0;
        while(0 != numFlatBuffers)
        {
            pCurrFlatBufDesc->dataLenInBytes =
                    pCurrClientFlatBuffer->dataLenInBytes;

            /* Calculate the total data length in bytes */
            totalDataLenInBytes += pCurrClientFlatBuffer->dataLenInBytes;

            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                    LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                            pCurrClientFlatBuffer->pData));

            if (pCurrFlatBufDesc->phyBuffer == 0)
            {
                LAC_LOG_ERROR("Unable to get the physical address of the "
                        "client buffer\n");
                return CPA_STATUS_FAIL;
            }

            pCurrFlatBufDesc++;
            pCurrClientFlatBuffer++;
            numFlatBuffers--;
        }

        /* Update input skip mode*/
        pBnpSkipInfo = &pBnpBufferListDesc->bnpOpData.src_bnp_skip_info;
        pBnpSkipInfo->firstSkipOffset =
                pBatchOpData[i].opData.inputSkipData.firstSkipOffset;
        pBnpSkipInfo->skip_length =
                pBatchOpData[i].opData.inputSkipData.skipLength;
        pBnpSkipInfo->stride_length =
                pBatchOpData[i].opData.inputSkipData.strideLength;
        pBnpSkipInfo->bnp_flags =
                ((pBatchOpData[i].resetSessionState             & 0x1)    ) |
                ((pBatchOpData[i].opData.inputSkipData.skipMode & 0x3) <<1) |
                ((pBatchOpData[i].opData.flushFlag              & 0x3) <<3);

        /* Update output skip mode*/
        pBnpSkipInfo = &pBnpBufferListDesc->bnpOpData.dst_bnp_skip_info;
        pBnpSkipInfo->firstSkipOffset =
                pBatchOpData[i].opData.outputSkipData.firstSkipOffset;
        pBnpSkipInfo->skip_length =
                pBatchOpData[i].opData.outputSkipData.skipLength;
        pBnpSkipInfo->stride_length =
                pBatchOpData[i].opData.outputSkipData.strideLength;
        pBnpSkipInfo->bnp_flags =
                (pBatchOpData[i].opData.outputSkipData.skipMode & 0x3) <<1;

        /* Update compression length */
        pCurrDescHeader->comp_len = totalDataLenInBytes;

        /* Update checksums in response tables */
        bnpResultTable = &pBnpBufferListDesc->bnpOutTable;
        if (CPA_DC_ADLER32 == checksumType)
        {
            bnpResultTable->comp_out_pars.curr_adler_32 =
                    pResults[i].checksum;
        }
        else if (CPA_DC_CRC32 == checksumType)
        {
            bnpResultTable->comp_out_pars.curr_crc32 =
                    pResults[i].checksum;
        }

        /* Update previous descriptor header */
        pPrevDescHeader = pCurrDescHeader;
    }

    /* Return address of first descriptor */
    *pBatchOpDataPhyAddr = (Cpa64U)firstBufListAlignedPhyAddr;

    return CPA_STATUS_SUCCESS;
}

/* This function implements the virtual to Physical address translation for
 * results of the BnP APIs. It is called from the callback and the CpaDcRqResults
 * is filled with compression job statistics. */
CpaStatus
LacBuffDesc_BnpRespResultsRead(const CpaDcBatchOpData *pBatchOpData,
                               const Cpa32U numRequests,
                               CpaDcRqResults *pResults,
                               CpaDcChecksum checksumType,
                               sal_service_t *pService)
{
    Cpa32U i = 0;
    CpaBufferList *pUserBufferList = NULL;
    icp_qat_addr_width_t bnpOpDataListAlignedPhyAddr = 0;
    icp_qat_addr_width_t bufListDescPhyAddr = 0;
    icp_bnp_buffer_list_desc_t *pBnpBufferListDesc = NULL;
    icp_qat_fw_comp_bnp_out_tbl_entry_t *bnpResultTableDesc = NULL;
#ifndef ICP_DC_DYN_NOT_SUPPORTED
    Cpa8U cmpErr = ERR_CODE_NO_ERROR, xlatErr = ERR_CODE_NO_ERROR;
#else
    Cpa8U cmpErr = ERR_CODE_NO_ERROR;
#endif
    CpaBoolean cmpPass = CPA_TRUE, xlatPass = CPA_TRUE;
    Cpa8U opStatus = ICP_QAT_FW_COMN_STATUS_FLAG_OK;

    /* Check input parameters */
    if (0 == numRequests)
    {
        LAC_LOG_ERROR("Number of Batch request must be > 0");
        return CPA_STATUS_INVALID_PARAM;
    }

    for (i=0; i<numRequests; i++)
    {
        /* Parameter checking on all the Op data in the batch */
        LAC_ENSURE_NOT_NULL(pBatchOpData[i]);
        LAC_ENSURE_NOT_NULL(pResults[i]);
        pUserBufferList = pBatchOpData[i].pSrcBuff;
        LAC_ENSURE_NOT_NULL(pUserBufferList);
        LAC_ENSURE_NOT_NULL(pUserBufferList->pPrivateMetaData);

        /* Retrieve descriptor
         * Get the physical address of this descriptor - need to offset by the
         * alignment restrictions on the buffer descriptors
         */
        bufListDescPhyAddr = (icp_qat_addr_width_t)
                              LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                                      pUserBufferList->pPrivateMetaData);
        if (INVALID_PHYSICAL_ADDRESS == bufListDescPhyAddr)
        {
            LAC_LOG_ERROR("Unable to get the physical address of the metadata\n");
            return CPA_STATUS_FAIL;
        }

        bnpOpDataListAlignedPhyAddr =
                LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
                                       ICP_DESCRIPTOR_ALIGNMENT_BYTES);
        pBnpBufferListDesc = (icp_bnp_buffer_list_desc_t *)(LAC_ARCH_UINT)
                            ((LAC_ARCH_UINT)pUserBufferList->pPrivateMetaData +
                             ((LAC_ARCH_UINT)bnpOpDataListAlignedPhyAddr
                               - (LAC_ARCH_UINT)bufListDescPhyAddr));
        bnpResultTableDesc = &pBnpBufferListDesc->bnpOutTable;

        if(CPA_DC_CRC32 == checksumType)
        {
            pResults[i].checksum =
                    bnpResultTableDesc->comp_out_pars.curr_crc32;
        }
        else if(CPA_DC_ADLER32 == checksumType)
        {
            pResults[i].checksum =
                    bnpResultTableDesc->comp_out_pars.curr_adler_32;
        }

        opStatus = bnpResultTableDesc->comn_status;
        /* Check compression response status */
        cmpPass = (CpaBoolean)(ICP_QAT_FW_COMN_STATUS_FLAG_OK ==
                ICP_QAT_FW_COMN_RESP_CMP_STAT_GET(opStatus));

        /* Get the cmp error code */
        cmpErr = bnpResultTableDesc->comn_error.s1.cmp_err_code;

        /* We return the compression error code for now. We would need to update
         * the API if we decide to return both error codes */
        pResults[i].status = (Cpa8S)cmpErr;

        if(CPA_DC_OVERFLOW == (Cpa8S)cmpErr)
            cmpPass = CPA_TRUE;

#ifndef ICP_DC_DYN_NOT_SUPPORTED
        if(CPA_DC_OVERFLOW == (Cpa8S)xlatErr)
            xlatPass = CPA_TRUE;
#endif
        if((CPA_TRUE == cmpPass) && (CPA_TRUE == xlatPass))
        {
            /* Extract the response from the firmware */
            pResults[i].consumed = bnpResultTableDesc->comp_out_pars.input_byte_counter;
            pResults[i].produced = bnpResultTableDesc->comp_out_pars.output_byte_counter;
        }
        else
        {
#ifdef ICP_DC_RETURN_COUNTERS_ON_ERROR
            /* Extract the response from the firmware */
            pResults[i].consumed = pCompRespMsg->comp_resp_pars.input_byte_counter;
            pResults[i].produced = pCompRespMsg->comp_resp_pars.output_byte_counter;
#else
            pResults[i].consumed = 0;
            pResults[i].produced = 0;
#endif
        }
    }

    return CPA_STATUS_SUCCESS;
}

/* This function implements the buffer description writes for the traditional APIs */
CpaStatus
LacBuffDesc_BufferListDescWrite(const CpaBufferList *pUserBufferList,
                                Cpa64U *pBufListAlignedPhyAddr,
                                CpaBoolean isPhysicalAddress,
                                sal_service_t* pService)
{
    Cpa32U numBuffers = 0;
    icp_qat_addr_width_t bufListDescPhyAddr = 0;
    icp_qat_addr_width_t bufListAlignedPhyAddr = 0;
    CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
    icp_buffer_list_desc_t *pBufferListDesc = NULL;
    icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;

    LAC_ENSURE_NOT_NULL(pUserBufferList);
    LAC_ENSURE_NOT_NULL(pUserBufferList->pBuffers);
    LAC_ENSURE_NOT_NULL(pUserBufferList->pPrivateMetaData);
    LAC_ENSURE_NOT_NULL(pBufListAlignedPhyAddr);

    numBuffers = pUserBufferList->numBuffers;
    pCurrClientFlatBuffer = pUserBufferList->pBuffers;

    /*
     * Get the physical address of this descriptor - need to offset by the
     * alignment restrictions on the buffer descriptors
     */
    bufListDescPhyAddr = (icp_qat_addr_width_t)
                          LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                                  pUserBufferList->pPrivateMetaData);

    if (INVALID_PHYSICAL_ADDRESS == bufListDescPhyAddr)
    {
        LAC_LOG_ERROR("Unable to get the physical address of the metadata\n");
        return CPA_STATUS_FAIL;
    }

    bufListAlignedPhyAddr = LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
                                    ICP_DESCRIPTOR_ALIGNMENT_BYTES);

    pBufferListDesc = (icp_buffer_list_desc_t *)(LAC_ARCH_UINT)
                        ((LAC_ARCH_UINT)pUserBufferList->pPrivateMetaData +
                         ((LAC_ARCH_UINT)bufListAlignedPhyAddr
                           - (LAC_ARCH_UINT)bufListDescPhyAddr));

    /* Go past the Buffer List descriptor to the list of buffer descriptors */
    pCurrFlatBufDesc = (icp_flat_buffer_desc_t *)(
                            (pBufferListDesc->phyBuffers));

    pBufferListDesc->numBuffers = numBuffers;

    /* Defining zero buffers is useful for example if running zero length
     * hash */
    if(0 == numBuffers)
    {
        /* In the case where there are zero buffers within the BufList
         * it is required by firmware that the number is set to 1
         * but the phyBuffer and dataLenInBytes are set to NULL.*/
         pBufferListDesc->numBuffers = 1;
         pCurrFlatBufDesc->dataLenInBytes = 0;
         pCurrFlatBufDesc->phyBuffer = 0;
    }

    while (0 != numBuffers)
    {
        pCurrFlatBufDesc->dataLenInBytes =
            pCurrClientFlatBuffer->dataLenInBytes;

        /* Check if providing a physical address in the function. If not we
         * need to convert it to a physical one */
        if (CPA_TRUE == isPhysicalAddress)
        {
            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                    (LAC_ARCH_UINT)(pCurrClientFlatBuffer->pData));
        }
        else {
            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                            LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                                              pCurrClientFlatBuffer->pData));

            if (INVALID_PHYSICAL_ADDRESS == pCurrFlatBufDesc->phyBuffer)
            {
                LAC_LOG_ERROR("Unable to get the physical address of the "
                    "client buffer\n");
                return CPA_STATUS_FAIL;
            }
        }

        pCurrFlatBufDesc++;
        pCurrClientFlatBuffer++;

        numBuffers--;
    }

    *pBufListAlignedPhyAddr = bufListAlignedPhyAddr;
    return CPA_STATUS_SUCCESS;
}

/* This function does the same processing as LacBuffDesc_BufferListDescWrite
 * but calculate as well the total length in bytes of the buffer list. */
CpaStatus
LacBuffDesc_BufferListDescWriteAndGetSize(const CpaBufferList *pUserBufferList,
                                 Cpa64U *pBufListAlignedPhyAddr,
                                 CpaBoolean isPhysicalAddress,
                                 Cpa64U *totalDataLenInBytes,
                                 sal_service_t *pService)
{
    Cpa32U numBuffers = 0;
    icp_qat_addr_width_t bufListDescPhyAddr = 0;
    icp_qat_addr_width_t bufListAlignedPhyAddr = 0;
    CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
    icp_buffer_list_desc_t *pBufferListDesc = NULL;
    icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;
    *totalDataLenInBytes = 0;

    LAC_ENSURE_NOT_NULL(pUserBufferList);
    LAC_ENSURE_NOT_NULL(pUserBufferList->pBuffers);
    LAC_ENSURE_NOT_NULL(pUserBufferList->pPrivateMetaData);
    LAC_ENSURE_NOT_NULL(pBufListAlignedPhyAddr);

    numBuffers = pUserBufferList->numBuffers;
    pCurrClientFlatBuffer = pUserBufferList->pBuffers;

    /*
     * Get the physical address of this descriptor - need to offset by the
     * alignment restrictions on the buffer descriptors
     */
    bufListDescPhyAddr = (icp_qat_addr_width_t)
                          LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                                  pUserBufferList->pPrivateMetaData);

    if (INVALID_PHYSICAL_ADDRESS == bufListDescPhyAddr)
    {
        LAC_LOG_ERROR("Unable to get the physical address of the metadata\n");
        return CPA_STATUS_FAIL;
    }

    bufListAlignedPhyAddr = LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
                                    ICP_DESCRIPTOR_ALIGNMENT_BYTES);

    pBufferListDesc = (icp_buffer_list_desc_t *)(LAC_ARCH_UINT)
                        ((LAC_ARCH_UINT)pUserBufferList->pPrivateMetaData +
                         ((LAC_ARCH_UINT)bufListAlignedPhyAddr
                           - (LAC_ARCH_UINT)bufListDescPhyAddr));

    /* Go past the Buffer List descriptor to the list of buffer descriptors */
    pCurrFlatBufDesc = (icp_flat_buffer_desc_t *)(
                            (pBufferListDesc->phyBuffers));

    pBufferListDesc->numBuffers = numBuffers;

    while(0 != numBuffers)
    {
        pCurrFlatBufDesc->dataLenInBytes =
            pCurrClientFlatBuffer->dataLenInBytes;

        /* Calculate the total data length in bytes */
        *totalDataLenInBytes += pCurrClientFlatBuffer->dataLenInBytes;

        if (isPhysicalAddress == CPA_TRUE)
        {
            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                    (LAC_ARCH_UINT)(pCurrClientFlatBuffer->pData));
        }
        else
        {
            pCurrFlatBufDesc->phyBuffer = LAC_MEM_CAST_PTR_TO_UINT64(
                            LAC_OS_VIRT_TO_PHYS_EXTERNAL((*pService),
                                            pCurrClientFlatBuffer->pData));

            if (INVALID_PHYSICAL_ADDRESS == pCurrFlatBufDesc->phyBuffer)
            {
                LAC_LOG_ERROR("Unable to get the physical address of the "
                    "client buffer\n");
                return CPA_STATUS_FAIL;
            }

        }

        pCurrFlatBufDesc++;
        pCurrClientFlatBuffer++;

        numBuffers--;
    }

    *pBufListAlignedPhyAddr = bufListAlignedPhyAddr;
    return CPA_STATUS_SUCCESS;
}

CpaStatus
LacBuffDesc_FlatBufferVerify(const CpaFlatBuffer *pUserFlatBuffer,
        Cpa64U *pPktSize, lac_aligment_shift_t alignmentShiftExpected)
{
    LAC_CHECK_NULL_PARAM(pUserFlatBuffer);
    LAC_ENSURE_NOT_NULL(pPktSize);
    LAC_CHECK_NULL_PARAM(pUserFlatBuffer->pData);

    if(0 == pUserFlatBuffer->dataLenInBytes)
    {
        LAC_INVALID_PARAM_LOG("FlatBuffer empty");
        return CPA_STATUS_INVALID_PARAM;
    }

    /* Expected alignment */
    if (LAC_NO_ALIGNMENT_SHIFT != alignmentShiftExpected)
    {
        if (!LAC_ADDRESS_ALIGNED(pUserFlatBuffer->pData,
                alignmentShiftExpected))
        {
            LAC_INVALID_PARAM_LOG1("FlatBuffer not aligned as expected."
                   " Expected alignment %u Bytes.",
                   (uintptr_t)(1 << alignmentShiftExpected));
            return CPA_STATUS_INVALID_PARAM;
        }
    }

    /* Update the total size of the packet. This function being called in a loop
     * for an entire buffer list we need to increment the value */
    *pPktSize += pUserFlatBuffer->dataLenInBytes;

    return CPA_STATUS_SUCCESS;
}

CpaStatus
LacBuffDesc_FlatBufferVerifyNull(const CpaFlatBuffer *pUserFlatBuffer,
        Cpa64U *pPktSize, lac_aligment_shift_t alignmentShiftExpected)
{
    LAC_CHECK_NULL_PARAM(pUserFlatBuffer);
    LAC_ENSURE_NOT_NULL(pPktSize);

    if(0 != pUserFlatBuffer->dataLenInBytes)
    {
        LAC_CHECK_NULL_PARAM(pUserFlatBuffer->pData);
    }

    /* Expected alignment */
    if (LAC_NO_ALIGNMENT_SHIFT != alignmentShiftExpected)
    {
        if (!LAC_ADDRESS_ALIGNED(pUserFlatBuffer->pData,
                alignmentShiftExpected))
        {
            LAC_INVALID_PARAM_LOG1("FlatBuffer not aligned as expected."
                   " Expected alignment %u Bytes.",
                   (uintptr_t)(1 << alignmentShiftExpected));
            return CPA_STATUS_INVALID_PARAM;
        }
    }

    /* Update the total size of the packet. This function being called in a loop
     * for an entire buffer list we need to increment the value */
    *pPktSize += pUserFlatBuffer->dataLenInBytes;

    return CPA_STATUS_SUCCESS;
}

CpaStatus
LacBuffDesc_BufferListVerify(const CpaBufferList *pUserBufferList,
        Cpa64U *pPktSize, lac_aligment_shift_t alignmentShiftExpected)
{
    CpaFlatBuffer     *pCurrClientFlatBuffer = NULL;
    Cpa32U             numBuffers = 0;
    CpaStatus          status = CPA_STATUS_SUCCESS;

    LAC_CHECK_NULL_PARAM(pUserBufferList);
    LAC_CHECK_NULL_PARAM(pUserBufferList->pBuffers);
    LAC_ENSURE_NOT_NULL(pPktSize);
    LAC_CHECK_NULL_PARAM(pUserBufferList->pPrivateMetaData);

    numBuffers = pUserBufferList->numBuffers;

    if (0 == pUserBufferList->numBuffers)
    {
        LAC_INVALID_PARAM_LOG("Number of Buffers");
        return CPA_STATUS_INVALID_PARAM;
    }

    pCurrClientFlatBuffer = pUserBufferList->pBuffers;

    *pPktSize = 0;
    while (0 != numBuffers && status == CPA_STATUS_SUCCESS)
    {
        status = LacBuffDesc_FlatBufferVerify(pCurrClientFlatBuffer,
                      pPktSize, alignmentShiftExpected);

        pCurrClientFlatBuffer++;
        numBuffers--;
    }
    return status;
}

CpaStatus
LacBuffDesc_BufferListVerifyNull(const CpaBufferList *pUserBufferList,
        Cpa64U *pPktSize, lac_aligment_shift_t alignmentShiftExpected)
{
    CpaFlatBuffer     *pCurrClientFlatBuffer = NULL;
    Cpa32U             numBuffers = 0;
    CpaStatus          status = CPA_STATUS_SUCCESS;

    LAC_CHECK_NULL_PARAM(pUserBufferList);
    LAC_CHECK_NULL_PARAM(pUserBufferList->pBuffers);
    LAC_ENSURE_NOT_NULL(pPktSize);
    LAC_CHECK_NULL_PARAM(pUserBufferList->pPrivateMetaData);

    numBuffers = pUserBufferList->numBuffers;

    if (0 == numBuffers)
    {
        LAC_INVALID_PARAM_LOG("Number of Buffers");
        return CPA_STATUS_INVALID_PARAM;
    }

    pCurrClientFlatBuffer = pUserBufferList->pBuffers;

    *pPktSize = 0;
    while (0 != numBuffers && status == CPA_STATUS_SUCCESS)
    {
        status = LacBuffDesc_FlatBufferVerifyNull(pCurrClientFlatBuffer,
                      pPktSize, alignmentShiftExpected);

        pCurrClientFlatBuffer++;
        numBuffers--;
    }
    return status;
}

/**
 ******************************************************************************
 * @ingroup LacBufferDesc
 *****************************************************************************/
void
LacBuffDesc_BufferListTotalSizeGet(
    const CpaBufferList *pUserBufferList,
    Cpa64U *pPktSize)
{
    CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
    Cpa32U numBuffers = 0;

    LAC_ENSURE_NOT_NULL(pUserBufferList);
    LAC_ENSURE_NOT_NULL(pPktSize);
    LAC_ENSURE_NOT_NULL(pUserBufferList->pBuffers);

    pCurrClientFlatBuffer = pUserBufferList->pBuffers;
    numBuffers = pUserBufferList->numBuffers;

    *pPktSize = 0;
    while (0 != numBuffers)
    {
        *pPktSize += pCurrClientFlatBuffer->dataLenInBytes;
        pCurrClientFlatBuffer++;
        numBuffers--;
    }
}


void
LacBuffDesc_BufferListZeroFromOffset(CpaBufferList *pBuffList,
                                     Cpa32U offset, Cpa32U lenToZero)
{
    Cpa32U zeroLen = 0, sizeLeftToZero = 0;
    Cpa64U currentBufferSize = 0;
    CpaFlatBuffer* pBuffer = NULL;
    Cpa8U* pZero = NULL;
    pBuffer = pBuffList->pBuffers;

    /* Take a copy of total length to zero. */
    sizeLeftToZero = lenToZero;

    while(sizeLeftToZero > 0)
    {
        currentBufferSize = pBuffer->dataLenInBytes;
        /* check where to start zeroing */
        if(offset >= currentBufferSize)
        {
            /* Need to get to next buffer and reduce
             * offset size by data len of buffer */
            pBuffer++;
            offset = offset - pBuffer->dataLenInBytes;
        }
        else
        {
            /* Start to Zero from this position */
            pZero = (Cpa8U*) pBuffer->pData + offset;

            /* Need to calculate the correct number of bytes to zero
             * for this iteration and for this location.
             */
            if (sizeLeftToZero >= pBuffer->dataLenInBytes)
            {
                /* The size to zero is spanning buffers, zeroLen in
                 * this case is from pZero (position) to end of buffer.
                 */
                zeroLen = pBuffer->dataLenInBytes - offset;
            }
            else
            {
                /* zeroLen is set to sizeLeftToZero, then check if zeroLen and
                 * the offset is greater or equal to the size of the buffer, if
                 * yes, adjust the zeroLen to zero out the remainder of this
                 * buffer.
                 */
                zeroLen = sizeLeftToZero;
                if ((zeroLen + offset) >= pBuffer->dataLenInBytes)
                {
                    zeroLen = pBuffer->dataLenInBytes - offset;
                }
            } /* end inner else */
            osalMemSet((void*)pZero, 0, zeroLen);
            sizeLeftToZero = sizeLeftToZero - zeroLen;
            /* offset is no longer required as any data left to zero is now
             * at the start of the next buffer. set offset to zero and move on
             * the buffer pointer to the next buffer.
             */
            offset = 0;
            pBuffer++;

        } /* end outer else */

    } /* end while */

}



