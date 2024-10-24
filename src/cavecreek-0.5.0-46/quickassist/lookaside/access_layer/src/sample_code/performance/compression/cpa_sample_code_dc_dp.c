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
 * @file cpa_sample_code_dc_dp.c
 *
 *
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *    This is a sample code that uses Data Plane - Data Compression(DC)  APIs.

 *    This code preallocates a number of buffers as based on the size of each
 *    file defined in the calgary/canterbury corpus. The preallocated buffers
 *    are then populated with the corpus files as defined in
 *    setup->testBuffersize.
 *    Time stamping is started prior to the first performed DC
 *    Operation and is stopped when all callbacks have returned.
 *
 *****************************************************************************/

#include "cpa_sample_code_utils_common.h"
#include "cpa_sample_code_dc_utils.h"
#include "cpa_sample_code_crypto_utils.h"
#include "cpa_sample_code_dc_dp.h"
#include "cpa_sample_code_framework.h"

#include "icp_sal_poll.h"

#define OPERATIONS_POLLING_INTERVAL         (10)
Cpa32U dcPollingInterval_g = OPERATIONS_POLLING_INTERVAL;

/*****************************************************************************
* @ingroup sampleCompressionDpPerf
*
* @description
* Set the polling interval. The Polling interval is the number of succesful
* submissions before the driver is polled for responses
* ***************************************************************************/
CpaStatus setDcPollingInterval(Cpa64U pollingInterval)
{
    dcPollingInterval_g = pollingInterval;
   return CPA_STATUS_SUCCESS;
}
EXPORT_SYMBOL(setDcPollingInterval);

/*****************************************************************************
* @ingroup sampleCompressionDpPerf
*
* @description
* Print the polling interval. The Polling interval is the number
* of succesful submissions before the driver is polled for responses
* ***************************************************************************/
CpaStatus printDcPollingInterval(void)
{
   PRINT("Compression Polling Interval: %u\n", dcPollingInterval_g);
   return CPA_STATUS_SUCCESS;
}
EXPORT_SYMBOL(printDcPollingInterval);

/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Callback function after a call to the DC API
 ******************************************************************************/
void dcDpCallbackFunction(CpaDcDpOpData *pOpData)
{

    perf_data_t* pPerfData = (perf_data_t *)pOpData->pCallbackTag;


    /* increment responses */
    pPerfData->responses++;

    /*if we have received the pre-set numOperations, then get the clock cycle
     * as a timestamp and post the Semaphore to release parent thread*/
    if (pPerfData->numOperations == pPerfData->responses)
    {
        pPerfData->endCyclesTimestamp=sampleCodeTimestamp();
    }
}

/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Allocate memory for PhysFlatBuffers
 ******************************************************************************/
static CpaStatus createBuffersDp(Cpa32U buffSize, Cpa32U numBuffs,
                        CpaPhysFlatBuffer **pFlatBuffArray, Cpa32U nodeId)
{
    Cpa32U i = 0;

    for(i = 0; i< numBuffs; i++)
    {
        pFlatBuffArray[i]= qaeMemAllocNUMA((sizeof(CpaPhysFlatBuffer)),
                nodeId, BYTE_ALIGNMENT_64);
        if(NULL == pFlatBuffArray[i])
        {
            PRINT_ERR(" Unable to allocate flat buffer\n");
            return CPA_STATUS_FAIL;
        }

        pFlatBuffArray[i]->dataLenInBytes = buffSize;

        /* At this point the bufferPhysAddr is not yet a physical address
         * We convert it to a physical address after we use it as a virtual
         * address
         */
        pFlatBuffArray[i]->bufferPhysAddr =(CpaPhysicalAddr)(SAMPLE_CODE_UINT)
          qaeMemAllocNUMA(buffSize, nodeId, BYTE_ALIGNMENT_64);

        memset((SAMPLE_CODE_UINT  *)(SAMPLE_CODE_UINT)
                pFlatBuffArray[i]->bufferPhysAddr,0, buffSize);

        if(NULL == (void *)(SAMPLE_CODE_UINT)pFlatBuffArray[i]->bufferPhysAddr)
        {
            PRINT_ERR(" Unable to allocate flat buffer phys addr\n");
            return CPA_STATUS_FAIL;
        }
    }
    return CPA_STATUS_SUCCESS;
}


/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Allocate memory for OpData structures
 ******************************************************************************/
static CpaStatus createOpDataDp(Cpa32U numBuffs,
        CpaDcDpOpData **pOpDataArray, Cpa32U nodeId)
{
    Cpa32U i = 0;

    for(i = 0; i< numBuffs; i++)
    {
        pOpDataArray[i]= qaeMemAllocNUMA((sizeof(CpaDcDpOpData)),
                nodeId, BYTE_ALIGNMENT_8);
        if(NULL == pOpDataArray[i])
        {
            PRINT_ERR(" Unable to allocate op data\n");
            return CPA_STATUS_FAIL;
        }

    }
    return CPA_STATUS_SUCCESS;
}


