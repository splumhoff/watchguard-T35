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
 * @file cpa_sample_code_dc_2k.c
 *
 *
 * @ingroup compressionThreads
 *
 * @description
 *    This is a sample code that uses Data Compression(DC)  APIs.

 *    This code preallocates a number of buffers as based on the size of each
 *    file defined in the calgary/canterbury corpus. The preallocated buffers
 *    are then populated with the corpus files as define in
 *    setup->testBuffersize.
 *    Time stamping is started prior to the first performed DC
 *    Operation and is stopped when all callbacks have returned.
 *****************************************************************************/

#include "cpa_sample_code_utils_common.h"
#include "cpa_sample_code_dc_perf.h"
#include "cpa_sample_code_dc_utils.h"
#include "cpa_sample_code_crypto_utils.h"

#include "icp_sal_poll.h"

#ifdef KERNEL_SPACE
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/delay.h>

void relDev(struct device *dev)
{
   PRINT("Perf Device released\n");
}

struct device perf_device = {
   .release = relDev
};
#endif


CpaDcChecksum checksum = CPA_DC_ADLER32;

extern CpaStatus deflate_init(struct z_stream_s *stream);
extern CpaStatus deflate_compress(struct z_stream_s *stream,
        const Cpa8U *src,
        Cpa32U slen,
        Cpa8U *dst,
        Cpa32U dlen);
extern void deflate_destroy(struct z_stream_s *stream);
extern Cpa32U dcPollingInterval_g;


#define DOUBLE_SUBMISSIONS  (2)




CpaStatus dcPerform( compression_test_params_t* setup )
{
    /* start of local variable declarations */
    Cpa32U i = 0;
    Cpa32U j = 0;
    Cpa8U *filePtr = NULL;
    CpaStatus status = CPA_STATUS_FAIL;
    Cpa32U nodeId = 0;
    /* Initialize to 0 and set later to size as declared in setup */
    Cpa32U totalBuffs = 0;
    Cpa32U bufferSize = 0;
    CpaDcRqResults ***cmpResult = NULL;
    CpaDcRqResults ***dcmpResult = NULL;
    Cpa32U amountOfFullBuffers = 0;
    Cpa32U metaSize = 0;
    /* Performance data Structure */
    perf_data_t *perfData = NULL;
    /* Src Buffer list for data to be compressed */
    CpaBufferList ***srcBuffListArray = NULL;
    /* BufferList for de-compressed Data */
    CpaBufferList ***dstBuffListArray = NULL ;
    /* BufferList for compressed data */
    CpaBufferList ***cmpBuffListArray = NULL;
    /* Initialize to compress and set later to direction as declared in setup */
    CpaDcSessionDir dcSessDir = CPA_DC_DIR_COMPRESS;

    if(NULL == setup)
    {
        PRINT_ERR(" Setup Pointer is NULL\n");
        return CPA_STATUS_FAIL;
    }
    bufferSize = setup->bufferSize;
    dcSessDir = setup->dcSessDir;
    bufferSize = setup->bufferSize;
    /* get the performance structure */
    perfData = setup->performanceStats;

    /* Get the Node Affinity to allocate memory */
    status = sampleCodeDcGetNode(setup->dcInstanceHandle, &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }


    /* Allocate buff list list pointers
     * this list array is used as source for compression
     */
    srcBuffListArray = qaeMemAllocNUMA(
                 (corpus_g.numFilesInCorpus*sizeof(CpaBufferList*)),
                 nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == srcBuffListArray)
    {
        PRINT_ERR("unable to allocate srcBuffListArray\n");
        return CPA_STATUS_FAIL;
    }

    /* Allocate buff list list pointers
     * This List array will be used as destination buffers
     * to store compressed data
     */
    dstBuffListArray = qaeMemAllocNUMA(
                  (corpus_g.numFilesInCorpus*sizeof(CpaBufferList *)),
                  nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == dstBuffListArray)
    {
        qaeMemFreeNUMA((void**)&srcBuffListArray);
        PRINT_ERR("unable to allocate dstBuffListArray \n");
        return CPA_STATUS_FAIL;
    }

     /* Allocate bufflist list pointers
    * This List array will be used as destination buffers
    * for the decompression
    */
    cmpBuffListArray = qaeMemAllocNUMA(
                  (corpus_g.numFilesInCorpus*sizeof(CpaBufferList *)),
                  nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == cmpBuffListArray)
    {
        qaeMemFreeNUMA((void**)&srcBuffListArray);
        qaeMemFreeNUMA((void**)&dstBuffListArray);
        PRINT_ERR("unable to allocate cmpBuffListArray \n");
        return CPA_STATUS_FAIL;
    }

    /* populate the bufflist array with number of Buffers required
    * for each file and allocate the memory
    */
    for(i=0; i< corpus_g.numFilesInCorpus; i++)
    {
        /* add up the number of buffers required for
         * complete corpus, this counter will be used to get the
         * number of call backs invoked
         */
        totalBuffs += setup->numberOfBuffers[i];
        /* allocate the memory for src, destination and compare buffers
         * for each file
         */
        srcBuffListArray[i] = qaeMemAllocNUMA(
               ( setup->numberOfBuffers[i]*(sizeof(CpaBufferList *)))
               ,nodeId, BYTE_ALIGNMENT_64);
        /* Check for NULL */
        if(NULL == srcBuffListArray[i])
        {
            PRINT_ERR("Unable to allocate Memory for File\n ");
            freeBuffers(srcBuffListArray, i, setup);
            freeBuffers(dstBuffListArray, i, setup);
            freeBuffers(cmpBuffListArray, i, setup);
            return CPA_STATUS_FAIL;
        }
        dstBuffListArray[i] = qaeMemAllocNUMA(
              ( setup->numberOfBuffers[i]*(sizeof(CpaBufferList *)))
              ,nodeId, BYTE_ALIGNMENT_64);
        /* Check for NULL */
        if(NULL == dstBuffListArray[i])
        {
            PRINT_ERR("Unable to allocate Memory for File\n ");
            freeBuffers(srcBuffListArray, i, setup);
            freeBuffers(dstBuffListArray, i, setup);
            freeBuffers(cmpBuffListArray, i, setup);
            return CPA_STATUS_FAIL;
        }
        cmpBuffListArray[i] = qaeMemAllocNUMA(
                ( setup->numberOfBuffers[i]*
                (sizeof(CpaBufferList *))),nodeId, BYTE_ALIGNMENT_64);
        /* Check for NULL */
        if(NULL == cmpBuffListArray[i])
        {
            PRINT_ERR("Unable to allocate Memory for File\n ");
            freeBuffers(srcBuffListArray, i, setup);
            freeBuffers(dstBuffListArray, i, setup);
            freeBuffers(cmpBuffListArray, i, setup);
            return CPA_STATUS_FAIL;
        }
    }
    /* update the number of operations to
     * total number of buffers required for
     * complete corpus based on the session direction
     *
    */
    perfData->numOperations = (Cpa64U)totalBuffs*(Cpa64U)setup->numLoops;

    if(CPA_DC_DIR_COMBINED == dcSessDir)
    {
        perfData->numOperations = ((Cpa64U)totalBuffs*(Cpa64U)setup->numLoops) *
                                    DOUBLE_SUBMISSIONS;
    }

    /* Allocate Flat Buffers for each file in buffer List array */
    for(i=0; i< corpus_g.numFilesInCorpus; i++)
    {
        status = createBuffers(bufferSize,
                               setup->numberOfBuffers[i],
                               srcBuffListArray[i], nodeId);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to Create Buffers for source List array\n");
            freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
            return CPA_STATUS_FAIL;
        }
        /* When compressing,small packet sizes the destination buffer
         * may need to be larger than the source buffer to accommodate
         * huffman data, so allocate double the source buffer size
         */
        if(MIN_DST_BUFFER_SIZE >= bufferSize)
        {
        status = createBuffers(bufferSize*EXTRA_BUFFER,
                               setup->numberOfBuffers[i],
                                   dstBuffListArray[i], nodeId);
        }
        else
        {
        status = createBuffers(bufferSize,
                                   setup->numberOfBuffers[i],
                                   dstBuffListArray[i], nodeId);
        }
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to Create Buffers for destination List array\n");
            freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
            return CPA_STATUS_FAIL;
        }
        /* When Decompression, the FW expects that the Buffer size
         * to be greater than the source buffer, so allocate double the
         * size of the source buffer
         */
        status = createBuffers((bufferSize*EXTRA_BUFFER),
                                setup->numberOfBuffers[i],
                               cmpBuffListArray[i], nodeId);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to Create Buffers for compare List array\n");
            freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
            return CPA_STATUS_FAIL;
        }
    }
    /* Allocate Memory for the results structure */
    cmpResult = qaeMemAllocNUMA((corpus_g.numFilesInCorpus*
                          sizeof(CpaDcRqResults *)), nodeId, BYTE_ALIGNMENT_64);
    if(NULL == cmpResult)
    {
        PRINT_ERR("unable to allocate memory for Results\n");
        freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
        freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
        freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
        return CPA_STATUS_FAIL;
    }
