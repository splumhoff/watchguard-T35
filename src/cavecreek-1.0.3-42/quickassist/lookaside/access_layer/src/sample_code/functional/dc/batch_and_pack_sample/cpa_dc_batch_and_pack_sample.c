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

/*
 * This is sample code that demonstrates usage of the data compression API,
 * and specifically using this API to statelessly compress an input buffer. It
 * will compress the data using deflate with dynamic huffman trees.
 */


#include "cpa_dc.h"
#include "cpa_dc_bp.h"

#include "cpa_sample_utils.h"
#include "cpa_dc_batch_and_pack_sgl.h"

#define SAMPLE_MAX_BUFF 1024
#define TIMEOUT_MS 5000 /* 5 seconds */
#define SINGLE_INTER_BUFFLIST 1
#define OUTPUT_FILENAME    "dc_batch_and_pack.deflate"
extern int gDebugParam;


/*
 * Callback function
 *
 * This function is "called back" (invoked by the implementation of
 * the API) when the asynchronous operation has completed.  The
 * context in which it is invoked depends on the implementation, but
 * as described in the API it should not sleep (since it may be called
 * in a context which does not permit sleeping, e.g. a Linux bottom
 * half).
 *
 * This function can perform whatever processing is appropriate to the
 * application.  For example, it may free memory, continue processing
 * of a packet, etc.  In this example, the function only sets the
 * complete variable to indicate it has been called.
 */
//<snippet name="dcCallback">
static void
dcCallback(void *pCallbackTag,
        CpaStatus status)
{
    PRINT_DBG("Callback called with status = %d.\n", status);

    if (NULL != pCallbackTag)
    {
        /* indicate that the function has been called */
        COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
    }
}
//</snippet>


/*
 * This function performs a compression and decompress operation.
 */