/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Free memory for PhysFlatBuffers
 ******************************************************************************/
static void freeBuffersDp(CpaPhysFlatBuffer ***pFlatBuffArray,
                     Cpa32U numberOfFiles, compression_test_params_t* setup)
{
    Cpa32U  i = 0, j = 0;

    if(NULL == pFlatBuffArray)
    {
       /* Return silently */
        return;
    }

    for(i=0; i < numberOfFiles; i++)
    {
       for(j = 0; j < setup->numberOfBuffers[i]; j++)
       {
           if (NULL != pFlatBuffArray[i][j])
           {
               if(NULL != (void *)(SAMPLE_CODE_UINT)
                       pFlatBuffArray[i][j]->bufferPhysAddr)
               {
                   qaeMemFreeNUMA((void**)&pFlatBuffArray[i][j]->
                           bufferPhysAddr);
               }
               qaeMemFreeNUMA((void**)&pFlatBuffArray[i][j]);
           }
       }
       if(NULL != pFlatBuffArray[i])
       {
           qaeMemFreeNUMA((void**)&pFlatBuffArray[i]);
       }
    }
    qaeMemFreeNUMA((void**)&pFlatBuffArray);
}

/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Free memory for the compression op data structs
 ******************************************************************************/
static void freeOpDataDp(CpaDcDpOpData ***compressionOpData,
                     Cpa32U numberOfFiles, compression_test_params_t* setup)
{
    Cpa32U  i = 0, j = 0;

    if(NULL == compressionOpData)
    {
       /* Return silently */
        return;
    }

    for(i=0; i < numberOfFiles; i++)
    {
       for(j = 0; j < setup->numberOfBuffers[i]; j++)
       {
           if(NULL != compressionOpData[i][j])
           {
               qaeMemFreeNUMA((void**)&compressionOpData[i][j]);
           }
       }
       if(NULL != compressionOpData[i])
       {
           qaeMemFreeNUMA((void**)&compressionOpData[i]);
       }
    }

    qaeMemFreeNUMA((void**)&compressionOpData);
}

/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Compress the corpus before we do decompression
 ******************************************************************************/
