/**
 ***************************************************************************
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

/**
 *****************************************************************************
 * @file cpa_sample_code_dc_bnp.c
 *
 *
 * @ingroup compressionThreads
 *
 * @description
 *****************************************************************************/

#include "cpa_sample_code_dc_bnp.h"
#include "icp_sal_poll.h"
#include "cpa_dc_bp.h"
#include "cpa_sample_code_dc_utils.h"

#ifdef KERNEL_SPACE
#include <linux/zlib.h>
#include <linux/crc32.h>
#include <linux/vmalloc.h>
#else
#include <zlib.h>
#endif


STATIC CpaStatus getTotalSglSize(const Cpa32U originalSize,
        CpaDcSkipData *skipData,
        Cpa32U *totalSglSize)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    BNP_CHECK_NULL_PARAM(skipData);

    switch (skipData->skipMode)
    {
        case CPA_DC_SKIP_DISABLED:
            *totalSglSize = originalSize;
            break;
        case CPA_DC_SKIP_AT_START:
        case CPA_DC_SKIP_AT_END:
            *totalSglSize = originalSize +
                skipData->skipLength;
            break;
        case CPA_DC_SKIP_STRIDE:
            if(skipData->strideLength > 0)
            {
                *totalSglSize = originalSize +
                    ((originalSize / skipData->strideLength) *
                     skipData->skipLength);
            }
            else
            {
                PRINT_ERR("Invalid Stride value. It cannot be 0");
                status = CPA_STATUS_FAIL;
            }
            break;
        default:
            PRINT_ERR("Invalid skipMode value.");
            status = CPA_STATUS_FAIL;
            break;
    }
    return status;
}

STATIC void fillSglWithSkipAndStride(CpaBufferList *pBufferList,
        const Cpa32U totalSglSize,
        Cpa8U *pSrcData,
        CpaDcSkipData *skipData)
{
    Cpa32U i = 0, n = 0;
    Cpa32U srcDataCount = 0;
    Cpa32U sizeToSkip = 0;
    Cpa32U strideSize = 0;
    Cpa32U currBuffer = 0;
    Cpa32U dataCountInCurrBuff = 0;
    file_state_t state = SKIP_STATE;
    if (NULL == skipData)
    {
        PRINT_ERR("skipData is NULL\n");
        return;
    }
    sizeToSkip = skipData->firstSkipOffset;
    if (NULL == pSrcData)
    {
        PRINT_ERR("pSrcData is NULL\n");
        return;
    }
    switch (skipData->skipMode)
    {
        case CPA_DC_SKIP_DISABLED:
            break;
        case CPA_DC_SKIP_AT_START:
            state = SKIP_STATE;
            sizeToSkip = skipData->skipLength;
            break;
        case CPA_DC_SKIP_AT_END:
            state = FILL_STATE;
            strideSize = totalSglSize - skipData->skipLength;
            break;
        case CPA_DC_SKIP_STRIDE:
            /* If offset is 0 start with a skip */
            if (0 == skipData->firstSkipOffset)
            {
                sizeToSkip = skipData->skipLength;
                state = SKIP_STATE;
            }
            else
            {
                state = FILL_STATE;
                strideSize = skipData->firstSkipOffset;
            }
            break;
    }
    /*
     * Implement state machine for writing SGL.
     */
    dataCountInCurrBuff = 0;
    currBuffer=0;
    n = 0;
    i = 0;
    do
    {
        switch(state)
        {
            case SKIP_STATE:
                if(i == sizeToSkip)
                {
                    if(0 == skipData->strideLength)
                    {
                        state = FILL_STATE_NO_STRIDE;
                    }
                    else
                    {
                        state = FILL_STATE;
                        strideSize = skipData->strideLength;
                    }
                    i = 0;
                    break;
                }
                if(dataCountInCurrBuff ==
                    pBufferList->pBuffers[currBuffer].dataLenInBytes)
                {
                    /* Move to next flat buffer inside SGL */
                    currBuffer++;
                    dataCountInCurrBuff = 0;
                }
                pBufferList->pBuffers[currBuffer].pData[dataCountInCurrBuff] =
                 SRC_METADATA;
                dataCountInCurrBuff++;
                n++;
                i++;
                break;
            case FILL_STATE:
                if(i == strideSize)
                {
                    state = SKIP_STATE;
                    i = 0;
                    sizeToSkip = skipData->skipLength;
                    break;
                }
                if(dataCountInCurrBuff ==
                    pBufferList->pBuffers[currBuffer].dataLenInBytes)
                {
                    /* Move to next flat buffer inside SGL */
                    currBuffer++;
                    dataCountInCurrBuff = 0;
                }
                i++;
                pBufferList->pBuffers[currBuffer].pData[dataCountInCurrBuff] =
                 pSrcData[srcDataCount++];
                dataCountInCurrBuff++;
                n++;
                break;
            case FILL_STATE_NO_STRIDE:
                pBufferList->pBuffers[currBuffer].pData[dataCountInCurrBuff] =
                 pSrcData[srcDataCount++];
                if(dataCountInCurrBuff ==
                    (pBufferList->pBuffers[currBuffer].dataLenInBytes-1))
                {
                    /* Move to next flat buffer inside SGL */
                    currBuffer++;
                    dataCountInCurrBuff = 0;
                }
                else
                {
                    dataCountInCurrBuff++;
                }
                n++;
                break;
        }
    }while (n != totalSglSize);
    /* Adjust size of last buffer */
    pBufferList->pBuffers[currBuffer].dataLenInBytes = dataCountInCurrBuff;
    return;
}

STATIC CpaStatus fillSglWithData(CpaBufferList *pBufferList,
        const Cpa32U totalSglSize,
        Cpa8U *pSrcData,
        CpaDcSkipData *skipData,
        Cpa32U inputFlatBufferSize)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U i = 0;
    Cpa32U sglSize = 0;
    Cpa32U nbOfFlatBuffers = pBufferList->numBuffers;
    Cpa32U finalBuffSize = totalSglSize;

    BNP_CHECK_NULL_PARAM(pBufferList);
    BNP_CHECK_NULL_PARAM(pSrcData);
    BNP_CHECK_NULL_PARAM(skipData);


    if(finalBuffSize > inputFlatBufferSize)
    {
        if((finalBuffSize % inputFlatBufferSize) > 0)
        {
            pBufferList->numBuffers--;
        }
        finalBuffSize = finalBuffSize - (finalBuffSize % inputFlatBufferSize);
    }
    switch (skipData->skipMode)
    {
        case CPA_DC_SKIP_DISABLED:
            /* If there is no skip mode enabled then data
             * can be inserted at the beginning of the SGL.
             * A straight memcpy is more efficient
             */
            sglSize = finalBuffSize;
            for (i=0; i<nbOfFlatBuffers; i++)
            {
                if (sglSize >= inputFlatBufferSize)
                {
                    pBufferList->pBuffers[i].dataLenInBytes
                        = inputFlatBufferSize;
                    sglSize -= inputFlatBufferSize;
                }
                else
                {
                    pBufferList->pBuffers[i].dataLenInBytes = sglSize;
                    sglSize = 0;
                }
                if (pBufferList->pBuffers[i].dataLenInBytes > 0)
                {
                    memcpy(pBufferList->pBuffers[i].pData,
                            pSrcData,
                            pBufferList->pBuffers[i].dataLenInBytes);
                    pSrcData += pBufferList->pBuffers[i].dataLenInBytes;
                }
            }
            break;
        case CPA_DC_SKIP_AT_START:
        case CPA_DC_SKIP_AT_END:
        case CPA_DC_SKIP_STRIDE:

            fillSglWithSkipAndStride(pBufferList,
                    finalBuffSize,
                    pSrcData,
                    skipData);
            break;
        default:
            status = CPA_STATUS_FAIL;
            break;
    }
    return status;
}

