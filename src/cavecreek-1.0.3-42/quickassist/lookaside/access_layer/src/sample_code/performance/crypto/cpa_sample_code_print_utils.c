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
#include "cpa_sample_code_crypto_utils.h"
#include "cpa_cy_common.h"
#include "cpa_cy_im.h"
#include "cpa_cy_prime.h"
#include "icp_sal_poll.h"

#ifdef NEWDISPLAY
int displayOnce = 0;
int displayAlgName = 0;
char PrintAlg[50];
char PrintAsymmName[30];
int AsymmModSize;
int wirelesDisplay;
int usePartial_g;
Cpa32U packetSizes[] = {
      BUFFER_SIZE_64,
     		//BUFFER_SIZE_128,
       BUFFER_SIZE_256,
//       BUFFER_SIZE_512,
//       BUFFER_SIZE_768,
//       BUFFER_SIZE_1024,
                //BUFFER_SIZE_1152,
//       BUFFER_SIZE_1280,
//       BUFFER_SIZE_1536,
       		//BUFFER_SIZE_2048,
//       BUFFER_SIZE_4096,
//       BUFFER_SIZE_16384,
       		//PACKET_IMIX, 
       }; 


Cpa32U numPacketSizes = sizeof(packetSizes)/sizeof(Cpa32U);
EXPORT_SYMBOL(numPacketSizes);

extern int signOfLife;
extern Cpa32U getThroughput(Cpa64U numPackets, Cpa32U packetSize,perf_cycles_t cycles); 
extern Cpa32U getOpsPerSecond(Cpa64U responses, perf_cycles_t cycles);
extern void getLongestCycleCount(perf_data_t *dest, perf_data_t *src[], Cpa32U count);

CpaStatus printAsymStatsAndStopServices(thread_creation_data_t* data)
{
 Cpa32U i = 0;
 Cpa32U opsPerSec = 0;
 Cpa32U responsesPerThread = 0;
 perf_cycles_t numOfCycles = 0;
 perf_data_t stats = {0};
 /*stop all crypto instances, There is no other place we can stop CyServices * as all other function run in thread context and its not safe to call
  * stopCyServices in thread context, otherwise we could stop threads that * have requests in flight. This function is called by the framework
  * after all threads have completed*/
 stopCyServices();

 stats.averagePacketSizeInBytes = data->packetSize;
 getLongestCycleCount(&stats, data->performanceStats,data->numberOfThreads);
 /*loop over each thread perf stats and total up the responses * also check of any of the threads has a fail status and return fail if * this is the case*/
 for (i=0; i<data->numberOfThreads; i++) {
        if(data->performanceStats[i]->threadReturnStatus == CPA_STATUS_FAIL)
        {
            PRINT("Thread %d Failed\n", i);
            return CPA_STATUS_FAIL;
        }
        stats.responses += data->performanceStats[i]->responses;
        stats.numOperations += data->performanceStats[i]->numOperations;
        stats.retries += data->performanceStats[i]->retries;
        responsesPerThread = data->performanceStats[i]->responses;
        clearPerfStats(data->performanceStats[i]);
    }
    numOfCycles = (stats.endCyclesTimestamp - stats.startCyclesTimestamp);
    if (signOfLife){
       PRINT("%-25s", PrintAsymmName);
       PrintAsymmName[0] = '\0';
       PRINT("%15d\n",AsymmModSize);
       PRINT("Number of Threads     %u\n", data->numberOfThreads);
       PRINT("Total Submissions     %llu\n",(unsigned long long )stats.numOperations);
       PRINT("Total Responses       %llu\n",(unsigned long long )stats.responses);
       PRINT("Total Retries         %u\n",stats.retries);
    }  
    if(!signOfLife)
    {
        opsPerSec = getOpsPerSecond(stats.responses, numOfCycles);
        if (displayOnce == 0) {
           PRINT("Number of Threads     %u\n", data->numberOfThreads); 
           PRINT("Total Submissions     %llu\n",(unsigned long long )stats.numOperations); 
           PRINT("CPU Frequency(kHz)    %u\n", sampleCodeGetCpuFreq()); 

           PRINT("%-25s%15s%35s%15s\n","ALGORITHM","MODULUS(BITS)","OPERATIONS_PER_SECOND","RETRIES")
           PRINT("%-25s", PrintAsymmName); 
           PrintAsymmName[0] = '\0'; 
           PRINT("%15d",AsymmModSize);     
           if(responsesPerThread < ASYM_THROUGHPUT_MIN_SUBMISSIONS)
           {
               PRINT("Need to submit >= %u per thread for accurate throughput\n",
                       ASYM_THROUGHPUT_MIN_SUBMISSIONS);
           }
           else
           {
               PRINT("%35u", opsPerSec); 
           }
           PRINT("%15u\n",stats.retries);
        displayOnce++;
       }
       else {
           PRINT("%-25s", PrintAsymmName);
           PrintAsymmName[0] = '\0';
           PRINT("%15d",AsymmModSize);
           if(responsesPerThread < ASYM_THROUGHPUT_MIN_SUBMISSIONS)
           {
               PRINT("Need to submit >= %u per thread for accurate throughput\n",
                       ASYM_THROUGHPUT_MIN_SUBMISSIONS);
           }
           else
           {
               PRINT("%35u", opsPerSec);
           }
          PRINT("%15u\n",stats.retries);
        }
    }

    return CPA_STATUS_SUCCESS;
}