/* Allocate Memory for the results structure */
    dcmpResult = qaeMemAllocNUMA((corpus_g.numFilesInCorpus*
                          sizeof(CpaDcRqResults *)), nodeId, BYTE_ALIGNMENT_64);
    if(NULL == dcmpResult)
    {
        PRINT_ERR("unable to allocate memory for Results\n");
        qaeMemFreeNUMA((void**)&cmpResult);
        freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
        freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
        freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
        return CPA_STATUS_FAIL;
    }

    /* Copy data into Flat Buffers from the corpus structure */
    for (i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        filePtr = corpus_g.fileArray[i].corpusBinaryData;
        /* get the number of full Buffers */
        amountOfFullBuffers = (corpus_g.fileArray[i].corpusBinaryDataLen) /
                                                               bufferSize;
        /* Copy the data into Flat buffers */
        for(j=0; j< amountOfFullBuffers; j++)
        {
            memcpy((srcBuffListArray[i][j]->pBuffers->pData),
                                       filePtr, bufferSize);
            filePtr += bufferSize;
        }

        filePtr = NULL;
    }

    /* allocate the results structure for each buffer in the
     * corpus file
     */
    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        dcmpResult[i] = qaeMemAllocNUMA(setup->numberOfBuffers[i] *
                                          sizeof(CpaDcRqResults *),nodeId,
                                          BYTE_ALIGNMENT_64);
        if(NULL == dcmpResult[i])
        {
            PRINT_ERR("unable to allocate memory for"
                  "Results structure for each buffer\n");
            freeResults(dcmpResult , i, setup);
            qaeMemFreeNUMA((void**)&cmpResult);
            freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
            freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
            return CPA_STATUS_FAIL;
        }
    }

    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        cmpResult[i] = qaeMemAllocNUMA(setup->numberOfBuffers[i] *
                                          sizeof(CpaDcRqResults *),nodeId,
                                          BYTE_ALIGNMENT_64);
        if(NULL == cmpResult[i])
        {
            PRINT_ERR("unable to allocate memory for"
                  "Results structure for each buffer\n");
            freeResults(cmpResult , i,setup);
            freeResults(dcmpResult , corpus_g.numFilesInCorpus,setup);
            freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus,setup);
            freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus,setup);
            freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus,setup);
            return CPA_STATUS_FAIL;
        }
    }

    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        for(j=0; j < setup->numberOfBuffers[i]; j++)
        {
            cmpResult[i][j] = qaeMemAllocNUMA(sizeof(CpaDcRqResults ),
                    nodeId,BYTE_ALIGNMENT_64);
            if(NULL == cmpResult[i][j])
            {
                freeResults(cmpResult,corpus_g.numFilesInCorpus,setup);
                freeResults(dcmpResult , corpus_g.numFilesInCorpus,setup);
                freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus,setup);
                freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus,setup);
                freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus,setup);
                return CPA_STATUS_FAIL;
            }
            dcmpResult[i][j] = qaeMemAllocNUMA(sizeof(CpaDcRqResults ),
                    nodeId,BYTE_ALIGNMENT_64);
            if(NULL == dcmpResult[i][j])
            {
                freeResults(cmpResult,corpus_g.numFilesInCorpus,setup);
                freeResults(dcmpResult , corpus_g.numFilesInCorpus,setup);
                freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus,setup);
                freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus,setup);
                freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus,setup);
                return CPA_STATUS_FAIL;
            }
        }
    }
    /* get the Meta Size for each buffer List and
     * allocate Private Meta Data
     */
    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        for(j=0; j < setup->numberOfBuffers[i]; j++)
        {
            /* Get the Meta size for each file in buffers list */
            status = cpaDcBufferListGetMetaSize(setup->dcInstanceHandle,
                                  srcBuffListArray[i][j]->numBuffers,
                                   &metaSize);
            if(CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Unable to get Meta Size\n");
                freeResults(dcmpResult , corpus_g.numFilesInCorpus, setup);
                freeResults(cmpResult , corpus_g.numFilesInCorpus, setup);
                freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
                return CPA_STATUS_FAIL;
            }

            srcBuffListArray[i][j]->pPrivateMetaData =
                        qaeMemAllocNUMA(metaSize, nodeId, BYTE_ALIGNMENT_64);
            if( NULL == srcBuffListArray[i][j]->pPrivateMetaData)
            {
                PRINT_ERR(" Unable to allocate pPrivateMetaData\n");
                freeResults(dcmpResult , corpus_g.numFilesInCorpus, setup);
                freeResults(cmpResult , corpus_g.numFilesInCorpus, setup);
                freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
                return CPA_STATUS_FAIL;
            }
            cmpBuffListArray[i][j]->pPrivateMetaData=
                        qaeMemAllocNUMA(metaSize, nodeId, BYTE_ALIGNMENT_64);
            if( NULL == cmpBuffListArray[i][j]->pPrivateMetaData )
            {
                PRINT_ERR(" Unable to allocate pPrivateMetaData\n");
                freeResults(dcmpResult , corpus_g.numFilesInCorpus, setup);
                freeResults(cmpResult , corpus_g.numFilesInCorpus, setup);
                freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
                return CPA_STATUS_FAIL;
            }
            dstBuffListArray[i][j]->pPrivateMetaData =
                        qaeMemAllocNUMA(metaSize, nodeId, BYTE_ALIGNMENT_64);
            if( dstBuffListArray[i][j]->pPrivateMetaData == NULL)
            {
                PRINT_ERR(" Unable to allocate pPrivateMetaData\n");
                freeResults(dcmpResult , corpus_g.numFilesInCorpus, setup);
                freeResults(cmpResult , corpus_g.numFilesInCorpus, setup);
                freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
                freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);
                return CPA_STATUS_FAIL;
            }
        }
    }


    if(CPA_DC_DIR_COMPRESS == dcSessDir)
    {
        status = performCompress(setup, srcBuffListArray,
                          dstBuffListArray, cmpResult, dcPerformCallback);
        dcSetBytesProducedAndConsumed(cmpResult,setup->performanceStats, setup);
    }
    if(CPA_DC_DIR_DECOMPRESS == dcSessDir)
    {
        status = performDeCompress(setup, srcBuffListArray,
                          dstBuffListArray, cmpBuffListArray, cmpResult,
                          dcmpResult, dcPerformCallback);
    }
    /* Free all the results structures */
    freeResults(cmpResult, corpus_g.numFilesInCorpus, setup);
    freeResults(dcmpResult, corpus_g.numFilesInCorpus, setup);
    /* Free all the Buffer Lists */
    freeBuffers(srcBuffListArray, corpus_g.numFilesInCorpus, setup);
    freeBuffers(dstBuffListArray, corpus_g.numFilesInCorpus, setup);
    freeBuffers(cmpBuffListArray, corpus_g.numFilesInCorpus, setup);

    /*clean up the callback semaphore*/
    sampleCodeSemaphoreDestroy(&perfData->comp);
    return status;

}

