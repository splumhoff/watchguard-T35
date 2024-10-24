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
#include "cpa_sample_code_utils_common.h"
#include "cpa_sample_code_dc_perf.h"
#include "cpa_sample_code_crypto_utils.h"
#include "cpa_sample_code_dc_utils.h"
#include "icp_sal_poll.h"


#ifdef NEWDISPLAY

int Header = 0;
extern CpaBoolean useZlib_g;
extern CpaStatus stopDcServices(compression_test_params_t *dcSetup);
extern Cpa32U getDcThroughput(Cpa32U totalBytes, perf_cycles_t cycles, Cpa32U numOfLoops);



void dcPrintHeaders() {
 PRINT("%-12s%22s%14s%17s%17s%15s%21s%15s","Algorithm","Corpus","State","Huffman_Type","Mode","Direction","Packet_Size(Bytes)","Comp_level");
 PRINT("%19s%17s%15s\n","Throughput(Mbps)","Comp_Ratio(%)","Total_Retries");
}

CpaStatus dcPrintStats(thread_creation_data_t* data)
{
    perf_cycles_t numOfCycles = {0};
    perf_data_t stats = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa32U i = 0;
    Cpa32U throughput = 0;
    Cpa32U bytesConsumed = 0, bytesProduced = 0;
    compression_test_params_t *dcSetup = (compression_test_params_t*)
                                                            data->setupPtr;
    static int first_time = 1;


    /* stop DC Services */
    status = stopDcServices(dcSetup);
    if(CPA_STATUS_SUCCESS != status)
    {
        PRINT_ERR("Unable to stop DC services\n");
        return status;
    }

    memset(&stats,0,sizeof(perf_data_t));
    stats.averagePacketSizeInBytes = data->packetSize;

    /* get the longest start time and longest End time
     * from the performance stats structure
     */
    getLongestCycleCount(&stats,data->performanceStats,data->numberOfThreads);

    /* Get the total number of responses, bytes consumed and bytes produced
     * for all the threads */
    for (i=0; i<data->numberOfThreads; i++)
    {
        if(CPA_STATUS_FAIL == data->performanceStats[i]->threadReturnStatus)
        {
            return CPA_STATUS_FAIL;
        }
        stats.retries += data->performanceStats[i]->retries;
        stats.responses += data->performanceStats[i]->responses;
        bytesConsumed += data->performanceStats[i]->bytesConsumedPerLoop;
        bytesProduced += data->performanceStats[i]->bytesProducedPerLoop;;
        clearPerfStats(data->performanceStats[i]);
    }
    /* get the maximum number of cycles Required */
    numOfCycles = (stats.endCyclesTimestamp - stats.startCyclesTimestamp);


  if (Header == 0){
     PRINT("Number of threads      %d\n", data->numberOfThreads);
     PRINT("CPU Frequency(kHz)     %u\n",sampleCodeGetCpuFreq());
     Header++;
  }
  if (first_time) {
      dcPrintHeaders();
      first_time = 0;
   }
   dcPrintTestData(dcSetup);

   //PRINT("Number of threads      %d\n", data->numberOfThreads);^M
   // PRINT("Total Responses        %llu\n",(unsigned long long)stats.responses);^M
   // PRINT("Total Retries          %u\n",stats.retries);
   // PRINT("Clock Cycles Start     %llu\n",stats.startCyclesTimestamp);^M
   // PRINT("Clock Cycles End       %llu\n",stats.endCyclesTimestamp);^M
   // PRINT("Total Cycles           %llu\n", numOfCycles);^M
   // PRINT("CPU Frequency(kHz)     %u\n",sampleCodeGetCpuFreq());^M

    throughput = getDcThroughput(bytesConsumed,
            numOfCycles, dcSetup->numLoops);
    PRINT("%19u", throughput);
    dcCalculateAndPrintCompressionRatio(bytesConsumed,bytesProduced);
    PRINT("%15u",stats.retries);
    PRINT ("\n");

    return status;
}

