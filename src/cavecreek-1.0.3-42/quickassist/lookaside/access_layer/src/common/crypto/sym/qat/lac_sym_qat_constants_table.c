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
 ***************************************************************************
 * @file lac_sym_qat_constants_table.c
 *
 * @ingroup LacSymQat
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

#include "cpa.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#include "lac_common.h"
#include "icp_qat_fw_la.h"
#include "lac_log.h"
#include "lac_mem.h"
#include "icp_platform.h"
#include "sal_string_parse.h"
#include "lac_sal_types_crypto.h"

STATIC uint8_t icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_DELIMITER][ICP_QAT_HW_CIPHER_MODE_DELIMITER][2][2] ; /* IA version */
STATIC uint8_t icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_DELIMITER][ICP_QAT_HW_AUTH_MODE_DELIMITER][2]; /* IA version */

#define ICP_QAT_HW_FILL_LOOKUP_TBLS { \
\
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_DES][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 9; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_DES][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_DECRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 10; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_DES][ICP_QAT_HW_CIPHER_CBC_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 11; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_DES][ICP_QAT_HW_CIPHER_CBC_MODE][ICP_QAT_HW_CIPHER_DECRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 12; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_DES][ICP_QAT_HW_CIPHER_CTR_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 13; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 14; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_KEY_CONVERT] = 15; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_DECRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 16; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_DECRYPT][ICP_QAT_HW_CIPHER_KEY_CONVERT] = 17; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_CBC_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 18; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_CBC_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_KEY_CONVERT] = 19; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_CBC_MODE][ICP_QAT_HW_CIPHER_DECRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 20; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_CBC_MODE][ICP_QAT_HW_CIPHER_DECRYPT][ICP_QAT_HW_CIPHER_KEY_CONVERT] = 21; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_CTR_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 22; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_AES128][ICP_QAT_HW_CIPHER_F8_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 23; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_ARC4][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_NO_CONVERT] = 24; \
icp_qat_hw_cipher_lookup_tbl[ICP_QAT_HW_CIPHER_ALGO_ARC4][ICP_QAT_HW_CIPHER_ECB_MODE][ICP_QAT_HW_CIPHER_ENCRYPT][ICP_QAT_HW_CIPHER_KEY_CONVERT] = 25; \
\
\
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_MD5][ICP_QAT_HW_AUTH_MODE0][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 37; \
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_SHA1][ICP_QAT_HW_AUTH_MODE0][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 41; \
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_SHA1][ICP_QAT_HW_AUTH_MODE1][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 46; \
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_SHA224][ICP_QAT_HW_AUTH_MODE0][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 48; \
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_SHA256][ICP_QAT_HW_AUTH_MODE0][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 54; \
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_SHA384][ICP_QAT_HW_AUTH_MODE0][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 60; \
icp_qat_hw_auth_lookup_tbl[ICP_QAT_HW_AUTH_ALGO_SHA512][ICP_QAT_HW_AUTH_MODE0][ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED] = 70; \
}

/**
 *****************************************************************************
 * @ingroup LacSymQat
 *      LacSymQat_ConstantsInitLookupTables
 *
 *
 *****************************************************************************/
void
LacSymQat_ConstantsInitLookupTables(CpaInstanceHandle instanceHandle,
                                    icp_accel_dev_t* device)
{
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;
    LAC_ENSURE_NOT_NULL(pService);
    LAC_ENSURE_NOT_NULL(device);

    /* Note the global tables are initialised first, then copied
     * to the service which probably seems like a waste of memory
     * and processing cycles as the global tables are never needed again
     * but this allows use of the ICP_QAT_HW_FILL_LOOKUP_TBLS macro
     * supplied by FW without modification */

    /* First fill the global lookup tables with zeroes. */
    ICP_MEMSET(icp_qat_hw_cipher_lookup_tbl,
                    0, sizeof(icp_qat_hw_cipher_lookup_tbl));
    ICP_MEMSET(icp_qat_hw_auth_lookup_tbl,
                    0, sizeof(icp_qat_hw_auth_lookup_tbl));

    /* Override lookup tables with the offsets into the SHRAM table
     * for supported algorithms/modes */
   ICP_QAT_HW_FILL_LOOKUP_TBLS;

   /* Copy the global tables to the service instance */
   ICP_MEMCPY(pService->constantsLookupTables.cipher_offset,
           icp_qat_hw_cipher_lookup_tbl,
           sizeof(pService->constantsLookupTables.cipher_offset));
   ICP_MEMCPY(pService->constantsLookupTables.auth_offset,
           icp_qat_hw_auth_lookup_tbl,
           sizeof(pService->constantsLookupTables.auth_offset));
}

/**
 *****************************************************************************
 * @ingroup LacSymQat
 *      LacSymQat_ConstantsGetCipherOffset
 *
 *
 *****************************************************************************/
void
LacSymQat_ConstantsGetCipherOffset(CpaInstanceHandle instanceHandle,
                                   uint8_t algo,
                                   uint8_t mode,
                                   uint8_t direction,
                                   uint8_t convert,
                                   uint8_t *poffset )
{
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;

    LAC_ENSURE_RANGE(algo,0,ICP_QAT_HW_CIPHER_DELIMITER);
    LAC_ENSURE_RANGE(mode,0,ICP_QAT_HW_CIPHER_MODE_DELIMITER);
    LAC_ENSURE_RANGE(direction,0,2);
    LAC_ENSURE_RANGE(convert,0,2);
    LAC_ENSURE_NOT_NULL(poffset);
    LAC_ENSURE_NOT_NULL(pService);

    *poffset = pService->constantsLookupTables.cipher_offset[algo][mode][direction][convert];

    LAC_LOG_DEBUG2("cipher constants offset (quad_words) = %d (0x%x)",
                                            *poffset, *poffset );

}

/**
 *****************************************************************************
 * @ingroup LacSymQat
 *      LacSymQat_ConstantsGetAuthOffset
 *
 *
 *****************************************************************************/
void
LacSymQat_ConstantsGetAuthOffset(CpaInstanceHandle instanceHandle,
                                    uint8_t algo,
                                    uint8_t mode,
                                    uint8_t nested,
                                    uint8_t *poffset )
{
    sal_crypto_service_t* pService = (sal_crypto_service_t*) instanceHandle;

    LAC_ENSURE_RANGE(algo,0,ICP_QAT_HW_AUTH_ALGO_DELIMITER);
    LAC_ENSURE_RANGE(mode,0,ICP_QAT_HW_AUTH_MODE_DELIMITER);
    LAC_ENSURE_RANGE(nested,0,2);
    LAC_ENSURE_NOT_NULL(poffset);
    LAC_ENSURE_NOT_NULL(pService);


   *poffset = pService->constantsLookupTables.auth_offset[algo][mode][nested];

   LAC_LOG_DEBUG2("auth constants offset (quad_words) = %d (0x%x)",
                                           *poffset, *poffset );

}