void ecdsaPrintStats(thread_creation_data_t* data)
{   strcat(PrintAsymmName, "ECDSA_VERIFY");
    AsymmModSize = data->packetSize;
    printAsymStatsAndStopServices(data);
}

void dsaPrintStats(thread_creation_data_t* data)
{   strcat (PrintAsymmName, "DSA_VERIFY");
    AsymmModSize = data->packetSize * NUM_BITS_IN_BYTE;
    printAsymStatsAndStopServices(data);
}

void ikeRsaPrintStats(thread_creation_data_t* data)
{   strcat (PrintAsymmName, "IKE_RSA_SIMULATION");
    AsymmModSize = data->packetSize;
    printAsymStatsAndStopServices(data);
}

void dhPrintStats(thread_creation_data_t* data)
{
   if(DH_PHASE_1 == ((asym_test_params_t*)data->setupPtr)->phase)
    {
        strcat(PrintAsymmName,"DIFFIE-HELLMAN_PHASE_1");
    }
    else
    {
        strcat(PrintAsymmName,"DIFFIE-HELLMAN_PHASE_2");
    }
    AsymmModSize = data->packetSize*NUM_BITS_IN_BYTE;
    printAsymStatsAndStopServices(data);
}

CpaStatus printRsaPerfData(thread_creation_data_t* data)
{
    strcat(PrintAsymmName,"RSA_DECRYPT");
    AsymmModSize = data->packetSize*NUM_BITS_IN_BYTE;
    return (printAsymStatsAndStopServices(data));
}

CpaStatus printRsaCrtPerfData(thread_creation_data_t* data)
{
    strcat(PrintAsymmName,"RSA_CRT_DECRYPT");
    AsymmModSize =data->packetSize*NUM_BITS_IN_BYTE;
    return (printAsymStatsAndStopServices(data));
}