CpaStatus performCompress(compression_test_params_t* setup,
                          CpaBufferList ***srcBuffListArray,
                          CpaBufferList ***dstBuffListArray,
                          CpaDcRqResults ***cmpResult,
                          CpaDcCallbackFn dcCbFn)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U sessionSize = 0, contextSize = 0;
    /* DC session Handle */
    CpaDcSessionHandle *pSessionHandle = NULL;
    Cpa32U numLoops = 0, i = 0, j = 0, totalBuffers = 0;
    Cpa32U compressLoops = setup->numLoops;
    perf_data_t *perfData = NULL;
    Cpa32U nodeId = 0;
    /* pContextBuffer is NULL for stateless requests */
    CpaBufferList *pContextBuffer = NULL;
    /* flushFlag set to CPA_DC_FLUSH_FULL for stateless requests */
    CpaDcFlush flushFlag = CPA_DC_FLUSH_FULL;
    CpaInstanceInfo2 instanceInfo2 = {0};
#ifdef LATENCY_CODE
    Cpa32U submissions = 0;
    perf_cycles_t request_submit_start[100] = {0};
    perf_cycles_t request_respnse_time[100] = {0};
#endif

    /* Calculate the number of individual buffers to be submitted */
    for(i = 0; i < corpus_g.numFilesInCorpus; i++ )
    {
        totalBuffers += setup->numberOfBuffers[i];
    }

    perfData = setup->performanceStats;

    /* Zero performance stats */
    memset(perfData, 0, sizeof(perf_data_t));

    perfData->numOperations = (Cpa64U)totalBuffers * (Cpa64U)compressLoops;

    status = cpaDcInstanceGetInfo2(setup->dcInstanceHandle, &instanceInfo2);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("cpaCyInstanceGetInfo2 error, status: %d\n", status);
        return CPA_STATUS_FAIL;
    }

#ifdef LATENCY_CODE
    if(perfData->numOperations > LATENCY_SUBMISSION_LIMIT)
    {
        PRINT_ERR("Error max submissions for latency  must be <= %d\n",
                LATENCY_SUBMISSION_LIMIT);
        return CPA_STATUS_FAIL;
    }
    perfData->nextCount = (setup->numberOfBuffers[0]*setup->numLoops)/100;
    perfData->countIncrement = (setup->numberOfBuffers[0]*setup->numLoops)/100;

    perfData->response_times=request_respnse_time;