void freeBnpBuffersDst(CpaBufferList *pBuffListArray,
        Cpa32U numberOfFiles, 
        compression_test_params_t* setup)
{
    Cpa32U i = 0;
    CpaBufferList *pBuffList = pBuffListArray;


    if(NULL == pBuffListArray)
    {
        /* Return Silent */
        return;
    }

    if(0 != numberOfFiles)
    {
        CpaFlatBuffer* pFlatBuffer = pBuffList->pBuffers;

        if(NULL != pFlatBuffer->pData)
        {
            qaeMemFreeNUMA((void**)&pFlatBuffer->pData);
            if(NULL != pFlatBuffer->pData)
            {
                PRINT("Could not free bufferList[%d]->pData\n", i);
            }
        }

        if(NULL != pBuffList->pPrivateMetaData)
        {
            qaeMemFreeNUMA((void**)&pBuffList->pPrivateMetaData);
            if(NULL != pBuffList->pPrivateMetaData)
            {
                PRINT("Could not free bufferList->pPrivateMetaData\n");
            }
        }
        if(NULL != pBuffList->pBuffers)
        {
            qaeMemFreeNUMA((void**)&pBuffList->pBuffers);
            if(NULL != pBuffList->pBuffers)
            {
                PRINT("Could not free bufferList->pBuffers\n");
            }
        }
        if(NULL != pBuffList)
        {
            qaeMemFree((void**)&pBuffList);
            if(NULL != pBuffList)
            {
                PRINT("Could not free bufferList[%d]\n", i);
            }
        }
    }
    return;
}


void freeBnpBuffers(CpaBufferList **pBuffListArray,
        Cpa32U numberOfFiles, 
        compression_test_params_t* setup)
{
    Cpa32U  i = 0, j = 0;
    CpaBufferList **pBuffList = pBuffListArray;


    if(NULL == pBuffListArray)
    {
        /* Return Silent */
        return;
    }

    if(0 != numberOfFiles)
    {
        for(i=0; i < numberOfFiles; i++)
        {
            Cpa32U numBuffers = pBuffList[i]->numBuffers;

            for (j=0; j < numBuffers; j++) {
                CpaFlatBuffer* pFlatBuffer = &pBuffList[i]->pBuffers[j];

                if(NULL != pFlatBuffer->pData)
                {
                    qaeMemFreeNUMA((void**)&pFlatBuffer->pData);

                    if(NULL != pFlatBuffer->pData)
                    {
                        PRINT("Could not free bufferList[%d]->pData\n", i);
                    }
                    PRINT("Free success for pFlatBuffer->pData\n");
                }
            }

            if(NULL != pBuffList[i]->pPrivateMetaData)
            {
                qaeMemFreeNUMA((void**)&pBuffList[i]->pPrivateMetaData);

                if(NULL != pBuffList[i]->pPrivateMetaData)
                {
                    PRINT("Could not free bufferList[%d]->pPrivateMetaData\n", i);
                }
                PRINT("Free success for pBuffList[i]->pPrivateMetaData\n");
            }
            if(NULL != pBuffList[i]->pBuffers)
            {
                qaeMemFreeNUMA((void**)&pBuffList[i]->pBuffers);
                if(NULL != pBuffList[i]->pBuffers)
                {
                    PRINT("Could not free bufferList[%d]->pBuffers\n", i);
                }
                PRINT("Free success for pBuffList[i]->pBuffers\n");
            }
            if(NULL != pBuffList[i])
            {
                qaeMemFree((void**)&pBuffList[i]);

                if(NULL != pBuffList[i])
                {
                    PRINT("Could not free bufferList[%d]\n", i);
                }
                PRINT("Free success for pBuffList[i]\n");
            }
        }
    }
    qaeMemFree((void**)&pBuffList);
    if(NULL != pBuffList)
    {
        PRINT("Could not free bufferList\n");
    }
    PRINT("Free success for pBuffList\n");
    return;
}

void dcPerformBnpCallback(void *pCallbackTag,
        CpaStatus status)
{
    perf_data_t *pPerfData = (perf_data_t *)pCallbackTag;


    /*check status */
    if(CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("Compression failed with status %d after response %llu\n",
                    status, (unsigned long long)pPerfData->responses);
        pPerfData->threadReturnStatus = CPA_STATUS_FAIL;
    }

    /*check perf_data pointer is valid*/
    if(NULL == pPerfData)
    {
        PRINT_ERR("Invalid data in CallbackTag\n");
        return;
    }
    pPerfData->responses++;
    if(pPerfData->responses >= pPerfData->numOperations)
    {
        /* generate end of the cycle stamp for Corpus */
        pPerfData->endCyclesTimestamp = sampleCodeTimestamp();
    }
    sampleCodeSemaphorePost(&pPerfData->comp);
}

CpaStatus setupDcBnPStatefulPerf(CpaDcCompLvl compLevel,
        CpaDcHuffType huffmanType,
        Cpa32U inputFlatBufferSize,
        Cpa32U outputFlatBufferSize,
        corpus_type_t corpusType,
        Cpa32U numLoops,
        Cpa32U numOfBatchJobs,
        CpaBoolean zeroLenthJob,
        CpaDcSkipMode inputSkipMode,
        CpaDcSkipMode outputSkipMode,
        Cpa32U inputSkipLength,
        Cpa32U outputSkipLength,
        CpaBoolean triggerOverflow,
        Cpa32U resetSessionOnJobn,
        CpaDcSessionState  state)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    stv_Bnp_t *bnp_params=NULL;

    /* Initialize the Performance device */
    if(testTypeCount_g >= MAX_THREAD_VARIATION)
    {
        PRINT_ERR("Maximum Support Thread Variation has been exceeded\n");
        PRINT_ERR("Number of Thread Variations created: %d",
                testTypeCount_g);
        PRINT_ERR(" Max is %d\n", MAX_THREAD_VARIATION);
        return CPA_STATUS_FAIL;
    }

    bnp_params = (stv_Bnp_t*)
        &thread_setup_g[testTypeCount_g][0];
    BNP_CHECK_NULL_PARAM(bnp_params);

    /* setup tests Params */
    bnp_params->numBuffersPerInputBufferList = 1;
    bnp_params->numBuffersPerOutputBufferList = 1;
    bnp_params->numJobsPerBatchRequest = numOfBatchJobs;
    bnp_params->resetSessionOnJobn = resetSessionOnJobn;
    bnp_params->inputDcSkipMode = inputSkipMode; 
    bnp_params->outputDcSkipMode = outputSkipMode; 
    bnp_params->inputSkipLength = inputSkipLength; 
    bnp_params->outputSkipLength = outputSkipLength; 

    if(ZERO_LENGTH_FILE == corpusType || OVERFLOW_AND_ZERO_FILE == corpusType)
    {
        zeroLenthJob = CPA_TRUE;
    }
    if(CPA_TRUE == zeroLenthJob &&
           (( CPA_DC_SKIP_DISABLED != inputSkipMode) ||
           (CPA_DC_SKIP_DISABLED != outputSkipMode)))
    {
        PRINT_ERR(" Skip mode is not supported for zero length file\n");
        return CPA_STATUS_FAIL;
    }
    if(CPA_TRUE == zeroLenthJob && CPA_TRUE == triggerOverflow) 
    {
        PRINT("OVERFLOW_AND_ZERO_FILE\n");
        bnp_params->compressTestParams.corpus = OVERFLOW_AND_ZERO_FILE;
    }
    else if(CPA_TRUE == zeroLenthJob && CPA_FALSE == triggerOverflow)
    {
        PRINT("ZERO_FILE\n");
        bnp_params->compressTestParams.corpus = ZERO_LENGTH_FILE;
    }
    else if(CPA_TRUE == triggerOverflow && CPA_FALSE == zeroLenthJob)
    {
        PRINT("OVERFLOW_FILE\n");
        bnp_params->compressTestParams.corpus = OVERFLOW_FILE;
    }
    else
    {
        PRINT("CORPUS_FILE\n");
        bnp_params->compressTestParams.corpus = corpusType;
    }
    bnp_params->compressTestParams.syncFlag = CPA_SAMPLE_SYNCHRONOUS;
    bnp_params->compressTestParams.numLoops = numLoops;
    bnp_params->compressTestParams.setupData.compLevel = compLevel;
    bnp_params->compressTestParams.setupData.compType = CPA_DC_DEFLATE;
    bnp_params->compressTestParams.setupData.huffType = huffmanType;
    bnp_params->compressTestParams.setupData.autoSelectBestHuffmanTree =
     CPA_DC_ASB_DISABLED;
#if(CPA_DC_API_VERSION_NUM_MAJOR == 1 && CPA_DC_API_VERSION_NUM_MINOR < 6)
    bnp_params->compressTestParams.setupData.fileType = CPA_DC_FT_ASCII;
    bnp_params->compressTestParams.setupData.deflateWindowSize =
     DEFAULT_COMPRESSION_WINDOW_SIZE;
