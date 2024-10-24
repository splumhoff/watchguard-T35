#ifndef E_QAT_H
#define E_QAT_H

#include <openssl/sha.h>

#include "cpa.h"
#include "cpa_types.h"
#include "cpa_cy_sym.h"

typedef struct qat_chained_ctx_t
{
    /*While decryption is done in SW the first elements
     *of this structure need to be the elements present
     *in EVP_AES_HMAC_SHA1 defined in 
     * crypto/evp/e_aes_cbc_hmac_sha1.c
     */
    AES_KEY             ks;
    SHA_CTX             head,tail,md;
    size_t              payload_length; /* AAD length in decrypt case */
    union {
        unsigned int    tls_ver;
        unsigned char   tls_aad[16];    /* 13 used */
    } aux;

    /* QAT Session Params */
    CpaInstanceHandle instanceHandle;
    CpaCySymSessionSetupData* session_data;
    CpaCySymSessionCtx qat_ctx;
    int initParamsSet;
    int initHmacKeySet;

    /* QAT Op Params */
    CpaCySymOpData    OpData;
    CpaBufferList     srcBufferList;
    CpaBufferList     dstBufferList;
    CpaFlatBuffer     srcFlatBuffer[2];
    CpaFlatBuffer     dstFlatBuffer[2];

    /* Crypto */
    unsigned char *hmac_key; //Why do we need this it should be in the context
    Cpa8U*   pIv;
    SHA_CTX  key_wrap;

    /* TLS SSL proto */
    /*  Reintroduce when QAT decryption added size_t       
     * payload_length;
     */
    Cpa8U*	 tls_virt_hdr;
    unsigned int tls_version;

} qat_chained_ctx;

#endif //E_QAT_H
