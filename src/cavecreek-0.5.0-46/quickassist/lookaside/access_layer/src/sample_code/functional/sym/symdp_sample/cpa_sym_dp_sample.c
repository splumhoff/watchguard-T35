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

/*
 * This is sample code that demonstrates usage of the symmetric DP API, and
 * specifically using this API to perform a "chained" cipher and hash
 * operation.  It encrypts some sample text using the 3DES algorithm in
 * CBC mode, and then performs an MD5 hash on the ciphertext.
 */

#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym_dp.h"
#include "icp_sal_poll.h"
#include "cpa_sample_utils.h"

/* The digest length must be less than or equal to md5 digest 
   length (16) for this example */
#define DIGEST_LENGTH 16

extern int gDebugParam;

/* Triple DES key, 192 bits long */
static Cpa8U sampleCipherKey[] = {
        0xEE,0xE2,0x7B,0x5B,0x10,0xFD,0xD2,0x58,0x49,0x77,0xF1,0x22,
        0xD7,0x1B,0xA4,0xCA,0xEC,0xBD,0x15,0xE2,0x52,0x6A,0x21,0x0B
};

/* Initialization vector */
static Cpa8U sampleCipherIv[] = {
        0x7E,0x9B,0x4C,0x1D,0x82,0x4A,0xC5,0xDF
};

/* Source data to encrypt */
static Cpa8U sampleAlgChainingSrc[] = {
        0xD7,0x1B,0xA4,0xCA,0xEC,0xBD,0x15,0xE2,0x52,0x6A,0x21,0x0B,
        0x81,0x77,0x0C,0x90,0x68,0xF6,0x86,0x50,0xC6,0x2C,0x6E,0xED,
        0x2F,0x68,0x39,0x71,0x75,0x1D,0x94,0xF9,0x0B,0x21,0x39,0x06,
        0xBE,0x20,0x94,0xC3,0x43,0x4F,0x92,0xC9,0x07,0xAA,0xFE,0x7F,
        0xCF,0x05,0x28,0x6B,0x82,0xC4,0xD7,0x5E,0xF3,0xC7,0x74,0x68,
        0xCF,0x05,0x28,0x6B,0x82,0xC4,0xD7,0x5E,0xF3,0xC7,0x74,0x68,
        0x80,0x8B,0x28,0x8D,0xCD,0xCA,0x94,0xB8,0xF5,0x66,0x0C,0x00,
        0x5C,0x69,0xFC,0xE8,0x7F,0x0D,0x81,0x97,0x48,0xC3,0x6D,0x24
};

/* Expected output of the encryption operation with the specified
 * cipher (CPA_CY_SYM_CIPHER_3DES_CBC), key (sampleCipherKey) and
 * initialization vector (sampleCipherIv) */
static Cpa8U expectedOutput[] = {
        0x35,0x0C,0x46,0xF8,0xFE,0x13,0x8A,0x7C,0x9B,0x66,0x83,0x5F,
        0x94,0xDC,0x4F,0x96,0x66,0x56,0x35,0xC3,0xFA,0xFD,0x51,0xA1,
        0xC9,0x3B,0xAF,0x06,0x2A,0xA9,0x54,0x0D,0xF1,0x0B,0xBB,0xB1,
        0x27,0x15,0x9D,0xD2,0x08,0xAC,0xF0,0x92,0x47,0x19,0xE2,0xC1,
        0x47,0xAC,0x34,0x30,0x8C,0x95,0x1B,0x14,0xD4,0x71,0x37,0x4B,
        0x50,0xCB,0x73,0xAA,0x4F,0x98,0x36,0xF1,0x97,0xE2,0x8C,0x37,
        0x6C,0x44,0xC2,0xFD,0xAD,0xE4,0xF5,0x56,0x62,0x92,0xEF,0x84,
        0x9E,0x33,0x0D,0x5B,0x34,0x27,0xA0,0x2B,0x9B,0x7C,0xE7,0x8A,
        /* Digest */
        0x6E,0x2F,0x42,0xCB,0x3A,0x0A,0x94,0x21,0x7E,0x0D,0x95,0x7D,
        0xD6,0x43,0xBC,0xEE
};

CpaStatus
symDpSample(void);


/*
 * Callback function
 *
 * This function is "called back" (invoked by the implementation of
 * the API) when the operation has completed.  
 *  
 */
static void
symDpCallback(CpaCySymDpOpData *pOpData,
        CpaStatus status,
        CpaBoolean verifyResult)
{
    PRINT_DBG("Callback called with status = %d.\n", status);
    pOpData->pCallbackTag = (void *) 1; 
}