#endif
    bnp_params->compressTestParams.setupData.sessDirection = CPA_DC_DIR_COMPRESS;
    bnp_params->compressTestParams.setupData.sessState = state;
    bnp_params->compressTestParams.setupData.checksum = CPA_DC_CRC32;
    bnp_params->compressTestParams.bufferSize = inputFlatBufferSize;
    bnp_params->inputFlatBufferSize = inputFlatBufferSize;
    bnp_params->outputFlatBufferSize = outputFlatBufferSize;

    status = populateBnpCorpus(bnp_params->numJobsPerBatchRequest,
              bnp_params->compressTestParams.corpus);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to load one or more corpus files, have they been "
                "extracted to /lib/firmware?\n");
        freeCorpus();
        return CPA_STATUS_FAIL;
    }

    /*Start DC Services */
    status = startDcServices(bnp_params->inputFlatBufferSize, TEMP_NUM_BUFFS);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error in Starting Dc Services\n");
        freeCorpus();
        return CPA_STATUS_FAIL;
    }
    if(!poll_inline_g)
    {
        /* start polling threads if polling is enabled in the configuration
         * file */
        if(CPA_STATUS_SUCCESS != dcCreatePollingThreadsIfPollingIsEnabled())
        {
            PRINT_ERR("Error creating polling threads\n");
            freeCorpus();
            return CPA_STATUS_FAIL;
        }
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


    /* Set the performance function to the actual performance function
     * that actually does all the performance
     */
    testSetupData_g[testTypeCount_g].performance_function =
        (performance_func_t)dcBnpPerformance;

    /* update the setup_g with buffersize */
    testSetupData_g[testTypeCount_g].packetSize=bnp_params->inputFlatBufferSize;

    return status;
}


CpaStatus setupDcBnPTest(stv_Bnp_t *bnp_params)
{
    CpaStatus status = CPA_STATUS_SUCCESS;



    /* Populate Corpus */
    status = populateBnpCorpus(bnp_params->numJobsPerBatchRequest,
              bnp_params->compressTestParams.corpus);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to load one or more corpus files, have they been "
                "extracted to /lib/firmware?\n");
        freeCorpus();
        return CPA_STATUS_FAIL;
    }

    /*Start DC Services */
    status = startDcServices(bnp_params->inputFlatBufferSize, TEMP_NUM_BUFFS);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error in Starting Dc Services\n");
        freeCorpus();
        return CPA_STATUS_FAIL;
    }
    if(!poll_inline_g)
    {
        /* start polling threads if polling is enabled in the configuration
         * file */
        if(CPA_STATUS_SUCCESS != dcCreatePollingThreadsIfPollingIsEnabled())
        {
            PRINT_ERR("Error creating polling threads\n");
            freeCorpus();
            return CPA_STATUS_FAIL;
        }
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


    /* Set the performance function to the actual performance function
     * that actually does all the performance
     */
    testSetupData_g[testTypeCount_g].performance_function =
        (performance_func_t)dcBnpPerformance;

    /* update the setup_g with buffersize */
    testSetupData_g[testTypeCount_g].packetSize=bnp_params->inputFlatBufferSize;

    return status;
}

void dcBnpPerformance(single_thread_test_data_t* testBnpSetup) 
{
    stv_Bnp_t dcBnpSetup = {0}, *tmpBnpSetup = NULL;
    Cpa16U numInstances = 0;
    CpaInstanceHandle *instances = NULL;
    CpaStatus status = CPA_STATUS_FAIL;
    CpaDcInstanceCapabilities capabilities = {0};
    if(NULL == testBnpSetup)
    {
        PRINT_ERR("testBnpSetup is NULL %s\n",__FUNCTION__);
        sampleCodeThreadExit();	
    }	
    /* Get the setup pointer */
    tmpBnpSetup = (stv_Bnp_t *)(testBnpSetup->setupPtr);
    if(NULL == tmpBnpSetup)
    {
        PRINT_ERR("tmpBnpSetup is NULL %s\n",__FUNCTION__);
        sampleCodeThreadExit();	
    }	

    /* update the setup structure with setup parameters */
    dcBnpSetup.compressTestParams.bufferSize = tmpBnpSetup->inputFlatBufferSize;
    dcBnpSetup.compressTestParams.corpus =
     tmpBnpSetup->compressTestParams.corpus;
    dcBnpSetup.compressTestParams.setupData =
     tmpBnpSetup->compressTestParams.setupData;
    dcBnpSetup.compressTestParams.dcSessDir =
     tmpBnpSetup->compressTestParams.dcSessDir;    
    dcBnpSetup.compressTestParams.syncFlag =
     tmpBnpSetup->compressTestParams.syncFlag;
    dcBnpSetup.compressTestParams.numLoops =
     tmpBnpSetup->compressTestParams.numLoops;
    dcBnpSetup.inputDcSkipMode = tmpBnpSetup->inputDcSkipMode; 
    dcBnpSetup.inputSkipLength = tmpBnpSetup->inputSkipLength;
    dcBnpSetup.outputDcSkipMode = tmpBnpSetup->outputDcSkipMode;
    dcBnpSetup.outputSkipLength = tmpBnpSetup->outputSkipLength;
    dcBnpSetup.inputFlatBufferSize = tmpBnpSetup->inputFlatBufferSize;
    dcBnpSetup.outputFlatBufferSize = tmpBnpSetup->outputFlatBufferSize;
    dcBnpSetup.numJobsPerBatchRequest = tmpBnpSetup->numJobsPerBatchRequest;

    /*give our thread a unique memory location to store performance stats*/
    dcBnpSetup.compressTestParams.performanceStats =
     testBnpSetup->performanceStats;    
    testBnpSetup->performanceStats->threadReturnStatus = CPA_STATUS_SUCCESS;
    dcBnpSetup.compressTestParams.isBnpSession = CPA_TRUE;

    status = calculateRequireBuffers(&dcBnpSetup.compressTestParams); 
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error calculating required buffers\n");
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
    testBnpSetup->statsPrintFunc=NULL;

    /* Get the number of instances */
    status = cpaDcGetNumInstances(&numInstances);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR(" Unable to get number of DC instances\n");
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }
    if(0 == numInstances)
    {
        PRINT_ERR(" DC Instances are not present\n");
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }
    instances = qaeMemAlloc(sizeof(CpaInstanceHandle)*numInstances);
    if(NULL == instances)
    {
        PRINT_ERR("Unable to allocate Memory for Instances\n");
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }

    /*get the instance handles so that we can start
     * our thread on the selected instance
     */
    status = cpaDcGetInstances(numInstances, instances);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("get instances failed");
        qaeMemFree((void**)&instances);
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }

    /* give our thread a logical quick assist instance to use
     * use % to wrap around the max number of instances*/
    dcBnpSetup.compressTestParams.dcInstanceHandle=
        instances[(testBnpSetup->logicalQaInstance)%numInstances];

    /*check if dynamic compression is supported*/
    status = cpaDcQueryCapabilities(dcBnpSetup.compressTestParams.dcInstanceHandle,
            &capabilities );
    if(CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("%s::%d cpaDcQueryCapabilities failed", __func__,__LINE__);
        qaeMemFree((void**)&instances);
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }
    if(CPA_FALSE == capabilities.dynamicHuffman &&
            (tmpBnpSetup->compressTestParams.setupData.huffType ==
              CPA_DC_HT_FULL_DYNAMIC))
    {
        PRINT("Dynamic is not supported on logical instance %d\n",
                (testBnpSetup->logicalQaInstance)%numInstances);
        dcBnpSetup.compressTestParams.performanceStats->threadReturnStatus =
         CPA_STATUS_FAIL;
        qaeMemFree((void**)&instances);
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }
    if(CPA_FALSE == capabilities.batchAndPack)
    {
        PRINT("Batchand pack is not supported on logical instance\n");
        dcBnpSetup.compressTestParams.performanceStats->threadReturnStatus =
         CPA_STATUS_FAIL;
        qaeMemFree((void**)&instances);
        qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);
        sampleCodeThreadExit();
    }


    /*launch function that does all the work*/
    status = dcBnpPerform(&dcBnpSetup);

    if(CPA_STATUS_SUCCESS != status)
    {
        dcPrintTestData(&dcBnpSetup.compressTestParams);
        PRINT_ERR("Compression Thread %u FAILED\n", testBnpSetup->threadID);
        dcBnpSetup.compressTestParams.performanceStats->threadReturnStatus =
         CPA_STATUS_FAIL;
    }
    else
    {
        /*set the print function that can be used to print
         * statistics at the end of the test
         * */


        testBnpSetup->statsPrintFunc=(stats_print_func_t)dcPrintBnpStats;
    }
    qaeMemFree((void**)&instances);
    qaeMemFree((void**)&dcBnpSetup.compressTestParams.numberOfBuffers);


    sampleCodeThreadExit();

}