void dcPrintTestData(compression_test_params_t* dcSetup)
{
#if 0 
//this parameters could only be printed once, there is no need to dislay them each time
    PRINT("API                    ");
    if(dcSetup->isDpApi){
        PRINT("Data_Plane\n");
    }else{
        PRINT("Traditional\n");
    }
    if(useZlib_g)
    {
        PRINT("(from zLib compressed data\n");
    }
    else
    {
        PRINT("\n");
    }
#endif 

//    PRINT("Algorithm              ");
    switch(dcSetup->setupData.compType)
    {
    case(CPA_DC_LZS):
        PRINT("%-12s","LZS");
    break;
    case(CPA_DC_DEFLATE):
         PRINT("%-12s","DEFLATE");
    break;
    default:
         PRINT("Unsupported %d",dcSetup->setupData.compType);
    }

//    PRINT("Corpus                 ");
    switch(dcSetup->corpus)
    {
    case(CANTERBURY_CORPUS):
          PRINT("%22s","CANTERBURY_CORPUS");
    break;
    case(CALGARY_CORPUS):
         PRINT("%22s","CALGARY_CORPUS");
    break;
    default:
        PRINT("Unsuported %d",dcSetup->corpus);
    }

//    PRINT("Session State          ");
    switch(dcSetup->setupData.sessState)
    {
    case(CPA_DC_STATEFUL):
         PRINT("%14s","STATEFUL");
    break;
    case(CPA_DC_STATELESS):
          PRINT("%14s","STATELESS");
    break;
    default:
           PRINT("Unsupported %d",dcSetup->setupData.sessState);
    }

//    PRINT("Huffman Type           ");
    switch(dcSetup->setupData.huffType)
    {
    case(CPA_DC_HT_STATIC):
         PRINT("%17s","STATIC");
    break;
    case(CPA_DC_HT_FULL_DYNAMIC):
         PRINT("%17s","DYNAMIC");
    break;
    default:
        PRINT("Unsupported %d",dcSetup->setupData.huffType);
    }

//    PRINT("Mode                   ");
    switch(dcSetup->syncFlag)
    {
    case(CPA_SAMPLE_SYNCHRONOUS):
         PRINT("%17s","SYNCHRONOUS");
    break;
    case(CPA_SAMPLE_ASYNCHRONOUS):
         PRINT("%17s","ASYNCHRONOUS");
    break;
    default:
         PRINT("Unsupported %d",dcSetup->syncFlag);
    }
//    PRINT("Direction              ");
    switch(dcSetup->dcSessDir)
    {
    case(CPA_DC_DIR_COMPRESS):
         PRINT("%15s","COMPRESS");
    break;
    case(CPA_DC_DIR_DECOMPRESS):
        PRINT("%15s","DECOMPRESS");
    break;
    case(CPA_DC_DIR_COMBINED):
        PRINT("%15s","COMBINED");
    break;
    default:
        PRINT("Unsupported %d",dcSetup->setupData.sessDirection);
    }

//    PRINT("Packet Size            %d\n",dcSetup->bufferSize);

//    PRINT("Compression Level      %d\n",dcSetup->setupData.compLevel);

    PRINT("%21d",dcSetup->bufferSize);
    PRINT("%15d",dcSetup->setupData.compLevel);

}

CpaStatus dcCalculateAndPrintCompressionRatio(Cpa32U bytesConsumed,
        Cpa32U bytesProduced)
{
    Cpa32U ratio = 0, remainder = 0;

    if(0 == bytesConsumed)
    {
        PRINT("Divide by zero error on calculating compression ratio\n");
        return CPA_STATUS_FAIL;
    }
#ifdef USER_SPACE
    PRINT("%17.02f",
                                      ((float)bytesProduced/bytesConsumed));
    return CPA_STATUS_SUCCESS;

#endif

    ratio = bytesProduced * SCALING_FACTOR_1000;
    do_div(ratio,bytesConsumed);
    remainder = ratio % BASE_10;
    ratio = bytesProduced * SCALING_FACTOR_100;
    do_div(ratio,bytesConsumed);
    PRINT("%17d.%d%%",ratio,remainder);
    return CPA_STATUS_SUCCESS;
}

#endif //NEWDISPLAY
