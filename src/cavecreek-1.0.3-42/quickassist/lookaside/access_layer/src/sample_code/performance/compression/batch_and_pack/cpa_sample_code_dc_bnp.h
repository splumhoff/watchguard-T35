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

/**
 *****************************************************************************
 * @file cpa_sample_code_dc_bnp.h
 *
 * @description
 * Contains function prototypes and structures needs for the bacth and pack
 * sample code 
 *
 ***************************************************************************/
#ifndef CPA_SAMPLE_CODE_DC_BNP_H_
#define CPA_SAMPLE_CODE_DC_BNP_H_

#include "cpa.h"
#include "cpa_dc.h"
#include "cpa_dc_dp.h"
#include "cpa_sample_code_utils_common.h"
#include "cpa_sample_code_dc_perf.h"
#include "cpa_sample_code_dc_utils.h"
#include "cpa_sample_code_crypto_utils.h"

/**
 *  *******************************************************************************
 *  * @ingroup sampleUtils
 *  *      Macro from the Mem_Alloc_Contig function
 *  *
 *  ******************************************************************************/
#define PHYS_CONTIG_ALLOC(ppMemAddr, sizeBytes) \
    Mem_Alloc_Contig((void *)(ppMemAddr), (sizeBytes), 1)

#define BNP_CHECK_NULL_PARAM(param)                               \
if (NULL == param)                                                \
{                                                                 \
    PRINT_ERR("%s(): invalid param: %s\n", __FUNCTION__, #param); \
    return CPA_STATUS_INVALID_PARAM;                              \
}                                                                 \

#define _4K_PAGE_SIZE (4 * 1024)
#define MAX_CORPUS_FILE_PATH_LEN (100)

#ifdef __x86_64__
#define SAMPLE_ADDR_LEN uint64_t
#else
#define SAMPLE_ADDR_LEN uint32_t
#endif

#define OPERATIONS_POLLING_INTERVAL   (10)
#define BUFFER_SIZE_32K               (1024* 32)
#define BUFFER_SIZE_8K                (1024 * 8)
#define BUFFER_SIZE                   BUFFER_SIZE_8K
#define DOUBLE_SUBMISSIONS            (2)
#define DEFAULT_FIRST_SKIP_OFFSET     20
#define DEFAULT_SKIP_LENGTH           30
#define SRC_METADATA                  'X'
#define DEST_METADATA                 'A'
#define STATIC                        static
#define NO_OVERFLOWN_JOB              (-1)
#ifdef KERNEL_SPACE
#define CRC32_XOR_VALUE         (0xffffffff)
#endif

extern Cpa16U numInst_g;

typedef enum
file_state_e{SKIP_STATE = 1,
    FILL_STATE,
    FILL_STATE_NO_STRIDE
}file_state_t;

typedef struct sgl_buffer_index
{
    Cpa32U bufferIndex;
    Cpa32U offsetInBuffer;
}sgl_buffer_index_t;

typedef struct stv_Bnp_s
{
    Cpa32U numBuffersPerInputBufferList;
    Cpa32U numBuffersPerOutputBufferList;
    Cpa32U numJobsPerBatchRequest;
    Cpa32U resetSessionOnJobn;
    CpaDcSkipMode inputDcSkipMode;
    CpaDcSkipMode outputDcSkipMode;
    Cpa32U inputSkipLength;
    Cpa32U outputSkipLength;
    Cpa32U inputFlatBufferSize;
    Cpa32U outputFlatBufferSize;
    compression_test_params_t compressTestParams;

}stv_Bnp_t;

CpaStatus 
getCompressionInstanceMapping(void);

CpaStatus 
setupDcBnPBenchmarkPerf(corpus_type_t corpusType,
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
                        CpaDcSessionState state);

CpaStatus 
setupDcBnPStatefulPerf(CpaDcCompLvl compLevel,
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
                       CpaDcSessionState  state);

CpaStatus 
setupDcBnPTest(stv_Bnp_t *bnp_params);

CpaStatus 
dcBnPCreateSession(stv_Bnp_t bnp_params,
                   CpaDcCallbackFn dcCbFn,
                   CpaDcSessionHandle *pSessionHandle);

void 
dcBnpPerformance(single_thread_test_data_t* testBnpSetup);

CpaStatus 
dcBnpPerform(stv_Bnp_t* bnpSetup);

CpaStatus 
performBnpCompress(stv_Bnp_t* bnpSetup,
                   CpaBufferList **srcBuffListArray,
                   CpaBufferList **dstBuffListArray,
                   CpaDcRqResults **cmpResult,
                   CpaDcCallbackFn dcCbFn);

CpaStatus 
dcPrintBnpStats(thread_creation_data_t* data);

char* 
populateopData(Cpa32U buffSize, 
               corpus_type_t corpusType);

extern CpaStatus deflate_init(struct z_stream_s *stream);
extern CpaStatus deflate_compress(struct z_stream_s *stream,
        const Cpa8U *src,
        Cpa32U slen,
        Cpa8U *dst,
        Cpa32U dlen);
extern void deflate_destroy(struct z_stream_s *stream);
extern CpaStatus inflate_init(z_stream *stream, CpaDcSessionState sessState);
extern CpaStatus inflate_decompress(z_stream *stream,
        const Cpa8U *src, Cpa32U slen, Cpa8U *dst, Cpa32U dlen,
        CpaDcSessionState sessState);


extern void inflate_destroy(struct z_stream_s *stream);

void freeBnpBuffers(CpaBufferList **pBuffListArray,
        Cpa32U numberOfFiles,
        compression_test_params_t* setup);

#endif    /*CPA_SAMPLE_CODE_DC_BNP_H_*/