#endif
     /* Get the Node Affinity to allocate memory */
    status = sampleCodeDcGetNode(setup->dcInstanceHandle, &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }

    /* Get Size for DC Session */
    status = cpaDcGetSessionSize(setup->dcInstanceHandle,
               &(setup->setupData), &sessionSize, &contextSize);
    if ( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("cpaDcGetSessionSize() returned %d status.\n",status);
        return CPA_STATUS_FAIL;
    }
    /* Allocate Memory for DC Session */
    pSessionHandle = (CpaDcSessionHandle)
                           qaeMemAllocNUMA((sessionSize + contextSize),
                           nodeId,BYTE_ALIGNMENT_64);
    if (NULL == pSessionHandle)
    {
        PRINT_ERR("Unable to allocate Memory for Session Handle\n");
        return CPA_STATUS_FAIL;
    }

    if(setup->syncFlag == CPA_SAMPLE_SYNCHRONOUS)
    {
        dcCbFn = NULL;
    }

    /* Initialize DC API Session */
    status = cpaDcInitSession( setup->dcInstanceHandle, pSessionHandle,
                                  &(setup->setupData), pContextBuffer, dcCbFn);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Problem in session creation: status = %d \n", status);
        qaeMemFreeNUMA((void**)&pSessionHandle);
        return CPA_STATUS_FAIL;
    }

    /* Completion used in callback */
    sampleCodeSemaphoreInit(&perfData->comp, 0);
    /* this Barrier will waits until all the threads get to this point */
    sampleCodeBarrier();
    /* generate the start time stamp */
    perfData->startCyclesTimestamp = sampleCodeTimestamp();
    for(numLoops = 0; numLoops < compressLoops; numLoops++)
    {


        /* compression API will be called for each buffer list
         * in the corpus File
         */
        for(i=0; i<corpus_g.numFilesInCorpus; i++)
        {
            /* call the compress api */
            for(j=0; j < setup->numberOfBuffers[i]; j++)
            {
                do {
#ifdef LATENCY_CODE
                        if(submissions+1 == perfData->nextCount)
                        {
                                request_submit_start[perfData->latencyCount] =
                                    sampleCodeTimestamp();
                        }
#endif
                        status = cpaDcCompressData(setup->dcInstanceHandle,
                               pSessionHandle, srcBuffListArray[i][j],
                                 dstBuffListArray[i][j], cmpResult[i][j],
                                    flushFlag, perfData);

                    if(CPA_STATUS_RETRY == status)
                    {
                        setup->performanceStats->retries++;
                        AVOID_SOFTLOCKUP;
                    }
                } while(CPA_STATUS_RETRY == status);
                /* Check Status */
                if(CPA_STATUS_SUCCESS != status )
                {
                    PRINT_ERR("Data Compression Failed %d\n\n", status);
                    perfData->threadReturnStatus = CPA_STATUS_FAIL;
                    break;
                }
#ifdef LATENCY_CODE
                submissions++;
#endif
                /* check if synchronous flag is set
                 * if set, invoke the callback API
                 */
                if(CPA_SAMPLE_SYNCHRONOUS == setup->syncFlag)
                {
                    /* invoke the Compression Callback only */
                    dcPerformCallback(perfData, status);
                } /* End of SYNC Flag Check */
            } /* End of number of buffers Loop */
            if(CPA_STATUS_SUCCESS  != status)
            {
                PRINT_ERR("Data Compression Failed %d\n\n", status);
                perfData->threadReturnStatus = CPA_STATUS_FAIL;
                break;
            }
        } /* End of number of Files Loop*/
        if(CPA_STATUS_SUCCESS  != status)
        {
           PRINT_ERR("Data Compression Failed %d\n\n", status);
           perfData->threadReturnStatus = CPA_STATUS_FAIL;
           break;
       }
    } /* End of compression Loops */
    /* Wait for the semaphore */
    if(CPA_STATUS_SUCCESS == status)
    {
        status = waitForSemaphore(perfData);
        if(CPA_STATUS_SUCCESS != status)
        {
            qaeMemFreeNUMA((void**)&pSessionHandle);
            return status;
        }
    }
    /* Close the DC Session */
    status = cpaDcRemoveSession(setup->dcInstanceHandle, pSessionHandle);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to remove session\n");
    }
    qaeMemFreeNUMA((void**)&pSessionHandle);
#ifdef LATENCY_CODE
    for(i=0; i<perfData->latencyCount;i++)
    {
        perfData->aveLatency +=
            perfData->response_times[i] - request_submit_start[i];
    }
    if(perfData->latencyCount>0)
    {
        do_div(perfData->aveLatency,perfData->latencyCount);
    }
#endif
    return status;
}

CpaStatus compressCorpus(compression_test_params_t* setup,
                          CpaBufferList ***srcBuffListArray,
                          CpaBufferList ***dstBuffListArray,
                          CpaDcRqResults ***cmpResult,
                          dc_callbacktag_t ***callbackTag)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U sessionSize = 0, contextSize = 0;
    /* DC session Handle */
    CpaDcSessionHandle *pSessionHandle = NULL;
    perf_data_t *perfData = NULL;
    Cpa32U i = 0, j = 0;
    Cpa32U numBuffs = 0,nodeId = 0;
    /* pContextBuffer is NULL for stateless requests */
    CpaBufferList *pContextBuffer = NULL;
    /* flushFlag set to CPA_DC_FLUSH_FULL for stateless requests */
    CpaDcFlush flushFlag = CPA_DC_FLUSH_FULL;
    CpaDcCallbackFn dcCbFn = NULL;

    perfData = setup->performanceStats;

    /* Zero performance stats */
    memset(perfData, 0, sizeof(perf_data_t));

    /* Get the Node Affinity to allocate memory */
    status = sampleCodeDcGetNode(setup->dcInstanceHandle, &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }
    /* Get Size for DC Session */
    status = cpaDcGetSessionSize(setup->dcInstanceHandle,
               &(setup->setupData), &sessionSize, &contextSize);
    if ( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("cpaDcGetSessionSize() returned %d status.\n",status);
        return CPA_STATUS_FAIL;
    }
    /* Allocate Memory for DC Session */
    pSessionHandle = (CpaDcSessionHandle)
                           qaeMemAllocNUMA((sessionSize + contextSize),
                           nodeId,BYTE_ALIGNMENT_64);
    if (NULL == pSessionHandle)
    {
        PRINT_ERR("Unable to allocate Memory for Session Handle\n");
        return CPA_STATUS_FAIL;
    }

    if(setup->syncFlag == CPA_SAMPLE_SYNCHRONOUS)
    {
        dcCbFn = NULL;
    }
    else
    {
        dcCbFn = deCompressCallback;
    }

    /* Initialize DC API Session */
    status = cpaDcInitSession( setup->dcInstanceHandle, pSessionHandle,
                      &(setup->setupData), pContextBuffer, dcCbFn);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Problem in session creation: status = %d \n", status);
        qaeMemFreeNUMA((void**)&pSessionHandle);
        return CPA_STATUS_FAIL;
    }
    /* calculate the number of buffers */
    for(i = 0; i < corpus_g.numFilesInCorpus; i++)
    {
        numBuffs += setup->numberOfBuffers[i];
    }
    perfData->numOperations = numBuffs;

    /* Completion used in callback */
    sampleCodeSemaphoreInit(&perfData->comp, 0);
    /* compression API will be called for each buffer list
     * in the corpus File
     */
    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        /* call the compress api */
        for(j=0; j < setup->numberOfBuffers[i]; j++)
        {
            do {
                status = cpaDcCompressData(setup->dcInstanceHandle,
                           pSessionHandle, srcBuffListArray[i][j],
                             dstBuffListArray[i][j], cmpResult[i][j],
                                flushFlag, callbackTag[i][j]);

                if(CPA_STATUS_RETRY == status)
                {
                    AVOID_SOFTLOCKUP;
                }
            } while(CPA_STATUS_RETRY == status);
            /* Check Status */
            if(CPA_STATUS_SUCCESS != status )
            {
                PRINT_ERR("Data Compression Failed %d\n\n", status);
                perfData->threadReturnStatus = CPA_STATUS_FAIL;
                break;
            }
            if(CPA_SAMPLE_SYNCHRONOUS == setup->syncFlag)
            {
                /* invoke the Compression Callback only */
                deCompressCallback(callbackTag[i][j], status);
            } /* End of SYNC Flag Check */
        } /* End of number of buffers Loop */
        if(CPA_STATUS_SUCCESS  != status)
        {
            PRINT_ERR("Data Compression Failed %d\n\n", status);
            perfData->threadReturnStatus = CPA_STATUS_FAIL;
            break;
        }
    } /* End of number of Files Loop*/
    /* check for semaphore */
    if(CPA_STATUS_SUCCESS == status)
    {
        status = waitForSemaphore(perfData);
        if(CPA_STATUS_SUCCESS != status)
        {
            qaeMemFreeNUMA((void**)&pSessionHandle);
            return status;
        }
    }
    /* Close the DC Session */
    status = cpaDcRemoveSession(setup->dcInstanceHandle, pSessionHandle);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to remove session\n");
    }
    qaeMemFreeNUMA((void**)&pSessionHandle);

    return status;
}
CpaStatus performDeCompress(compression_test_params_t* setup,
                          CpaBufferList ***srcBuffListArray,
                          CpaBufferList ***dstBuffListArray,
                          CpaBufferList ***cmpBuffListArray,
                          CpaDcRqResults ***cmpResult,
                          CpaDcRqResults ***dcmpResult,
                          CpaDcCallbackFn dcCbFn)