static CpaStatus compressCorpusPreDecomp(compression_test_params_t* setup,
                                     CpaDcDpOpData ***compressionOpData,
                                     perf_data_t *perfData)
{
    /* Local Variable Declaration */
    Cpa32U i = 0, j = 0;
    Cpa32U submittedOps = 0;

    /* Status variable */
    CpaStatus status = CPA_STATUS_FAIL;

    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        /* call the compress api */
        for(j=0; j < setup->numberOfBuffers[i]; j++)
        {
            do {
                status = cpaDcDpEnqueueOp(compressionOpData[i][j],
                                          CPA_TRUE);
                if(CPA_STATUS_RETRY == status)
                {
                    setup->performanceStats->retries++;
                    icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
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
            if(++submittedOps == OPERATIONS_POLLING_INTERVAL)
            {
                icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
            }
        } /* End of number of buffers Loop */
    } /* End of number of Files Loop*/

    /* While there are pending requests, continue to poll */
    status = dcDpPollNumOperations(setup->performanceStats,
            setup->dcInstanceHandle,setup->performanceStats->numOperations);
    return status;
}


/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Perform EnqueueBatchOp compression
 *****************************************************************************/
static CpaStatus performDcDpBatchOp(compression_test_params_t* setup,
                                    CpaDcDpOpData ***compressionOpData,
                                    perf_data_t *perfData,
                                    Cpa32U numOfOpsToBatch)
{
    /* Local Variable Declaration */
    Cpa32U i = 0, j = 0, numLoops = 0;
    Cpa32U compressLoops = 0;
    Cpa32U remainingOps = 0;
    Cpa32U submittedOps = 0;
    Cpa32U numOps = 0;
    Cpa32U totalBuffers = 0;
    /* Status variable */
    CpaStatus status = CPA_STATUS_FAIL;

    /* Zero performance stats */
    memset(perfData, 0, sizeof(perf_data_t));

    for(i = 0; i < corpus_g.numFilesInCorpus; i++ )
    {
        totalBuffers += setup->numberOfBuffers[i];
    }
    setup->performanceStats->numOperations =
        (Cpa64U)totalBuffers * (Cpa64U)setup->numLoops;
    compressLoops = setup->numLoops;

    /* this Barrier will waits until all the threads get to this point */
    sampleCodeBarrier();

    /* generate the start time stamp */
    perfData->startCyclesTimestamp = sampleCodeTimestamp();

    for(numLoops = 0; numLoops < compressLoops; numLoops++)
    {
        for(i=0; i<corpus_g.numFilesInCorpus; i++)
        {
            /* call the compress api */
            for(j=0; j < setup->numberOfBuffers[i]; j=j+numOfOpsToBatch)
            {
                    do{
                       /* Is the next batch size greater than the amount of
                       * buffers that we have left
                       */
                       if (j+numOfOpsToBatch > setup->numberOfBuffers[i])
                       {
                           remainingOps = setup->numberOfBuffers[i] - j;
                       }
                       else
                       {
                           remainingOps = numOfOpsToBatch;
                       }

                       status = cpaDcDpEnqueueOpBatch(remainingOps,
                                    &compressionOpData[i][j], CPA_TRUE);

                       if(CPA_STATUS_RETRY == status)
                       {
                           setup->performanceStats->retries++;
                           icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
                           AVOID_SOFTLOCKUP;

                       }
                    } while(CPA_STATUS_RETRY == status);
                    numOps += remainingOps;

                /* Check Status */
                if(CPA_STATUS_SUCCESS != status )
                {
                    PRINT_ERR("Data Compression Failed %d\n\n", status);
                    perfData->threadReturnStatus = CPA_STATUS_FAIL;
                    break;
                }
                if(++submittedOps == OPERATIONS_POLLING_INTERVAL)
                {
                    icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
                }
            } /* End of number of buffers Loop */
            if(CPA_STATUS_SUCCESS  != status)
            {
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

    if( CPA_STATUS_SUCCESS == status )
    {
        /* While there are pending requests, continue to poll */
        status = dcDpPollNumOperations(setup->performanceStats,
                setup->dcInstanceHandle,setup->performanceStats->numOperations);
    }
    return status;
}

/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Perform EnqueueOp compression
 ******************************************************************************/
static CpaStatus performDcDpEnqueueOp(compression_test_params_t* setup,
                                     CpaDcDpOpData ***compressionOpData,
                                     perf_data_t *perfData)
{
    /* Local Variable Declaration */
    Cpa32U i = 0, j = 0, numLoops = 0;
    Cpa32U numOps = 0;
    Cpa32U compressLoops = 0;
    CpaBoolean performOpNowFlag = CPA_FALSE;
    /* Status variable */
    CpaStatus status = CPA_STATUS_FAIL;
    Cpa64U numOps2 = 0;
    Cpa32U totalBuffers = 0;
    Cpa64U nextPoll = dcPollingInterval_g;

    /* Zero performance stats */
    memset(perfData, 0, sizeof(perf_data_t));

    for(i = 0; i < corpus_g.numFilesInCorpus; i++ )
    {
        totalBuffers += setup->numberOfBuffers[i];
    }

    setup->performanceStats->numOperations =
        (Cpa64U)totalBuffers * (Cpa64U)setup->numLoops;
    compressLoops = setup->numLoops;

    /* this Barrier will waits until all the threads get to this point */
    sampleCodeBarrier();

    /* generate the start time stamp */
    perfData->startCyclesTimestamp = sampleCodeTimestamp();

    for(numLoops = 0; numLoops < compressLoops; numLoops++)
    {
        for(i=0; i<corpus_g.numFilesInCorpus; i++)
        {
            /* Loop through all our buffers and call EnqueueOp, until we have
             * enqueued numRequests, and when we do, call performOpNow to clear
             * the ring and actually execute the operations */
            for(j=0; j < setup->numberOfBuffers[i]; j++)
            {
                    /* if we have reached the enqueue limit or we are about to
                     * submit the last buffer of the current corpus file then
                     * enqueue and perform the enqueued operations now.
                     */
                    if( ++numOps % setup->numRequests == 0 ||
                            j+1 == setup->numberOfBuffers[i] )
                    {
                        performOpNowFlag = CPA_TRUE;
                    }
                    else
                    {
                        performOpNowFlag = CPA_FALSE;
                    }
                do {
                    status = cpaDcDpEnqueueOp(compressionOpData[i][j],
                            performOpNowFlag);

                    if(CPA_STATUS_RETRY == status)
                    {
                        setup->performanceStats->retries++;
                        icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
                        nextPoll = numOps2 + dcPollingInterval_g;
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
                ++numOps2;
                if(numOps2 == nextPoll)
                {
                    icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
                    nextPoll = numOps2 + dcPollingInterval_g;
                }

                if(numOps % OPERATIONS_POLLING_INTERVAL == 0)
                {
                    icp_sal_DcPollDpInstance(setup->dcInstanceHandle,0);
                }
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
            break;
        }
    } /* End of compression Loops */
    if( CPA_STATUS_SUCCESS == status )
    {
        /* While there are pending requests, continue to poll */
        status = dcDpPollNumOperations(setup->performanceStats,
                setup->dcInstanceHandle,setup->performanceStats->numOperations);
    }

    return status;
}

/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Main executing function which selects the operation type to be performed(
 *  Enqueue/Batch) and the direction of the operation(compress/decompress)
 ******************************************************************************/
static CpaStatus PerformOp(compression_test_params_t* setup,
                            CpaDcDpOpData ***compressionOpData,
                            CpaDcDpOpData ***decompressionOpData,
                            perf_data_t *perfData)
{
    CpaStatus status = CPA_STATUS_FAIL;
    switch(setup->dpTestType)
    {
    case DC_DP_ENQUEUEING:
        if(CPA_DC_DIR_COMPRESS == setup->dcSessDir)
        {
            status = performDcDpEnqueueOp(setup, compressionOpData,
                    perfData);

        }
        else
        {
            status = performDcDpEnqueueOp(setup, decompressionOpData,
                    perfData);
        }
        break;
    case DC_DP_BATCHING:
        if(CPA_DC_DIR_COMPRESS == setup->dcSessDir)
        {
            status = performDcDpBatchOp(setup, compressionOpData,
                    perfData, setup->numRequests);
        }
        else
        {
            status = performDcDpBatchOp(setup, decompressionOpData,
                    perfData, setup->numRequests);
        }
        break;
    default:
        PRINT_ERR("Neither enqueueing or batching mode\n");
        return CPA_STATUS_FAIL;
    }
    return status;
}




/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Main executing function which allocates/frees memory which is required,
 *  and performs Enqueue/Batch operations as required
 ******************************************************************************/
static CpaStatus dcDpPerform( compression_test_params_t* setup )
{
    /* Looping control variables */
    Cpa32U i = 0, j = 0;

    /* Status variable */
    CpaStatus status = CPA_STATUS_SUCCESS;

    /* NUMA node ID */
    Cpa32U nodeId = 0;

    /* File data pointer */
    Cpa8U *fileDataPtr = NULL;

    /* Performance data Structure */
    perf_data_t *perfData = NULL;

    /* Total num of buffers */
    Cpa32U totalBuffs = 0;

    /* Buffer size */
    Cpa32U bufferSize = 0;

   /* Session size */
    Cpa32U sessionSize = 0;

    /* Session handle */
    CpaDcSessionHandle *pSessionHandle = NULL;

    /* Session direction */
    CpaDcSessionDir dcSessDirReq = CPA_DC_DIR_COMPRESS;

    /* Buffer counters */
    Cpa32U amountOfFullBuffers = 0;

    /* Two dimensional array of CpaDcDpOpData pointers which will be used to
     * reference values from a 2 dimensional array through the corpus file and
     * buffer number
     */
    CpaDcDpOpData ***compressionOpData = NULL;
    CpaDcDpOpData ***decompressionOpData = NULL;

    /* Declare src, dst & comp buffers */
    CpaPhysFlatBuffer ***srcFlatBuffArray = NULL;
    CpaPhysFlatBuffer ***dstFlatBuffArray = NULL;
    CpaPhysFlatBuffer ***cmpFlatBuffArray = NULL;

    if(NULL == setup)
    {
        PRINT_ERR("Test Setup Pointer is NULL\n");
        return CPA_STATUS_FAIL;
    }

    dcSessDirReq = setup->dcSessDir;
    perfData = setup->performanceStats;
    bufferSize = setup->bufferSize;

    /* Check what NUMA node we are on in order to allocate memory */
    status = sampleCodeDcGetNode(setup->dcInstanceHandle, &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }

    srcFlatBuffArray = qaeMemAllocNUMA(
                 (corpus_g.numFilesInCorpus*sizeof(CpaPhysFlatBuffer *)),
                 nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == srcFlatBuffArray)
    {
        PRINT_ERR("unable to allocate srcFlatBuffArray\n");
        return CPA_STATUS_FAIL;
    }

    dstFlatBuffArray = qaeMemAllocNUMA(
                  (corpus_g.numFilesInCorpus*sizeof(CpaPhysFlatBuffer *)),
                  nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == dstFlatBuffArray)
    {
        qaeMemFreeNUMA((void**)&srcFlatBuffArray);
        PRINT_ERR("unable to allocate dstFlatBuffArray \n");
        return CPA_STATUS_FAIL;
    }

    cmpFlatBuffArray = qaeMemAllocNUMA(
                  (corpus_g.numFilesInCorpus*sizeof(CpaPhysFlatBuffer *)),
                  nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == cmpFlatBuffArray)
    {
        qaeMemFreeNUMA((void**)&srcFlatBuffArray);
        qaeMemFreeNUMA((void**)&dstFlatBuffArray);
        PRINT_ERR("unable to allocate cmpFlatBuffArray \n");
        return CPA_STATUS_FAIL;
    }

    compressionOpData = qaeMemAllocNUMA(
                  (corpus_g.numFilesInCorpus*sizeof(CpaDcDpOpData *)),
                  nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == compressionOpData)
    {
        qaeMemFreeNUMA((void**)&srcFlatBuffArray);
        qaeMemFreeNUMA((void**)&dstFlatBuffArray);
        qaeMemFreeNUMA((void**)&cmpFlatBuffArray);
        PRINT_ERR("unable to allocate compressionOpData \n");
        return CPA_STATUS_FAIL;
    }

    decompressionOpData = qaeMemAllocNUMA(
                  (corpus_g.numFilesInCorpus*sizeof(CpaDcDpOpData *)),
                  nodeId, BYTE_ALIGNMENT_64);
    /* Check for NULL */
    if( NULL == decompressionOpData)
    {
        qaeMemFreeNUMA((void**)&srcFlatBuffArray);
        qaeMemFreeNUMA((void**)&dstFlatBuffArray);
        qaeMemFreeNUMA((void**)&cmpFlatBuffArray);
        qaeMemFreeNUMA((void**)&compressionOpData);
        PRINT_ERR("unable to allocate decompressionOpData \n");
        return CPA_STATUS_FAIL;
    }

    /* populate the flat buffer array with number of buffers required
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
        srcFlatBuffArray[i] = qaeMemAllocNUMA(
               ( setup->numberOfBuffers[i]*(sizeof(CpaPhysFlatBuffer *)))
               ,nodeId, BYTE_ALIGNMENT_64);
        /* Check for NULL */
        if(NULL == srcFlatBuffArray[i])
        {
            PRINT_ERR("Unable to allocate Memory for srcFlatBuffArray\n ");
            freeBuffersDp(srcFlatBuffArray, i, setup);
            freeBuffersDp(dstFlatBuffArray, i, setup);
            freeBuffersDp(cmpFlatBuffArray, i, setup);
            freeOpDataDp(compressionOpData, i, setup);
            freeOpDataDp(decompressionOpData, i, setup);

            return CPA_STATUS_FAIL;
        }

        dstFlatBuffArray[i] = qaeMemAllocNUMA(
              ( setup->numberOfBuffers[i]*(sizeof(CpaPhysFlatBuffer *)))
              ,nodeId, BYTE_ALIGNMENT_64);
        /* Check for NULL */
        if(NULL == dstFlatBuffArray[i])
        {
            PRINT_ERR("Unable to allocate Memory for dstFlatBuffArray\n ");
            freeBuffersDp(srcFlatBuffArray, i, setup);
            freeBuffersDp(dstFlatBuffArray, i, setup);
            freeBuffersDp(cmpFlatBuffArray, i, setup);
            freeOpDataDp(compressionOpData, i, setup);
            freeOpDataDp(decompressionOpData, i, setup);

            return CPA_STATUS_FAIL;
        }

        cmpFlatBuffArray[i] = qaeMemAllocNUMA(
                ( setup->numberOfBuffers[i]*
                (sizeof(CpaPhysFlatBuffer *))),nodeId, BYTE_ALIGNMENT_64);
        /* Check for NULL */
        if(NULL == cmpFlatBuffArray[i])
        {
            PRINT_ERR("Unable to allocate Memory for cmpFlatBuffArray\n ");
            freeBuffersDp(srcFlatBuffArray, i, setup);
            freeBuffersDp(dstFlatBuffArray, i, setup);
            freeBuffersDp(cmpFlatBuffArray, i, setup);
            freeOpDataDp(compressionOpData, i, setup);
            freeOpDataDp(decompressionOpData, i, setup);

            return CPA_STATUS_FAIL;
        }

        compressionOpData[i] = qaeMemAllocNUMA(
                 ( setup->numberOfBuffers[i]*
                 (sizeof(CpaDcDpOpData *))),nodeId, BYTE_ALIGNMENT_64);
         /* Check for NULL */
         if(NULL == compressionOpData[i])
         {
             PRINT_ERR("Unable to allocate Memory for compressionOpData\n ");
             freeBuffersDp(srcFlatBuffArray, i, setup);
             freeBuffersDp(dstFlatBuffArray, i, setup);
             freeBuffersDp(cmpFlatBuffArray, i, setup);
             freeOpDataDp(compressionOpData, i, setup);
             freeOpDataDp(decompressionOpData, i, setup);

             return CPA_STATUS_FAIL;
         }

         decompressionOpData[i] = qaeMemAllocNUMA(
                  ( setup->numberOfBuffers[i]*
                  (sizeof(CpaDcDpOpData *))),nodeId, BYTE_ALIGNMENT_64);
          /* Check for NULL */
          if(NULL == decompressionOpData[i])
          {
              PRINT_ERR("Unable to allocate Memory for decompressionOpData\n ");
              freeBuffersDp(srcFlatBuffArray, i, setup);
              freeBuffersDp(dstFlatBuffArray, i, setup);
              freeBuffersDp(cmpFlatBuffArray, i, setup);
              freeOpDataDp(compressionOpData, i, setup);
              freeOpDataDp(decompressionOpData, i, setup);

              return CPA_STATUS_FAIL;
          }
     }

    /* Allocate flat buffers for each file */
    for(i=0; i< corpus_g.numFilesInCorpus; i++)
    {
        status = createBuffersDp(bufferSize,
                               setup->numberOfBuffers[i],
                               srcFlatBuffArray[i], nodeId);

        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to create flat buffers for srcFlatBuffArray\n");
            freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

            return CPA_STATUS_FAIL;
        }

        status = createOpDataDp(setup->numberOfBuffers[i],
                    compressionOpData[i], nodeId);

        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to create compression opdata\n");
            freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

            return CPA_STATUS_FAIL;
        }

        /* When compressing,small packet sizes the destination buffer
         * may need to be larger than the source buffer to accommodate
         * huffman data, so allocate double the source buffer size
         */
        if(MIN_DST_BUFFER_SIZE >= bufferSize)
        {
            status = createBuffersDp(bufferSize*EXTRA_BUFFER,
                                   setup->numberOfBuffers[i],
                                       dstFlatBuffArray[i], nodeId);
        }
        else
        {
            status = createBuffersDp(bufferSize,
                                       setup->numberOfBuffers[i],
                                       dstFlatBuffArray[i], nodeId);
        }
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to create buffers for dstFlatBuffArray\n");
            freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

            return CPA_STATUS_FAIL;
        }
        /* When Decompression, the FW expects the buffer size
         * to be greater than the source buffer, so allocate double the
         * size of the source buffer
         */
        status = createBuffersDp((bufferSize*EXTRA_BUFFER),
                                setup->numberOfBuffers[i],
                               cmpFlatBuffArray[i], nodeId);

        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to create buffers for cmpFlatBuffArray\n");
            freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

            return CPA_STATUS_FAIL;
        }

        status = createOpDataDp(setup->numberOfBuffers[i],
                    decompressionOpData[i], nodeId);

        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to create decompression opdata\n");
            freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

            return CPA_STATUS_FAIL;
        }
    }

    /* Copy data into Flat Buffers from the corpus structure */
    for (i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        fileDataPtr = corpus_g.fileArray[i].corpusBinaryData;
        /* get the number of full Buffers */
        amountOfFullBuffers = (corpus_g.fileArray[i].corpusBinaryDataLen) /
                                                               bufferSize;
        /* Copy the data into Flat buffers */
        for(j=0; j< amountOfFullBuffers; j++)
        {
            memcpy(((void *)(SAMPLE_CODE_UINT)
                    srcFlatBuffArray[i][j]->bufferPhysAddr),
                                       fileDataPtr, bufferSize);
            fileDataPtr += bufferSize;
        }
        fileDataPtr = NULL;
    }


    setup->setupData.sessDirection = CPA_DC_DIR_COMBINED;

    /* Get Size for DC Session */
    status = cpaDcDpGetSessionSize(setup->dcInstanceHandle,
              &(setup->setupData), &sessionSize);
    if ( CPA_STATUS_SUCCESS != status )
    {
       PRINT_ERR("cpaDcGetSessionSize() returned %d status.\n",status);
       return CPA_STATUS_FAIL;
    }

    /* Allocate Memory for DC Session */
    pSessionHandle = (CpaDcSessionHandle)
                       qaeMemAllocNUMA((sessionSize),
                       nodeId,BYTE_ALIGNMENT_64);
    if (NULL == pSessionHandle)
    {
        PRINT_ERR("Unable to allocate Memory for Session Handle\n");
        freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
        freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

        return CPA_STATUS_FAIL;
    }
    /* Setup and init Session */
    status = cpaDcDpInitSession(setup->dcInstanceHandle, pSessionHandle,
                                &(setup->setupData));
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Problem in session creation: status = %d \n", status);
        qaeMemFreeNUMA((void**)&pSessionHandle);
        freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
        freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

        return CPA_STATUS_FAIL;
    }

    /* Register a callback function */
    status = cpaDcDpRegCbFunc(setup->dcInstanceHandle,
            (CpaDcDpCallbackFn)dcDpCallbackFunction);

    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to register callback fn, status = %d \n", status);
        freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
        freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
        freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

        return CPA_STATUS_FAIL;
    }

    /* Populate OpData struct */
    for(i=0; i<corpus_g.numFilesInCorpus; i++)
    {
        for(j=0; j < setup->numberOfBuffers[i]; j++)
        {
            compressionOpData[i][j]->dcInstance = setup->dcInstanceHandle;

            compressionOpData[i][j]->pSessionHandle = pSessionHandle;

            compressionOpData[i][j]->srcBuffer =
                     (CpaPhysicalAddr)(SAMPLE_CODE_UINT)qaeVirtToPhysNUMA(
              (void *)(SAMPLE_CODE_UINT)srcFlatBuffArray[i][j]->bufferPhysAddr);

            compressionOpData[i][j]->destBuffer =
                    (CpaPhysicalAddr)(SAMPLE_CODE_UINT)qaeVirtToPhysNUMA(
              (void *)(SAMPLE_CODE_UINT)dstFlatBuffArray[i][j]->bufferPhysAddr);

            compressionOpData[i][j]->srcBufferLen =
                                srcFlatBuffArray[i][j]->dataLenInBytes;

            compressionOpData[i][j]->destBufferLen =
                                dstFlatBuffArray[i][j]->dataLenInBytes;

            /* Even though we are using CpaFlatBuffers we also have to set the
             * bufferLenToCompress and bufferLenForData fields which are used to
             * support CpaBufferList where srcBufferLen/destBufferLen take the
             * value CPA_DP_BUFLIST.
             */
            compressionOpData[i][j]->bufferLenToCompress =
                                srcFlatBuffArray[i][j]->dataLenInBytes;

            compressionOpData[i][j]->bufferLenForData =
                                dstFlatBuffArray[i][j]->dataLenInBytes;

            compressionOpData[i][j]->pCallbackTag =
                                perfData;

            compressionOpData[i][j]->sessDirection = CPA_DC_DIR_COMPRESS;

            compressionOpData[i][j]->thisPhys = (CpaPhysicalAddr)
                  (SAMPLE_CODE_UINT)qaeVirtToPhysNUMA(compressionOpData[i][j]);
        }
    }

    if(CPA_DC_DIR_COMPRESS == dcSessDirReq)
    {
        /* Our compression session is already set up. We need to set our
         * number of operations and then we are ready to batch or enqueue
         */
        perfData->numOperations = (Cpa64U)totalBuffs*(Cpa64U)setup->numLoops;
    }
    else if(CPA_DC_DIR_DECOMPRESS == dcSessDirReq)
    {
        perfData->numOperations = totalBuffs;

        /* Compress the corpus so we can de-compress it */
        status = compressCorpusPreDecomp(setup, compressionOpData, perfData);
        if( CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Could not compress corpus before Decompression = %d \n",
                    status);
            freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
            freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus, setup);

            return CPA_STATUS_FAIL;
        }
        /* Swap the data in OpData structs */
        for(i=0; i<corpus_g.numFilesInCorpus; i++)
        {
            for(j=0; j < setup->numberOfBuffers[i]; j++)
            {
                /* Update the compressed buffers length */
                dstFlatBuffArray[i][j]->dataLenInBytes =
                            compressionOpData[i][j]->results.produced;

                decompressionOpData[i][j]->dcInstance = setup->dcInstanceHandle;

                decompressionOpData[i][j]->pSessionHandle = pSessionHandle;

                decompressionOpData[i][j]->srcBuffer = (CpaPhysicalAddr)
                    (SAMPLE_CODE_UINT)qaeVirtToPhysNUMA((void *)
                    (SAMPLE_CODE_UINT)dstFlatBuffArray[i][j]->bufferPhysAddr);

                decompressionOpData[i][j]->destBuffer = (CpaPhysicalAddr)
                    (SAMPLE_CODE_UINT)qaeVirtToPhysNUMA( (void *)
                     (SAMPLE_CODE_UINT)cmpFlatBuffArray[i][j]->bufferPhysAddr);

                decompressionOpData[i][j]->srcBufferLen =
                            dstFlatBuffArray[i][j]->dataLenInBytes;

                decompressionOpData[i][j]->destBufferLen =
                            cmpFlatBuffArray[i][j]->dataLenInBytes;

                /* Even though we are using CpaFlatBuffers we also have to set
                 * the bufferLenToCompress and bufferLenForData fields which are
                 * used to support CpaBufferList where
                 * srcBufferLen/destBufferLen take the value CPA_DP_BUFLIST.
                 */
                decompressionOpData[i][j]->bufferLenToCompress =
                            dstFlatBuffArray[i][j]->dataLenInBytes;

                decompressionOpData[i][j]->bufferLenForData =
                            cmpFlatBuffArray[i][j]->dataLenInBytes;

                decompressionOpData[i][j]->sessDirection =
                    CPA_DC_DIR_DECOMPRESS;

                decompressionOpData[i][j]->pCallbackTag = perfData;

                decompressionOpData[i][j]->thisPhys = (CpaPhysicalAddr)
                 (SAMPLE_CODE_UINT)qaeVirtToPhysNUMA(decompressionOpData[i][j]);
           }
        }

        /* Update the number of operations/responses for the expected for the
         * pre decompression stage */
        perfData->numOperations = (Cpa64U)totalBuffs*(Cpa64U)setup->numLoops;
        perfData->responses = 0;
    }

    status = PerformOp(setup, compressionOpData,
                        decompressionOpData, perfData);
    if(CPA_STATUS_SUCCESS != status)
    {
           freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
           freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
           freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
           freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
           freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus,setup);

           return CPA_STATUS_FAIL;
    }

    /* Record the bytes consumed and produced from the compressionOpData
     * structures for later printing.
     */
    dcDpSetBytesProducedAndConsumed(compressionOpData,perfData,setup);

    /* Remove the DC session */
    status = cpaDcDpRemoveSession(setup->dcInstanceHandle, pSessionHandle);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to remove session\n");
    }
    qaeMemFreeNUMA((void**)&pSessionHandle);

    /* Free allocated src, dst & cmp memory */
    freeBuffersDp(srcFlatBuffArray, corpus_g.numFilesInCorpus, setup);
    freeBuffersDp(dstFlatBuffArray, corpus_g.numFilesInCorpus, setup);
    freeBuffersDp(cmpFlatBuffArray, corpus_g.numFilesInCorpus, setup);
    /* Free OpData structures */
    freeOpDataDp(compressionOpData, corpus_g.numFilesInCorpus, setup);
    freeOpDataDp(decompressionOpData, corpus_g.numFilesInCorpus,setup);



    return status;
}