CpaStatus createBNPBuffers(Cpa32U buffSize, 
        Cpa32U numBuffs,
        CpaBufferList **pBuffListArray, 
        Cpa32U nodeId, 
        Cpa32U metaSize)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U i = 0;
    CpaBufferList *pBuffList = NULL;
    CpaFlatBuffer *pFlatBuff = NULL;
    Cpa8U *pMsg = NULL;
    Cpa32U sizeOfPkt = 0;

    pBuffList = qaeMemAlloc(sizeof(CpaBufferList));
    if(NULL== pBuffList)
    {
        PRINT_ERR("Unable to allocate pBuffList\n");
        return CPA_STATUS_FAIL;
    }
    pBuffList->numBuffers = numBuffs;


    if( numBuffs != 0 ) 
    {
        sizeOfPkt = buffSize;
    }
    else
    {
        numBuffs=1;
        pBuffList->numBuffers = numBuffs;
        sizeOfPkt = 0;
    }
    if (metaSize) {
        pBuffList->pPrivateMetaData =
            qaeMemAllocNUMA(metaSize, nodeId, BYTE_ALIGNMENT_64);
        if( NULL == pBuffList->pPrivateMetaData)
        {
            PRINT_ERR(" Unable to allocate pPrivateMetaData\n");
            qaeMemFree((void**)&pBuffList);
            return CPA_STATUS_FAIL;
        }
    } else {
        pBuffList->pPrivateMetaData = NULL;

    }
    pFlatBuff = qaeMemAllocNUMA(numBuffs * sizeof(CpaFlatBuffer),
               nodeId, BYTE_ALIGNMENT_64);
    if(NULL == pFlatBuff)
    {
        PRINT_ERR("Unable to allocate pFlatBuff\n");
        qaeMemFreeNUMA((void**)&pBuffList->pPrivateMetaData);
        qaeMemFree((void**)&pBuffList);
        return CPA_STATUS_FAIL;
    }
    pBuffList->pBuffers = pFlatBuff;

    if(sizeOfPkt==0)
    {
        pMsg = qaeMemAllocNUMA(1,nodeId, BYTE_ALIGNMENT_64);
        if(NULL == pMsg)
        {
            PRINT_ERR("Unable to allocate pMsg\n");
            qaeMemFreeNUMA((void**)&pBuffList->pPrivateMetaData);
            qaeMemFree((void**)&pBuffList);
            qaeMemFreeNUMA((void**)&pFlatBuff);
            return CPA_STATUS_FAIL;
        }
        memset(pMsg, 0, sizeOfPkt);
        pFlatBuff->pData = pMsg;
        pFlatBuff->dataLenInBytes = sizeOfPkt;
    }
    else
    {
        for(i = 0; i< numBuffs; i++)
        {
            pMsg = qaeMemAllocNUMA(sizeOfPkt,nodeId, BYTE_ALIGNMENT_64);
            if(NULL == pMsg)
            {
                PRINT_ERR("Unable to allocate pMsg\n");
                qaeMemFreeNUMA((void**)&pBuffList->pPrivateMetaData);
                qaeMemFree((void**)&pBuffList);
                qaeMemFreeNUMA((void**)&pFlatBuff);
                return CPA_STATUS_FAIL;
            }
            memset(pMsg, 0, sizeOfPkt);
            pFlatBuff->pData = pMsg;

            pFlatBuff->dataLenInBytes = sizeOfPkt;
            pFlatBuff++;
        }
    }

    *pBuffListArray = pBuffList;

    return status;
}

CpaStatus dcBnpPerform(stv_Bnp_t* bnpSetup)
{
    /* start of local variable declarations */
    CpaStatus status = CPA_STATUS_FAIL;
    Cpa32U nodeId = 0;
    /* Initialize to 0 and set later to size as declared in setup */
    CpaDcRqResults **cmpResult = NULL;
    /* Performance data Structure */
    perf_data_t *perfData = NULL;
    /* Src Buffer list for data to be compressed */
    CpaBufferList **srcBuffListArray = NULL;
    /* BufferList for de-compressed Data */
    CpaBufferList **dstBuffListArray = NULL ;

    if(NULL == bnpSetup)
    {
        PRINT_ERR(" bnpSetup Pointer is NULL\n");
        return CPA_STATUS_FAIL;
    }

    /* get the performance structure */
    perfData = bnpSetup->compressTestParams.performanceStats;

    /* Get the Node Affinity to allocate memory */
    status = sampleCodeDcGetNode(bnpSetup->compressTestParams.dcInstanceHandle,
            &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }


    status = performBnpCompress(bnpSetup, 
            srcBuffListArray, 
            dstBuffListArray,
            cmpResult, 
            dcPerformBnpCallback);

    /*clean up the callback semaphore*/
    sampleCodeSemaphoreDestroy(&perfData->comp);
    return status;
}


STATIC Cpa32U calculateSglSize (CpaBufferList *pBufferList)
{
    Cpa32U size = 0;
    Cpa32U i = 0;

    for(i = 0; i < pBufferList->numBuffers; i++)
    {
        size += pBufferList->pBuffers[i].dataLenInBytes;
    }
    return size;
}

STATIC void copySgl(CpaBufferList *pResubmitList,
        CpaBufferList *pSrcList,
        Cpa32U sglDataLen, 
        sgl_buffer_index_t *pIndex)
{
    Cpa32U i = 0, j = 0;
    Cpa32U byteOffset = 0;
    Cpa32U bufferOffset = 0;
    if(0 == sglDataLen)
        return;

    if(NULL == pSrcList)
        return;

    if(NULL != pIndex)
    {
        bufferOffset = pIndex->bufferIndex;
        byteOffset   = pIndex->offsetInBuffer;
    }
    for(i = bufferOffset, j = 0; i < pSrcList->numBuffers; i++, j++)
    {
        memcpy (pResubmitList->pBuffers[j].pData,
                pSrcList->pBuffers[i].pData,
                (pSrcList->pBuffers[i].dataLenInBytes - byteOffset));
        pResubmitList->pBuffers[j].dataLenInBytes =
            pSrcList->pBuffers[i].dataLenInBytes - byteOffset;
        byteOffset = 0;
    }

}