/*
 * Perform an algorithm chaining operation (cipher + hash)
 */
static CpaStatus
symDpPerformOp(CpaInstanceHandle cyInstHandle, CpaCySymSessionCtx sessionCtx)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    CpaCySymDpOpData *pOpData = NULL;
    Cpa32U bufferSize = sizeof(sampleAlgChainingSrc) + DIGEST_LENGTH;
    Cpa8U  *pSrcBuffer = NULL;
    Cpa8U  *pIvBuffer = NULL;

    /* Allocate Src buffer */
    status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate IV buffer */
        status = PHYS_CONTIG_ALLOC(&pIvBuffer, sizeof(sampleCipherIv));
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* copy source into buffer */
        memcpy(pSrcBuffer, sampleAlgChainingSrc, sizeof(sampleAlgChainingSrc));

        /* copy IV into buffer */
        memcpy(pIvBuffer, sampleCipherIv, sizeof(sampleCipherIv));

        /* Allocate memory for operational data. Note this needs to be
         * 8-byte aligned, contiguous, resident in DMA-accessible
         * memory. 
         */
        status = PHYS_CONTIG_ALLOC_ALIGNED(&pOpData, sizeof(CpaCySymDpOpData), 8);
    }

    if (CPA_STATUS_SUCCESS == status) {
        /** Populate the structure containing the operational data that is
         * needed to run the algorithm 
         */
//<snippet name="opDataDp">
        pOpData->cryptoStartSrcOffsetInBytes = 0;
        pOpData->messageLenToCipherInBytes = sizeof(sampleAlgChainingSrc);
        pOpData->iv = sampleVirtToPhys(pIvBuffer); 
        pOpData->hashStartSrcOffsetInBytes = 0;
        pOpData->messageLenToHashInBytes = sizeof(sampleAlgChainingSrc);
        /* Even though MAC follows immediately after the region to hash
           digestIsAppended is set to false in this case to workaround
           errata number IXA00378322 */
        pOpData->digestResult = 
              sampleVirtToPhys(pSrcBuffer)+sizeof(sampleAlgChainingSrc);
        pOpData->instanceHandle = cyInstHandle;
        pOpData->sessionCtx = sessionCtx;
        pOpData->ivLenInBytes = sizeof(sampleCipherIv);
        pOpData->srcBuffer = sampleVirtToPhys(pSrcBuffer);
        pOpData->srcBufferLen = bufferSize;
        pOpData->dstBuffer = sampleVirtToPhys(pSrcBuffer);
        pOpData->dstBufferLen = bufferSize;
        pOpData->thisPhys = sampleVirtToPhys(pOpData);
        pOpData->pCallbackTag = (void *) 0;
//</snippet>
    }

    if (CPA_STATUS_SUCCESS == status)
    {

        PRINT_DBG("cpaCySymDpEnqueueOp\n");
        /** Enqueue symmetric operation */
//<snippet name="enqueue">
        status = cpaCySymDpEnqueueOp(pOpData,
                CPA_FALSE);
//</snippet>
        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("cpaCySymDpEnqueueOp failed. (status = %d)\n", status);
        }
        else
        {

        /* Can now enqueue other requests before submitting all requests to 
         * the hardware. The cost of submitting the request to the hardware is
         * then amortized across all enqueued requests. 
         * In this simple example we have only 1 request to send
         */

           PRINT_DBG("cpaCySymDpPerformOpNow\n");

        /** Submit all enqueued symmetric operations to the hardware */  
//<snippet name="perform">
           status = cpaCySymDpPerformOpNow(cyInstHandle);
//</snippet>
           if (CPA_STATUS_SUCCESS != status)
           {
               PRINT_ERR("cpaCySymDpPerformOpNow failed. (status = %d)\n", status);
           }
        }
     }
        /* Can now enqueue more operations and/or do other work while
         * hardware processes the request. 
         * In this simple example we have no other work to do 
         * */
    
     if (CPA_STATUS_SUCCESS == status)
     {
       /* Poll for responses. 
        * Polling functions are implementation specific */
        do{
           status = icp_sal_CyPollDpInstance(cyInstHandle, 1);
        }while(((CPA_STATUS_SUCCESS == status) || (CPA_STATUS_RETRY == status))
                && (pOpData->pCallbackTag == (void *) 0)); 
     }

     /* Check result */
     if (CPA_STATUS_SUCCESS == status)
     {
        if (0 == memcmp(pSrcBuffer, expectedOutput, bufferSize))
        {
             PRINT_DBG("Output matches expected output\n");
         }
         else
         {
             PRINT_ERR("Output does not match expected output\n");
             status = CPA_STATUS_FAIL;
         }
     }

    PHYS_CONTIG_FREE(pSrcBuffer);
    PHYS_CONTIG_FREE(pIvBuffer);
    PHYS_CONTIG_FREE(pOpData);

    return status;
}