static CpaStatus
compPerformOp(CpaInstanceHandle dcInstHandle, CpaDcSessionHandle sessionHdl,
              const char **paramList, const Cpa32U numParams)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U i = 0, j = 0;
    /* Create Batch OP data structure */
    CpaDcBatchOpData *pBatchOpData = NULL;
    Cpa32U inputSglSize = 0;
    Cpa32U nbOfFlatBuffers = 0;
    Cpa32U metaSizeInBytes = 0;
    const Cpa32U numJobs = (numParams - BNP_FIRST_FILE_PARAM) / NUM_BNP_JOB_PARAMS;
    char *filename = NULL;
    FILE *fp = NULL;
    FILE *bnpOutFp = NULL;
    Cpa32U fileSize = 0;
    Cpa32U totalProducedToWrite = 0;
    Cpa32U destinationSglSize = 0;
    CpaDcSkipData *inputSkipData = NULL;
    CpaDcSkipData *outputSkipData = NULL;
    Cpa32U totalBatchSize = 0;
    Cpa32U totalProduced = 0;
    CpaBufferList *pBufferListBnpSrc = NULL;
    CpaBufferList *pBufferListBnpDst = NULL;
    CpaFlatBuffer *pFlatBuffer = NULL;
    /* The following variables are allocated on the stack because we block
     * until the callback comes back. If a non-blocking approach was to be
     * used then these variables should be dynamically allocated */
    CpaDcRqResults *pDcResults = NULL;
    struct COMPLETION_STRUCT complete;

    status = OS_MALLOC(&pBatchOpData, sizeof(CpaDcBatchOpData) * numJobs);
    if (CPA_STATUS_SUCCESS == status)
    {
        for (i=0; i<numJobs; i++)
        {
            filename = (char*)paramList[BNP_FIRST_FILE_PARAM +
                                NUM_BNP_JOB_PARAMS * i];
            fp = fopen(filename, "r");
            if (NULL == fp)
            {
                status = CPA_STATUS_FAIL;
                PRINT_ERR("Error opening %s!\n", filename);
                goto cleanup;
            }

            /*
             * Figure out the file size
             */
            fseek(fp, 0L, SEEK_END);
            fileSize = ftell(fp);
            fseek(fp, 0L, SEEK_SET);

            /*
             * Create the SGL depending on the size of the file
             * and the size of the skip data
             */
            inputSkipData = &pBatchOpData[i].opData.inputSkipData;
            inputSkipData->skipMode = CPA_DC_SKIP_DISABLED;
            inputSkipData->skipLength = 0;

            inputSglSize = fileSize;
            totalBatchSize += fileSize;


            /* Output skip mode configuration */
            outputSkipData = &pBatchOpData[i].opData.outputSkipData;
            outputSkipData->skipMode = CPA_DC_SKIP_DISABLED;
            outputSkipData->skipLength = 0;

            pBatchOpData[i].resetSessionState = atoi(paramList[BNP_FIRST_RESET_PARAM +
                                             NUM_BNP_JOB_PARAMS * i]);

            if (0 != pBatchOpData[i].resetSessionState &&
                    1 != pBatchOpData[i].resetSessionState)
            {
                PRINT_ERR("Invalid reset value. Allowed values are 0 or 1\n");
                status = CPA_STATUS_FAIL;
                fclose (fp);
                goto cleanup;
            }
            if (i == (numJobs-1))
            {
                pBatchOpData[i].opData.flushFlag = CPA_DC_FLUSH_FINAL;
            }
            else
            {
                pBatchOpData[i].opData.flushFlag = CPA_DC_FLUSH_SYNC;
            }

            PRINT_DBG("File %s (%d bytes) assigned to Job %d (%s)\n",
                    filename,
                    fileSize,
                    i,
                    (i == (numJobs-1))? "Flush final" : "Flush sync");

            /* Figure out how many 4K flat buffers we need */
            nbOfFlatBuffers = 0;
            do{
                if (inputSglSize >= _4K_PAGE_SIZE)
                {
                    inputSglSize -= _4K_PAGE_SIZE;
                }
                else
                {
                    inputSglSize = 0;
                }
                nbOfFlatBuffers++;
            }while (inputSglSize > 0);

            /* Now that we have the number of flat buffers, we
             * can get the meta size
             */
            if (CPA_STATUS_SUCCESS == status)
            {
                status = cpaDcBnpBufferListGetMetaSize(dcInstHandle,
                                               nbOfFlatBuffers,
                                               &metaSizeInBytes);
            }
            if (CPA_STATUS_SUCCESS == status)
            {
                status = dcBuildBnpBufferList(&pBufferListBnpSrc, nbOfFlatBuffers,
                                 _4K_PAGE_SIZE, metaSizeInBytes);
                /* Assign buffer list to current job */
                pBatchOpData[i].pSrcBuff = pBufferListBnpSrc;
            }

            /* Fill SGL with input file data */
            if (CPA_STATUS_SUCCESS == status)
            {
                pFlatBuffer = &pBufferListBnpSrc->pBuffers[0];
                for (j=0; j<pBufferListBnpSrc->numBuffers; j++)
                {
                    pFlatBuffer->dataLenInBytes = fread(pFlatBuffer->pData,
                          1,
                          _4K_PAGE_SIZE,
                          fp);
                    pFlatBuffer++;
                }
            }
            fclose (fp);
        }
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Calculate Total buffer list size for destination buffer
         * so that we do not get overflow */
        PRINT_DBG("Total batch payload size: %d\n", totalBatchSize);
        destinationSglSize = totalBatchSize + (totalBatchSize >> 2);

        /* Figure out how many 4K flat buffers we need */
        inputSglSize = destinationSglSize;
        nbOfFlatBuffers = 0;
        do{
            if (inputSglSize >= _4K_PAGE_SIZE)
            {
                inputSglSize -= _4K_PAGE_SIZE;
            }
            else
            {
                inputSglSize = 0;
            }
            nbOfFlatBuffers++;
        }while (inputSglSize > 0);

        /* Allocate Destination SGLs */
        status = dcGetMetaAndBuildBufferList(dcInstHandle,
                             &pBufferListBnpDst,
                             nbOfFlatBuffers,
                             _4K_PAGE_SIZE);
        if (CPA_STATUS_FAIL == status)
        {
            PRINT_ERR("Failed to build pDestBuffer!\n");
            goto cleanup;
        }
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate table of CpaDcRqResults structure */
        status = OS_MALLOC(&pDcResults, sizeof(CpaDcRqResults) * numJobs);
    }

    /* Initialize checksums */
    if (CPA_STATUS_SUCCESS == status)
    {
        memset(pDcResults, 0, sizeof(CpaDcRqResults) * numJobs);
    }

//<snippet name="perfOp">
    COMPLETION_INIT(&complete);

    /* Call batch and pack API */
    if (CPA_STATUS_SUCCESS == status)
    {
        status = cpaDcBPCompressData(dcInstHandle,
                sessionHdl,
                numJobs,
                pBatchOpData,
                pBufferListBnpDst,
                pDcResults,
                (void *)&complete);
    }
