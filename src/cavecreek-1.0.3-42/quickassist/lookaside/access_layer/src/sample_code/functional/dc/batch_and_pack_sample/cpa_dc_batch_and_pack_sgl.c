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

#include "cpa_sample_utils.h"
#include "cpa_dc.h"

CpaStatus
dcBuildBnpBufferList(CpaBufferList **testBufferList, Cpa32U numberChains,
                  Cpa32U sizeOfPkt, Cpa32U metaSize)
{
    CpaFlatBuffer *pFlatBuff = NULL;
    Cpa32U currentChain = 0;
    Cpa8U *pMsg = NULL;
    CpaBufferList *pBuffList = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;

    status = OS_MALLOC(&pBuffList, sizeof(CpaBufferList));
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error allocating pBuffList\n");
        return CPA_STATUS_FAIL;
    }
    pBuffList->numBuffers = numberChains;

    if (metaSize)
    {
        PHYS_CONTIG_ALLOC(&pBuffList->pPrivateMetaData, metaSize);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Error allocating pBuffList->pPrivateMetaData\n");
            PHYS_CONTIG_FREE(pBuffList);
            return CPA_STATUS_FAIL;
        }
    }
    else
    {
        pBuffList->pPrivateMetaData = NULL;
    }

    PHYS_CONTIG_ALLOC(&pFlatBuff, sizeof(CpaFlatBuffer) * numberChains);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error allocating pFlatBuff\n");
        if (metaSize)
        {
        	PHYS_CONTIG_FREE(pBuffList->pPrivateMetaData);
        }
        PHYS_CONTIG_FREE(pBuffList);
        return CPA_STATUS_FAIL;
    }
    pBuffList->pBuffers = pFlatBuff;

    while (currentChain < numberChains)
    {
        if (0 != sizeOfPkt)
        {
            PHYS_CONTIG_ALLOC(&pMsg, sizeOfPkt);
            if (NULL == pMsg)
            {
                PRINT_ERR("Error allocating pMsg\n");
                if (metaSize)
                {
                	PHYS_CONTIG_FREE(pBuffList->pPrivateMetaData);
                }
                PHYS_CONTIG_FREE(pFlatBuff);
                PHYS_CONTIG_FREE(pBuffList);
                return CPA_STATUS_FAIL;
            }
            memset(pMsg, currentChain, sizeOfPkt);
            pFlatBuff->pData = pMsg;
        }
        else
        {
            pFlatBuff->pData = NULL;
        }
        pFlatBuff->dataLenInBytes = sizeOfPkt;
        pFlatBuff++;
        currentChain++;
    }
    *testBufferList = pBuffList;

    return CPA_STATUS_SUCCESS;
}

void dcFreeBnpBufferList(CpaBufferList **testBufferList)
{
    CpaBufferList *pBuffList = *testBufferList;
    CpaFlatBuffer *pFlatBuff = NULL;
    Cpa32U currentChain = 0;

    if (NULL == pBuffList)
    {
        PRINT_ERR("Nothing to free *testBufferList is NULL\n");
        return;
    }

    pFlatBuff = pBuffList->pBuffers;
    while (currentChain < pBuffList->numBuffers)
    {
        if (NULL != pFlatBuff->pData)
        {
        	PHYS_CONTIG_FREE(pFlatBuff->pData);
            pFlatBuff->pData = NULL;
        }
        pFlatBuff++;
        currentChain++;
    }

    if (NULL != pBuffList->pPrivateMetaData)
    {
    	PHYS_CONTIG_FREE(pBuffList->pPrivateMetaData);
        pBuffList->pPrivateMetaData = NULL;
    }

    if (NULL != pBuffList->pBuffers)
    {
    	PHYS_CONTIG_FREE(pBuffList->pBuffers);
        pBuffList->pBuffers = NULL;
    }

    OS_FREE(pBuffList);
    pBuffList = NULL;
    *testBufferList = pBuffList;
}

CpaStatus
dcGetMetaAndBuildBufferList(CpaInstanceHandle dc_instance, CpaBufferList **pBufferList,
          const Cpa32U numberOfFlatBuffers, const Cpa32U bufferSize)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U metaSizeInBytes = 0;

    status = cpaDcBufferListGetMetaSize(dc_instance,
                                        numberOfFlatBuffers,
                                        &metaSizeInBytes);
    if (CPA_STATUS_FAIL == status)
    {
        PRINT_ERR("Failed to get meta size for buffer list!\n");
        return CPA_STATUS_FAIL;
    }

    status = dcBuildBnpBufferList(pBufferList, numberOfFlatBuffers,
                  bufferSize, metaSizeInBytes);
    if (CPA_STATUS_FAIL == status)
    {
        PRINT_ERR("Failed to build destination buffer list !\n");
        dcFreeBnpBufferList(pBufferList);
        return CPA_STATUS_FAIL;
    }
    return status;
}