{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U sessionSize = 0, contextSize = 0;
    /* DC session Handle */
    CpaDcSessionHandle *pSessionHandle = NULL;
    Cpa32U numLoops = 0, i = 0, j = 0, totalBuffers = 0;
    Cpa32U deCompressLoops = setup->numLoops;
    perf_data_t *perfData = NULL;
    dc_callbacktag_t ***dcCallbackTag = NULL;
    Cpa32U numBuffs = 0,nodeId = 0;
    /* pContextBuffer is NULL for stateless requests */
    CpaBufferList *pContextBuffer = NULL;
    /* flushFlag set to CPA_DC_FLUSH_FULL for stateless requests */
    CpaDcFlush flushFlag = CPA_DC_FLUSH_FULL;
    CpaInstanceInfo2 instanceInfo2 = {0};

#ifdef LATENCY_CODE
    Cpa32U submissions = 0;
    perf_cycles_t request_submit_start[100] = {0};
    perf_cycles_t request_respnse_time[100] = {0};
#endif

    struct z_stream_s stream = {0};

    perfData = setup->performanceStats;

    /* Calculate the number of individual buffers to be submitted */
    for(i = 0; i < corpus_g.numFilesInCorpus; i++ )
    {
        totalBuffers += setup->numberOfBuffers[i];
    }


    /* Zero performance stats */
    memset(perfData, 0, sizeof(perf_data_t));

    perfData->numOperations = (Cpa64U)totalBuffers * (Cpa64U)deCompressLoops;

    status = cpaDcInstanceGetInfo2(setup->dcInstanceHandle, &instanceInfo2);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("cpaCyInstanceGetInfo2 error, status: %d\n", status);
        return CPA_STATUS_FAIL;
    }
     /* Get the Node Affinity to allocate memory */
    status = sampleCodeDcGetNode(setup->dcInstanceHandle, &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }

#ifdef LATENCY_CODE
    if(perfData->numOperations > LATENCY_SUBMISSION_LIMIT)
    {
        PRINT_ERR("Error max submissions for latency  must be <= %d\n",
                LATENCY_SUBMISSION_LIMIT);
        return CPA_STATUS_FAIL;
    }
    perfData->nextCount = (setup->numberOfBuffers[0]*setup->numLoops)/100;
    perfData->countIncrement = (setup->numberOfBuffers[0]*setup->numLoops)/100;

    perfData->response_times=request_respnse_time;