//</snippet>
    if (CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("cpaDcBPCompressData failed. (status = %d)\n", status);
    }

    /*
     * We now wait until the completion of the operation.  This uses a macro
     * which can be defined differently for different OSes.
     */
    if (CPA_STATUS_SUCCESS == status)
    {
        if (!COMPLETION_WAIT(&complete, TIMEOUT_MS))
        {
            PRINT_ERR("timeout or interruption in cpaDcCompressData\n");
            status = CPA_STATUS_FAIL;
        }
    }

    /* Log destination SGL in a debug file called "dc_batch_and_pack.deflate" */
    if (CPA_STATUS_SUCCESS == status)
    {
        /* Calculate total produced output */
        totalProduced = 0;
        for (i=0; i<numJobs; i++)
        {
            totalProduced += pDcResults[i].produced;
        }
        totalProducedToWrite = totalProduced;

        bnpOutFp = fopen (OUTPUT_FILENAME, "w");
        for (i=0; i<pBufferListBnpDst->numBuffers; i++)
        {
            if (totalProducedToWrite >= pBufferListBnpDst->pBuffers[i].dataLenInBytes)
            {
                totalProducedToWrite -= pBufferListBnpDst->pBuffers[i].dataLenInBytes;
            }
            else
            {
                totalProducedToWrite = 0;
            }
            fwrite(pBufferListBnpDst->pBuffers[i].pData,
                   1,
                   totalProducedToWrite,
                   bnpOutFp);
        }
        fflush(bnpOutFp);
        fclose(bnpOutFp);
    }

    /*
     * We now print the results
     */
    if(CPA_STATUS_SUCCESS == status)
    {
        for (i=0; i<numJobs; i++)
        {
            if(pDcResults[i].status != CPA_DC_OK)
            {
                PRINT_ERR("Results status not as expected (status[%d] = %d)\n",
                         i, pDcResults[i].status);
                status = CPA_STATUS_FAIL;
            }
            else
            {
                PRINT_DBG("Batch job %d\n", i);
                PRINT_DBG("    Data consumed %d\n", pDcResults[i].consumed);
                PRINT_DBG("    Data produced %d\n", pDcResults[i].produced);
                PRINT_DBG("    CRC checksum 0x%x\n", pDcResults[i].checksum);
            }
        }
        PRINT_DBG("Total batch produced data: %d\n", totalProduced);
    }

cleanup:
    for (i=0; i<numJobs; i++)
    {
        pBufferListBnpSrc = pBatchOpData[i].pSrcBuff;
        if (NULL != pBufferListBnpSrc)
        {
            dcFreeBnpBufferList(&pBufferListBnpSrc);
        }
    }
    OS_FREE(pBatchOpData);
    if (NULL != pBufferListBnpDst)
    {
        dcFreeBnpBufferList(&pBufferListBnpDst);
    }
    OS_FREE(pDcResults);
    COMPLETION_DESTROY(&complete);

    return status;
}


/*
 * This is the main entry point for the sample data compression code.
 * demonstrates the sequence of calls to be made to the API in order
 * to create a session, perform one or more statless compression operations,
 * and then tear down the session.
 */
CpaStatus
dcBatchAndPackSample(const char **paramList, const Cpa32U numParams)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaDcInstanceCapabilities cap = {0};
    CpaBufferList **bufferInterArray = NULL;
    CpaBufferList *pBufferCtx = NULL;
    Cpa16U numInterBuffLists = 0;
    Cpa16U bufferNum = 0;
    Cpa32U buffMetaSize = 0;
    Cpa32U sess_size = 0;
    Cpa32U ctx_size = 0;
    CpaDcSessionHandle sessionHdl = NULL;
    CpaInstanceHandle dcInstHandle = NULL;
    CpaDcSessionSetupData sd = {0};
    CpaDcStats dcStats = {0};
    CpaDcCompLvl compLevel = CPA_DC_L1;
    CpaDcSessionState sessionState = CPA_DC_STATEFUL;

    /*
     * In this simplified version of instance discovery, we discover
     * exactly one instance of a data compression service.
     */
    sampleDcGetInstance(&dcInstHandle);
    if (dcInstHandle == NULL)
    {
        return CPA_STATUS_FAIL;
    }

    /* Query Capabilities */
    PRINT_DBG("cpaDcQueryCapabilities\n");