void dcFreeBufferListBnp(CpaBufferList **testBufferList)
{
    CpaBufferList *pBuffList = *testBufferList;
    CpaFlatBuffer *pFlatBuff = NULL;
    Cpa32U currentChain = 0;

    if(NULL == pBuffList)
    {
        PRINT_ERR("Nothing to free *testBufferList is NULL");
        return;
    }

    pFlatBuff = pBuffList->pBuffers;
    while (currentChain < pBuffList->numBuffers)
    {
        if(NULL != pFlatBuff->pData)
        {
            qaeMemFreeNUMA((void**)&pFlatBuff->pData);
            pFlatBuff->pData = NULL;
        }
        pFlatBuff++;
        currentChain++;
    }

    if(NULL != pBuffList->pPrivateMetaData)
    {
        qaeMemFreeNUMA((void**)&pBuffList->pPrivateMetaData);
        pBuffList->pPrivateMetaData = NULL;
    }

    if(NULL != pBuffList->pBuffers)
    {
        qaeMemFreeNUMA((void**)&pBuffList->pBuffers);
        pBuffList->pBuffers = NULL;
    }

    qaeMemFree((void **)&pBuffList);
    pBuffList = NULL;
    *testBufferList = pBuffList;
}
STATIC CpaStatus createNewBatchJob(CpaDcBatchOpData **pOriginalBatchOpData, 
        const Cpa32U numStreams,
        CpaDcRqResults *pResults, 
        const Cpa32U overflownJob,
        CpaDcBatchOpData **pBnpResubmitBatch, 
        Cpa32U nodeId,
        stv_Bnp_t* bnpSetup,
        CpaDcRqResults *pcpResults,
        CpaBoolean resubmission)
{
    CpaStatus status = CPA_STATUS_FAIL;
    Cpa32U i = 0, j = 0;
    Cpa32U numJobsUnprocessed = 0;
    Cpa32U sglDataLen = 0;
    Cpa32U metaSizeInBytes = 0;
    CpaBufferList *pSrcList = NULL;
    CpaBufferList *pResubmitList = NULL;
    sgl_buffer_index_t index = {0};
    Cpa32U unConsumedBytes = 0;
    Cpa32U numUnconsumedBuffers = 0;
    Cpa32U totalDataLen = 0;
    Cpa32U previousTotalDataLen = 0;
    CpaDcBatchOpData *pBnpOrign = *pOriginalBatchOpData;
    CpaDcBatchOpData *pBnpResubmit = NULL;
    CpaBufferList *pSrcBuff = NULL;


    BNP_CHECK_NULL_PARAM(bnpSetup);
    BNP_CHECK_NULL_PARAM(pBnpOrign);
    /* Find out where the overflow occurred in the batch job list */
    pSrcList = pBnpOrign[overflownJob].pSrcBuff;
    BNP_CHECK_NULL_PARAM(pSrcList);
    for (i = 0; i < pSrcList->numBuffers; i++)
    {
        previousTotalDataLen = totalDataLen;
        totalDataLen += pSrcList->pBuffers[i].dataLenInBytes;
        if(totalDataLen >= pResults[overflownJob].consumed)
        {
            index.bufferIndex = i;
            index.offsetInBuffer = pResults[overflownJob].consumed -
                previousTotalDataLen;
            status = CPA_STATUS_SUCCESS;
            break;
        }
    }
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Failed to find overflow index!\n");
        return (status);
    }

    /* Find un-consumed data size in source SGL */
    sglDataLen = calculateSglSize(pSrcList);
    unConsumedBytes = sglDataLen - pResults[overflownJob].consumed;
    numJobsUnprocessed = numStreams - overflownJob;

    /*
     * Allocate Batch Op data structure
     */
    pBnpResubmit = (CpaDcBatchOpData*)
        qaeMemAlloc(sizeof(CpaDcBatchOpData) * numJobsUnprocessed);
    if (NULL == pBnpResubmit)
    {
        PRINT_ERR("Failed to allocate pBnpResubmit data structure.\n");
        return CPA_STATUS_FAIL;
    }

    for (i=overflownJob, j=0; i<numStreams; i++, j++)
    {
        pSrcList = pBnpOrign[i].pSrcBuff;

        /* Copy skip mode information without
         * setting the reset on the first job
         */
        if (i == overflownJob)
        {
            pBnpResubmit[j].resetSessionState = /*CPA_TRUE */CPA_FALSE;
            sglDataLen = unConsumedBytes;
        }
        else
        {
            pBnpResubmit[j].resetSessionState =
                pBnpOrign[i].resetSessionState;
            sglDataLen = calculateSglSize(pSrcList);
        }
        if (sglDataLen >= pBnpOrign[i].opData.inputSkipData.skipLength)
        {
            memcpy(&pBnpResubmit[j].opData.inputSkipData,
                    &pBnpOrign[i].opData.inputSkipData,
                    sizeof(CpaDcSkipData));
        }
        else
        {
            pBnpResubmit[j].opData.inputSkipData.skipMode =
                pBnpOrign[i].opData.inputSkipData.skipMode;

            pBnpResubmit[j].opData.inputSkipData.skipLength =
                sglDataLen;

            pBnpResubmit[j].opData.inputSkipData.strideLength =
                pBnpOrign[i].opData.inputSkipData.strideLength;

            pBnpResubmit[j].opData.inputSkipData.firstSkipOffset =
                pBnpOrign[i].opData.inputSkipData.firstSkipOffset;
        }

        memcpy(&pBnpResubmit[j].opData.outputSkipData,
                &pBnpOrign[i].opData.outputSkipData,
                sizeof(CpaDcSkipData));

        /* Copy flush flag */
        pBnpResubmit[j].opData.flushFlag = pBnpOrign[i].opData.flushFlag;

        /*
         * Build buffer list for each unprocessed SGL
         */
        /* Determine how many buffers have not been consumed */
        if (i == overflownJob)
        {
            numUnconsumedBuffers = pSrcList->numBuffers -index.bufferIndex;
        }
        else
        {
            numUnconsumedBuffers = pSrcList->numBuffers;
        }

        status = cpaDcBnpBufferListGetMetaSize(
                  bnpSetup->compressTestParams.dcInstanceHandle,
                  numUnconsumedBuffers,
                  &metaSizeInBytes);
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("ERR in cpaDcBnpBufferListGetMetaSize %x",status);
            qaeMemFree((void**)&pBnpResubmit);
            return status;
        }
        if (pSrcList->numBuffers ==1 )
        {
            if( pSrcList->pBuffers->dataLenInBytes == 0 )
            {/*while sending zero length for resubmission numof buffers==0*/
                status = createBNPBuffers(0, 0, &pResubmitList,
                        nodeId, metaSizeInBytes);
                if((CPA_STATUS_SUCCESS != status) || (NULL == pResubmitList))
                {
                    PRINT_ERR("Unable to Create Buffers for pResubmitList\n");
                    qaeMemFree((void**)&pBnpResubmit);
                    return CPA_STATUS_FAIL;
                }
            }
        }
        else{
            /* Allocate resubmit SGL buffer */
                status = createBNPBuffers(bnpSetup->inputFlatBufferSize,
                        numUnconsumedBuffers,
                        &pResubmitList, nodeId, metaSizeInBytes);
                if((CPA_STATUS_SUCCESS != status) || (NULL == pResubmitList))
                {
                    PRINT_ERR("Unable to Create Buffers for pResubmitList\n");
                    qaeMemFree((void**)&pBnpResubmit);
                    return CPA_STATUS_FAIL;
                }
        }
        /* Copy original SGL input data re-submission SGLs */
        if (i == overflownJob)
        {
            copySgl(pResubmitList, pSrcList, sglDataLen, &index);
        }
        else
        {
            copySgl(pResubmitList, pSrcList, sglDataLen, NULL);
        }
        pBnpResubmit[j].pSrcBuff = pResubmitList;
        /* Shift the checksums */
        pResults[j].checksum = pResults[i].checksum;
    }

    /* Destroy original job */
    if(resubmission)
    {
        if (NULL != pBnpOrign)
        {
            for (i=0; i<numStreams; i++)
            {
                pSrcBuff = pBnpOrign[i].pSrcBuff;
                dcFreeBufferListBnp(&pSrcBuff);
                pSrcBuff = NULL;
            }
            qaeMemFree ((void **)&pBnpOrign);
        }
    }
    *pOriginalBatchOpData = pBnpOrign; /* Should be NULL */
    /* Should have the new address of the batch job */
    *pBnpResubmitBatch = pBnpResubmit;


    return status;
}

STATIC void initSeedSwChecksums (Cpa32U *seedSwChecksums,
        CpaDcRqResults *pDcResults,
        const Cpa32U numJobsInBatch,
        const CpaDcSessionState sessionState)
{
    Cpa32U i = 0;

    for(i=0; i<numJobsInBatch; i++)
    {
        *(seedSwChecksums+i) = pDcResults[i].checksum;
    }
    return;
}

STATIC void getBatchConsumedTotalSize(CpaDcRqResults *pResults,
        const Cpa32U numStreams,
        Cpa32U *totalBatchConsumed, 
        Cpa32U *totalBatchProduced)
{
    Cpa32U stream = 0;
    Cpa32U consumed = 0;
    Cpa32U produced = 0;

    for(stream=0; stream<numStreams; stream++)
    {
        consumed += pResults[stream].consumed;
        produced += pResults[stream].produced;
    }
    *totalBatchConsumed += consumed;
    *totalBatchProduced += produced;
    return;
}