#endif

    /* Get Size for DC Session */
    status = cpaDcGetSessionSize(setup->dcInstanceHandle,
               &(setup->setupData), &sessionSize, &contextSize);
    if ( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("cpaDcGetSessionSize() returned %d status.\n",status);
        return CPA_STATUS_FAIL;
    }
    /* Allocate Memory for DC Session */
    pSessionHandle = (CpaDcSessionHandle)
                           qaeMemAllocNUMA((sessionSize + contextSize),
                           nodeId,BYTE_ALIGNMENT_64);
    if (NULL == pSessionHandle)
    {
        PRINT_ERR("Unable to allocate Memory for Session Handle\n");
        return CPA_STATUS_FAIL;
    }
    /* Setup callback Tags */
   dcCallbackTag = qaeMemAllocNUMA(corpus_g.numFilesInCorpus *
                              sizeof(dc_callbacktag_t**), nodeId,
                              BYTE_ALIGNMENT_64);

    if(NULL == dcCallbackTag)
    {
        PRINT("Unable to allocate memory for callback tags\n");
        return CPA_STATUS_FAIL;
    }

    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {

        dcCallbackTag[i] = qaeMemAllocNUMA(
                              setup->numberOfBuffers[i] *
                              sizeof(dc_callbacktag_t *), nodeId,
                              BYTE_ALIGNMENT_64);
        if(NULL == dcCallbackTag[i])
        {
            PRINT("Unable to allocate memory for callback tags\n");
            freeCbTags(dcCallbackTag, i, setup);
            return CPA_STATUS_FAIL;
        }

    }
    /* Setup callbacktags for each buffer with results structure
     * and performance structure
     * */
    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        for(j=0;j < setup->numberOfBuffers[i]; j++)
        {
            dcCallbackTag[i][j] = qaeMemAllocNUMA(sizeof(dc_callbacktag_t),
            nodeId,BYTE_ALIGNMENT_64);
            if(NULL == dcCallbackTag[i])
            {
                PRINT("Unable to allocate memory for callback tags\n");
                freeCbTags(dcCallbackTag, i, setup);
                return CPA_STATUS_FAIL;
            }
            dcCallbackTag[i][j]->perfData = perfData;
            dcCallbackTag[i][j]->dcResult = cmpResult[i][j];
            dcCallbackTag[i][j]->pBuffList= dstBuffListArray[i][j];
        }
    }

     /* make sure to compress the corpus before starting
     * de compression
     */

    if(!useZlib_g)
    {
        status =  compressCorpus(setup, srcBuffListArray, dstBuffListArray,
                      cmpResult, dcCallbackTag);

        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT("Compression Failed before decompressing the corpus\n");
            return CPA_STATUS_FAIL;
        }
    }
    else
    {
        deflate_init(&stream);
        for(i=0; i<corpus_g.numFilesInCorpus; i++)
        {
            /* call the compress api */
            for(j=0; j < setup->numberOfBuffers[i]; j++)
            {

                status = deflate_compress(&stream,
                        srcBuffListArray[i][j]->pBuffers[0].pData,
                        srcBuffListArray[i][j]->pBuffers[0].dataLenInBytes,
                        dstBuffListArray[i][j]->pBuffers[0].pData,
                        dstBuffListArray[i][j]->pBuffers[0].dataLenInBytes);
                if(CPA_STATUS_SUCCESS != status)
                {
                    PRINT("i: %d, j: %d, srcLen: %d, destLen: %d \n",
                            i, j,
                            srcBuffListArray[i][j]->pBuffers[0].dataLenInBytes,
                            dstBuffListArray[i][j]->pBuffers[0].dataLenInBytes);
                }
                dstBuffListArray[i][j]->pBuffers[0].dataLenInBytes =
                    /*srcBuffListArray[i][j]->pBuffers[0].dataLenInBytes -
                    stream.avail_out*/stream.total_out;
                cmpResult[i][j]->consumed =
                    srcBuffListArray[i][j]->pBuffers[0].dataLenInBytes;
                cmpResult[i][j]->produced =
                    dstBuffListArray[i][j]->pBuffers[0].dataLenInBytes;
            }
        }
        deflate_destroy(&stream);
    }
    dcSetBytesProducedAndConsumed(cmpResult,setup->performanceStats,setup);


    /* calculate the number of buffers */
    for(i = 0; i < corpus_g.numFilesInCorpus; i++)
    {
        numBuffs += setup->numberOfBuffers[i];
    }
    perfData->numOperations = (Cpa64U)numBuffs * (Cpa64U)setup->numLoops;
    perfData->responses = 0 ;

    if(CPA_SAMPLE_SYNCHRONOUS == setup->syncFlag)
    {
        dcCbFn = NULL;
    }

    /* Initialize DC API Session */
    status = cpaDcInitSession( setup->dcInstanceHandle, pSessionHandle,
                                  &(setup->setupData), pContextBuffer, dcCbFn);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Problem in session creation: status = %d \n", status);
        qaeMemFreeNUMA((void**)&pSessionHandle);
        return CPA_STATUS_FAIL;
    }

   /* Completion used in callback */
     sampleCodeSemaphoreInit(&perfData->comp, 0);
    /* this Barrier will waits until all the threads get to this point */
    sampleCodeBarrier();
    /* generate the start time stamp */
    perfData->startCyclesTimestamp = sampleCodeTimestamp();
    for(numLoops = 0; numLoops < deCompressLoops; numLoops++)
    {
        /* Check if terminated by global falg.
         * If yes, update numOperations, compressLoops,
         *  numLoops: added by waterman */

        /* for each file in corpus */
        for(i=0; i<corpus_g.numFilesInCorpus;i++)
        {
            /* de compress the data */
            for(j=0; j < setup->numberOfBuffers[i]; j++)
            {
                do {
#ifdef LATENCY_CODE
                        if(submissions+1 == perfData->nextCount)
                        {
                                request_submit_start[perfData->latencyCount] =
                                    sampleCodeTimestamp();
                        }
#endif
                        status = cpaDcDecompressData(setup->dcInstanceHandle,
                                 pSessionHandle, dstBuffListArray[i][j],
                                cmpBuffListArray[i][j], dcmpResult[i][j],
                                 flushFlag, perfData);
                    if(CPA_STATUS_RETRY == status)
                    {
                        setup->performanceStats->retries++;
                        AVOID_SOFTLOCKUP;
                    }
                } while(CPA_STATUS_RETRY == status);
                if(CPA_STATUS_SUCCESS != status)
                {
                    PRINT_ERR(" Data De-Compression Failed\n");
                    perfData->threadReturnStatus = CPA_STATUS_FAIL;
                    break;
                }
#ifdef LATENCY_CODE
                submissions++;
#endif                /* check if synchronous flag is set
                 * if set, invoke the callback API
                 */
                if(CPA_SAMPLE_SYNCHRONOUS == setup->syncFlag)
                {
                    /* invoke the decompression Callback only */
                    dcPerformCallback(perfData, status);
                } /* End of Synchronous Flag check */
            } /* End of number of buffers Loop */
            if(CPA_STATUS_SUCCESS  != status)
            {
                PRINT_ERR("Data De-Compression Failed %d\n\n", status);
                perfData->threadReturnStatus = CPA_STATUS_FAIL;
                break;
            }
        } /* End of number of File Loop */
        if(CPA_STATUS_SUCCESS  != status)
        {
            PRINT_ERR("Data De-Compression Failed %d\n\n", status);
            perfData->threadReturnStatus = CPA_STATUS_FAIL;
            break;
        }


    }/* End of de-compression Loops*/

    /* Wait for the semaphore */
    if(CPA_STATUS_SUCCESS == status)
    {
        status = waitForSemaphore(perfData);
        if(CPA_STATUS_SUCCESS != status)
        {
            freeCbTags(dcCallbackTag, corpus_g.numFilesInCorpus, setup);
            qaeMemFreeNUMA((void**)&pSessionHandle);
            return status;
        }
    }

    /* Close the DC Session */
    status = cpaDcRemoveSession(setup->dcInstanceHandle, pSessionHandle);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to remove session\n");
    }
    freeCbTags(dcCallbackTag, corpus_g.numFilesInCorpus, setup);
    qaeMemFreeNUMA((void**)&pSessionHandle);
#ifdef LATENCY_CODE
    for(i=0; i<perfData->latencyCount;i++)
    {
        perfData->aveLatency +=
            perfData->response_times[i] - request_submit_start[i];
    }
    if(perfData->latencyCount>0)
    {
        do_div(perfData->aveLatency,perfData->latencyCount);
    }
#endif

    return status;
}

void freeResults(CpaDcRqResults ***ppDcResult , Cpa32U numFiles,
                 compression_test_params_t* setup)
{
    Cpa32U  i = 0,j = 0;


    if(NULL == ppDcResult)
    {
       /* Return Silent */
        return;
    }
    if(0 != numFiles)
    {
        for(i=0; i < numFiles; i++)
        {
            for(j = 0; j < setup->numberOfBuffers[i]; j++)
            {
                if(NULL != ppDcResult[i][j])
                {
                    qaeMemFreeNUMA((void**)&ppDcResult[i][j]);
                }
            }
           if(NULL != ppDcResult[i])
           {
               qaeMemFreeNUMA((void**)&ppDcResult[i]);
           }
        }
    }
    qaeMemFreeNUMA((void**)&ppDcResult);
}