CpaStatus
symDpSample(void)
{
    CpaStatus status = CPA_STATUS_FAIL;
    CpaCySymSessionCtx sessionCtx = NULL;
    Cpa32U sessionCtxSize = 0;
    CpaInstanceHandle cyInstHandle = NULL;
    CpaCySymSessionSetupData sessionSetupData = {0};

    /*
     * In this simplified version of instance discovery, we discover
     * exactly one instance of a crypto service.
     */
    sampleCyGetInstance(&cyInstHandle);
    if (cyInstHandle == NULL)
    {
        return CPA_STATUS_FAIL;
    }

    /* Start Cryptographic component */
    PRINT_DBG("cpaCyStartInstance\n");
    status = cpaCyStartInstance(cyInstHandle);

    if (CPA_STATUS_SUCCESS == status)
    {

       /*
        * Set the address translation function for the instance
        */
        status = cpaCySetAddressTranslation(cyInstHandle, sampleVirtToPhys);
    }

    if (CPA_STATUS_SUCCESS == status) 
    {
        /* Register callback function for the instance */
//<snippet name="regCb">
       status = cpaCySymDpRegCbFunc(cyInstHandle, symDpCallback);
//</snippet>
    }

    if (CPA_STATUS_SUCCESS == status) {
        /* populate symmetric session data structure */
//<snippet name="initSession">
        sessionSetupData.sessionPriority =  CPA_CY_PRIORITY_HIGH;
        sessionSetupData.symOperation = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
        sessionSetupData.algChainOrder =
                                    CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;

        sessionSetupData.cipherSetupData.cipherAlgorithm =
                                    CPA_CY_SYM_CIPHER_3DES_CBC;
        sessionSetupData.cipherSetupData.pCipherKey = sampleCipherKey;
        sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                                    sizeof(sampleCipherKey);
        sessionSetupData.cipherSetupData.cipherDirection =
                                    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;

        sessionSetupData.hashSetupData.hashAlgorithm =  CPA_CY_SYM_HASH_MD5;
        sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
        sessionSetupData.hashSetupData.digestResultLenInBytes = DIGEST_LENGTH;
        sessionSetupData.hashSetupData.authModeSetupData.authKey =
                                    sampleCipherKey;
        sessionSetupData.hashSetupData.authModeSetupData.authKeyLenInBytes =
                                    sizeof(sampleCipherKey);

        /* Even though MAC follows immediately after the region to hash
           digestIsAppended is set to false in this case to workaround
           errata number IXA00378322 */        
        sessionSetupData.digestIsAppended = CPA_FALSE; 
        sessionSetupData.verifyDigest = CPA_FALSE;

        /* Determine size of session context to allocate */
        PRINT_DBG("cpaCySymDpSessionCtxGetSize\n");
        status = cpaCySymDpSessionCtxGetSize(cyInstHandle,
                    &sessionSetupData, &sessionCtxSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate session context */
        status = PHYS_CONTIG_ALLOC(&sessionCtx, sessionCtxSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Initialize the session */
        PRINT_DBG("cpaCySymDpInitSession\n");
        status = cpaCySymDpInitSession(cyInstHandle,
                    &sessionSetupData, sessionCtx);
    }
//</snippet>

    if (CPA_STATUS_SUCCESS == status) {
        CpaStatus sessionStatus = CPA_STATUS_SUCCESS;

        /* Perform algchaining operation */
        status = symDpPerformOp(cyInstHandle, sessionCtx);

        /* Remove the session - session init has already succeeded */
        PRINT_DBG("cpaCySymDpRemoveSession\n");

//<snippet name="removeSession">
        sessionStatus = cpaCySymDpRemoveSession(
                            cyInstHandle, sessionCtx);
//</snippet>

        /* maintain status of remove session only when status of all operations
         * before it are successful. */
        if (CPA_STATUS_SUCCESS == status)
        {
            status = sessionStatus;
        }
    }

    /* Clean up */

    /* Free session Context */
    PHYS_CONTIG_FREE(sessionCtx);

    PRINT_DBG("cpaCyStopInstance\n");
    cpaCyStopInstance(cyInstHandle);

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