CpaStatus performBnpCompress(stv_Bnp_t* bnpSetup,
        CpaBufferList **srcBuffListArray,
        CpaBufferList **dstBuffListArray,
        CpaDcRqResults **cmpResult,
        CpaDcCallbackFn dcCbFn)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U sessionSize = 0, contextSize = 0;
    /* DC session Handle */
    CpaDcSessionHandle *pSessionHandle = NULL;
    Cpa32U numLoops = 0, i = 0, totalBuffers = 0;

    Cpa32U compressLoops = bnpSetup->compressTestParams.numLoops;
    perf_data_t *perfData = NULL;
    Cpa32U nodeId = 0;
    /* pContextBuffer is NULL for stateless requests */
    CpaBufferList *pContextBuffer = NULL;
    /* flushFlag set to CPA_DC_FLUSH_FINAL for stateless requests */
    /* Create Batch OP data structure */
    CpaBufferList* dstBuffer = NULL;
    CpaDcRqResults* cmpResults = NULL;
    CpaDcRqResults* pcmpResults = NULL;
    CpaDcBatchOpData *pBatchOpData = NULL;
    CpaDcBatchOpData *pBnPOpData = NULL;
    CpaDcBatchOpData *pResubmitBnpOpData = NULL;
    Cpa32U *seedSwChecksums = NULL;
    Cpa32U inputSglSize = 0;
    Cpa32U numJobs = 0;
    Cpa32U numJobsInBatch = 0, fillfrom = 0 ;
    Cpa32S overflownJob = -1;
    Cpa32U bufferSize = 0;
    CpaDcSkipData *inputSkipData = NULL;
    CpaDcSkipData setupinputSkipData ={0} ;
    Cpa32U totalSglSize = 0;
    CpaDcSkipData *outputSkipData = NULL;
    Cpa32U totalBatchSize = 0;
    Cpa32U *actualNumFlatBuffers = NULL;
    /* The following variables are allocated on the stack because we block
     * until the callback comes back. If a non-blocking approach was to be
     * used then these variables should be dynamically allocated */
    Cpa32U  count = 0, off_set=0;
    Cpa32U resubmission = 0;
    Cpa32S numUnprocessedJobs = 0;
    Cpa32U batchConsumed = 0;
    Cpa32U batchProduced = 0;
    Cpa32U totalBuffs = 0;
    Cpa32U noOfFlatBuffers = 0;
    Cpa32U metaSize = 0;
    CpaInstanceInfo2 instanceInfo2 = {0};
    CpaDcSessionState sessionState;
    Cpa32U noOfFlatBuffersT = 1;
    Cpa32U metaSizeT = 0;
    BNP_CHECK_NULL_PARAM(bnpSetup);
    /* Calculate the number of individual buffers to be submitted */
    for(i = 0; i < bnp_corpus_g.numFilesInCorpus; i++ )
    {
        totalBuffers += bnpSetup->compressTestParams.numberOfBuffers[i];
    }

    perfData = bnpSetup->compressTestParams.performanceStats;
    BNP_CHECK_NULL_PARAM(perfData);

    /* Zero performance stats */
    memset(perfData, 0, sizeof(perf_data_t));

    perfData->numOperations = (Cpa64U)compressLoops;
    status = cpaDcInstanceGetInfo2(bnpSetup->compressTestParams.dcInstanceHandle,
            &instanceInfo2);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("cpaCyInstanceGetInfo2 error, status: %d\n", status);
        return CPA_STATUS_FAIL;
    }

    /* Get the Node Affinity to allocate memory */
    status = sampleCodeDcGetNode(bnpSetup->compressTestParams.dcInstanceHandle,
            &nodeId);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Node ID\n");
        return status;
    }

    /* Get Size for DC Session */
    status = cpaDcGetSessionSize(bnpSetup->compressTestParams.dcInstanceHandle,
            &(bnpSetup->compressTestParams.setupData),
            &sessionSize, &contextSize);    
    if ( CPA_STATUS_SUCCESS != status )
    {
        PRINT_ERR("cpaDcGetSessionSize() returned %d status.\n",status);
        return CPA_STATUS_FAIL;
    }

    status = cpaDcBufferListGetMetaSize(bnpSetup->compressTestParams.dcInstanceHandle,
            ONE_BUFFER_DC, &metaSize);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to get Meta size: status = %d \n", status);
        return CPA_STATUS_FAIL;
    }

    status = dcSampleCreateStatefulContextBuffer((contextSize*EXTRA_BUFFER),
            metaSize,&pContextBuffer, nodeId);
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to allocate context : status = %d \n", status);
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

    if(bnpSetup->compressTestParams.syncFlag == CPA_SAMPLE_SYNCHRONOUS)
    {
        dcCbFn = NULL;
    }

    /* Initialize DC API Session */
    status = cpaDcInitSession( bnpSetup->compressTestParams.dcInstanceHandle,
            pSessionHandle,
            &(bnpSetup->compressTestParams.setupData), pContextBuffer, dcCbFn);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Problem in session creation: status = %d \n", status);
        qaeMemFreeNUMA((void**)&pSessionHandle);
        return CPA_STATUS_FAIL;
    }
    perfData->numLoops = bnpSetup->compressTestParams.numLoops;

    /* Completion used in callback */
    sampleCodeSemaphoreInit(&perfData->comp, 0);

    /* this Barrier will waits until all the threads get to this point */
    sampleCodeBarrier();

    numJobs = bnpSetup->numJobsPerBatchRequest;
    actualNumFlatBuffers = qaeMemAlloc(sizeof(Cpa32U) * numJobs);
    if(NULL == actualNumFlatBuffers)
    {
        PRINT_ERR("Unable to allocate Memory for actualNumFlatBuffers\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        return CPA_STATUS_FAIL;
    }
    pBatchOpData = qaeMemAlloc(sizeof(CpaDcBatchOpData) * numJobs);
    if(NULL == pBatchOpData)
    {
        PRINT_ERR("Unable to allocate Memory for pBatchOpData\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        return CPA_STATUS_FAIL;
    }
    /* Allocate buff list list pointers
     * this list array is used as source for compression
     */
    srcBuffListArray = qaeMemAlloc( numJobs * sizeof(CpaBufferList*) );
    /* Check for NULL */
    if(NULL == srcBuffListArray)
    {
        PRINT_ERR("unable to allocate srcBuffListArray\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        qaeMemFree((void**)&pBatchOpData);
        return CPA_STATUS_FAIL;
    }

    for(i=0; i < numJobs; i++)
    {
        /* Input skip mode configuration */
        setupinputSkipData.skipMode = bnpSetup->inputDcSkipMode;
        setupinputSkipData.skipLength = bnpSetup->inputSkipLength;
        /*Adding skip and stride length*/
        /*calculating the total sgl size including skip and stride */
        status = getTotalSglSize( bnp_corpus_g.fileArray[i].corpusBinaryDataLen,
                &setupinputSkipData,
                &totalSglSize);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("getTotal SGL space failed\n");
            qaeMemFreeNUMA((void**)&pSessionHandle);
            qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
            freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
           &(bnpSetup->compressTestParams));
            qaeMemFree((void**)&pBatchOpData);
            return CPA_STATUS_FAIL;
        }
        inputSglSize = totalSglSize;
        totalBatchSize += inputSglSize;
        /*reset no of flat buffs*/
        noOfFlatBuffers = 0;
        bufferSize = bnpSetup->inputFlatBufferSize;
        if(0 != bufferSize)
        {    
            do {
                if (inputSglSize >= bnpSetup->inputFlatBufferSize) {
                    inputSglSize -= bnpSetup->inputFlatBufferSize; 
                } else {
                    inputSglSize = 0;
                }
                noOfFlatBuffers++;

            } while (inputSglSize > 0);
        }   
        else
        {
            noOfFlatBuffers=1; 

        }
        /*reset metasize*/
        metaSize = 0;

        /* Get the Meta size for each file in buffers list */
        status = cpaDcBnpBufferListGetMetaSize(
                  bnpSetup->compressTestParams.dcInstanceHandle,
                  noOfFlatBuffers,
                  &metaSize);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to get Meta Size\n");
            qaeMemFreeNUMA((void**)&pSessionHandle);
            qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
            freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
           &(bnpSetup->compressTestParams));
            qaeMemFree((void**)&pBatchOpData);
            return CPA_STATUS_FAIL;
        }
        /*reset noOfFlatBuffers for zero length case after metasize calculation*/
        if(bufferSize == 0) 
        {
            noOfFlatBuffers=0;
        }
        status = createBNPBuffers(bufferSize,
                noOfFlatBuffers,
                &srcBuffListArray[i], nodeId, metaSize);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Unable to Create Buffers for source List array\n");
            qaeMemFreeNUMA((void**)&pSessionHandle);
            qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
            freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
           &(bnpSetup->compressTestParams));
            qaeMemFree((void**)&pBatchOpData);
            return CPA_STATUS_FAIL;
        }
        /*add Actual created buffers to actualNumFlatBuffers which used to free*/
        actualNumFlatBuffers[i] = srcBuffListArray[i]->numBuffers;

        /*
         * Create the SGL depending on the size of the file
         * and the size of the skip data
         */
        inputSkipData = &pBatchOpData[i].opData.inputSkipData;
        inputSkipData->skipMode =  bnpSetup->inputDcSkipMode;

        if(CPA_DC_SKIP_DISABLED != inputSkipData->skipMode)
        { 
            inputSkipData->skipLength = bnpSetup->inputSkipLength;
            inputSkipData->firstSkipOffset = 0;
            inputSkipData->strideLength = 0;
        }
        else
        {
            inputSkipData->skipLength = 0;
            inputSkipData->firstSkipOffset = 0;
            inputSkipData->strideLength = 0;
        }

        /* Output skip mode configuration */
        outputSkipData = &pBatchOpData[i].opData.outputSkipData;
        outputSkipData->skipMode =  bnpSetup->outputDcSkipMode;

        if(CPA_DC_SKIP_DISABLED != outputSkipData->skipMode)
        {
            outputSkipData->skipLength = bnpSetup->outputSkipLength;
            outputSkipData->firstSkipOffset = 0;
            outputSkipData->strideLength = 0;
        }
        else
        {
            outputSkipData->skipLength = 0;
            outputSkipData->firstSkipOffset = 0;
            outputSkipData->strideLength = 0;
        }

        if(i == bnpSetup->resetSessionOnJobn)
        {
            pBatchOpData[i].resetSessionState = 1;
        }
        else
        {
            pBatchOpData[i].resetSessionState = 0;
        } 

        if(0 != pBatchOpData[i].resetSessionState &&
                1 != pBatchOpData[i].resetSessionState)
        {
            PRINT_ERR("Invalid reset value. Allowed values are 0 or 1\n");
            qaeMemFreeNUMA((void**)&pSessionHandle);
            qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
            freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
           &(bnpSetup->compressTestParams));
            qaeMemFree((void**)&pBatchOpData);
            status = CPA_STATUS_FAIL;
            return status;
        }

        if(i == (numJobs-1))
        {
            pBatchOpData[i].opData.flushFlag = CPA_DC_FLUSH_FINAL;
        }
        else
        {
            pBatchOpData[i].opData.flushFlag = CPA_DC_FLUSH_SYNC;
        }

        pBatchOpData[i].pSrcBuff=srcBuffListArray[i];
        status = fillSglWithData(srcBuffListArray[i],
                totalSglSize,
                bnp_corpus_g.fileArray[i].corpusBinaryData,
                &pBatchOpData[i].opData.inputSkipData,
                bnpSetup->inputFlatBufferSize);
        if(CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("Failed to insert data in input SGL.\n");
            qaeMemFreeNUMA((void**)&pSessionHandle);
            qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
            freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
           &(bnpSetup->compressTestParams));
            qaeMemFree((void**)&pBatchOpData);
            status = CPA_STATUS_FAIL;
            return status;

        }

    }

    status = cpaDcBufferListGetMetaSize(
              bnpSetup->compressTestParams.dcInstanceHandle,
              noOfFlatBuffersT, &metaSizeT);
    if(CPA_STATUS_FAIL == status) {
        PRINT_ERR("Failed to get meta size for buffer list!\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        qaeMemFree((void**)&pBatchOpData);
        return CPA_STATUS_FAIL;
    }

    /* When compressing,small packet sizes the destination buffer
     * may need to be larger than the source buffer to accommodate
     * huffman data, so allocate double the source buffer size
     */
    if(0 == bnpSetup->outputFlatBufferSize)
    {
        if(MIN_DST_BUFFER_SIZE >= totalBatchSize)
        {
            bnpSetup->outputFlatBufferSize = totalBatchSize * EXTRA_BUFFER;
        }
        else
        {
            bnpSetup->outputFlatBufferSize = totalBatchSize;
        }
    }
    /* Allocate Destination SGLs */
    status = createBNPBuffers(bnpSetup->outputFlatBufferSize,
            noOfFlatBuffersT,
            &dstBuffer, nodeId, metaSizeT);
    if(CPA_STATUS_FAIL == status) {
        PRINT_ERR("Failed to build destination buffer list !\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        qaeMemFree((void**)&pBatchOpData);
        return CPA_STATUS_FAIL;
    }

    cmpResults = qaeMemAlloc(numJobs * sizeof(CpaDcRqResults));
    if (NULL == cmpResults)
    {
        PRINT_ERR("Unable to allocate Memory for cmpResults\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        freeBnpBuffersDst(dstBuffer,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        qaeMemFree((void**)&pBatchOpData);
        return CPA_STATUS_FAIL;
    }
    else
    {
        memset(cmpResults, 0, numJobs * sizeof(CpaDcRqResults));
    }
    pcmpResults = qaeMemAlloc(numJobs * sizeof(CpaDcRqResults));
    if (NULL == pcmpResults)
    {
        PRINT_ERR("Unable to allocate Memory for pcmpResults\n");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        freeBnpBuffersDst(dstBuffer,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        qaeMemFree((void**)&pBatchOpData);
        qaeMemFree((void**)&cmpResults);
        return CPA_STATUS_FAIL;
    }
    else
    {
        memset(pcmpResults, 0, numJobs * sizeof(CpaDcRqResults));
    }
    /* Allocate checksum values */
    seedSwChecksums = (Cpa32U*)qaeMemAlloc(sizeof(Cpa32U) * numJobs);
    if (NULL == seedSwChecksums)
    {
        PRINT_ERR("Failed to allocate seed checksum table");
        qaeMemFreeNUMA((void**)&pSessionHandle);
        qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
        freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        freeBnpBuffersDst(dstBuffer,  bnp_corpus_g.numFilesInCorpus,
       &(bnpSetup->compressTestParams));
        qaeMemFree((void**)&pBatchOpData);
        qaeMemFree((void**)&cmpResults);
        qaeMemFree((void**)&pcmpResults);
        return  CPA_STATUS_FAIL;
    }

    /* generate the start time stamp */
    perfData->startCyclesTimestamp = sampleCodeTimestamp();
    perfData->retries=0;

    for(numLoops = 0; numLoops < compressLoops; numLoops++)
    {

        /* compression API will be called for each buffer list
         * in the corpus File
         */
        numJobs = bnpSetup->numJobsPerBatchRequest;
        resubmission = 0;
        totalBuffs = 0;
        BNP_CHECK_NULL_PARAM(cmpResults);    
        BNP_CHECK_NULL_PARAM(pcmpResults);    
        for(i=0; i< numJobs; i++)
        {
            cmpResults[i].consumed = 0;
            cmpResults[i].produced = 0;
            pcmpResults[i].produced = 0;
            pcmpResults[i].consumed = 0;
            /* add up the number of buffers required for
             * complete corpus, this counter will be used to get the
             * number of call backs invoked
             */
            totalBuffs += bnpSetup->compressTestParams.numberOfBuffers[i];
        }


        numJobsInBatch =numJobs;
        pBnPOpData=pBatchOpData;

        sessionState = bnpSetup->compressTestParams.setupData.sessState;
        /*Reset the status to incomplete for all*/
        for(i=0; i<numJobs; i++ )
        {
            if(pcmpResults[i].status==0)
            {
                pcmpResults[i].status = CPA_DC_OVERFLOW ;
            }
        }
        BNP_CHECK_NULL_PARAM(seedSwChecksums);
        do {
            /* Initialize seed checksums for verifying later on in software */
            initSeedSwChecksums (seedSwChecksums,
                    cmpResults,
                    numJobsInBatch,
                    sessionState);


            status = cpaDcBPCompressData(bnpSetup->compressTestParams.dcInstanceHandle,
                    pSessionHandle, numJobsInBatch, pBnPOpData,
                    dstBuffer, cmpResults,
                    perfData);

            /* Check Status */
            if(CPA_STATUS_SUCCESS != status )
            {
                PRINT_ERR("Data Compression Failed %d\n\n", status);
                perfData->threadReturnStatus = CPA_STATUS_FAIL;
                break;
            }


            /* check if synchronous flag is set
             * if set, invoke the callback API
             */

            if(CPA_SAMPLE_SYNCHRONOUS == bnpSetup->compressTestParams.syncFlag)
            {
                /* invoke the Compression Callback only */
                dcPerformBnpCallback(perfData, status);
            } /* End of SYNC Flag Check */

            status = waitForSemaphore(perfData);
            if(CPA_STATUS_SUCCESS != status)
            {
                PRINT_ERR("Wait for Semaphore Failed %d\n\n", status);
                perfData->threadReturnStatus = CPA_STATUS_FAIL;
                break;
            }

            /*reset the overflow flag to nooverflow*/
            overflownJob = NO_OVERFLOWN_JOB;
            for(i=0; i<numJobsInBatch; i++)
            {
                if((CPA_DC_OK != cmpResults[i].status) &&
                        (CPA_DC_OVERFLOW != cmpResults[i].status))
                {
                    PRINT_ERR("Error! Invalid error code returned by"
                            " cpaDcBPCompressData() %d.\n",
                            cmpResults[i].status);
                    qaeMemFreeNUMA((void**)&pSessionHandle);
                    qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
                    freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
                   &(bnpSetup->compressTestParams));
                    freeBnpBuffersDst(dstBuffer,  bnp_corpus_g.numFilesInCorpus,
                   &(bnpSetup->compressTestParams));
                    qaeMemFree((void**)&pBatchOpData);
                    qaeMemFree((void**)&cmpResults);
                    qaeMemFree((void**)&pcmpResults);
                    qaeMemFree((void**)&seedSwChecksums);
                    return CPA_STATUS_FAIL;
                }
            /*check the overflow*/
                else if(CPA_DC_OVERFLOW == cmpResults[i].status)
                {
                    overflownJob = (Cpa32S)i;
                }
            }

            getBatchConsumedTotalSize(cmpResults, numJobsInBatch,
                    &batchConsumed, &batchProduced);

            fillfrom=numJobs-numJobsInBatch; 
            /*
             *  Handle overflow use case
             */
            if(overflownJob >= 0)
            {
                /* Calculate number of jobs unprocessed */
                numUnprocessedJobs = numJobsInBatch - overflownJob;
                /*fill the actual value in pseudo result stucture */
                for(i=fillfrom, count=0; count<=overflownJob ; i++, count++)
                {
                    if(pcmpResults[i].status==CPA_DC_OVERFLOW)
                    {
                        pcmpResults[i].status = cmpResults[count].status;
                        pcmpResults[i].produced += cmpResults[count].produced;
                        pcmpResults[i].consumed += cmpResults[count].consumed;
                        off_set += pcmpResults[i].produced;
                    }
                }
                /* Find where we stopped in the batch job list and
                 * create a new one based on what was not consumed.
                 */
                status = createNewBatchJob(&pBnPOpData, numJobsInBatch,
                        cmpResults, (Cpa32U)overflownJob,
                        &pResubmitBnpOpData, nodeId, bnpSetup, pcmpResults,
                        resubmission);
                if(CPA_STATUS_SUCCESS != status)
                {
                    PRINT_ERR("Error! Failed to recreate re-submission"
                            "batch job");
                    qaeMemFreeNUMA((void**)&pSessionHandle);
                    qaeMemFreeNUMA((void**)&actualNumFlatBuffers);
                    freeBuffers(&srcBuffListArray,  bnp_corpus_g.numFilesInCorpus,
                   &(bnpSetup->compressTestParams));
                    freeBnpBuffersDst(dstBuffer,  bnp_corpus_g.numFilesInCorpus,
                   &(bnpSetup->compressTestParams));
                    qaeMemFree((void**)&pBatchOpData);
                    qaeMemFree((void**)&cmpResults);
                    qaeMemFree((void**)&pcmpResults);
                    qaeMemFree((void**)&seedSwChecksums);
                    return CPA_STATUS_FAIL;
                }
                perfData->retries ++ ;

                /* Update number jobs with new value */
                numJobsInBatch = numUnprocessedJobs;

                /* Point to new batch job */
                pBnPOpData = pResubmitBnpOpData;
                /*keep the resubmission flag for cleanup purpose and perf_data 
                 for overflow case */
                resubmission = 1;
            }
        }while (overflownJob!=NO_OVERFLOWN_JOB);
        /* here BNP jobs are done for this loop*/

        if(!resubmission)
        {
        } /* without overflow case done */
        else
        {
          /* Overflow Case*/
            for(i=0; i<numJobsInBatch; i++)
            {/*free the last iteration packets*/
                dcFreeBufferListBnp(&pBnPOpData[i].pSrcBuff);
                pBnPOpData[i].pSrcBuff  =  NULL;
            }
            if (NULL != pBnPOpData )
            {
                qaeMemFree ((void **)&pBnPOpData);
            }
            /*Adding final iteration data*/
            if(NO_OVERFLOWN_JOB == overflownJob)
            {
                for(i = fillfrom,count=0; count < numJobsInBatch ;i++,count++)
                {
                    pcmpResults[i].consumed += cmpResults[count].consumed;
                    pcmpResults[i].produced += cmpResults[count].produced; 
                }
            }
        }

    } /* End of compression Loops */

    if(!(resubmission==1 || overflownJob != NO_OVERFLOWN_JOB))
    {
        for(i = 0; i < numJobs; i++)
        {
            perfData->bytesConsumedPerLoop += cmpResults[i].consumed;
            perfData->bytesProducedPerLoop += cmpResults[i].produced;
        }
    }

    if(resubmission)
    {
            for(i = 0; i < numJobs; i++)
            {
                perfData->bytesConsumedPerLoop += pcmpResults[i].consumed;
                perfData->bytesProducedPerLoop += pcmpResults[i].produced;
            }
        if (NULL != pBatchOpData)
        {
            for(i = 0; i < numJobs; i++)
            {
                dcFreeBufferListBnp(&pBatchOpData[i].pSrcBuff);
                pBatchOpData[i].pSrcBuff = NULL;
            }
            if (NULL != pBatchOpData)
            {
                qaeMemFree ((void **)&pBatchOpData);
            }
        }    
    }
    if(!resubmission)
    {  
    /*free the without overflow jobs packets for all the loops*/ 
	BNP_CHECK_NULL_PARAM(pBnPOpData);
        for(i=0; i<numJobsInBatch; i++)
        {
            pBnPOpData[i].pSrcBuff->numBuffers = actualNumFlatBuffers[i];
            dcFreeBufferListBnp(&pBnPOpData[i].pSrcBuff);
            pBnPOpData[i].pSrcBuff  = NULL;
        }
    }
    if (NULL != pBnPOpData)
    {
        qaeMemFree ((void **)&pBnPOpData);
    }
    dcSampleFreeStatefulContextBuffer3(pContextBuffer);
    qaeMemFree((void**)&actualNumFlatBuffers);
    qaeMemFree((void**)&srcBuffListArray);
    freeBnpBuffersDst(dstBuffer,  bnp_corpus_g.numFilesInCorpus,
                       &(bnpSetup->compressTestParams));
    qaeMemFree((void**)&cmpResults);
    qaeMemFree((void**)&pcmpResults);
    qaeMemFree((void**)&seedSwChecksums);
    /* Close the DC Session */
    status = cpaDcRemoveSession(bnpSetup->compressTestParams.dcInstanceHandle,
              pSessionHandle);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to remove session\n");
    }
    qaeMemFreeNUMA((void**)&pSessionHandle);
    return status;
}


CpaStatus setupDcBnPBenchmarkPerf(
        corpus_type_t corpusType,
        CpaDcCompLvl compLevel,
        CpaDcHuffType huffmanType,
        Cpa32U inputFlatBufferSize,
        Cpa32U outputFlatBufferSize,
        Cpa32U numLoops,
        Cpa32U numBuffersPerInputBufferList,
        Cpa32U numBuffersPerOutputBufferList,
        Cpa32U numJobsPerBatchRequest,
        CpaDcSkipMode inputDcSkipMode,
        CpaDcSkipMode outputDcSkipMode,
        Cpa32U inputDcSkipLength,
        Cpa32U outputDcSkipLength,
        Cpa32U resetSessionOnJobn,
        CpaDcSessionState  state)
{
    CpaStatus status = CPA_STATUS_SUCCESS;

    status = setupDcBnPStatefulPerf(compLevel,
            huffmanType,
            inputFlatBufferSize,
            outputFlatBufferSize,
            corpusType,
            numLoops,
            numJobsPerBatchRequest, /*number of jobs in batch */
            CPA_FALSE, /* trigger zero length job*/
            inputDcSkipMode, /* input skip mode */
            outputDcSkipMode, /* output skip mode */
            inputDcSkipLength, /* input skip length */
            outputDcSkipLength, /* output skip length */
            CPA_FALSE, /* trigger overflow */
            resetSessionOnJobn,
            state
            );
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Error calling setupDcBnPStatefulPerf\n");
    }
    return status;
}
EXPORT_SYMBOL(setupDcBnPStatefulPerf);
EXPORT_SYMBOL(setupDcBnPBenchmarkPerf);
EXPORT_SYMBOL(setupDcBnPTest);