void freeCbTags(dc_callbacktag_t ***callbackTag, Cpa32U numFiles,
                    compression_test_params_t* setup)
{
    Cpa32U  i = 0, j = 0;


    if(NULL == callbackTag)
    {
       /* Return Silent */
        return;
    }
    if(0 != numFiles)
    {
        for(i=0;i < numFiles; i++)
        {

            for(j = 0; j < setup->numberOfBuffers[i]; j++)
            {
                if(NULL != callbackTag[i][j])
                {
                    qaeMemFreeNUMA((void**)&callbackTag[i][j]);
                }
            }
            if(NULL != callbackTag[i])
            {
                qaeMemFreeNUMA((void**)&callbackTag[i]);
            }
        }
    }
    qaeMemFreeNUMA((void**)&callbackTag);
}




void freeBuffers(CpaBufferList ***pBuffListArray,
                     Cpa32U numberOfFiles, compression_test_params_t* setup)
{
    Cpa32U  i = 0, j = 0;


    if(NULL == pBuffListArray)
    {
       /* Return Silent */
        return;
    }
    if(0 != numberOfFiles)
    {
        for(i=0; i < numberOfFiles; i++)
        {
           for(j = 0; j < setup->numberOfBuffers[i]; j++)
           {
               if(NULL != pBuffListArray[i][j]->pBuffers->pData)
               {
                   qaeMemFreeNUMA((void**)&pBuffListArray[i][j]->
                           pBuffers->pData);
               }
               if(NULL != pBuffListArray[i][j]->pPrivateMetaData)
               {
                   qaeMemFreeNUMA((void**)&pBuffListArray[i][j]->
                           pPrivateMetaData);
               }
               if(NULL != pBuffListArray[i][j]->pBuffers)
               {
                   qaeMemFreeNUMA((void**)&pBuffListArray[i][j]->pBuffers);
               }
               if(NULL != pBuffListArray[i][j])
               {
                   qaeMemFreeNUMA((void**)&pBuffListArray[i][j]);
               }
           }
           if(NULL != pBuffListArray[i])
           {
               qaeMemFreeNUMA((void**)&pBuffListArray[i]);
           }
        }
    }
    qaeMemFreeNUMA((void**)&pBuffListArray);
    return;
}

CpaStatus createBuffers(Cpa32U buffSize, Cpa32U numBuffs,
                        CpaBufferList **pBuffListArray, Cpa32U nodeId)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U i = 0;

    for(i = 0; i< numBuffs; i++)
    {
        pBuffListArray[i]= qaeMemAllocNUMA((sizeof(CpaBufferList)),
                nodeId, BYTE_ALIGNMENT_64);
        if(NULL== pBuffListArray[i])
        {
            PRINT_ERR(" Unable to allocate Buffers\n");
            return CPA_STATUS_FAIL;
        }
        pBuffListArray[i]->pBuffers =
                       qaeMemAllocNUMA((sizeof(CpaFlatBuffer)),
                                        nodeId, BYTE_ALIGNMENT_64);
        if(NULL == pBuffListArray[i]->pBuffers)
        {
            PRINT_ERR(" Unable to allocate Buffers\n");
            return CPA_STATUS_FAIL;
        }
        /* Allocate Flat buffer for each buffer List */
        pBuffListArray[i]->pBuffers->dataLenInBytes = buffSize;
        pBuffListArray[i]->pBuffers->pData =
                           qaeMemAllocNUMA(buffSize, nodeId, BYTE_ALIGNMENT_64);
        memset(pBuffListArray[i]->pBuffers->pData, 0 ,buffSize);
        if(NULL == pBuffListArray[i]->pBuffers->pData )
        {
            PRINT(" Unable to allocate Flat buffer\n");
            return CPA_STATUS_FAIL;
        }
        pBuffListArray[i]->numBuffers = ONE_BUFFER_DC;
    }



    return status;
}


void deCompressCallback(
        void       *pCallbackTag,
        CpaStatus  status)
{

    dc_callbacktag_t *cbTag = (dc_callbacktag_t *)pCallbackTag;
    perf_data_t *pPerfData;

    /*check perf_data pointer is valid*/
    if (NULL == cbTag)
    {
        PRINT_ERR("Invalid data in CallbackTag\n");
        return;
    }
    pPerfData = cbTag->perfData;
    if(CPA_STATUS_SUCCESS != status )
    {
        PRINT("DC Function Failed, status = %d \n", status);
        pPerfData->threadReturnStatus = CPA_STATUS_FAIL;
    }
    /* Update the data length with produced */
    cbTag->pBuffList->pBuffers->dataLenInBytes = cbTag->dcResult->produced;
    /* increment the responses */
    pPerfData->responses++;
    if(pPerfData->responses >= pPerfData->numOperations)
    {
        /* Release the semaphore */
        sampleCodeSemaphorePost(&pPerfData->comp);
    }

}

/*********** Call Back Function **************/
void dcPerformCallback(
        void       *pCallbackTag,
        CpaStatus  status)
{
    perf_data_t *pPerfData = (perf_data_t *)pCallbackTag;


    /*check status */
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("DC Function Failed \n");
        pPerfData->threadReturnStatus = CPA_STATUS_FAIL;
    }

    /*check perf_data pointer is valid*/
    if (NULL == pPerfData)
    {
        PRINT_ERR("Invalid data in CallbackTag\n");
        return;
    }
    pPerfData->responses++;
#ifdef LATENCY_CODE
    if((0 != pPerfData->responses) && (NULL != pPerfData->response_times))
    {
        if(pPerfData->responses ==  pPerfData->nextCount)
        {
            pPerfData->response_times[pPerfData->latencyCount++] =
                sampleCodeTimestamp();
            pPerfData->nextCount += pPerfData->countIncrement;
        }
    }
#endif
    if(pPerfData->responses >= pPerfData->numOperations)
    {
        /* generate end of the cycle stamp for Corpus */
        pPerfData->endCyclesTimestamp = sampleCodeTimestamp();
        sampleCodeSemaphorePost(&pPerfData->comp);
    }
}