//<snippet name="queryStart">
    status = cpaDcQueryCapabilities(dcInstHandle, &cap);
    if(status != CPA_STATUS_SUCCESS)
    {
       return status;
    }

    if( !cap.statelessDeflateCompression ||
         !cap.statelessDeflateDecompression ||
         !cap.checksumAdler32 ||
         !cap.dynamicHuffman )
    {
        PRINT_DBG("Error: Unsupported functionality\n");
        return CPA_STATUS_FAIL;
    }

    if( cap.dynamicHuffmanBufferReq )
    {
        status = cpaDcBufferListGetMetaSize(dcInstHandle,
                1, &buffMetaSize);

        if(CPA_STATUS_SUCCESS == status)
        {
            status = cpaDcGetNumIntermediateBuffers(dcInstHandle,
                    &numInterBuffLists);
        }
        if(CPA_STATUS_SUCCESS == status && 0 != numInterBuffLists)
        {
            status = PHYS_CONTIG_ALLOC(&bufferInterArray,
                    numInterBuffLists*sizeof(CpaBufferList *));

        }
        for(bufferNum = 0; bufferNum < numInterBuffLists; bufferNum++)
        {
            if(CPA_STATUS_SUCCESS == status)
            {
                status = PHYS_CONTIG_ALLOC(
                        &bufferInterArray[bufferNum],sizeof(CpaBufferList));
            }
            if(CPA_STATUS_SUCCESS == status)
            {
                status = PHYS_CONTIG_ALLOC(
                        &bufferInterArray[bufferNum]->pPrivateMetaData,
                         buffMetaSize);
            }
            if(CPA_STATUS_SUCCESS == status)
            {
                status = PHYS_CONTIG_ALLOC(
                        &bufferInterArray[bufferNum]->pBuffers,
                        sizeof(CpaFlatBuffer));
            }
            if(CPA_STATUS_SUCCESS == status)
            {
                /* Implementation requires an intermediate buffer approximately
                           twice the size of the output buffer */
                status = PHYS_CONTIG_ALLOC(
                        &bufferInterArray[bufferNum]->pBuffers->pData,
                        2*SAMPLE_MAX_BUFF);
                bufferInterArray[bufferNum]->numBuffers = 1;
                bufferInterArray[bufferNum]->pBuffers->dataLenInBytes =
                        2*SAMPLE_MAX_BUFF;
            }

        } /* End numInterBuffLists */
    }

    if(CPA_STATUS_SUCCESS == status)
    {
       /*
        * Set the address translation function for the instance
        */
       status = cpaDcSetAddressTranslation(dcInstHandle, sampleVirtToPhys);
    }

    if(CPA_STATUS_SUCCESS == status)
    {
       /* Start DataCompression component */
       PRINT_DBG("cpaDcStartInstance\n");
       status = cpaDcStartInstance(dcInstHandle, numInterBuffLists,
               bufferInterArray);
    }