void printCipherAlg(CpaCySymCipherSetupData cipherSetupData)
{
 switch (cipherSetupData.cipherAlgorithm) {
    case CPA_CY_SYM_CIPHER_NULL: strcat (PrintAlg, "NULL"); break;
    case CPA_CY_SYM_CIPHER_ARC4: strcat (PrintAlg, "ARC4"); break;
    case CPA_CY_SYM_CIPHER_AES_ECB:
    case CPA_CY_SYM_CIPHER_AES_CBC:
    case CPA_CY_SYM_CIPHER_AES_CTR:
    case CPA_CY_SYM_CIPHER_AES_CCM:
    case CPA_CY_SYM_CIPHER_AES_GCM:
         if (cipherSetupData.cipherKeyLenInBytes == KEY_SIZE_128_IN_BYTES) {
            strcat (PrintAlg, "AES128-"); }
         else if(cipherSetupData.cipherKeyLenInBytes == KEY_SIZE_192_IN_BYTES) {
            strcat (PrintAlg, "AES192"); }
         else if(cipherSetupData.cipherKeyLenInBytes == KEY_SIZE_256_IN_BYTES) {
            strcat (PrintAlg, "AES256"); }
         else {
            strcat (PrintAlg, "AES with unknown key size\n"); }

         if (cipherSetupData.cipherAlgorithm == CPA_CY_SYM_CIPHER_AES_ECB) {
            strcat (PrintAlg, "ECB"); }
         if (cipherSetupData.cipherAlgorithm == CPA_CY_SYM_CIPHER_AES_CBC) {
            strcat (PrintAlg, "CBC"); }
         if (cipherSetupData.cipherAlgorithm == CPA_CY_SYM_CIPHER_AES_CTR) {
            strcat (PrintAlg, "CTR"); }
         if (cipherSetupData.cipherAlgorithm == CPA_CY_SYM_CIPHER_AES_CCM) {
            strcat (PrintAlg, "CCM"); }
         if (cipherSetupData.cipherAlgorithm == CPA_CY_SYM_CIPHER_AES_GCM) {
            strcat (PrintAlg, "GCM"); }
         break;
    case CPA_CY_SYM_CIPHER_DES_ECB:     strcat (PrintAlg, "DES-ECB");   break;
    case CPA_CY_SYM_CIPHER_DES_CBC:     strcat (PrintAlg, "DES-CBC");   break;
    case CPA_CY_SYM_CIPHER_3DES_ECB:    strcat (PrintAlg, "3DES-ECB");  break;
    case CPA_CY_SYM_CIPHER_3DES_CBC:    strcat (PrintAlg, "3DES-CBC");  break;
    case CPA_CY_SYM_CIPHER_3DES_CTR:    strcat (PrintAlg, "3DES-CTR");  break;
    case CPA_CY_SYM_CIPHER_KASUMI_F8:   strcat (PrintAlg, "KASUMI_F8"); break;
    case CPA_CY_SYM_CIPHER_SNOW3G_UEA2: strcat (PrintAlg, "SNOW3G_UEA2"); break;
#if CPA_CY_API_VERSION_NUM_MAJOR >= 2
#endif
    case CPA_CY_SYM_CIPHER_AES_F8:
        if (cipherSetupData.cipherKeyLenInBytes == KEY_SIZE_256_IN_BYTES) {
            strcat (PrintAlg, "AES128-");
        } else if(cipherSetupData.cipherKeyLenInBytes == KEY_SIZE_384_IN_BYTES) {
            strcat (PrintAlg, "AES192-");
        } else if(cipherSetupData.cipherKeyLenInBytes == KEY_SIZE_512_IN_BYTES) {
            strcat (PrintAlg, "AES256-");
        } else {
              strcat (PrintAlg, "AES with unknown key size\n");
        }
        strcat (PrintAlg, "F8");
        break;

    default: strcat (PrintAlg, "UNKNOWN_CIPHER\n"); }
}


void printHashAlg(CpaCySymHashSetupData hashSetupData)
{
 if (hashSetupData.hashMode == CPA_CY_SYM_HASH_MODE_AUTH && (hashSetupData.hashAlgorithm != CPA_CY_SYM_HASH_AES_XCBC ||
     hashSetupData.hashAlgorithm != CPA_CY_SYM_HASH_AES_CCM || hashSetupData.hashAlgorithm != CPA_CY_SYM_HASH_AES_GCM ||
     hashSetupData.hashAlgorithm != CPA_CY_SYM_HASH_AES_CMAC)) {
     strcat (PrintAlg, "HMAC-"); }
 switch(hashSetupData.hashAlgorithm) {
    case CPA_CY_SYM_HASH_MD5: strcat (PrintAlg, "MD5"); break;
    case CPA_CY_SYM_HASH_SHA1: strcat (PrintAlg, "SHA1"); break;
    case CPA_CY_SYM_HASH_SHA224: strcat (PrintAlg, "SHA224"); break;
    case CPA_CY_SYM_HASH_SHA256: strcat (PrintAlg, "SHA256"); break;
    case CPA_CY_SYM_HASH_SHA384: strcat (PrintAlg, "SHA384"); break;
    case CPA_CY_SYM_HASH_SHA512: strcat (PrintAlg, "SHA512"); break;
    case CPA_CY_SYM_HASH_AES_XCBC: strcat (PrintAlg, "AES-XCBC"); break;
    case CPA_CY_SYM_HASH_AES_CCM: strcat (PrintAlg, "AES-CCM"); break;
    case CPA_CY_SYM_HASH_AES_GCM: strcat (PrintAlg, "AES-GCM"); break;
    case CPA_CY_SYM_HASH_KASUMI_F9: strcat (PrintAlg, "KASUMI-F9"); break;
    case CPA_CY_SYM_HASH_SNOW3G_UIA2: strcat (PrintAlg, "SNOW3G-UIA2"); break;
    case CPA_CY_SYM_HASH_AES_CMAC: strcat (PrintAlg, "AES-CMAC"); break;
    case CPA_CY_SYM_HASH_AES_GMAC: strcat (PrintAlg, "AES-GMAC"); break;
#if CPA_CY_API_VERSION_NUM_MAJOR >= 2
#endif
    default: strcat (PrintAlg, "UNKNOWN_HASH\n");
    }
}




