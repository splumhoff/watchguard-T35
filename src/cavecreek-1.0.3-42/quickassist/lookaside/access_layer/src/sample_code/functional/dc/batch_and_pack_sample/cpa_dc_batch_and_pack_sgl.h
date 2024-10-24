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
 * @file cpa_dc_batch_and_Pack_sgl.h
 *
 * @defgroup sample Utils Macro and inline function definitions
 *
 * @ingroup sampleCode
 *
 * @description
 *
 ***************************************************************************/

#ifndef CPA_DC_BATCH_AND_PACK_SGL_H
#define CPA_DC_BATCH_AND_PACK_SGL_H

#include "cpa.h"

CpaStatus
dcBatchAndPackSample(const char **paramList, const Cpa32U numParams);

CpaStatus
dcBuildBnpBufferList(CpaBufferList **testBufferList, Cpa32U numberChains,
                  Cpa32U sizeOfPkt, Cpa32U metaSize);

void dcFreeBnpBufferList(CpaBufferList **testBufferList);

CpaStatus
dcGetMetaAndBuildBufferList(CpaInstanceHandle dc_instance, CpaBufferList **pBufferList,
          const Cpa32U numberOfFlatBuffers, const Cpa32U bufferSize);


/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Functions for SGL handling
 *
 ******************************************************************************/

#endif /* CPA_DC_BATCH_AND_PACK_SGL_H */