//</snippet>

    if (CPA_STATUS_SUCCESS == status)
    {
       /*
        * If the instance is polled start the polling thread. Note that
        * how the polling is done is implementation-dependant.
        */
       sampleDcStartPolling(dcInstHandle);
       /*
        * We now populate the fields of the session operational data and create
        * the session.  Note that the size required to store a session is
        * implementation-dependent, so we query the API first to determine how
        * much memory to allocate, and then allocate that memory.
        */
//<snippet name="initSession">
        compLevel = atoi (paramList[BNP_DC_LEVEL_PARAM]);
        if (compLevel < CPA_DC_L1 || compLevel > CPA_DC_L4)
        {
            PRINT_ERR("Invalid compression level, allowed values range from %d to %d\n",CPA_DC_L1,CPA_DC_L4);
            status = CPA_STATUS_FAIL;
        }
        sd.compLevel = compLevel;
        sd.compType = CPA_DC_DEFLATE;
        sd.huffType = CPA_DC_HT_STATIC;
        /* If the implementation supports it, the session will be configured
         * to select static Huffman encoding over dynamic Huffman as
         * the static encoding will provide better compressibility.
         */
        if(cap.autoSelectBestHuffmanTree)
        {
           sd.autoSelectBestHuffmanTree = CPA_TRUE;
        }
        else
        {
           sd.autoSelectBestHuffmanTree = CPA_FALSE;
        }
        sd.sessDirection = CPA_DC_DIR_COMPRESS;

        sessionState = atoi (paramList[BNP_STATE_PARAM]);
        if (0 != sessionState  && 1 != sessionState)
        {
            PRINT_ERR("Invalid compression state, allowed values range from 0 to 1\n");
            status = CPA_STATUS_FAIL;
        }
        sd.sessState = sessionState;
#if ( CPA_DC_API_VERSION_NUM_MAJOR == 1 && CPA_DC_API_VERSION_NUM_MINOR < 6 )
        sd.deflateWindowSize = 7;
#endif
        sd.checksum = CPA_DC_CRC32;

        /* Determine size of session context to allocate */
        PRINT_DBG("cpaDcGetSessionSize\n");
        status = cpaDcGetSessionSize(dcInstHandle,
                    &sd, &sess_size, &ctx_size);
    }
    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate session memory */
        status = PHYS_CONTIG_ALLOC(&sessionHdl, sess_size);
    }
    if((CPA_STATUS_SUCCESS == status) && (ctx_size != 0))
    {
        /* Allocate context bufferlist */
        dcGetMetaAndBuildBufferList(dcInstHandle, &pBufferCtx,
                  1, ctx_size);
    }
    /* Initialize the Stateless session */
    if (CPA_STATUS_SUCCESS == status)
    {
        PRINT_DBG("cpaDcInitSession (%s, %s, Level %d)\n",
                ((1==sessionState) ? "Stateless": "Stateful"),
                "Static", compLevel);
        status = cpaDcInitSession(dcInstHandle,
                            sessionHdl, /* session memory */
                            &sd,  /* session setup data */
                            pBufferCtx,
                            dcCallback); /* callback function */
    }
//</snippet>
    if (CPA_STATUS_SUCCESS == status)
    {
        CpaStatus sessionStatus = CPA_STATUS_SUCCESS;
        /* Perform Compression operation */
        status = compPerformOp(dcInstHandle, sessionHdl,
                 paramList, numParams);
        /*
         * In a typical usage, the session might be used to compression
         * multiple buffers.  In this example however, we can now
         * tear down the session.
         */
        PRINT_DBG("cpaDcRemoveSession\n");
//<snippet name="removeSession">
        sessionStatus = cpaDcRemoveSession(
                dcInstHandle, sessionHdl);
//</snippet>

        /* Maintain status of remove session only when status of all operations
         * before it are successful. */
        if (CPA_STATUS_SUCCESS == status)
        {
            status = sessionStatus;
        }
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /*
         * We can now query the statistics on the instance.
         *
         * Note that some implementations may also make the stats
         * available through other mechanisms, e.g. in the /proc
         * virtual filesystem.
         */
        status = cpaDcGetStats(dcInstHandle, &dcStats);

        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("cpaDcGetStats failed, status = %d\n", status);
        }
        else
        {
            PRINT_DBG("Number of compression operations completed: %llu\n",
                        (unsigned long long)dcStats.numCompCompleted);
            PRINT_DBG("Number of decompression operations completed: %llu\n",
                    (unsigned long long)dcStats.numDecompCompleted);
        }
    }

    /*
     * Free up memory, stop the instance, etc.
     */

    /* Stop the polling thread */
    sampleDcStopPolling();

    PRINT_DBG("cpaDcStopInstance\n");
    cpaDcStopInstance(dcInstHandle);

    /* Free session Context */
    PHYS_CONTIG_FREE(sessionHdl);

    /* Free context buffer */
    if (NULL != pBufferCtx)
    {
        dcFreeBnpBufferList(&pBufferCtx);
    }

    /* Free intermediate buffers */
    if(bufferInterArray != NULL)
    {
        for(bufferNum = 0; bufferNum < numInterBuffLists; bufferNum++)
        {
            PHYS_CONTIG_FREE(bufferInterArray[bufferNum]->pBuffers->pData);
            PHYS_CONTIG_FREE(bufferInterArray[bufferNum]->pBuffers);
            PHYS_CONTIG_FREE(bufferInterArray[bufferNum]->pPrivateMetaData);
            PHYS_CONTIG_FREE(bufferInterArray[bufferNum]);
        }
        PHYS_CONTIG_FREE(bufferInterArray);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        PRINT_DBG("Sample code ran successfully\n");
    }
    else
    {
        PRINT_DBG("Sample code failed with status of %d\n", status);
    }

    return status;
}