void printSymTestType(symmetric_test_params_t *setup)
{
 if (setup->setupData.symOperation == CPA_CY_SYM_OP_CIPHER) {
    PRINT("Cipher_Encrypt   ");
    printCipherAlg(setup->setupData.cipherSetupData); }
 else if (setup->setupData.symOperation == CPA_CY_SYM_OP_HASH) {
    PRINT("HASH             ");
    printHashAlg(setup->setupData.hashSetupData); }
 else if (setup->setupData.symOperation == CPA_CY_SYM_OP_ALGORITHM_CHAINING) {
    PRINT("CIPHER+HASH      ");
    printCipherAlg(setup->setupData.cipherSetupData);
    strcat (PrintAlg, "_");
    printHashAlg(setup->setupData.hashSetupData); }

 if (setup->isDpApi) {
    strcat (PrintAlg, "-DP"); }
}


CpaStatus printSymmetricPerfDataAndStopCyService(thread_creation_data_t* data)
{
 CpaStatus status = CPA_STATUS_SUCCESS;
 perf_data_t stats = {0};
 perf_cycles_t numOfCycles = 0;
 Cpa32U responsesPerThread = 0;
 Cpa32U thoughputSize = 0;
 Cpa32U throughput = 0;
 Cpa32U buffersProcessed = 0;
 int i=0;
 int j=0;
 int header = 0;

 symmetric_test_params_t *setup= (symmetric_test_params_t *)data->setupPtr;

 /*stop crypto services if not already stopped, this is the only reasonable * location we can do this as this function is called after all threads are * complete*/
 status = stopCyServices();
 if (CPA_STATUS_SUCCESS != status) {
    /*no need to print error, stopCyServices already does it*/
    return status; }

 /*point perf stats to clear structure*/
 setup->performanceStats = &stats;
 /*get our test bufferSize*/
 stats.averagePacketSizeInBytes = data->packetSize;
 thoughputSize = data->packetSize;
 if (setup->performanceStats->averagePacketSizeInBytes == PACKET_IMIX) {
    thoughputSize = BUFFER_SIZE_1152; }

 /*get the lowest and highest cycle count from the list of threads (all the * same setup executed*/
 getLongestCycleCount(&stats, data->performanceStats, data->numberOfThreads);
 /*accumulate the responses into one perf_data_t structure*/
 for (i=0; i<data->numberOfThreads; i++) {
     if (CPA_STATUS_FAIL == data->performanceStats[i]->threadReturnStatus) {
        /*PRINT("Thread %d Failed\n", i);*/
        return CPA_STATUS_FAIL; }
     stats.responses += data->performanceStats[i]->responses;
     /*is the data was submitted in multiple buffers per list, then the * number of buffers processed is  number of responses multiplied * by the numberOfBuffers*/
     if (setup->isMultiSGL) {
        buffersProcessed += data->performanceStats[i]->responses * setup->numBuffers; }
     else {
        buffersProcessed += data->performanceStats[i]->responses; }
     stats.retries += data->performanceStats[i]->retries;
     stats.numOperations += data->performanceStats[i]->numOperations;
     responsesPerThread = data->performanceStats[i]->responses;
     clearPerfStats(data->performanceStats[i]); }
 /*calc the total cycles of all threads (of one setup type) took to complete * and then print out the data*/
 numOfCycles = (stats.endCyclesTimestamp - stats.startCyclesTimestamp);

 if (signOfLife) {
    printSymTestType(setup);
    PRINT("%-25s\n", PrintAlg);
    PrintAlg[0] = '\0';

    if (setup->performanceStats->averagePacketSizeInBytes == PACKET_IMIX) {
       PRINT("Packet Mix 40%%-64B 20%%-752B 35%% 1504B 5%%-8892B\n"); }
    else {
       PRINT("Packet Size           %u\n", setup->performanceStats->averagePacketSizeInBytes); }
    PRINT("Number of Threads     %u\n", data->numberOfThreads);
    PRINT("Total Submissions     %llu\n", (unsigned long long )stats.numOperations);
    PRINT("Total Responses       %llu\n",(unsigned long long )stats.responses);
    PRINT("Total Retries         %u\n",stats.retries);
    }
 if (!signOfLife) {
    throughput = getThroughput(buffersProcessed, thoughputSize, numOfCycles);
    if (displayOnce == 0)  {
        for (displayOnce=0; displayOnce < 1; displayOnce++) {
       PRINT("Threads:\t\t\t%u\n", data->numberOfThreads);
       PRINT("Submissions:\t\t\t%llu\n", (unsigned long long )stats.numOperations);
       PRINT("CPU Frequency(kHz):\t\t%u\n", sampleCodeGetCpuFreq());
       PRINT("%-17s%-25s","CATEGORY","ALGORITHM");   
#ifdef USER_SPACE
       fflush(stdout);
#endif
if (wirelesDisplay==0)
{
       for (; header < (int)numPacketSizes; header++) {
           if (header == (int)numPacketSizes-1) {
              PRINT("%13s%13s\n", "THROUGHPUT","RETRIES" );
#ifdef USER_SPACE
	      fflush(stdout);
#endif
              }
           else {
              PRINT("%13s%13s", "THROUGHPUT","RETRIES" );
#ifdef USER_SPACE
	      fflush(stdout);
#endif
              } }
       PRINT("%-42s",",");

       for (j = 0; j < (int)numPacketSizes; j++) {
           PRINT("%13d%13d", packetSizes[j], packetSizes[j]);
#ifdef USER_SPACE
	   fflush(stdout);
#endif
           }
       PRINT("\n");
}
else {
        for (; header < (int)numWirelessPacketSizes; header++) {
           if (header == (int)numWirelessPacketSizes-1) {
              PRINT("%13s%13s\n", "THROUGHPUT","RETRIES" );
#ifdef USER_SPACE
	      fflush(stdout);
#endif
              }
           else {
              PRINT("%13s%13s", "THROUGHPUT","RETRIES" );
#ifdef USER_SPACE
	      fflush(stdout);
#endif
              } }
       PRINT("%-42s",",");

       for (j = 0; j < (int)numWirelessPacketSizes; j++) {
           PRINT("%13d%13d", wirelessPacketSizes[j], wirelessPacketSizes[j]);
#ifdef USER_SPACE
	   fflush(stdout);
#endif
           }
       PRINT("\n");
       
}}}


if (wirelesDisplay== 0)
{

    if (displayAlgName < (int)numPacketSizes-1) {
       if (displayAlgName == 0) {
          printSymTestType(setup);
          PRINT("%-25s", PrintAlg);
          PrintAlg[0] = '\0';
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u",stats.retries);
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          displayAlgName++; }
       else {
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u",stats.retries);
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          displayAlgName++; } }


    else {
       if (displayAlgName == 0) {
          printSymTestType(setup);
          PRINT("%-25s", PrintAlg);
          PrintAlg[0] = '\0';
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u\n",stats.retries); 
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          }
       else {
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u\n",stats.retries);
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          displayAlgName=0; } }
}
else {

    if (displayAlgName < (int)numWirelessPacketSizes-1) {
       if (displayAlgName == 0) {
          printSymTestType(setup);
          PRINT("%-25s", PrintAlg);
          PrintAlg[0] = '\0';
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u",stats.retries);
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          displayAlgName++; }
       else {
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u",stats.retries);
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          displayAlgName++; } }


    else {
       if (displayAlgName == 0) {
          printSymTestType(setup);
          PRINT("%-25s", PrintAlg);
          PrintAlg[0] = '\0';
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u\n",stats.retries); 
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          }
       else {
          if (responsesPerThread < THROUGHPUT_MIN_SUBMISSIONS) {
             PRINT("Need to submit >= %u per thread for accurate throughput\n", THROUGHPUT_MIN_SUBMISSIONS); }
          else {
             PRINT("%13u", throughput); }                                            //Throughput
          PRINT("%13u\n",stats.retries);
#ifdef USER_SPACE
	  fflush(stdout);
#endif                                              //Total Retries
          displayAlgName=0; } }
}

 
#ifdef LATENCY_CODE
    do_div(stats.aveLatency, data->numberOfThreads);
    PRINT("AverageLatency      %llu\n",stats.aveLatency);
#endif
    } 
 return CPA_STATUS_SUCCESS;
}

#endif //NEWDISPLAY