/**
 *****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 *  Setup a compression thread for a given packet size.
 ******************************************************************************/

static void dcDpPerformance(single_thread_test_data_t* testSetup)
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
    dcSetup.dpTestType = tmpSetup->dpTestType;
    dcSetup.numRequests = tmpSetup->numRequests;
    dcSetup.numLoops = tmpSetup->numLoops;
    dcSetup.isDpApi = CPA_TRUE;
    /*give our thread a unique memory location to store performance stats*/
    dcSetup.performanceStats = testSetup->performanceStats;
    dcSetup.performanceStats->threadReturnStatus = CPA_STATUS_SUCCESS;
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
    status = dcDpPerform(&dcSetup);
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

/**
*****************************************************************************
*
*  External Function Interfaces
*
******************************************************************************/

/**
 ****************************************************************************
 * @ingroup sampleCompressionDpPerf
 *
 * @description
 * setup a Compression Data Plane API test
 * This function needs to be called from main to setup a compression test.
 * The framework createThreads function is then used to propagate this setup
 * across cores using different compression logical instances
 *****************************************************************************/

CpaStatus setupDcDpTest(CpaDcCompType algorithm,
        CpaDcSessionDir direction ,
        CpaDcCompLvl compLevel,
        CpaDcHuffType huffmanType,
        CpaDcFileType fileType,
        Cpa32U windowSize,
        Cpa32U testBufferSize,
        corpus_type_t corpusType,
        synchronous_flag_t syncFlag,
        dp_request_type_t dpTestType,
        Cpa32U numRequests,
        Cpa32U numLoops)
{
    compression_test_params_t* dcSetup = NULL;
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U numberOfBuffersPerFile = 0, i = 0;
    /* Ensure that the number of threads created do not exceed the amount of
     * threads supported by the sample code framework.
     */
    if(testTypeCount_g >= MAX_THREAD_VARIATION)
    {
        PRINT_ERR("Maximum Support Thread Variation has been exceeded\n");
        PRINT_ERR("Number of Thread Variations created: %d",
                testTypeCount_g);
        PRINT_ERR(" Max is %d\n", MAX_THREAD_VARIATION);
        return CPA_STATUS_FAIL;
    }

    status = populateCorpus(testBufferSize,corpusType);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to Populate corpus file\n");
        return CPA_STATUS_FAIL;
    }
    /*
     * Create DC instances handles, allocate temporary memory for dynamic
     * compression and create polling threads(if enabled in configuration)
     * */
    status = startDcServices(DYNAMIC_BUFFER_AREA, TEMP_NUM_BUFFS);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT("Error in Starting Dc Services\n");
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
            (performance_func_t)dcDpPerformance;

        /* update the setup_g with buffersize */
        testSetupData_g[testTypeCount_g].packetSize=testBufferSize;
        /* Data compression setup data */
        dcSetup->setupData.compLevel = compLevel;
        dcSetup->setupData.compType = algorithm;
        dcSetup->setupData.sessDirection = CPA_DC_DIR_COMPRESS;
        dcSetup->setupData.checksum = CPA_DC_NONE;
        dcSetup->setupData.huffType = huffmanType;
        dcSetup->setupData.fileType = fileType;
        dcSetup->setupData.sessState = CPA_DC_STATELESS;
        dcSetup->setupData.deflateWindowSize = DEFAULT_COMPRESSION_WINDOW_SIZE;
        dcSetup->corpus = corpusType;
        dcSetup->bufferSize = testBufferSize;
        dcSetup->dcSessDir = direction;
        dcSetup->syncFlag = syncFlag;
        dcSetup->dpTestType = dpTestType;
        dcSetup->numRequests = numRequests;
        dcSetup->numLoops = numLoops;
        dcSetup->setupData.autoSelectBestHuffmanTree = CPA_DC_ASB_DISABLED;
        dcSetup->isDpApi = CPA_TRUE;


        /* Ensure that the numbers of buffers required for each file is less
         * than or equal to the batch/enqueue amount.
         */
        for(i = 0; i < corpus_g.numFilesInCorpus; i++)
        {
            numberOfBuffersPerFile =
                corpus_g.fileArray[i].corpusBinaryDataLen / testBufferSize;

            if(corpus_g.fileArray[i].corpusBinaryDataLen % testBufferSize != 0)
            {
                numberOfBuffersPerFile++;
            }
            if(numberOfBuffersPerFile < numRequests)
            {
                PRINT_ERR("The batch/enqueue amount (%d) specified exceeds the"
                        " number of buffers available (%d)\n"
                        ,numRequests,numberOfBuffersPerFile);

                return CPA_STATUS_FAIL;
            }
            numberOfBuffersPerFile = 0;
        }


        return status;
}