void dcPerformance(single_thread_test_data_t* testSetup)
{
    compression_test_params_t dcSetup, *tmpSetup = NULL;
    Cpa16U numInstances = 0;
    CpaInstanceHandle *instances = NULL;
    CpaStatus status = CPA_STATUS_FAIL;


    /* Get the setup pointer */
    tmpSetup = (compression_test_params_t *)(testSetup->setupPtr);
    /* update the setup structure with setup parameters */
    dcSetup.bufferSize = tmpSetup->bufferSize;
    dcSetup.corpus =  tmpSetup->corpus;
    dcSetup.setupData = tmpSetup->setupData;
    dcSetup.dcSessDir = tmpSetup->dcSessDir;
    dcSetup.syncFlag = tmpSetup->syncFlag;
    dcSetup.numLoops = tmpSetup->numLoops;
    /*give our thread a unique memory location to store performance stats*/
    dcSetup.performanceStats = testSetup->performanceStats;
    dcSetup.isDpApi = CPA_FALSE;
    testSetup->performanceStats->threadReturnStatus = CPA_STATUS_SUCCESS;

    status = calculateRequireBuffers(&dcSetup);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT("Error calculating required buffers\n");
        sampleCodeThreadExit();
    }
    /*this barrier is to halt this thread when run in user space context, the
      * startThreads function releases this barrier, in kernel space is does
      * nothing, but kernel space threads do not start
      * until we call startThreads anyway
      */
    startBarrier();

    /*Initialise the statsPrintFunc to NULL, the dcPrintStats function will
     * be assigned if compression completes successfully
     */
    testSetup->statsPrintFunc=NULL;

    /* Get the number of instances */
    status = cpaDcGetNumInstances(&numInstances);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR(" Unable to get number of DC instances\n");
        qaeMemFree((void**)&dcSetup.numberOfBuffers);
        return ;
    }
    if(0 == numInstances)
    {
        PRINT_ERR(" DC Instances are not present\n");
        qaeMemFree((void**)&dcSetup.numberOfBuffers);
        sampleCodeThreadExit();
    }
    instances = qaeMemAlloc(sizeof(CpaInstanceHandle)*numInstances);
    if(NULL == instances )
    {
        PRINT_ERR("Unable to allocate Memory for Instances\n");
        qaeMemFree((void**)&dcSetup.numberOfBuffers);
        sampleCodeThreadExit();
    }

    /*get the instance handles so that we can start
     * our thread on the selected instance
     */
    status = cpaDcGetInstances(numInstances, instances);
    if( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("get instances failed");
        qaeMemFree((void**)&instances);
        qaeMemFree((void**)&dcSetup.numberOfBuffers);
        sampleCodeThreadExit();
    }
    /* give our thread a logical quick assist instance to use
     * use % to wrap around the max number of instances*/
    dcSetup.dcInstanceHandle=
        instances[(testSetup->logicalQaInstance)%numInstances];


    /*launch function that does all the work*/
    status = dcPerform(&dcSetup);
    if(CPA_STATUS_SUCCESS != status)
    {
        dcPrintTestData(&dcSetup);
        PRINT_ERR("Compression Thread FAILED\n");
        testSetup->performanceStats->threadReturnStatus = CPA_STATUS_FAIL;
    }
    else
    {
        /*set the print function that can be used to print
         * statistics at the end of the test
         * */
        testSetup->statsPrintFunc=(stats_print_func_t)dcPrintStats;
    }
    qaeMemFree((void**)&dcSetup.numberOfBuffers);
    qaeMemFree((void**)&instances);


    sampleCodeThreadExit();

}

CpaStatus setupDcTest(CpaDcCompType algorithm,
                 CpaDcSessionDir direction ,
                 CpaDcCompLvl compLevel,
                 CpaDcHuffType huffmanType,
                 CpaDcFileType fileType,
                 CpaDcSessionState state,
                 Cpa32U windowsSize,
                 Cpa32U testBufferSize,
                 corpus_type_t corpusType,
                 synchronous_flag_t syncFlag,
                 Cpa32U numLoops)
{

    compression_test_params_t* dcSetup = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;


    /* Initialize the Performance device */
    if(testTypeCount_g >= MAX_THREAD_VARIATION)
    {
        PRINT_ERR("Maximum Support Thread Variation has been exceeded\n");
        PRINT_ERR("Number of Thread Variations created: %d",
                                                 testTypeCount_g);
        PRINT_ERR(" Max is %d\n", MAX_THREAD_VARIATION);
        return CPA_STATUS_FAIL;
    }

    /* Populate Corpus */
    status = populateCorpus(testBufferSize,corpusType);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to load one or more corpus files, have they been "
                "extracted to /lib/firmware?\n");
        return CPA_STATUS_FAIL;
    }

    /*Start DC Services */
    status = startDcServices(DYNAMIC_BUFFER_AREA, TEMP_NUM_BUFFS);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error in Starting Dc Services\n");
        return CPA_STATUS_FAIL;
    }
    /* start polling threads if polling is enabled in the configuration file */
    if(CPA_STATUS_SUCCESS != dcCreatePollingThreadsIfPollingIsEnabled())
    {
        PRINT_ERR("Error creating polling threads\n");
        return CPA_STATUS_FAIL;
    }

    /* Get the framework setup pointer */
    /* thread_setup_g is a multi-dimensional array that
     * stores the setup for all thread
     * variations in an array of characters.
     * we store our test setup at the
     * start of the second array ie index 0.
     * There maybe multi thread types
     * (setups) running as counted by testTypeCount_g*/

    /* thread_setup_g is a multi-dimensional char array
     * we need to cast it to the
     * Compression structure
     */
    dcSetup = (compression_test_params_t*)
                     &thread_setup_g[testTypeCount_g][0];

    /* Set the performance function to the actual performance function
     * that actually does all the performance
     */
    testSetupData_g[testTypeCount_g].performance_function =
                                            (performance_func_t)dcPerformance;

    /* update the setup_g with buffersize */
    testSetupData_g[testTypeCount_g].packetSize=testBufferSize;
    /* Data compression setup data */
    dcSetup->setupData.compLevel = compLevel;
    dcSetup->setupData.compType = algorithm;
    /* always Set the Session direction to COMBINED
     * but, the time stamps will be taken as per the
     * session direction given by the User
     */
    dcSetup->setupData.sessDirection = CPA_DC_DIR_COMBINED;
    dcSetup->setupData.checksum = CPA_DC_NONE;
    dcSetup->setupData.huffType = huffmanType;
    dcSetup->setupData.fileType = fileType;
    dcSetup->setupData.sessState = state;
    dcSetup->setupData.deflateWindowSize = windowsSize;
    dcSetup->corpus = corpusType;
    dcSetup->bufferSize = testBufferSize;
    dcSetup->dcSessDir = direction;
    dcSetup->setupData.autoSelectBestHuffmanTree = CPA_DC_ASB_DISABLED;
    dcSetup->syncFlag = syncFlag;
    dcSetup->numLoops = numLoops;
    dcSetup->isDpApi = CPA_FALSE;

    /* Stateful Compression only supports a single request in-flight for
     * each session. Each request can be submitted asynchronously but is
     * required to block until callback fires.
     * For the sample code we only issue Stateful requests synchronously.
     */
    if( CPA_DC_STATEFUL == state)
    {
        PRINT_ERR("Stateful Compression not supported in this sample code\n");
    }

    return status;
}


