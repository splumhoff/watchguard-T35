
/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

/*****************************************************************************
 * @file e_qat.c
 *
 * This file provides a OpenSSL engine for the  quick assist API
 *
 *****************************************************************************/

/* macros defined to allow use of the cpu get and set affinity functions */
#define _GNU_SOURCE
#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/obj_mac.h>
#include <openssl/aes.h>
#include <openssl/rc4.h>
#include <openssl/x509.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/tls1.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>

#include "qae_mem_utils.h"
#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_types.h"
#include "cpa_cy_sym.h"
#include "cpa_cy_rsa.h"
#include "cpa_cy_dsa.h"
#include "cpa_cy_dh.h"
#include "cpa_cy_ln.h"
#include "cpa_cy_common.h"
#include "cpa_cy_im.h"
#include "qat_mem.h"
#include "icp_sal_user.h"
#include "icp_sal_poll.h"
#include "e_qat.h"

#define AES_BLOCK_SIZE      16
#define AES_IV_LEN          16
#define AES_KEY_SIZE_256    32
#define AES_KEY_SIZE_192    24
#define AES_KEY_SIZE_128    16
#define AES_FLAGS           0

#define RC4_BLOCK_SIZE      1
#define RC4_IV_LEN          0
#define RC4_KEY_SIZE        16
#define RC4_FLAGS           0

#define DES3_BLOCK_SIZE     8
#define DES3_IV_LEN         8
#define DES3_KEY_SIZE_192   24
#define DES3_FLAGS          0

#define DES_BLOCK_SIZE      8
#define DES_IV_LEN          8
#define DES_KEY_SIZE_56     8
#define DES_FLAGS           0

#define SHA1_SIZE           20
#define SHA1_BLOCK_SIZE     8
#define SHA1_FLAGS          0

#define SHA256_SIZE         32
#define SHA256_BLOCK_SIZE   32
#define SHA256_FLAGS        0

#define SHA512_SIZE         64
#define SHA512_BLOCK_SIZE   64
#define SHA512_FLAGS        0

#define MD5_SIZE            16
#define MD5_BLOCK_SIZE      64
#define MD5_FLAGS           0

#define CTX_NOT_CLEAN       255

typedef enum _CipherType
{
    AES128 = 1,
    AES192,
    AES256,
    RC_4,
    DES,
    DES3

}CipherType;

typedef enum _DigestType
{
    SHA_1 = 1,
    SHA_256,
    SHA_512,
    MD_5

}DigestType;

#ifndef QAT_MAX_DIGEST_CHAIN_LENGTH
#define QAT_MAX_DIGEST_CHAIN_LENGTH 1000000000
#endif

#define QAT_POLL_CORE_AFFINITY
#define QAT_WARN

//#define QAT_DEBUG

#ifdef QAT_DEBUG
#define DEBUG(...) printf(__VA_ARGS__)
#define DUMPL(var,p,l) hexDump(__func__,var,p,l);
#define DUMPREQ(inst, cb, opD, sess, src, dst) dumpRequest(inst, cb, opD, sess, src, dst);
#else
#define DEBUG(...)
#define DUMPL(...)
#define DUMPREQ(...)
#endif

/* warning message for qat engine and cpa function */
#ifdef QAT_WARN
#define WARN(...) printf (__VA_ARGS__)
#else
#define WARN(...)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define QAT_DEV "/dev/qat_mem"
#define POLL_PERIOD_IN_NS 10000

CpaInstanceHandle *qatInstanceHandles = NULL;
static pthread_key_t qatInstanceForThread;
pthread_t *icp_polling_threads;
static int keep_polling = 1;
Cpa16U numInstances = 0;
int qatPerformOpRetries = 0;
static int currInst = 0;

char *ICPConfigSectionName_start = "SSL";

/* Use the nanosleep version of sendpoll */
/*#define USE_NS_SENDPOLL*/

#define SAL_STOP_DELAY 1

/******************************************************************************
* function:
*         incr_curr_inst(void)
*
* description:
*   Increment the logical Cy instance number to use for the next operation.
*
******************************************************************************/
static inline void incr_curr_inst(void)
{
    currInst = (currInst + 1) % numInstances;
}

/******************************************************************************
* function:
*         get_next_inst(void)
*
* description:
*   Return the next instance handle to use for an operation.
*
******************************************************************************/
static CpaInstanceHandle get_next_inst(void)
{
    CpaInstanceHandle instanceHandle;

    if ((instanceHandle = pthread_getspecific(qatInstanceForThread)) == NULL)
    {
        instanceHandle = qatInstanceHandles[currInst];
        incr_curr_inst();
    }
    return instanceHandle;
}

/******************************************************************************
* function:
*         qat_set_instance_for_gthread(ENGINE *e)
*
* @param instanceNum [IN] - logical instance number
*
* description:
*   Bind the current thread to a particular logical Cy instance. Note that if
*   instanceNum is greater than the number of configured instances, the
*   modulus operation is used.
*
******************************************************************************/
void qat_set_instance_for_thread(int instanceNum)
{
    int rc;

    if ((rc =
         pthread_setspecific(qatInstanceForThread,
                             qatInstanceHandles[instanceNum %
                                                numInstances])) != 0)
    {
        fprintf(stderr, "pthread_setspecific: %s\n", strerror(rc));
        return;
    }
}

/* Struct for tracking threaded QAT operation completion. */
struct op_done
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int flag;
    CpaBoolean verifyResult;
};

/* Callback to indicate QAT completion. */
static void qat_callbackFn(void *callbackTag,
                           CpaStatus status,
                           const CpaCySymOp operationType,
                           void *pOpData,
                           CpaBufferList * pDstBuffer, CpaBoolean verifyResult);

/* Callback to indicate QAT completion of RSA. */
void qat_rsaCallbackFn(void *pCallbackTag,
                       CpaStatus status, void *pOpData, CpaFlatBuffer * pOut);

/* Qat engine id declaration */
static const char *engine_qat_id = "qat";
static const char *engine_qat_name =
    "Reference implementation of QAT crypto engine";

/* Common Qat engine cipher function declarations */
static int cipher_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                     const unsigned char *iv, int enc, CipherType cipherType);

static int common_cipher_cleanup(EVP_CIPHER_CTX * ctx);

/* Common Qat engine digest function declarations */
static int digest_init(EVP_MD_CTX * ctx);

static int digest_update(EVP_MD_CTX * ctx, const void *data, size_t count);

static int digest_final(EVP_MD_CTX * ctx, unsigned char *md);

static int digest_cleanup(EVP_MD_CTX * ctx);

/* Qat engine AES256 Cipher function declarations */
static int qat_aes_256_cbc_init(EVP_CIPHER_CTX * ctx,
                                const unsigned char *key,
                                const unsigned char *iv, int enc);

static int qat_aes_256_cbc_do_cipher(EVP_CIPHER_CTX * ctx,
                                     unsigned char *out,
                                     const unsigned char *in, size_t inl);

/* Qat engine AES192 Cipher function declarations */
static int qat_aes_192_cbc_init(EVP_CIPHER_CTX * ctx,
                                const unsigned char *key,
                                const unsigned char *iv, int enc);

static int qat_aes_192_cbc_do_cipher(EVP_CIPHER_CTX * ctx,
                                     unsigned char *out,
                                     const unsigned char *in, size_t inl);

/* Qat engine AES128 Cipher function declarations */
static int qat_aes_128_cbc_init(EVP_CIPHER_CTX * ctx,
                                const unsigned char *key,
                                const unsigned char *iv, int enc);

static int qat_aes_128_cbc_do_cipher(EVP_CIPHER_CTX * ctx,
                                     unsigned char *out,
                                     const unsigned char *in, size_t inl);

/* Qat engine RC4 Cipher function declarations */
static int qat_rc4_init(EVP_CIPHER_CTX * ctx,
                        const unsigned char *key,
                        const unsigned char *iv, int enc);

static int qat_rc4_do_cipher(EVP_CIPHER_CTX * ctx,
                             unsigned char *out,
                             const unsigned char *in, size_t inl);

/* Qat engine 3DES Cipher function declarations */
static int qat_des_ede3_cbc_init(EVP_CIPHER_CTX * ctx,
                                 const unsigned char *key,
                                 const unsigned char *iv, int enc);

static int qat_des_ede3_cbc_do_cipher(EVP_CIPHER_CTX * ctx,
                                      unsigned char *out,
                                      const unsigned char *in, size_t inl);

/* Qat engine DES Cipher function declarations */
static int qat_des_cbc_init(EVP_CIPHER_CTX * ctx,
                            const unsigned char *key,
                            const unsigned char *iv, int enc);

static int qat_des_cbc_do_cipher(EVP_CIPHER_CTX * ctx,
                                 unsigned char *out,
                                 const unsigned char *in, size_t inl);

/* Qat engine AES-SHA1 function declaration */
static int qat_aes_cbc_hmac_sha1_init(EVP_CIPHER_CTX *ctx,
            			      const unsigned char *inkey,
            			      const unsigned char *iv, int enc);
static int qat_aes_cbc_hmac_sha1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
              				const unsigned char *in, size_t len);
static int qat_aes_cbc_hmac_sha1_cleanup(EVP_CIPHER_CTX *ctx);
static int qat_aes_cbc_hmac_sha1_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr);

/* Qat engine SHA1 digest function declaration */
static int qat_sha1_init(EVP_MD_CTX * ctx);
static int qat_sha1_update(EVP_MD_CTX * ctx, const void *data, size_t count);
static int qat_sha1_copy(EVP_MD_CTX * ctx_out, const EVP_MD_CTX *ctx_in);
static int qat_sha1_final(EVP_MD_CTX * ctx, unsigned char *md);

/* Qat engine SHA256 digest function declaration */
static int qat_sha256_init(EVP_MD_CTX * ctx);
static int qat_sha256_update(EVP_MD_CTX * ctx, const void *data, size_t count);
static int qat_sha256_copy(EVP_MD_CTX * ctx_out, const EVP_MD_CTX *ctx_in);
static int qat_sha256_final(EVP_MD_CTX * ctx, unsigned char *md);

/* Qat engine SHA512 digest function declaration */
static int qat_sha512_init(EVP_MD_CTX * ctx);
static int qat_sha512_update(EVP_MD_CTX * ctx, const void *data, size_t count);
static int qat_sha512_copy(EVP_MD_CTX * ctx_out, const EVP_MD_CTX *ctx_in);
static int qat_sha512_final(EVP_MD_CTX * ctx, unsigned char *md);

/* Qat engine MD5 digest function declaration */
static int qat_md5_init(EVP_MD_CTX * ctx);
static int qat_md5_update(EVP_MD_CTX * ctx, const void *data, size_t count);
static int qat_md5_copy(EVP_MD_CTX * ctx_out, const EVP_MD_CTX *ctx_in);
static int qat_md5_final(EVP_MD_CTX * ctx, unsigned char *md);
/* Qat engine RSA methods declaration */
static int qat_rsa_priv_enc(int flen, const unsigned char *from,
                            unsigned char *to, RSA * rsa, int padding);
static int qat_rsa_pub_dec(int flen, const unsigned char *from,
                           unsigned char *to, RSA * rsa, int padding);
static int qat_rsa_priv_dec(int flen, const unsigned char *from,
                            unsigned char *to, RSA * rsa, int padding);
static int qat_rsa_mod_exp(BIGNUM * r0, const BIGNUM * I, RSA * rsa,
                           BN_CTX * ctx);
static int qat_bn_mod_exp(BIGNUM * r, const BIGNUM * a, const BIGNUM * p,
                          const BIGNUM * m, BN_CTX * ctx, BN_MONT_CTX * m_ctx);
static int qat_rsa_pub_enc(int flen, const unsigned char *from,
                           unsigned char *to, RSA * rsa, int padding);

/* Qat engine DSA methods declaration */
DSA_SIG *qat_dsa_do_sign(const unsigned char *dgst, int dlen, DSA * dsa);
int qat_dsa_sign_setup(DSA * dsa, BN_CTX * ctx_in, BIGNUM ** kinvp,
                       BIGNUM ** rp);
static int qat_dsa_do_verify(const unsigned char *dgst, int dgst_len,
                             DSA_SIG * sig, DSA * dsa);

#if OPENSSL_VERSION_NUMBER < 0x01000000
static int qat_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1, BIGNUM *p1,
		           BIGNUM *a2, BIGNUM *p2, BIGNUM *m, BN_CTX *ctx,
                           BN_MONT_CTX *in_mont);
#endif
static int qat_mod_exp_dsa(DSA * dsa, BIGNUM * r, BIGNUM * a, const BIGNUM * p,
                           const BIGNUM * m, BN_CTX * ctx, BN_MONT_CTX * m_ctx);

/* Qat engine DH methods declaration */
static int qat_mod_exp_dh(const DH * dh, BIGNUM * r, const BIGNUM * a,
                          const BIGNUM * p, const BIGNUM * m, BN_CTX * ctx,
                          BN_MONT_CTX * m_ctx);
int qat_dh_generate_params(DH * dh, int prime_len, int generator,
                           BN_GENCB * cb);
int qat_dh_generate_key(DH * dh);
int qat_dh_compute_key(unsigned char *key, const BIGNUM * pub_key, DH * dh);

static CpaPhysicalAddr realVirtualToPhysical(void *virtualAddr);

#ifdef USE_NS_SENDPOLL
int ns_handler(struct timespec* reqTime);
#endif

/* qat_buffer structure for partial hash */
typedef struct qat_buffer_t
{
    struct qat_buffer_t *next;  /* next buffer in the list */
    void *data;                 /* point to data buffer    */
    int len;                    /* lenght of data          */
} qat_buffer;

/* Qat ctx structure declaration */
typedef struct qat_ctx_t
{
    int paramNID;               /* algorithm nid */
    CpaCySymSessionCtx ctx;     /* session context */
    unsigned char hashResult[SHA512_SIZE];
    				/* hash digest result */
    int enc;                    /* encryption flag */
    int init;			/* has been initialised */
    CpaInstanceHandle instanceHandle;

    /* the memory for the private meta data must be allocated as contiguous
     * memory. The cpaCyBufferListGetMetaSize() will return the size (in
     * bytes) for memory allocation routine to allocate the private meta data
     * memory 
     */
    void *srcPrivateMetaData;   /* meta data pointer */
    void *dstPrivateMetaData;   /* meta data pointer */

    /*  For partial operations, we maintain a linked list of buffers
     *  to be processed in the final function.
     */

    qat_buffer *first;          /* first buffer pointer for partial op */
    qat_buffer *last;           /* last buffer pointe for partial op */
    int buff_count;		/* buffer count */
    int buff_total;		/* total number of buffer */

} qat_ctx;

/* Qat cipher AES256 function structure declaration */
EVP_CIPHER qat_aes_256_cbc = {
    NID_aes_256_cbc,            /* nid */
    AES_BLOCK_SIZE,             /* block_size */
    AES_KEY_SIZE_256,           /* key_size in Bytes - defined above */
    AES_IV_LEN,                 /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE,           /* flags defined above */
    qat_aes_256_cbc_init,
    qat_aes_256_cbc_do_cipher,
    common_cipher_cleanup,
    sizeof(qat_ctx),            /* ctx_size */
    NULL,                       /* qat_aes_256_cbc_set_asn1_parameters */
    NULL,                       /* qat_aes_256_cbc_get_asn1_parameters */
    NULL,                       /* qat_aes_256_cbc_ctrl */
    NULL                        /* qat_aes_256_cbc_app_data */
};

/* Qat cipher AES192 function structure declaration */
EVP_CIPHER qat_aes_192_cbc = {
    NID_aes_192_cbc,            /* nid */
    AES_BLOCK_SIZE,             /* block_size */
    AES_KEY_SIZE_192,           /* key_size in Bytes - defined above */
    AES_IV_LEN,                 /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE,          /* flags defined above */
    qat_aes_192_cbc_init,
    qat_aes_192_cbc_do_cipher,
    common_cipher_cleanup,
    sizeof(qat_ctx),            /* ctx_size */
    NULL,                       /* qat_aes_192_cbc_set_asn1_parameters */
    NULL,                       /* qat_aes_192_cbc_get_asn1_parameters */
    NULL,                       /* qat_aes_192_cbc_ctrl */
    NULL                        /* qat_aes_192_cbc_app_data */
};

/* Qat cipher AES128 function structure declaration */
EVP_CIPHER qat_aes_128_cbc = {
    NID_aes_128_cbc,            /* nid */
    AES_BLOCK_SIZE,             /* block_size */
    AES_KEY_SIZE_128,           /* key_size in Bytes - defined above */
    AES_IV_LEN,                 /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE,          /* flags defined above */
    qat_aes_128_cbc_init,
    qat_aes_128_cbc_do_cipher,
    common_cipher_cleanup,
    sizeof(qat_ctx),            /* ctx_size */
    NULL,                       /* qat_aes_128_cbc_set_asn1_parameters */
    NULL,                       /* qat_aes_128_cbc_get_asn1_parameters */
    NULL,                       /* qat_aes_128_cbc_ctrl */
    NULL                        /* qat_aes_128_cbc_app_data */
};

#ifdef NID_aes_128_cbc_hmac_sha1
/* Qat cipher AES128-SHA1 function structure declaration */
EVP_CIPHER qat_aes_128_cbc_hmac_sha1 = {
    NID_aes_128_cbc_hmac_sha1,  /* nid */
    AES_BLOCK_SIZE,             /* block_size */
    AES_KEY_SIZE_128,           /* key_size in Bytes - defined above */
    AES_IV_LEN,                 /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE|EVP_CIPH_FLAG_DEFAULT_ASN1|EVP_CIPH_FLAG_AEAD_CIPHER,
    qat_aes_cbc_hmac_sha1_init,
    qat_aes_cbc_hmac_sha1_cipher,
    qat_aes_cbc_hmac_sha1_cleanup,
    sizeof(qat_chained_ctx),  /* ctx_size */
    EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_set_asn1_iv,
    EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_get_asn1_iv,
    qat_aes_cbc_hmac_sha1_ctrl,
    NULL 
};
#endif

#ifdef NID_aes_256_cbc_hmac_sha1
/* Qat cipher AES256-SHA1 function structure declaration */
EVP_CIPHER qat_aes_256_cbc_hmac_sha1 = {
    NID_aes_256_cbc_hmac_sha1,  /* nid */
    AES_BLOCK_SIZE,             /* block_size */
    AES_KEY_SIZE_256,           /* key_size in Bytes - defined above */
    AES_IV_LEN,                 /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE|EVP_CIPH_FLAG_DEFAULT_ASN1|EVP_CIPH_FLAG_AEAD_CIPHER, 
    qat_aes_cbc_hmac_sha1_init,
    qat_aes_cbc_hmac_sha1_cipher,
    qat_aes_cbc_hmac_sha1_cleanup,
    sizeof(qat_chained_ctx),  /* ctx_size */
    EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_set_asn1_iv,
    EVP_CIPH_FLAG_DEFAULT_ASN1?NULL:EVP_CIPHER_get_asn1_iv,
    qat_aes_cbc_hmac_sha1_ctrl,
    NULL
};
#endif

/* Qat cipher RC4 function structure declaration */
EVP_CIPHER qat_rc4 = {
    NID_rc4,                    /* nid */
    RC4_BLOCK_SIZE,             /* block_size */
    RC4_KEY_SIZE,               /* key_size in Bytes - defined above */
    RC4_IV_LEN,                 /* iv_len in Bytes - defined above */
    RC4_FLAGS,                  /* flags defined above */
    qat_rc4_init,
    qat_rc4_do_cipher,
    common_cipher_cleanup,
    sizeof(qat_ctx),            /* ctx_size */
    NULL,                       /* qat_rc4_set_asn1_parameters */
    NULL,                       /* qat_rc4_get_asn1_parameters */
    NULL,                       /* qat_rc4_ctrl */
    NULL                        /* qat_rc4_app_data */
};

/* Qat cipher 3DES function structure declaration */
EVP_CIPHER qat_des_ede3_cbc = {
    NID_des_ede3_cbc,           /* nid */
    DES3_BLOCK_SIZE,            /* block_size */
    DES3_KEY_SIZE_192,          /* key_size in Bytes - defined above */
    DES3_IV_LEN,                /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE,           /* flags defined above */
    qat_des_ede3_cbc_init,
    qat_des_ede3_cbc_do_cipher,
    common_cipher_cleanup,
    sizeof(qat_ctx),            /* ctx_size */
    NULL,                       /* qat_des_ede3_cbc_set_asn1_parameters */
    NULL,                       /* qat_des_ede3_cbc_get_asn1_parameters */
    NULL,                       /* qat_des_ede3_cbc_ctrl */
    NULL                        /* qat_des_ede3_cbc_app_data */
};

/* Qat cipher DES function structure declaration */
EVP_CIPHER qat_des_cbc = {
    NID_des_cbc,                /* nid */
    DES_BLOCK_SIZE,             /* block_size */
    DES_KEY_SIZE_56,            /* key_size in Bytes - defined above */
    DES_IV_LEN,                 /* iv_len in Bytes - defined above */
    EVP_CIPH_CBC_MODE,          /* flags defined above */
    qat_des_cbc_init,
    qat_des_cbc_do_cipher,
    common_cipher_cleanup,
    sizeof(qat_ctx),            /* ctx_size */
    NULL,                       /* qat_des_cbc_set_asn1_parameters */
    NULL,                       /* qat_des_cbc_get_asn1_parameters */
    NULL,                       /* qat_des_cbc_ctrl */
    NULL                        /* qat_des_cbc_app_data */
};

/* Qat digest SHA1 function structure declaration */
EVP_MD qat_sha1 = {
    NID_sha1,                   /* nid */
    NID_undef,
    SHA1_SIZE,                  /* output size */
    SHA1_FLAGS,                 /* flags defined above */
    qat_sha1_init,
    qat_sha1_update,
    qat_sha1_final,
    qat_sha1_copy,
    digest_cleanup,
    NULL,                       /* qat_sha1_sign */
    NULL,                       /* qat_sha1_verify */
   {0, 0, 0, 0, 0}
    ,                           /* EVP pkey */
    SHA1_BLOCK_SIZE,            /* block size */
    sizeof(qat_ctx),            /* ctx_size */
#if OPENSSL_VERSION_NUMBER > 0x01000000
    NULL                        /* qat_sha1_ctrl */
#endif
};

/* Qat digest SHA256 function structure declaration */
EVP_MD qat_sha256 = {
    NID_sha256,                 /* nid */
    NID_undef,
    SHA256_SIZE,                /* output size */
    SHA256_FLAGS,               /* flags defined above */
    qat_sha256_init,
    qat_sha256_update,
    qat_sha256_final,
    qat_sha256_copy,
    digest_cleanup,
    NULL,                       /* qat_sha256_sign */
    NULL,                       /* qat_sha256_verify */
    {0, 0, 0, 0, 0}
    ,                           /* EVP pkey */
    SHA256_BLOCK_SIZE,          /* block size */
    sizeof(qat_ctx),            /* ctx_size */
#if OPENSSL_VERSION_NUMBER > 0x01000000
    NULL                        /* qat_sha256_ctrl */
#endif
};

/* Qat digest SHA512 function structure declaration */
EVP_MD qat_sha512 = {
    NID_sha512,                 /* nid */
    NID_undef,
    SHA512_SIZE,                /* output size */
#if OPENSSL_VERSION_NUMBER > 0x01000000
    EVP_MD_FLAG_PKEY_METHOD_SIGNATURE|EVP_MD_FLAG_DIGALGID_ABSENT, 
#else
    SHA512_FLAGS,
#endif
    qat_sha512_init,
    qat_sha512_update,
    qat_sha512_final,
    qat_sha512_copy,
    digest_cleanup,
    NULL,                       /* qat_sha512_sign */
    NULL,                       /* qat_sha512_verify */
    {0, 0, 0, 0, 0}
    ,                           /* EVP pkey */
    SHA512_BLOCK_SIZE,          /* block size */
    sizeof(qat_ctx),            /* ctx_size */
#if OPENSSL_VERSION_NUMBER > 0x01000000
    NULL                        /* qat_sha512_ctrl */
#endif
};

/* Qat digest MD5 function structure declaration */
EVP_MD qat_md5 = {
    NID_md5,                    /* nid */
    NID_undef,
    MD5_SIZE,                   /* output size */
    MD5_FLAGS,                  /* flags defined above */
    qat_md5_init,
    qat_md5_update,
    qat_md5_final,
    qat_md5_copy,
    digest_cleanup,
    NULL,                       /* qat_md5_sign */
    NULL,                       /* qat_md5_verify */
    {0, 0, 0, 0, 0}
    ,                           /* EVP pkey */
    MD5_BLOCK_SIZE,             /* block size */
    sizeof(qat_ctx),            /* ctx_size */
#if OPENSSL_VERSION_NUMBER > 0x01000000
    NULL                        /* qat_md5_ctrl */
#endif
};

/* Qat Symmetric cipher function register */
static int qat_cipher_nids[] = {
#ifndef QAT_ZERO_COPY_MODE
    NID_aes_256_cbc,
    NID_aes_192_cbc,
    NID_aes_128_cbc,
    NID_rc4,
    NID_des_ede3_cbc,
    NID_des_cbc,
#endif
#ifdef NID_aes_128_cbc_hmac_sha1
    NID_aes_128_cbc_hmac_sha1,
#endif
#ifdef NID_aes_256_cbc_hmac_sha1
    NID_aes_256_cbc_hmac_sha1,
#endif
};

/* Qat digest function register */
static int qat_digest_nids[] = {
    NID_sha1,
    NID_sha256,
    NID_sha512,
    NID_md5,
};

/* Qat RSA method structure declaration. */
static RSA_METHOD qat_rsa_method = {
    "QAT RSA method",           /* name */
    qat_rsa_pub_enc,            /* rsa_pub_enc */
    qat_rsa_pub_dec,            /* rsa_pub_dec */
    qat_rsa_priv_enc,           /* rsa_priv_enc */
    qat_rsa_priv_dec,           /* rsa_priv_dec */
    qat_rsa_mod_exp,            /* rsa_mod_exp */
    qat_bn_mod_exp,             /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL,                       /* app_data */
    NULL,                       /* rsa_sign */
    NULL                        /* rsa_verify */
};

/* Qat DSA method structure declaration. */
static DSA_METHOD qat_dsa_method = {
    "QAT DSA method",           /* name */
    qat_dsa_do_sign,            /* do_sign */
    qat_dsa_sign_setup,         /* sign_setup */
    qat_dsa_do_verify,          /* do_verify */
#if OPENSSL_VERSION_NUMBER < 0x01000000
    /*
     * openssl-0.9.8w will segfault without this as dsa_do_verify doesnt
     * check to see if this is null.  This is fixed in openssl-1.0.1c
    */
    qat_dsa_mod_exp,            /* mod_exp */
#else
    NULL,                       /* mod_exp */
#endif
    qat_mod_exp_dsa,            /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL                        /* app_data */
};

/* Qat DH method structure declaration. */
static DH_METHOD qat_dh_method = {
    "QAT DH method",            /* name */
    qat_dh_generate_key,        /* generate_key */
    qat_dh_compute_key,         /* compute_key */
    qat_mod_exp_dh,             /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL,                       /* app_data */
    NULL                        /* generate_params */
};

/* OpenSSL  structs for default function pointers. */
static const DSA_METHOD *openssl_dsa_method;
static const DH_METHOD *openssl_dh_method;
static const RSA_METHOD *openssl_rsa_method;

static CpaVirtualToPhysical myVirtualToPhysical = realVirtualToPhysical;
static int qat_inited = 0;
static int zero_copy_memory_mode = 0;

/* Qat engine function declarations */
static int qat_ciphers(ENGINE * e,
                       const EVP_CIPHER ** cipher, const int **nids, int nid);

static int qat_digests(ENGINE * e,
                       const EVP_MD ** digests, const int **nids, int nid);

static void hexDump(const char *func, const char *var, const unsigned char p[],
                    int l)
{
    int i;

    printf("%s: %s", func, var);
    for (i = 0; i < l; i++)
    {
        if (i % 16 == 0)
            putchar('\n');
        else if (i % 8 == 0)
            fputs("- ", stdout);
        printf("%02x ", p[i]);
    }
    putchar('\n');
}

#ifdef QAT_DEBUG
static void dumpRequest(const CpaInstanceHandle instanceHandle,
                        void *pCallbackTag,
                        const CpaCySymOpData * pOpData,
                        const CpaCySymSessionSetupData * sessionData,
                        const CpaBufferList * pSrcBuffer,
                        CpaBufferList * pDstBuffer)
{
    unsigned int index = 0;
    struct op_done *opDoneCB = (struct op_done*)pCallbackTag;
 
    printf("\nInstance Handle:    %p\n", instanceHandle);
    printf("Callback Ptr:       %p\n", opDoneCB);
    printf("OpData->packetType:        %u\n", pOpData->packetType);
    hexDump(__func__, "Cipher Key:      ",
            sessionData->cipherSetupData.pCipherKey,
            sessionData->cipherSetupData.cipherKeyLenInBytes);
    printf("Cipher Key Len:     %u\n",
            sessionData->cipherSetupData.cipherKeyLenInBytes);
    hexDump(__func__, "Cipher IV:               ",
            pOpData->pIv, pOpData->ivLenInBytes);
    hexDump(__func__, "MAC Key:                 ",
            sessionData->hashSetupData.authModeSetupData.authKey,
            sessionData->hashSetupData.authModeSetupData.authKeyLenInBytes);
    for(index = 0; index < pSrcBuffer->numBuffers; index++)
    {
        printf("pSrcBuffer->pBuffers[%u].pData:                   %p\n", index, pSrcBuffer->pBuffers[index].pData);
        hexDump(__func__, " ",
            pSrcBuffer->pBuffers[index].pData,
            pSrcBuffer->pBuffers[index].dataLenInBytes);
        printf("pSrcBuffer->pBuffers[%u].dataLenInBytes:          %u\n\n",
           index, pSrcBuffer->pBuffers[index].dataLenInBytes);
    }
 
    for(index = 0; index < pDstBuffer->numBuffers; index++)
    {
        printf("pDstBuffer->pBuffers[%u].pData:                  %p\n", index, pDstBuffer->pBuffers[index].pData);
        hexDump(__func__, " ",
            pDstBuffer->pBuffers[index].pData,  
            pDstBuffer->pBuffers[index].dataLenInBytes);
        printf("pDstBuffer->pBuffers[%u].dataLenInBytes:         %u\n\n",
            index, pDstBuffer->pBuffers[index].dataLenInBytes);
    }
 
    printf("sessionData->cipherSetupData.cipherAlgorithm:	%u\n", sessionData->cipherSetupData.cipherAlgorithm);
    printf("sessionData->cipherSetupData.cipherDirection:	%u\n", sessionData->cipherSetupData.cipherDirection);
    printf("sessionData->algChainOrder:            		%u\n", sessionData->algChainOrder);
    printf("pOpData->cryptoStartSrcOffsetInBytes:		%u\n", pOpData->cryptoStartSrcOffsetInBytes);
    printf("pOpData->messageLenToCipherInBytes:  		%u\n", pOpData->messageLenToCipherInBytes);
    printf("sessionData->hashSetupData.hashAlgorithm:		%u\n", sessionData->hashSetupData.hashAlgorithm);
    printf("sessionData->hashSetupData.hashMode:	        %u\n", sessionData->hashSetupData.hashMode);
    printf("pOpData->hashStartSrcOffsetInBytes:         	%u\n", pOpData->hashStartSrcOffsetInBytes);
    printf("sessionData->hashSetupData.digestResultLenInBytes:	%u\n", sessionData->hashSetupData.digestResultLenInBytes);
    printf("pOpData->messageLenToHashInBytes:			%u\n", pOpData->messageLenToHashInBytes);
    printf("pOpData->pDigestResult:				%p\n", pOpData->pDigestResult);
    printf("sessionData->verifyDigest:				%u\n", sessionData->verifyDigest);
    printf("opDoneCB->verifyResult:				%u\n", opDoneCB->verifyResult);
}
#endif



/******************************************************************************
* function:
*         initOpDone(struct op_done *opDone)
*
* @param opDone [IN] - pointer to op done callback structure
*
* description:
*   Initialise the QAT operation "done" callback structure.
*
******************************************************************************/
static void initOpDone(struct op_done *opDone)
{
    pthread_mutex_init(&(opDone->mutex), NULL);
    pthread_cond_init(&(opDone->cond), NULL);
    opDone->flag = 0;
    opDone->verifyResult = CPA_FALSE;
}

/******************************************************************************
* function:
*         cleanupOpDone(struct op_done *opDone)
*
* @param opDone [IN] - pointer to op done callback structure
*
* description:
*   Cleanup the thread and mutex used in the QAT operation "done" callback.
*
******************************************************************************/
static void cleanupOpDone(struct op_done *opDone)
{
    pthread_mutex_destroy(&(opDone->mutex));
    pthread_cond_destroy(&(opDone->cond));
}

/******************************************************************************
* function:
*         waitForOpToComplete(struct op_done *opDone)
*
* @param opdone [IN] - pointer to op done callback structure
*
* description:
*   Wait on a mutex lock with a timeout for cpaCySymPerformOp to complete.
*
******************************************************************************/
static void waitForOpToComplete(struct op_done *opDone)
{
    struct timespec ts;
    int rc;

    pthread_mutex_lock(&(opDone->mutex));

    while (!opDone->flag)
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        rc = pthread_cond_timedwait(&(opDone->cond), &(opDone->mutex), &ts);
        if (rc != 0)
            WARN("pthread_cond_timedwait: %s\n", strerror(rc));
    }
    pthread_mutex_unlock(&(opDone->mutex));
}

/******************************************************************************
* function:
*         qat_callbackFn(void *callbackTag, CpaStatus status,
*                        const CpaCySymOp operationType, void *pOpData,
*                        CpaBufferList * pDstBuffer, CpaBoolean verifyResult)
*

* @param pCallbackTag  [IN] -  Opaque value provided by user while making
*                              individual function call. Cast to op_done.
* @param status        [IN] -  Status of the operation.
* @param operationType [IN] -  Identifies the operation type requested.
* @param pOpData       [IN] -  Pointer to structure with input parameters.
* @param pDstBuffer    [IN] -  Destination buffer to hold the data output.
* @param verifyResult  [IN] -  Used to verify digest result.
*
* description:
*   Callback function used by cpaCySymPerformOp to indicate completion.
*
******************************************************************************/
static void qat_callbackFn(void *callbackTag, CpaStatus status,
                           const CpaCySymOp operationType, void *pOpData,
                           CpaBufferList * pDstBuffer, CpaBoolean verifyResult)
{
    struct op_done *opDone = (struct op_done *)callbackTag;

    DEBUG("e_qat.%s: status %d verifyResult %d\n", __func__, status,
          verifyResult);
    pthread_mutex_lock(&(opDone->mutex));
    opDone->flag = 1;
    opDone->verifyResult = verifyResult;
    pthread_cond_signal(&(opDone->cond));
    pthread_mutex_unlock(&(opDone->mutex));
}

/******************************************************************************
* function:
*         qat_rsaCallbackFn(void *pCallbackTag, CpaStatus status,
*                           void *pOpData, CpaFlatBuffer * pOut)
*
* @param instanceHandle [IN]  - Instance handle.
* @param pRsaEncryptCb  [IN]  - Pointer to callback function to be invoked
*                               when the operation is complete.
* @param pCallbackTag   [IN]  - Opaque User Data for this specific call. Will
*                               be returned unchanged in the callback.
* @param pEncryptOpData [IN]  - Structure containing all the data needed to
*                               perform the RSA encryption operation.
* @param pOutputData    [Out] - Pointer to structure into which the result of
*                               the RSA encryption primitive is written.
* description:
*   Callback function used by cpaCyRsaEncrypt to indicate completion.
*   Calls back to qat_callbackFn().
*
******************************************************************************/
void qat_rsaCallbackFn(void *pCallbackTag, CpaStatus status, void *pOpData,
                       CpaFlatBuffer * pOut)
{
    qat_callbackFn(pCallbackTag, status, CPA_CY_SYM_OP_CIPHER, pOpData,
                   NULL, CPA_FALSE);
}

/******************************************************************************
* function:
*         realVirtualToPhysical(void *virtualAddr)
*
* @param virtualAddr [IN] - Virtual address.
*
* description:
*   Translates virtual address to hardware physical address. See the qae_mem
*   module for more details. The virtual to physical translator is required
*   by the QAT hardware to map from virtual addresses to physical locations
*   in pinned memory.
*
*   This function is designed to work with the allocator defined in
*   qae_mem_utils.c and qat_mem/qat_mem.c
*
******************************************************************************/
static CpaPhysicalAddr realVirtualToPhysical(void *virtualAddr)
{
    return qaeMemV2P(virtualAddr);
}

/******************************************************************************
* function:
*         setMyVirtualToPhysical(CpaVirtualToPhysical fp)
*
* @param CpaVirtualToPhysical [IN] - Function pointer to translation function
*
* description:
*   External API to allow users to specify their own virtual to physical
*   address translation function.
*
******************************************************************************/
void setMyVirtualToPhysical(CpaVirtualToPhysical fp)
{
    /*  If user specifies a V2P function then 
     *  the user is taking responsibility for 
     *  allocating and freeing pinned memory
     *  so we switch to zero_copy_memory mode
     */
    if (!qat_inited)
    {
        myVirtualToPhysical = fp;
        zero_copy_memory_mode = 1;
    }
    else
        WARN("%s: can't set virtual to physical translation function after initialisation\n", __func__);
}

/******************************************************************************
* function:
*         copyAllocPinnedMemory(void *ptr, int size)
*
* @param ptr [IN]  - Pointer to data to be copied
* @param size [IN] - Size of data to be copied
*
* description:
*   Internal API to allocate a pinned memory
*   buffer and copy data to it.  
*
* @retval NULL      failed to allocate memory
* @retval non-NULL  pointer to allocated memory
******************************************************************************/
static void *copyAllocPinnedMemory(void *ptr, size_t size)
{
    void *nptr;
    
    if ((nptr = qaePinnedMemAlloc (size)) == NULL)
    {
        WARN("%s: pinned memory allocation failure\n", __func__);
        return NULL;
    }

    memcpy (nptr, ptr, size);
    return nptr;
}

/******************************************************************************
* function:
*         copyFreePinnedMemory(void *kptr, void *uptr, int size)
*
* @param uptr [IN] - Pointer to user data
* @param kptr [IN] - Pointer to pinned memory to be copied
* @param size [IN] - Size of data to be copied
*
* description:
*   Internal API to allocate a pinned memory
*   buffer and copy data to it.
*
******************************************************************************/
static void copyFreePinnedMemory(void *uptr, void *kptr, int size)
{
    memcpy (uptr, kptr, size);
    qaeMemFree (kptr);
}

/******************************************************************************
* function:
*         CpaStatus myPerformOp(const CpaInstanceHandle  instanceHandle,
*                     void *                     pCallbackTag,
*                     const CpaCySymOpData      *pOpData,
*                     const CpaBufferList       *pSrcBuffer,
*                     CpaBufferList             *pDstBuffer,
*                     CpaBoolean                *pVerifyResult)
*
* @param ih [IN] - Instance handle
* @param instanceHandle [IN]  - Instance handle
* @param pCallbackTag   [IN]  - Pointer to op_done struct
* @param pOpData        [IN]  - Operation parameters
* @param pSrcBuffer     [IN]  - Source buffer list
* @param pDstBuffer     [OUT] - Destination buffer list
* @param pVerifyResult  [OUT] - Whether hash verified or not
*
* description:
*   Wrapper around cpaCySymPerformOp which handles retries for us.
*
******************************************************************************/
CpaStatus myPerformOp(const CpaInstanceHandle instanceHandle,
                      void *pCallbackTag,
                      const CpaCySymOpData * pOpData,
                      const CpaBufferList * pSrcBuffer,
                      CpaBufferList * pDstBuffer, CpaBoolean * pVerifyResult)
{
    CpaStatus status;

    do
    {
        status = cpaCySymPerformOp(instanceHandle,
                                   pCallbackTag,
                                   pOpData,
                                   pSrcBuffer, pDstBuffer, pVerifyResult);
        if (status == CPA_STATUS_RETRY)
        {
            qatPerformOpRetries++;
            pthread_yield();
        }
    }
    while (status == CPA_STATUS_RETRY);
    return status;
}

#ifndef USE_NS_SENDPOLL
/******************************************************************************
* function:
*         void *sendPoll(void *ih)
*
* @param ih [IN] - Instance handle
*
* description:
*   Poll the QAT instances every 2 microseconds.
*
******************************************************************************/
static void *sendPoll(void *ih)
{

    CpaStatus status = 0;
    CpaInstanceHandle instanceHandle;

    instanceHandle = (CpaInstanceHandle) ih;
    
    while (keep_polling)
    {
        /* Poll for 0 means process all packets on the ET ring */
        status = icp_sal_CyPollInstance(instanceHandle, 0);

        if (CPA_STATUS_SUCCESS == status)
        {
            /* Do nothing */
        }
        else if (CPA_STATUS_RETRY == status)
        {
            pthread_yield();
        }
        else
        {
            WARN("WARNING icp_sal_CyPollInstance returned status %d\n", status);
        }
    }

    return NULL;
}
#else
/******************************************************************************
* function:
*         void *sendPoll_ns(void *ih)
*
* @param ih [IN] - Instance handle
*
* description:
*   Poll the QAT instances (nanosleep version)
*       NB: Delay in this function is set by default at runtime by pulling a value
*       in nsecs from /etc/send_poll_interval. If no such file is available,
*       default falls through to POLL_PERIOD_IN_NS.
*
******************************************************************************/
static void *sendPoll_ns(void *ih)
{
    CpaStatus status = 0;
    CpaInstanceHandle instanceHandle;
    struct timespec reqTime;
    struct timespec interUpdate;
    FILE *fp;
    char interBuf[32] = {'0'};
    size_t bytesRead = 0;
    long int lastUpdate = 0;
    int pollFilePres = 1;

    instanceHandle = (CpaInstanceHandle) ih;
    reqTime.tv_sec = 0;
    reqTime.tv_nsec = POLL_PERIOD_IN_NS;
 
    fp = fopen("/etc/send_poll_interval", "r");
    if(fp == NULL)
    {
        pollFilePres = 0;
    }
 
    if(1 == pollFilePres)
    {
        bytesRead = fread(interBuf, 1, sizeof(interBuf), fp);
 
        if(bytesRead > 0)
        {
            reqTime.tv_nsec = atoi(interBuf);
            DEBUG("sendPoll: Interval is set to %i", reqTime.tv_nsec);
            fseek(fp, 0L, SEEK_SET);
        }
        clock_gettime(CLOCK_MONOTONIC, &interUpdate);
        lastUpdate = interUpdate.tv_sec;
    }
 
    while (keep_polling)
    {
        /* Poll for 0 means process all packets on the ET ring */
        status = icp_sal_CyPollInstance(instanceHandle, 0);
 
        if (CPA_STATUS_SUCCESS == status || CPA_STATUS_RETRY == status)
        {
            /* Do nothing */
        }
        else
        {
            WARN("WARNING icp_sal_CyPollInstance returned status %d\n", status);
        }
 
        ns_handler(&reqTime);
               
        if(1 == pollFilePres)
        {
            clock_gettime(CLOCK_MONOTONIC, &interUpdate);
            if(interUpdate.tv_sec > (lastUpdate + 300))//use 300 (5 minutes)
            {
                bytesRead = fread(interBuf, 1, sizeof(interBuf), fp);
                if(bytesRead > 0)
                {
                    reqTime.tv_nsec = atoi(interBuf);
                    DEBUG("sendPoll: Interval is set dynamically to %li\n", reqTime.tv_nsec);
                    fseek(fp, 0L, SEEK_SET);
                }
                lastUpdate = interUpdate.tv_sec;
            }
        }
    }
    if(fp != NULL)
    { 
        fclose(fp);
    }

    return NULL;
}

/******************************************************************************
* function:
*         int ns_handler(struct timespec* reqTime)         
*
* @param reqTime [IN] - timespec structure
*
* description:
*            Handle the call to nanosleep.   
******************************************************************************/
int ns_handler(struct timespec* reqTime)
{
    static unsigned int recDepth = 0; //to prevent too much time drift
    struct timespec remTime;
    int retVal = 1;

    recDepth++;
    if(recDepth >= 5)
    {
        retVal = 0; //break out of the recursion and start a new nanosleep seq
    }
    else
    {
        if(nanosleep(reqTime, &remTime) < 0)
        {
            if(EINTR == errno)
            {
                retVal = ns_handler(&remTime);
            }
            else
            {
                WARN("WARNING nanosleep system call failed: errno %i\n", errno);
                retVal = 0;
            }
        }
    }

    recDepth--;
    return retVal;
}


#endif

/******************************************************************************
* function:
*         qat_engine_init(ENGINE *e)
*
* @param e [IN] - OpenSSL engine pointer
*
* description:
*   Qat engine init function, associated with Crypto memory setup
*   and cpaStartInstance setups.
******************************************************************************/
static int qat_engine_init(ENGINE * e)
{
        int instNum;
    CpaStatus status = CPA_STATUS_SUCCESS;

    DEBUG("[%s] ---- Engine Initing\n\n", __func__);

   /* Initialise the QAT hardware */
    /*
     * The second argument of icp_sal_userStartMultiProcess needs to match
     * the value of LimitDevAccess in the SSL section of the icp device
     * configuration file (ie /etc/dh89xxcc_qa_dev0.conf).
     * If LimitDevAccess = 0, then use CPA_FALSE, otherwise CPA_TRUE.
     */
    if (CPA_STATUS_SUCCESS != icp_sal_userStartMultiProcess("SSL",CPA_FALSE))
    {
       WARN("icp_sal_userStart failed\n");
        exit(EXIT_FAILURE);
    }   

 
   /* Get the number of available instances */
    status = cpaCyGetNumInstances(&numInstances);
    if (CPA_STATUS_SUCCESS != status)
    {
        WARN("cpaCyGetNumInstances failed, status=%d\n", status);
        return 0;
    }
    if (!numInstances)
    {
        WARN("No crypto instances found\n"
             "make sure the icp device configuration file has "
	     "LimitDevAccess = 0\n");
        return 0;
    }

    DEBUG("%s: %d Cy instances got\n", __func__, numInstances);

    /* Allocate memory for the instance handle array */
    qatInstanceHandles =
        (CpaInstanceHandle *) OPENSSL_malloc(numInstances *
                                             sizeof(CpaInstanceHandle));
    if (qatInstanceHandles == NULL)
    {
        WARN("OPENSSL_malloc() failed for instance handles.\n");
        return 0;
    }

    /* Allocate memory for the polling threads */
    icp_polling_threads =
        (pthread_t *) OPENSSL_malloc(numInstances * sizeof(pthread_t));
    if (icp_polling_threads == NULL)
    {
        DEBUG("malloc() failed for icp_polling_threads.\n");
        return 0;
    }

    /* Get the Cy instances */
    status = cpaCyGetInstances(numInstances, qatInstanceHandles);
    if (CPA_STATUS_SUCCESS != status)
    {
        WARN("cpaCyGetInstances failed, status=%d\n", status);
        return 0;
    }

    /* Set translation function and start each instance */
    for (instNum = 0; instNum < numInstances; instNum++)
    {
        /* Set the address translation function */
        status = cpaCySetAddressTranslation(qatInstanceHandles[instNum],
                                            myVirtualToPhysical);
        if (CPA_STATUS_SUCCESS != status)
        {
            WARN("cpaCySetAddressTranslation failed, status=%d\n", status);
            return 0;
        }

        /* Start the instances */
        status = cpaCyStartInstance(qatInstanceHandles[instNum]);
        if (CPA_STATUS_SUCCESS != status)
        {
            WARN("cpaCyStartInstance failed, status=%d\n", status);
            return 0;
        }

        /* Create the polling threads */
#ifdef USE_NS_SENDPOLL
        pthread_create(&icp_polling_threads[instNum], NULL, sendPoll_ns,
                       qatInstanceHandles[instNum]);
#else
        pthread_create(&icp_polling_threads[instNum], NULL, sendPoll,
                       qatInstanceHandles[instNum]);
#endif
#ifdef QAT_POLL_CORE_AFFINITY
        {
            int coreID = 0;
            int sts = 1;
            cpu_set_t cpuset;

            CPU_ZERO(&cpuset);

            coreID = 0;
            CPU_SET(coreID, &cpuset);

            sts =
                pthread_setaffinity_np(icp_polling_threads[instNum],
                                       sizeof(cpu_set_t), &cpuset);
            if (sts != 0)
            {
                DEBUG("pthread_setaffinity_np error, status = %d \n", sts);
                exit(EXIT_FAILURE);
            }
            sts =
                pthread_getaffinity_np(icp_polling_threads[instNum],
                                       sizeof(cpu_set_t), &cpuset);
            if (sts != 0)
            {
                DEBUG("pthread_getaffinity_np error, status = %d \n", sts);
                exit(EXIT_FAILURE);
            }

            if (CPU_ISSET(coreID, &cpuset))
                DEBUG("Polling thread assigned on CPU core %d\n", coreID);
        }
#endif
    }

    return 1;
}

/******************************************************************************
* function:
*         qat_engine_finish(ENGINE *e)
*
* @param e [IN] - OpenSSL engine pointer
*
* description:
*   Qat engine finish function.
******************************************************************************/
static int qat_engine_finish(ENGINE * e)
{
    
    int i; 
    CpaStatus status = CPA_STATUS_SUCCESS;

    DEBUG("[%s] ---- Engine Finishing...\n\n", __func__);

    keep_polling = 0;

    for ( i = 0; i < numInstances; i++)
    {
        status = cpaCyStopInstance(qatInstanceHandles[i]);

        if (CPA_STATUS_SUCCESS != status)
        {
            WARN("cpaCyStopInstance failed, status=%d\n", status);
            return 0;
        }

        pthread_join(icp_polling_threads[i], NULL);

    }

    //DEBUG(" [%s] release  element: %p with engine id: %p\n",__FUNCTION__,gConfigEntry,e);

    OPENSSL_free(qatInstanceHandles);
    OPENSSL_free(icp_polling_threads);

    icp_sal_userStop();
#ifdef SAL_STOP_DELAY
    sleep(SAL_STOP_DELAY);
#endif

    return 1;
}

/******************************************************************************
* function:
*         qat_engine_destroy(ENGINE *e)
*
* @param e [IN] - OpenSSL engine pointer
*
* description:
*   Qat engine destroy function, required by Openssl engine API.
*   all the clean up are implemented in qat_engine_finish(), thus we just do 
*   nothing here but return 1.  
*
******************************************************************************/
static int qat_engine_destroy(ENGINE * e)
{
    DEBUG("[%s] ---- Destroying Engine...\n\n", __func__);
    return 1;
}

/******************************************************************************
* function:
*         cipher_init( EVP_CIPHER_CTX *ctx,
*                          const unsigned char *key,
*                          const unsigned char *iv,
*                          int enc, CipherType cipherType )
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector
* @param enc [IN] - encryption indicator
*
* description:
*   This function is rewrite of aes_xxx_cbc_init() function in OpenSSL.
*   All the inputs are passed form the above OpenSSL layer to the
*   corresponding API cpaCySymInitSession() function.
*   It is the first function called in AES cipher routine sequences,
*   in order to initialize the cipher ctx structure and CpaCySymSession.
*   The function will return 1 if successful.
******************************************************************************/
static int cipher_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                     const unsigned char *iv, int enc, CipherType cipherType)
{

    CpaCySymSessionSetupData sessionSetupData = {0};
    Cpa32U sessionCtxSize = 0;
    CpaCySymSessionCtx pSessionCtx = NULL;
    qat_ctx *qat_context = NULL;
    CpaStatus sts = 0;
    CpaInstanceHandle instanceHandle;
    void *srcPrivateMetaData = NULL;
    void *dstPrivateMetaData = NULL;
    Cpa32U metaSize = 0;

    DEBUG("[%s] ---- AES init %p, enc %d...\n\n", __func__, ctx, enc);

    if(cipherType != RC_4)
    { 
        if ((!key) || (!iv) || (!ctx))
        {
            WARN("[%s] --- key, iv or ctx is NULL.\n", __func__);
            return 0;
        }
#ifdef QAT_DEBUG
        {
            int i;

            printf("%s: dumping iv\n", __func__);
            for (i = 0; i < 16; i++)
                printf("%2x ", iv[i]);
            putchar('\n');
        }
#endif
    }
    else
    {
        if ((!key) || (!ctx))
        {
            WARN("[%s] --- key, iv or ctx is NULL.\n", __func__);
            return 0;
        }
    }


    /* Priority of this session */
    sessionSetupData.sessionPriority = CPA_CY_PRIORITY_HIGH;
    sessionSetupData.symOperation = CPA_CY_SYM_OP_CIPHER;
    /* Cipher algorithm and mode */
    if(cipherType != RC_4 && cipherType != DES3 && cipherType != DES) 
    sessionSetupData.cipherSetupData.cipherAlgorithm =
        CPA_CY_SYM_CIPHER_AES_CBC;

    switch (cipherType)
    {
        case AES128:
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                (Cpa32U) qat_aes_128_cbc.key_len;
          break;
        case AES192:
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                (Cpa32U) qat_aes_192_cbc.key_len;
          break;
        case AES256:
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                (Cpa32U) qat_aes_256_cbc.key_len;
          break;
        case RC_4:
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                (Cpa32U)qat_rc4.key_len;
            sessionSetupData.cipherSetupData.cipherAlgorithm =
                CPA_CY_SYM_CIPHER_ARC4;
          break;
        case DES3:
            sessionSetupData.cipherSetupData.cipherAlgorithm =
                CPA_CY_SYM_CIPHER_3DES_CBC;
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                (Cpa32U) qat_des_ede3_cbc.key_len;
          break;
        case DES:
            sessionSetupData.cipherSetupData.cipherAlgorithm =
                CPA_CY_SYM_CIPHER_DES_CBC;
            sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                (Cpa32U) qat_des_cbc.key_len;
          break;
        default:
          break;
}

    
    /* Cipher key */
    sessionSetupData.cipherSetupData.pCipherKey = (Cpa8U *) key;

    sessionSetupData.verifyDigest = CPA_FALSE;

    /* Operation to perform */
    if (enc)
    {
        sessionSetupData.cipherSetupData.cipherDirection =
            CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
    }
    else
    {
        sessionSetupData.cipherSetupData.cipherDirection =
            CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;
    }

    instanceHandle = get_next_inst();
    if ((sts = cpaCySymSessionCtxGetSize
         (instanceHandle, &sessionSetupData,
          &sessionCtxSize)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymSessionCtxGetSize failed, sts = %d.\n",
             __func__, sts);
        return 0;
    }

    pSessionCtx = (CpaCySymSessionCtx) qaePinnedMemAlloc (sessionCtxSize);
    DEBUG("session %p size %d alloc\n", pSessionCtx, sessionCtxSize);

    if (pSessionCtx == NULL)
    {
        WARN("[%s] --- pSessionCtx malloc failed !\n", __func__);
        return 0;
    }
    if ((sts = cpaCySymInitSession
         (instanceHandle, qat_callbackFn, &sessionSetupData,
          pSessionCtx)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymInitSession failed, sts = %d \n", __func__, sts);
        qaeMemFree (pSessionCtx);
        return 0;
    }

    if ((sts = cpaCyBufferListGetMetaSize(instanceHandle,
                                          1, &metaSize)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyBufferListGetBufferSize failed sts=%d.\n",
             __func__, sts);
        qaeMemFree (pSessionCtx);
        return 0;
    }

    if (metaSize)
    {
        srcPrivateMetaData = qaePinnedMemAlloc (metaSize);
        if (!srcPrivateMetaData)
        {
            WARN("[%s] --- srcBufferList.pPrivateMetaData is NULL.\n",
                 __func__);
            qaeMemFree (pSessionCtx);
            return 0;
        }
        dstPrivateMetaData = qaePinnedMemAlloc (metaSize);
        if (!dstPrivateMetaData)
        {
            WARN("[%s] --- dstBufferList.pPrivateMetaData is NULL.\n",
                 __func__);
            qaeMemFree (pSessionCtx);
            qaeMemFree (srcPrivateMetaData);
            return 0;
        }
    }
    else
    {
        srcPrivateMetaData = NULL;
        dstPrivateMetaData = NULL;
    }

    /* pinned memory is not required for qat_context */
    qat_context = (qat_ctx *) OPENSSL_malloc(sizeof(qat_ctx));

    if (qat_context == NULL)
    {
        WARN("[%s] --- qat_context not allocated.\n", __func__);
        qaeMemFree (pSessionCtx);
        qaeMemFree (srcPrivateMetaData);
        qaeMemFree (dstPrivateMetaData);
        return 0;
    }

    switch (cipherType)
    {
        case AES128:
            qat_context->paramNID = qat_aes_128_cbc.nid;
          break;
        case AES192:
            qat_context->paramNID = qat_aes_192_cbc.nid;
          break;
        case AES256:
            qat_context->paramNID = qat_aes_256_cbc.nid;
          break;
        case RC_4:
            qat_context->paramNID = qat_rc4.nid;
          break;
        case DES3:
            qat_context->paramNID = qat_des_ede3_cbc.nid;
          break;
        case DES:
            qat_context->paramNID = qat_des_cbc.nid;
          break;    
        default:
             WARN("[%s] --- Unsupported Cipher Type.\n", __func__);
             qaeMemFree (pSessionCtx);
             qaeMemFree (srcPrivateMetaData);
             qaeMemFree (dstPrivateMetaData);
             return 0;
          break;
    }

    qat_context->ctx = pSessionCtx;
    qat_context->srcPrivateMetaData = srcPrivateMetaData;
    qat_context->dstPrivateMetaData = dstPrivateMetaData;

    /* set app_data pointer in ctx to qat_context pointer */
    ctx->app_data = (void *)qat_context;

    qat_context->enc = enc;
    qat_context->instanceHandle = instanceHandle;

    if(cipherType != RC_4)
    {

        /* copy iv value to ctx */
        memcpy(ctx->oiv, iv, EVP_CIPHER_CTX_iv_length(ctx));
        memcpy(ctx->iv, ctx->oiv, EVP_CIPHER_CTX_iv_length(ctx));
        DEBUG("%s: ctx %p\n", __func__, ctx);
        DEBUG ("%s iv len is %d\n", __func__, EVP_CIPHER_CTX_iv_length(ctx));
        DUMPL("ctx->iv", ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));
        DUMPL("CIPHER INTI KEY", key , 32);   
 }
    return 1;


}

/******************************************************************************
* function:
*         qat_aes_256_cbc_init( EVP_CIPHER_CTX *ctx,
*                          const unsigned char *key,
*                          const unsigned char *iv,
*                                          int enc)
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector
* @param enc [IN] - encryption indicator
*
* description:
*   This function is rewrite of aes_256_cbc_init() function in OpenSSL.
*   All the inputs are passed form the above OpenSSL layer to the
*   corresponding API cpaCySymInitSession() function.
*   It is the first function called in AES cipher routine sequences,
*   in order to initialize the cipher ctx structure and CpaCySymSession.
*   The function will return 1 if successful.
******************************************************************************/
static int
qat_aes_256_cbc_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                     const unsigned char *iv, int enc)
{
   DEBUG("[%s] --- called.\n", __func__);

   return cipher_init(ctx, key,iv,enc,AES256);
}


/******************************************************************************
* function:
*         do_cipher(EVP_CIPHER_CTX *ctx,
*                                   unsigned char *out,
*                                   const unsigned char *in,
*                                   size_t inl,
                                    CipherType cipherType)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN]  - pointer to input data
* @param inl [IN]  - Message length to cipher in bytes
* @param cipherType [in] - Enum cipher type
*
* description:
*    This function is rewrite of aes_xxx_cbc_do_cipher() in OpenSSL.
*    All the inputs are passed form the above OpenSSL layer to the
*    corresponding API cpaCySymPerformOp() function.
*    The function encrypt inl bytes from the buffer pointer in and writes
*    the encrypted version to the output pointer.
*    It is the second function called in AES cipher routine sequences
*    and return 1 if successful
******************************************************************************/
static int do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                          const unsigned char *in, size_t inl, CipherType cipherType)
{

    CpaCySymSessionCtx pSessionCtx = NULL;
    CpaCySymOpData OpData = { 0, };
    CpaBufferList srcBufferList = { 0, };
    CpaFlatBuffer srcFlatBuffer = { 0, };
    CpaBufferList dstBufferList = { 0, };
    CpaFlatBuffer dstFlatBuffer = { 0, };
    qat_ctx *qat_context = NULL;
    CpaStatus sts = 0;
    struct op_done opDone;

    DEBUG("\n[%s] --- do_cipher %p BEGIN, inl %d\n", __func__, ctx, (int) inl);

    if ((!in) || (!out) || (!ctx))
    {
        WARN("[%s] --- in, out or ctx is NULL.\n", __func__);
        return 0;
    }
    
#ifdef QAT_DEBUG
    {
        int i;

        printf("%s: inl %d\n", __func__, (int) inl);
        for (i = 0; i < inl; i++)
            printf("%2x ", in[i]);
        putchar('\n');
    }
#endif

    qat_context = (qat_ctx *) (ctx->app_data);
    pSessionCtx = qat_context->ctx;

    OpData.sessionCtx = pSessionCtx;
    OpData.packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
    /* Initialization Vector or Counter. */

    DEBUG ("%s iv len is %d\n", __func__, EVP_CIPHER_CTX_iv_length(ctx));
    DUMPL("ctx->iv", ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));

    if (zero_copy_memory_mode)
    {
        OpData.pIv = (Cpa8U *) ctx->iv;
        srcFlatBuffer.pData = (Cpa8U *) in;
        dstFlatBuffer.pData = (Cpa8U *) out;
    }
    else
    {
        OpData.pIv = (Cpa8U *) copyAllocPinnedMemory (ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));
        srcFlatBuffer.pData = (Cpa8U *) copyAllocPinnedMemory ((void*)in, inl);
        dstFlatBuffer.pData = (Cpa8U *) qaePinnedMemAlloc (inl);
    }


    /* Starting point for cipher processing - given as number of bytes from
       start of data in the source buffer. The result of the cipher operation


    will be written back into the output buffer starting at this location. */
    OpData.cryptoStartSrcOffsetInBytes = 0;
    /* Starting point for hash processing - given as number of bytes from start
       of packet in source buffer. */
    OpData.hashStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the hash will be
       computed on.  */
    OpData.messageLenToHashInBytes = 0;
    /* Pointer to the location where the digest result either exists or will be
       inserted. */
    OpData.pDigestResult = NULL;
    /* Pointer to Additional Authenticated Data (AAD) needed for authenticated
       cipher mechanisms - CCM and GCM. For other authentication mechanisms
       this pointer is ignored. */
    OpData.pAdditionalAuthData = NULL;
    /* The message length, in bytes, of the source buffer that the crypto
       operation will be computed on. This must be a multiple to the block size
       if a block cipher is being used. */
    OpData.messageLenToCipherInBytes = inl;

    /* Cipher IV length in bytes.  Determines the amount of valid IV data
       pointed to by the pIv parameter. */
    OpData.ivLenInBytes = (Cpa32U) EVP_CIPHER_CTX_iv_length(ctx);



    srcFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    /* Number of pointers */
    srcBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    srcBufferList.pBuffers = &srcFlatBuffer;
    srcBufferList.pUserData = NULL;

    srcBufferList.pPrivateMetaData = qat_context->srcPrivateMetaData;

    dstFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    /* Number of pointers */
    dstBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    dstBufferList.pBuffers = &dstFlatBuffer;
    /* This is an opaque field that is not read or modified internally. */
    dstBufferList.pUserData = NULL;

    dstBufferList.pPrivateMetaData = qat_context->dstPrivateMetaData;

    DEBUG("[%s] performing with %d bytes (iv-len=%d)\n", __func__, (int)inl,
          EVP_CIPHER_CTX_iv_length(ctx));

    initOpDone(&opDone);

    if ((sts = myPerformOp(qat_context->instanceHandle,
                           &opDone,
                           &OpData,
                           &srcBufferList,
                           &dstBufferList,
                           CPA_FALSE)) != CPA_STATUS_SUCCESS)

{
        WARN("[%s] --- cpaCySymPerformOp failed sts=%d.\n", __func__, sts);
        qaeMemFree (srcBufferList.pPrivateMetaData);
        qaeMemFree (dstBufferList.pPrivateMetaData);
        return 0;
    }

    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);

    /*  If encrypting, the IV is the last block of the destination (ciphertext)
     *  buffer.  If decrypting, the source buffer is the ciphertext.
     */
    if (qat_context->enc)
    {
        memcpy(ctx->iv,
               dstBufferList.pBuffers[0].pData + inl - AES_BLOCK_SIZE,
               EVP_CIPHER_CTX_iv_length(ctx));
    }
    else
    {
        memcpy(ctx->iv,
               srcBufferList.pBuffers[0].pData + inl - AES_BLOCK_SIZE,
               EVP_CIPHER_CTX_iv_length(ctx));
    }

    DUMPL("src buffer - END", srcBufferList.pBuffers[0].pData, inl);

    DUMPL("dest buffer - END", dstBufferList.pBuffers[0].pData, inl);
    DUMPL("ctx->iv- END", ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));

    if (!zero_copy_memory_mode)
    {
        qaeMemFree (OpData.pIv);
        qaeMemFree (srcFlatBuffer.pData);
        copyFreePinnedMemory (out, dstFlatBuffer.pData, inl);
    }



    DEBUG("[%s] --- do_cipher END\n\n", __func__);

    return 1;
}


/******************************************************************************
* function:
*         qat_aes_256_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
*                                   unsigned char *out,
*                                   const unsigned char *in,
*                                   size_t inl)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN]  - pointer to input data
* @param inl [IN]  - Message length to cipher in bytes
*
* description:
*    This function is rewrite of aes_256_cbc_do_cipher() in OpenSSL.
*    All the inputs are passed form the above OpenSSL layer to the
*    corresponding API cpaCySymPerformOp() function.
*    The function encrypt inl bytes from the buffer pointer in and writes
*    the encrypted version to the output pointer.
*    It is the second function called in AES cipher routine sequences
*    and return 1 if successful
******************************************************************************/
static int
qat_aes_256_cbc_do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                          const unsigned char *in, size_t inl)
{
    DEBUG("[%s] --- called.\n", __func__);

    return do_cipher(ctx, out, in, inl, AES256);
}

/******************************************************************************
* function:
*         qat_aes_192_cbc_init(EVP_CIPHER_CTX *ctx,
*                              const unsigned char *key,
*                              const unsigned char *iv,
*                              int enc)
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector
* @param enc [IN] - encryption indicator
*
* description:
*   This function is rewrite of aes_192_cbc_init() function in OpenSSL.
*   All the inputs are passed form the above OpenSSL layer to the
*   corresponding API cpaCySymInitSession() function.
*   It is the first function called in AES cipher routine sequences,
*   in order to initialize the cipher ctx structure and CpaCySymSession.
*   The function will return 1 if successful.
******************************************************************************/
static int
qat_aes_192_cbc_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                     const unsigned char *iv, int enc)
{
    DEBUG("[%s] --- called.\n", __func__);

    return cipher_init(ctx, key, iv, enc, AES192);
}

/******************************************************************************
* function:
*         qat_aes_192_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
*                                   unsigned char *out,
*                                   const unsigned char *in,
*                                   size_t inl)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN] - pointer to input data
* @param inl [IN] - Message length to cipher in bytes
*
* description:
*    This function is rewrite of aes_192_cbc_do_cipher() in OpenSSL.
*    All the inputs are passed form the above OpenSSL layer to the
*    corresponding API cpaCySymPerformOp() function.
*    The function encrypt inl bytes from the buffer pointer in and writes
*    the encrypted version to the output pointer.
*    It is the second function called in AES cipher routine sequences
*    and return 1 if successful
******************************************************************************/
static int
qat_aes_192_cbc_do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                          const unsigned char *in, size_t inl)
{
    DEBUG("[%s] --- called.\n", __func__);

    return do_cipher(ctx, out, in, inl, AES192);
}

/******************************************************************************
* function:
*         qat_aes_128_cbc_init(EVP_CIPHER_CTX *ctx,
*                              const unsigned char *key,
*                              const unsigned char *iv,
*                              int enc)
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector
* @param enc [IN] - encryption indicator
*
* description:
*   This function is rewrite of aes_128_cbc_init() function in OpenSSL.
*   All the inputs are passed form the above OpenSSL layer to the
*   corresponding API cpaCySymInitSession() function.
*   It is the first function called in AES cipher routine sequences,
*   in order to initialize the cipher ctx structure and CpaCySymSession.
*   The function will return 1 if successful.
******************************************************************************/
static int
qat_aes_128_cbc_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                     const unsigned char *iv, int enc)
{
    DEBUG("[%s] --- called.\n", __func__);

    return cipher_init(ctx, key, iv, enc, AES128);
}

/******************************************************************************
* function:
*         qat_aes_128_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
*                                   unsigned char *out,
*                                   const unsigned char *in,
*                                   size_t inl)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN] - pointer to input data
* @param inl [IN] - Message length to cipher in bytes
*
* description:
*    This function is rewrite of aes_128_cbc_do_cipher() in OpenSSL.
*    All the inputs are passed form the above OpenSSL layer to the
*    corresponding API cpaCySymPerformOp() function.
*    The function encrypt inl bytes from the buffer pointer in and writes
*    the encrypted version to the output pointer.
*    It is the second function called in AES cipher routine sequences
*    and return 1 if successful
******************************************************************************/
static int
qat_aes_128_cbc_do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                          const unsigned char *in, size_t inl)
{
    DEBUG("[%s] --- called.\n", __func__);

    return do_cipher(ctx, out, in, inl, AES128);
}

/******************************************************************************
* function:
*         common_cipher_cleanup(EVP_CIPHER_CTX *ctx)
*
* @param ctx [IN]  - pointer to cipher ctx
*
* description:
*    This function is rewrite of aes_xxx_cbc_cleanup() in OpenSSL. The function is design
*    to clears all information form a cipher context and free up any allocated memory
*    associate it. It is the last function called in AES cipher routine sequences
*    in order to make sure the sensitive information does not remain in memory.
*    The function will return 1 if successful
******************************************************************************/
static int common_cipher_cleanup(EVP_CIPHER_CTX      *ctx)
{
    CpaStatus sts = 0;
    CpaCySymSessionCtx pSessionCtx = NULL;
    qat_ctx *qat_context = NULL;

    DEBUG("[%s] --- cleaning\n\n", __func__);

    if (!ctx)
    {
        WARN("[%s] --- ctx is NULL.\n", __func__);
        return 0;
    }

    if (!ctx->app_data)
    {
        WARN("[%s] --- ctx->app_data is NULL.\n", __func__);
        return 0;
    }

    qat_context = (qat_ctx *) (ctx->app_data);

    pSessionCtx = qat_context->ctx;

    if ((sts =
         cpaCySymRemoveSession(qat_context->instanceHandle,
                               pSessionCtx)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymRemoveSession failed, sts = %d.\n",
             __func__, sts);
        return 0;
    }

    if (pSessionCtx)
        qaeMemFree (pSessionCtx);
    if (qat_context->srcPrivateMetaData)
        qaeMemFree (qat_context->srcPrivateMetaData);
    if (qat_context->dstPrivateMetaData)
        qaeMemFree (qat_context->dstPrivateMetaData);

    if (qat_context)
    {
        OPENSSL_free(qat_context);
        ctx->app_data = NULL;
    }

    return 1;

}

/******************************************************************************
* function:
*         qat_rc4_init(EVP_CIPHER_CTX      *ctx,
*                      const unsigned char *key,
*                      const unsigned char *iv,
*                      int                  enc)
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector (not used for RC4.)
* @param enc [IN] - encryption indicator
*
* description:
*   This function is a rewrite of rc4_init() function in OpenSSL.
*   All the inputs are passed form the above OpenSSL layer to the
*   corresponding API cpaCySymInitSession() function.
*   It is the first function called in RC4 cipher routine sequences,
*   in order to initialize the cipher ctx structure and CpaCySymSession.
*   The function will return 1 if successful.
******************************************************************************/
static int
qat_rc4_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
             const unsigned char *iv, int enc)
{
    DEBUG("[%s] --- called.\n", __func__);

    return cipher_init(ctx, key, iv, enc, RC_4);
}

/******************************************************************************
* function:
*         qat_rc4_do_cipher(EVP_CIPHER_CTX      *ctx,
*                           unsigned char       *out,
*                           const unsigned char *in,
*                           size_t              inl)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN]  - pointer to input data
* @param inl [IN]  - Message length to cipher in bytes
*
* description:
*    This function is rewrite of rc4_do_cipher() in OpenSSL.
*    All the inputs are passed form the above OpenSSL layer to the
*    corresponding API cpaCySymPerformOp() function.
*    The function encrypts inl bytes from the buffer pointer in and writes
*    the encrypted version to the output pointer.
*    It is the second function called in RC4 cipher routine sequences
*    and return 1 if successful
******************************************************************************/
static int
qat_rc4_do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                  const unsigned char *in, size_t inl)
{
    CpaCySymSessionCtx pSessionCtx = NULL;
    CpaCySymOpData OpData = { 0, };
    CpaBufferList srcBufferList = { 0, };
    CpaFlatBuffer srcFlatBuffer = { 0, };
    CpaBufferList dstBufferList = { 0, };
    CpaFlatBuffer dstFlatBuffer = { 0, };
    qat_ctx *qat_context = NULL;
    CpaStatus sts = 0;
    struct op_done opDone;
    int in_place = 0;

    DEBUG("\n[%s] --- do_cipher BEGIN\n", __func__);

    if ((!in) || (!out) || (!ctx))
    {
        WARN("[%s] --- in, out or ctx is NULL.\n", __func__);
        return 0;
    }

    qat_context = (qat_ctx *) (ctx->app_data);
    pSessionCtx = qat_context->ctx;

    OpData.sessionCtx = pSessionCtx;
    OpData.packetType = CPA_CY_SYM_PACKET_TYPE_PARTIAL;

    /* Note, IV parameters not set for RC4 */
    if (zero_copy_memory_mode)
    {
        srcFlatBuffer.pData = (Cpa8U *) in;
        dstFlatBuffer.pData = (Cpa8U *) out;
    }
    else
    {
        srcFlatBuffer.pData = (Cpa8U *) copyAllocPinnedMemory ((void*)in, inl);
        in_place = 1;
    }
    /* Starting point for cipher processing - given as number of bytes from
       start of data in the source buffer. The result of the cipher operation
       will be written back into the output buffer starting at this location. */
    OpData.cryptoStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the crypto
       operation will be computed on. This must be a multiple to the block size 
       if a block cipher is being used. This is also the same as the result
       length. */
    OpData.messageLenToCipherInBytes = inl;
    /* Starting point for hash processing - given as number of bytes from start 
       of packet in source buffer. */
    OpData.hashStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the hash will be 
       computed on. */
    OpData.messageLenToHashInBytes = 0;
    /* Pointer to the location where the digest result either exists or will be 
       inserted. */
    OpData.pDigestResult = NULL;
    /* Pointer to Additional Authenticated Data (AAD) needed for authenticated
       cipher mechanisms - CCM and GCM. */
    OpData.pAdditionalAuthData = NULL;
    /* Compute the digest and compare it to the digest contained at the
       location pointed to by pDigestResult. */

    srcFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    /* Number of pointers */
    srcBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    srcBufferList.pBuffers = &srcFlatBuffer;
    srcBufferList.pUserData = NULL;

    srcBufferList.pPrivateMetaData = qat_context->srcPrivateMetaData;

    dstFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    /* Number of pointers */
    dstBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    dstBufferList.pBuffers = &dstFlatBuffer;
    /* This is an opaque field that is not read or modified internally. */
    dstBufferList.pUserData = NULL;

    dstBufferList.pPrivateMetaData = qat_context->dstPrivateMetaData;

    initOpDone(&opDone);

    /*  Note that RC4 partial packet processing must be done in
     *  place.  i.e. source and dest buffer must be the same.  If
     *  in user memory mode we allocate the buffers to be the same
     *  ourselves.  For zero copy mode responsibity is with the 
     *  caller to ensure buffers are the same.  We do not copy
     *  src to dst since the caller has explicitly set zero-copy
     *  mode to avoid copies.
     */

    if ((sts = myPerformOp(qat_context->instanceHandle,
                           &opDone,
                           &OpData,
                           &srcBufferList,
                           in_place ? &srcBufferList : &dstBufferList,
                           CPA_FALSE )) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymPerformOp failed sts=%d.\n", __func__, sts);
        qaeMemFree (srcBufferList.pPrivateMetaData);
        qaeMemFree (dstBufferList.pPrivateMetaData);
        return 0;
    }

    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);

    if (!zero_copy_memory_mode)
    {
        copyFreePinnedMemory (out, srcFlatBuffer.pData, inl);
    }
	
    DEBUG("[%s] --- do_cipher END\n\n", __func__);

    return 1;
}
/******************************************************************************
* function:
*         qat_des_ede3_cbc_init(EVP_CIPHER_CTX *ctx,
*                               const unsigned char *key,
*                               const unsigned char *iv,
*                               int enc)
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector
* @param enc [IN] - encryption indicator
*
* description:
*    This function is rewrite of des_ede3_cbc_init() function in OpenSSL.
*    All the inputs are pass from OpenSSL above layer down to the corresponding API
*    cpaCySymInitSession() function. It is the first function called in DES3 cipher routine
*    sequences, in order to initialize the cipher ctx structure and CpaCySymSession.
*    The function will return 1 if successful
******************************************************************************/
static int
qat_des_ede3_cbc_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                      const unsigned char *iv, int enc)
{
    DEBUG("[%s] --- called.\n", __func__);

    return cipher_init(ctx, key, iv, enc, DES3);
}

/******************************************************************************
* function:
*         qat_des_ede3_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
*                                    unsigned char *out,
*                                    const unsigned char *in,
*                                    size_t inl)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN] - pointer to input data
* @param inl [IN] - Message length to cipher in bytes
*
* description:
*   This function is rewrite of des_ede3_cbc_do_cipher() in OpenSSL.
*   All the inputs are pass form the above OpenSSL layer to the
*   corresponding API cpaCySymPerformOp() function.
*   The function encrypt inl bytes from the buffer pointer "in"
*   and writes the encrypted version to output pointer.
*   It is the second function called in DES3 cipher routine sequence
*   and return 1 if successful
******************************************************************************/
static int
qat_des_ede3_cbc_do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                           const unsigned char *in, size_t inl)
{

    CpaCySymSessionCtx pSessionCtx = NULL;
    CpaCySymOpData OpData = { 0, };
    CpaBufferList srcBufferList = { 0, };
    CpaFlatBuffer srcFlatBuffer = { 0, };
    CpaBufferList dstBufferList = { 0, };
    CpaFlatBuffer dstFlatBuffer = { 0, };
    qat_ctx *qat_context = NULL;
    CpaStatus sts = 0;
    struct op_done opDone;

    DEBUG("\n[%s] --- do_cipher BEGIN\n", __func__);

    if ((!in) || (!out) || (!ctx))
    {
        WARN("[%s] --- in, out or ctx is NULL.\n", __func__);
        return 0;
    }

    if ((!ctx->app_data))
    {
        WARN("[%s] --- app_data is NULL.\n", __func__);
        return 0;
    }

    qat_context = (qat_ctx *) (ctx->app_data);
    pSessionCtx = qat_context->ctx;

    OpData.sessionCtx = pSessionCtx;
    OpData.packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
    /* Initialization Vector or Counter. */

    if (zero_copy_memory_mode)
    {
        OpData.pIv = (Cpa8U *) ctx->iv;
        srcFlatBuffer.pData = (Cpa8U *) in;
        dstFlatBuffer.pData = (Cpa8U *) out;
    }
    else
    {
        OpData.pIv = (Cpa8U *) copyAllocPinnedMemory (ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));
        srcFlatBuffer.pData = (Cpa8U *) copyAllocPinnedMemory ((void*)in, inl);
        dstFlatBuffer.pData = (Cpa8U *) qaePinnedMemAlloc (inl);
    }
    /* Cipher IV length in bytes.  Determines the amount of valid IV data
       pointed to by the pIv parameter. */
    OpData.ivLenInBytes = (Cpa32U) EVP_CIPHER_CTX_iv_length(ctx);
    /* Starting point for cipher processing - given as number of bytes from
       start of data in the source buffer. The result of the cipher operation
       will be written back into the output buffer starting at this location. */
    OpData.cryptoStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the crypto
       operation will be computed on. This must be a multiple to the block size 
       if a block cipher is being used. This is also the same as the result
       length. */
    OpData.messageLenToCipherInBytes = inl;
    /* Starting point for hash processing - given as number of bytes from start 
       of packet in source buffer. */
    OpData.hashStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the hash will be 
       computed on. */
    OpData.messageLenToHashInBytes = 0;
    /* Pointer to the location where the digest result either exists or will be 
       inserted. */
    OpData.pDigestResult = NULL;
    /* Pointer to Additional Authenticated Data (AAD) needed for authenticated
       cipher mechanisms - CCM and GCM. */
    OpData.pAdditionalAuthData = NULL;


    srcFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    srcBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    srcBufferList.pBuffers = &srcFlatBuffer;
    srcBufferList.pUserData = NULL;

    srcBufferList.pPrivateMetaData = qat_context->srcPrivateMetaData;

    dstFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    /* Number of pointers */
    dstBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    dstBufferList.pBuffers = &dstFlatBuffer;
    dstBufferList.pUserData = NULL;

    dstBufferList.pPrivateMetaData = qat_context->dstPrivateMetaData;

    initOpDone(&opDone);

    if ((sts = myPerformOp(qat_context->instanceHandle,
                           &opDone,
                           &OpData,
                           &srcBufferList,
                           &dstBufferList,
                           CPA_FALSE )) != CPA_STATUS_SUCCESS)                         
    {
        WARN("[%s] --- cpaCySymPerformOp failed sts=%d.\n", __func__, sts);
        qaeMemFree (srcBufferList.pPrivateMetaData);
        qaeMemFree (dstBufferList.pPrivateMetaData);
        return 0;
    }

    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);

    /*  If encrypting, the IV is the last block of the destination (ciphertext)
     *  buffer.  If decrypting, the source buffer is the ciphertext.
     */
    if (qat_context->enc)
    {
        memcpy(ctx->iv,
               dstBufferList.pBuffers[0].pData + inl - DES_BLOCK_SIZE,
               EVP_CIPHER_CTX_iv_length(ctx));
    }
    else
    {
        memcpy(ctx->iv,
               srcBufferList.pBuffers[0].pData + inl - DES_BLOCK_SIZE,
               EVP_CIPHER_CTX_iv_length(ctx));
    }

    if (!zero_copy_memory_mode)
    {
        qaeMemFree (OpData.pIv);
        qaeMemFree (srcFlatBuffer.pData);
        copyFreePinnedMemory (out, dstFlatBuffer.pData, inl);
    }

    DEBUG ("%s inl=%d\n", __func__, (int) inl);
    DEBUG("[%s] --- do_cipher END\n\n", __func__);

    return 1;
}

/******************************************************************************
* function:
*         qat_des_cbc_init(EVP_CIPHER_CTX *ctx,
*                          const unsigned char *key,
*                          const unsigned char *iv,
*                          int enc)
*
* @param ctx [IN] - cipher ctx
* @param key [IN] - pointer to the key value
* @param iv  [IN] - pointer to initial vector
* @param enc [IN] - encryption indicator
*
* description:
*    This function is rewrite of des_cbc_init() function in OpenSSL.
*    All the inputs are pass from OpenSSL above layer down to the corresponding API
*    cpaCySymInitSession() function. It is the first function called in DES3 cipher routine
*    sequences, in order to initialize the cipher ctx structure and CpaCySymSession.
*    The function will return 1 if successful
******************************************************************************/
static int
qat_des_cbc_init(EVP_CIPHER_CTX * ctx, const unsigned char *key,
                 const unsigned char *iv, int enc)
{
    DEBUG("[%s] --- called.\n", __func__);

    return cipher_init(ctx, key, iv, enc, DES);
}

/******************************************************************************
* function:
*         qat_des_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
*                               unsigned char *out,
*                               const unsigned char *in,
*                               size_t inl)
*
* @param ctx [IN]  - pointer to cipher ctx
* @param out [OUT] - pointer to the output file
* @param in  [IN] - pointer to input data
* @param inl [IN] - Message length to cipher in bytes
*
* description:
*   This function is rewrite of des_cbc_do_cipher() in OpenSSL.
*   All the inputs are pass form the above OpenSSL layer to the
*   corresponding API cpaCySymPerformOp() function.
*   The function encrypt inl bytes from the buffer pointer "in"
*   and writes the encrypted version to output pointer.
*   It is the second function called in DES3 cipher routine sequence
*   and return 1 if successful
******************************************************************************/
static int
qat_des_cbc_do_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                      const unsigned char *in, size_t inl)
{
    CpaCySymSessionCtx pSessionCtx = NULL;
    CpaCySymOpData OpData = { 0, };
    CpaBufferList srcBufferList = { 0, };
    CpaFlatBuffer srcFlatBuffer = { 0, };
    CpaBufferList dstBufferList = { 0, };
    CpaFlatBuffer dstFlatBuffer = { 0, };
    qat_ctx *qat_context = NULL;
    CpaStatus sts = 0;
    struct op_done opDone;

    DEBUG("\n[%s] --- do_cipher BEGIN\n", __func__);

    if ((!in) || (!out) || (!ctx))
    {
        WARN("[%s] --- in, out or ctx is NULL.\n", __func__);
        return 0;
    }

    qat_context = (qat_ctx *) (ctx->app_data);
    pSessionCtx = qat_context->ctx;

    OpData.sessionCtx = pSessionCtx;
    OpData.packetType = CPA_CY_SYM_PACKET_TYPE_FULL;

    /* Initialization Vector or Counter. */
    if (zero_copy_memory_mode)
    {
        OpData.pIv = (Cpa8U *) ctx->iv;
        srcFlatBuffer.pData = (Cpa8U *) in;
        dstFlatBuffer.pData = (Cpa8U *) out;
    }
    else
    {
        OpData.pIv = (Cpa8U *) copyAllocPinnedMemory (ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));
        srcFlatBuffer.pData = (Cpa8U *) copyAllocPinnedMemory ((void*)in, inl);
        dstFlatBuffer.pData = (Cpa8U *) qaePinnedMemAlloc (inl);
    }
		
    /* Cipher IV length in bytes.  Determines the amount of valid IV data
       pointed to by the pIv parameter. */
    OpData.ivLenInBytes = (Cpa32U) EVP_CIPHER_CTX_iv_length(ctx);
    /* Starting point for cipher processing - given as number of bytes from
       start of data in the source buffer. The result of the cipher operation
       will be written back into the output buffer starting at this location. */
    OpData.cryptoStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the crypto
       operation will be computed on. This must be a multiple to the block size 
       if a block cipher is being used. This is also the same as the result
       length. */
    OpData.messageLenToCipherInBytes = inl;
    /* Starting point for hash processing - given as number of bytes from start 
       of packet in source buffer. */
    OpData.hashStartSrcOffsetInBytes = 0;
    /* The message length, in bytes, of the source buffer that the hash will be 
       computed on. */
    OpData.messageLenToHashInBytes = 0;
    /* Pointer to the location where the digest result either exists or will be 
       inserted. */
    OpData.pDigestResult = NULL;
    /* Pointer to Additional Authenticated Data (AAD) needed for authenticated
       cipher mechanisms - CCM and GCM. */
    OpData.pAdditionalAuthData = NULL;


    srcFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    srcBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    srcBufferList.pBuffers = &srcFlatBuffer;
    srcBufferList.pUserData = NULL;

    srcBufferList.pPrivateMetaData = qat_context->srcPrivateMetaData;

    dstFlatBuffer.dataLenInBytes = (Cpa32U) inl;
    /* Number of pointers */
    dstBufferList.numBuffers = 1;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    dstBufferList.pBuffers = &dstFlatBuffer;
    dstBufferList.pUserData = NULL;

    dstBufferList.pPrivateMetaData = qat_context->dstPrivateMetaData;

    initOpDone(&opDone);

    if ((sts = myPerformOp(qat_context->instanceHandle,
                           &opDone,
                           &OpData,
                           &srcBufferList,
                           &dstBufferList,
                           CPA_FALSE)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymPerformOp failed sts=%d.\n", __func__, sts);
        qaeMemFree (srcBufferList.pPrivateMetaData);
        qaeMemFree (dstBufferList.pPrivateMetaData);
        return 0;
    }

    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);

    /*  If encrypting, the IV is the last block of the destination (ciphertext)
     *  buffer.  If decrypting, the source buffer is the ciphertext.
     */
    if (qat_context->enc)
    {
        memcpy(ctx->iv,
               dstBufferList.pBuffers[0].pData + inl - DES_BLOCK_SIZE,
               EVP_CIPHER_CTX_iv_length(ctx));
    }
    else
    {
        memcpy(ctx->iv,
               srcBufferList.pBuffers[0].pData + inl - DES_BLOCK_SIZE,
               EVP_CIPHER_CTX_iv_length(ctx));
    }

    if (!zero_copy_memory_mode)
    {
        if(OpData.pIv)
		qaeMemFree (OpData.pIv);
        if(srcFlatBuffer.pData)
        	qaeMemFree (srcFlatBuffer.pData);
        
        copyFreePinnedMemory (out, dstFlatBuffer.pData, inl);
    }

    DEBUG("[%s] --- do_cipher END\n\n", __func__);

    return 1;
}

/******************************************************************************
* function:
*         qat_aes_sha1_session_init(EVP_MD_CTX *ctx_out,
*                          const EVP_MD_CTX *ctx_in)
*
* @param ctx_out [OUT] - pointer to new ctx
* @param ctx_in  [IN]  - pointer to existing context
*
* @retval 0      function succeeded
* @retval 1      function failed
*
* description:
*    This function synchronises the initialisation of the QAT session and
*  pre-allocates the necessary buffers for the session.
******************************************************************************/
#define data(ctx) ((qat_chained_ctx *)(ctx)->cipher_data)
#define HMAC_KEY_SIZE 64
#define TLS_VIRT_HDR_SIZE 13

static int qat_aes_sha1_session_init(EVP_CIPHER_CTX *ctx)
{
    qat_chained_ctx* evp_ctx = data(ctx);
    CpaCySymSessionSetupData *sessionSetupData = NULL;
    Cpa32U sessionCtxSize = 0;
    CpaCySymSessionCtx pSessionCtx = NULL;
    Cpa32U metaSize = 0;
 
    /* All parameters have not been set yet. */
    if(evp_ctx->initParamsSet != 1)
        return 0;
 
    sessionSetupData = evp_ctx->session_data;
 
    evp_ctx->instanceHandle = get_next_inst();
    if (cpaCySymSessionCtxGetSize(evp_ctx->instanceHandle, sessionSetupData,
                        &sessionCtxSize) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymSessionCtxGetSize failed.\n", __func__);
        return 0;
    }
 
    pSessionCtx = (CpaCySymSessionCtx) qaePinnedMemAlloc(sessionCtxSize);
 
    if (pSessionCtx == NULL)
    {
        WARN("[%s] --- pSessionCtx malloc failed !\n", __func__);
        return 0;
    }
    
   if(ctx->encrypt)
       sessionSetupData->verifyDigest = CPA_FALSE;
   else
       sessionSetupData->verifyDigest = CPA_TRUE;

   sessionSetupData->digestIsAppended =  CPA_TRUE;
   

    if (cpaCySymInitSession(evp_ctx->instanceHandle, qat_callbackFn, sessionSetupData,
              pSessionCtx) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymInitSession failed.\n", __func__);
        qaeMemFree(pSessionCtx);
        return 0;
    }
 
    evp_ctx->qat_ctx = pSessionCtx;
 
    evp_ctx->srcBufferList.numBuffers = 2;
    evp_ctx->srcBufferList.pBuffers = (evp_ctx->srcFlatBuffer);
    evp_ctx->srcBufferList.pUserData = NULL;
 
    evp_ctx->dstBufferList.numBuffers = 2;
    evp_ctx->dstBufferList.pBuffers = (evp_ctx->dstFlatBuffer);
    evp_ctx->dstBufferList.pUserData = NULL;

    /* setup meta data for buffer lists */
    if (cpaCyBufferListGetMetaSize(evp_ctx->instanceHandle,
                                   evp_ctx->srcBufferList.numBuffers,
                                   &metaSize) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyBufferListGetBufferSize failed.\n",__func__);
        return 0;
    }
 
    if (metaSize)
    {
        evp_ctx->srcBufferList.pPrivateMetaData = qaePinnedMemAlloc(metaSize);
        if (!(evp_ctx->srcBufferList.pPrivateMetaData))
        {
            WARN("[%s] --- srcBufferList.pPrivateMetaData is NULL.\n", __func__);
            return 0;
        }
    } else
    {
        evp_ctx->srcBufferList.pPrivateMetaData = NULL;
    }
    metaSize = 0;

    if (cpaCyBufferListGetMetaSize(evp_ctx->instanceHandle,
                                   evp_ctx->dstBufferList.numBuffers,
                                   &metaSize) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyBufferListGetBufferSize failed.\n",__func__);
        return 0;
    }
 
    if (metaSize)
    {
        evp_ctx->dstBufferList.pPrivateMetaData = qaePinnedMemAlloc(metaSize);
        if (!(evp_ctx->dstBufferList.pPrivateMetaData))
        {
            WARN("[%s] --- dstBufferList.pPrivateMetaData is NULL.\n", __func__);
            return 0;
        }
    } else
    {
        evp_ctx->dstBufferList.pPrivateMetaData = NULL;
    }

    /* Create the OpData structure to remove this processing from the data path */
    evp_ctx->OpData.sessionCtx = evp_ctx->qat_ctx;
    evp_ctx->OpData.packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
 
    evp_ctx->OpData.pIv = evp_ctx->pIv;
    evp_ctx->OpData.ivLenInBytes = (Cpa32U)EVP_CIPHER_CTX_iv_length(ctx);
 
    evp_ctx->OpData.cryptoStartSrcOffsetInBytes = TLS_VIRT_HDR_SIZE;
 
    evp_ctx->OpData.hashStartSrcOffsetInBytes = 0;
    evp_ctx->OpData.pAdditionalAuthData = NULL;
    
    return 1;
}

/******************************************************************************
* function:
*         qat_aes_cbc_hmac_sha1_init(EVP_CIPHER_CTX *ctx,
*                                    const unsigned char *inkey,
*                                    const unsigned char *iv, int enc)
*
* @param ctx    [IN]  - pointer to existing ctx
* @param inKey  [IN]  - input cipher key
* @param iv     [IN]  - initialisation vector
* @param enc    [IN]  - 1 encrypt 0 decrypt
*
* @retval 0      function succeeded
* @retval 1      function failed
*
* description:
*    This function initialises the cipher and hash algorithm parameters for this
*  EVP context.
*
******************************************************************************/
static int qat_aes_cbc_hmac_sha1_init(EVP_CIPHER_CTX *ctx,
                        const unsigned char *inkey,
                        const unsigned char *iv, int enc)
{
    /* Initialise a QAT session  and set the cipher keys*/
    qat_chained_ctx* evp_ctx = data(ctx);
    CpaCySymSessionSetupData *sessionSetupData = NULL;

    evp_ctx->session_data = OPENSSL_malloc(sizeof(CpaCySymSessionSetupData));
    if(NULL == evp_ctx->session_data)
    {
        WARN("qaePinnedMemAlloc() failed for session setup data allocation.\n");
        return 0;
    }
 
    sessionSetupData = evp_ctx->session_data;
 
    if ((!inkey) || (!iv) || (!ctx))
    {
        WARN("[%s] --- key, iv or ctx is NULL.\n", __func__);
        return 0;
    }

    DUMPL("iv", iv, EVP_CIPHER_CTX_iv_length(ctx));
    DUMPL("key", inkey, EVP_CIPHER_CTX_key_length(ctx));
 
    /* Priority of this session */
    sessionSetupData->sessionPriority = CPA_CY_PRIORITY_HIGH;
    sessionSetupData->symOperation = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
 
    /* Cipher algorithm and mode */
    sessionSetupData->cipherSetupData.cipherAlgorithm = CPA_CY_SYM_CIPHER_AES_CBC;
    /* Cipher key length 256 bits (32 bytes) */
    sessionSetupData->cipherSetupData.cipherKeyLenInBytes = (Cpa32U)EVP_CIPHER_CTX_key_length(ctx);
    /* Cipher key */
    if(NULL == (sessionSetupData->cipherSetupData.pCipherKey = OPENSSL_malloc(EVP_CIPHER_CTX_key_length(ctx))))
    {
        WARN("[%s] --- unable to allocate memory for Cipher key.\n", __func__);
	return 0;
    }
	
    memcpy(sessionSetupData->cipherSetupData.pCipherKey, inkey, EVP_CIPHER_CTX_key_length(ctx));
 
    /* Operation to perform */
    if(enc)
    {
        sessionSetupData->cipherSetupData.cipherDirection = CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
        sessionSetupData->algChainOrder = CPA_CY_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER;
    } else
    {
        sessionSetupData->cipherSetupData.cipherDirection = CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;
        sessionSetupData->algChainOrder = CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;
    }

    /* Hash Configuration */
    sessionSetupData->hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA1;
    sessionSetupData->hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
    sessionSetupData->hashSetupData.digestResultLenInBytes = SHA1_SIZE;
    sessionSetupData->hashSetupData.authModeSetupData.aadLenInBytes = 0;
 
    /* copy iv value to ctx */
    memcpy(ctx->iv, iv, EVP_CIPHER_CTX_iv_length(ctx));
 
    /* Pre-allocate necessary memory */
    evp_ctx->tls_virt_hdr = qaePinnedMemAlloc(TLS_VIRT_HDR_SIZE);
    if(NULL == evp_ctx->tls_virt_hdr)
    {
        WARN("[%s] Unable to allcoate memory for MAC preamble\n", __func__);
        return 0;
    }
    memset(evp_ctx->tls_virt_hdr, 0, TLS_VIRT_HDR_SIZE);
    evp_ctx->srcFlatBuffer[0].pData = evp_ctx->tls_virt_hdr;
    evp_ctx->srcFlatBuffer[0].dataLenInBytes = TLS_VIRT_HDR_SIZE;
    evp_ctx->dstFlatBuffer[0].pData = evp_ctx->srcFlatBuffer[0].pData;
    evp_ctx->dstFlatBuffer[0].dataLenInBytes = TLS_VIRT_HDR_SIZE;

    evp_ctx->hmac_key = qaePinnedMemAlloc(HMAC_KEY_SIZE);
    if(NULL == evp_ctx->hmac_key)
    {
        WARN("[%s] Unable to allocate memory or HMAC Key\n", __func__);
        return 0;
    }
    memset(evp_ctx->hmac_key, 0, HMAC_KEY_SIZE);
    sessionSetupData->hashSetupData.authModeSetupData.authKey = evp_ctx->hmac_key;
    sessionSetupData->hashSetupData.authModeSetupData.authKeyLenInBytes = HMAC_KEY_SIZE; 

    evp_ctx->pIv = qaePinnedMemAlloc(EVP_CIPHER_CTX_iv_length(ctx));
    if(!evp_ctx->pIv)
    {
        WARN("[%s] --- pIv is NULL.\n", __func__);
        return 0;
    }
 
    evp_ctx->initParamsSet = 1;
    qat_aes_sha1_session_init(ctx);
    evp_ctx->payload_length = 0;

    return 1;
}

/******************************************************************************
* function:
*    qat_aes_cbc_hmac_sha1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
*                                 const unsigned char *in, size_t len)
*
* @param ctx    [IN]  - pointer to existing ctx
* @param out   [OUT]  - output buffer for transform result
* @param in     [IN]  - input buffer
* @param len    [IN]  - length of input buffer
*
* @retval 0      function succeeded
* @retval 1      function failed
*
* description:
*    This function perfrom the cryptographic transfornm according to the
*  parameters setup during initialisation.
*
******************************************************************************/
static int qat_aes_cbc_hmac_sha1_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
                                        const unsigned char *in, size_t len)
{
    CpaStatus sts = 0;
    unsigned int pad_len = 0;
    struct op_done opDone;
    qat_chained_ctx *evp_ctx = data(ctx);
    int retVal = 0;
    size_t plen = evp_ctx->payload_length,
           iv   = 0; /* explicit IV in TLS 1.1 and later */

    if (len%AES_BLOCK_SIZE) return 0;
 
    if (plen==0)
        plen = len;
    else if (ctx->encrypt && len!=((plen+SHA_DIGEST_LENGTH+AES_BLOCK_SIZE)&-AES_BLOCK_SIZE))
        return 0;
#ifdef TLS1_1_VERSION
    else if (evp_ctx->tls_version >= TLS1_1_VERSION)
    {
        iv = AES_BLOCK_SIZE;
	memcpy(evp_ctx->OpData.pIv, in, EVP_CIPHER_CTX_iv_length(ctx));
	/* Note: The OpenSSL framework assumes that the IV field will be part of the 
         * encrypted data, yet never looks at the output of the encryptio/decryption
         * process for this field. In order to chain HASH and CIPHER we need to present
         * contiguous SGL to QAT, thus we need to copy the IV from input to output in
         * in order to skip this field in encryption */ 
        if(in != out)
            memcpy(out, in, EVP_CIPHER_CTX_iv_length(ctx));
	in += iv;
        out += iv;	
	len -= iv;
	evp_ctx->payload_length -= iv;
	plen -= iv;
    }
#endif
    else    
	memcpy(evp_ctx->OpData.pIv, ctx->iv, EVP_CIPHER_CTX_iv_length(ctx));
 
    /* Build request/response buffers */
    if (zero_copy_memory_mode)
    {
        evp_ctx->srcFlatBuffer[1].pData = (Cpa8U*)in;
        evp_ctx->dstFlatBuffer[1].pData = (Cpa8U*)out;
    } else
    {
        evp_ctx->srcFlatBuffer[1].pData = qaePinnedMemAlloc(len);
        if(!(evp_ctx->srcFlatBuffer[1].pData))
        {
            WARN("[%s] --- src/dst buffer allocation.\n", __func__);
            return 0;
        }
        evp_ctx->dstFlatBuffer[1].pData = evp_ctx->srcFlatBuffer[1].pData;
        memcpy(evp_ctx->dstFlatBuffer[1].pData, in, len);
    }
    evp_ctx->srcFlatBuffer[1].dataLenInBytes = len;
    evp_ctx->srcBufferList.pUserData = NULL;
    evp_ctx->dstFlatBuffer[1].dataLenInBytes = len;
    evp_ctx->dstBufferList.pUserData = NULL;
	
    evp_ctx->OpData.messageLenToCipherInBytes = len;

    if(!(ctx->encrypt))
    {
        AES_KEY aes_key;
        unsigned char in_blk[AES_BLOCK_SIZE] = {0x0};
        unsigned char *key = evp_ctx->session_data->cipherSetupData.pCipherKey;
        unsigned int  key_len = EVP_CIPHER_CTX_key_length(ctx);
        unsigned char ivec[AES_BLOCK_SIZE] = {0x0};
        unsigned char out_blk[AES_BLOCK_SIZE] = {0x0};

        key_len = key_len * 8; //convert to bits
        memcpy(in_blk, (in + (len - AES_BLOCK_SIZE)), AES_BLOCK_SIZE);
        memcpy(ivec, (in + (len - (AES_BLOCK_SIZE + AES_BLOCK_SIZE))), AES_BLOCK_SIZE);

        /* Dump input parameters */
        DUMPL("Key :", key, EVP_CIPHER_CTX_key_length(ctx));
        DUMPL("IV :", ivec, AES_BLOCK_SIZE);
        DUMPL("Input Blk :", in_blk, AES_BLOCK_SIZE);
 
        AES_set_decrypt_key(key, key_len, &aes_key);
        AES_cbc_encrypt(in_blk, out_blk, AES_BLOCK_SIZE, &aes_key, ivec, 0);

        DUMPL("Output Blk :", out_blk, AES_BLOCK_SIZE);

        /* Extract pad length */
        pad_len = out_blk[AES_BLOCK_SIZE - 1];
 
        /* Calculate and update length */
        evp_ctx->payload_length = len - (pad_len + 1 + SHA_DIGEST_LENGTH);
       
        evp_ctx->tls_virt_hdr[TLS_VIRT_HDR_SIZE-2] = evp_ctx->payload_length>>8;
        evp_ctx->tls_virt_hdr[TLS_VIRT_HDR_SIZE-1] = evp_ctx->payload_length;
 
        /* HMAC Length */
        evp_ctx->OpData.messageLenToHashInBytes = TLS_VIRT_HDR_SIZE + evp_ctx->payload_length;

        memcpy(evp_ctx->dstFlatBuffer[0].pData, evp_ctx->tls_virt_hdr, TLS_VIRT_HDR_SIZE);
    }else
    {
        evp_ctx->OpData.messageLenToHashInBytes = TLS_VIRT_HDR_SIZE + evp_ctx->payload_length;
    }

    /* Add record padding */
    if(ctx->encrypt)
    {
        plen += SHA_DIGEST_LENGTH;
        for (pad_len=len-plen-1;plen<len;plen++) evp_ctx->dstFlatBuffer[1].pData[plen]=pad_len;
    }

    initOpDone(&opDone);
    
#ifdef TLS1_1_VERSION
    if(!(ctx->encrypt) && ((evp_ctx->tls_version) < TLS1_1_VERSION)) 
#else
    if(!(ctx->encrypt)) 
#endif
        memcpy(ctx->iv, in + len - AES_BLOCK_SIZE, EVP_CIPHER_CTX_iv_length(ctx));

    DEBUG("Pre Perform Op\n"); 
    DUMPREQ(evp_ctx->instanceHandle, &opDone, &(evp_ctx->OpData), 
   	    evp_ctx->session_data, &(evp_ctx->srcBufferList), 
	    &(evp_ctx->dstBufferList));

    if ((sts = myPerformOp(evp_ctx->instanceHandle,
                           &opDone,
                           &(evp_ctx->OpData),
                           &(evp_ctx->srcBufferList),
                           &(evp_ctx->dstBufferList),
                           &(evp_ctx->session_data->verifyDigest))) != CPA_STATUS_SUCCESS)
    {

        WARN("[%s] --- cpaCySymPerformOp failed sts=%d.\n", __func__,
              sts);
        return 0;
    }

    waitForOpToComplete(&opDone);
    
    if(ctx->encrypt)
        retVal = 1;
    else if(CPA_TRUE == opDone.verifyResult)
	retVal = 1;
   
    DEBUG("Post Perform Op\n"); 
    DUMPREQ(evp_ctx->instanceHandle, &opDone, &(evp_ctx->OpData), 
   	    evp_ctx->session_data, &(evp_ctx->srcBufferList), 
	    &(evp_ctx->dstBufferList));
    
    cleanupOpDone(&opDone);
 
#ifdef TLS1_1_VERSION
    if((ctx->encrypt) && ((evp_ctx->tls_version) < TLS1_1_VERSION)) 
#else
    if(ctx->encrypt) 
#endif
        memcpy(ctx->iv, 
	    evp_ctx->dstBufferList.pBuffers[1].pData + len - AES_BLOCK_SIZE, 
	    EVP_CIPHER_CTX_iv_length(ctx));
       
    if (!zero_copy_memory_mode)
    {
        memcpy(out, evp_ctx->dstFlatBuffer[1].pData, len);
	if(NULL != evp_ctx->srcFlatBuffer[1].pData)
	{

	    qaeMemFree(evp_ctx->srcFlatBuffer[1].pData);
	    evp_ctx->srcFlatBuffer[1].pData = NULL;
	    evp_ctx->dstFlatBuffer[1].pData = NULL;
	}
    } 

    return retVal;
}

#if defined(NID_aes_128_cbc_hmac_sha1)
/******************************************************************************
* function:
*    qat_aes_cbc_hmac_sha1_ctrl(EVP_CIPHER_CTX *ctx, 
*				int type, int arg, void *ptr)
*
* @param ctx    [IN]  - pointer to existing ctx
* @param type   [IN]  - type of request either 
*			EVP_CTRL_AEAD_SET_MAC_KEY or EVP_CTRL_AEAD_TLS1_AAD
* @param arg    [IN]  - size of the pointed to by ptr
* @param ptr    [IN]  - input buffer contain the necessary parameters
*
* @retval x      The return value is dependent on the type of request being made
*	EVP_CTRL_AEAD_SET_MAC_KEY return of 1 is success
*	EVP_CTRL_AEAD_TLS1_AAD return value indicates the amount fo padding to 
*		be applied to the SSL/TLS record
* @retval -1     function failed
*
* description:
*    This function is a generic control interface provided by the EVP API. For 
*  chained requests this interface is used fro setting the hmac key value for 
*  authentication of the SSL/TLS record. The second type is used to specify the
*  TLS virtual header which is used in the authentication calculationa nd to 
*  identify record payload size.
*
******************************************************************************/
static int qat_aes_cbc_hmac_sha1_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{

    qat_chained_ctx *evp_ctx = data(ctx);
    int retVal = 0;
    CpaCySymSessionSetupData* sessionSetupData = evp_ctx->session_data;

    switch (type)
    {
        case EVP_CTRL_AEAD_SET_MAC_KEY:
        {
            unsigned char *hmac_key = evp_ctx->hmac_key;
 
            if(NULL == hmac_key)
            {
                WARN("HMAC Key is NULL");
                return -1;
            } 
 
            memset (hmac_key,0,HMAC_KEY_SIZE);
 
            if (arg > HMAC_KEY_SIZE) 
	     {
	        SHA1_Init(&(evp_ctx->key_wrap));
                SHA1_Update(&(evp_ctx->key_wrap),ptr,arg);
                SHA1_Final(hmac_key,&(evp_ctx->key_wrap));
                sessionSetupData->hashSetupData.authModeSetupData.authKeyLenInBytes = HMAC_KEY_SIZE;
            } else 
	    {
                memcpy(hmac_key,ptr,arg);
                sessionSetupData->hashSetupData.authModeSetupData.authKeyLenInBytes = arg;
            }
 
            DUMPL("hmac_key", hmac_key, arg);
 
            qat_aes_sha1_session_init(ctx);
            retVal = 1;
            break;
        }
        case EVP_CTRL_AEAD_TLS1_AAD:
        {
            /* Values to include in the record MAC calculation are included in this type
               This returns the amount of padding required for the send/encrypt direction */
            unsigned char *p=ptr;
            unsigned int   len=p[arg-2]<<8|p[arg-1];

	    evp_ctx->tls_version = (p[arg-4]<<8|p[arg-3]);
            if (ctx->encrypt)
            {
                evp_ctx->payload_length = len;
                if (evp_ctx->tls_version >= TLS1_1_VERSION)
		{
                    len -= AES_BLOCK_SIZE;
		    //BWILL: Why does this code reduce the len in the TLS header by the IV for the framework?
                    p[arg-2] = len>>8;
                    p[arg-1] = len;
                }

                if(NULL == evp_ctx->tls_virt_hdr)
                {
                    WARN("Unable to allocate memory for mac preamble in qat/n");
                    return -1;
                }
                memcpy(evp_ctx->tls_virt_hdr, p, TLS_VIRT_HDR_SIZE);
                DUMPL("tls_virt_hdr", evp_ctx->tls_virt_hdr, arg);
 
                retVal = (int)(((len+SHA_DIGEST_LENGTH+AES_BLOCK_SIZE)&-AES_BLOCK_SIZE) - len);
                break;
            } else
            {
                if (arg>13) arg = 13;
                memcpy(evp_ctx->tls_virt_hdr,ptr,arg);
                evp_ctx->payload_length = arg;
 
                retVal = SHA_DIGEST_LENGTH;
                break;
            }
        }
        default:
        return -1;
    }
    return retVal;
}
#endif

/******************************************************************************
* function:
*    qat_aes_cbc_hmac_sha1_cleanup(EVP_CIPHER_CTX *ctx)
*
* @param ctx    [IN]  - pointer to existing ctx
*
* @retval 1      function succeeded
* @retval 0      function failed
*
* description:
*    This function will cleanup all allocated resources required to perfrom the 
*  cryptographic transform.
*
******************************************************************************/
static int qat_aes_cbc_hmac_sha1_cleanup(EVP_CIPHER_CTX *ctx)
{
    qat_chained_ctx* evp_ctx = data(ctx);
    CpaStatus sts = 0;
    CpaCySymSessionSetupData* sessSetup = evp_ctx->session_data;

    if((sts = cpaCySymRemoveSession(evp_ctx->instanceHandle, evp_ctx->qat_ctx))
           != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] cpaCySymRemoveSession FAILED, sts = %d.!\n", __func__, sts);
        return 0;
    }
 
    if(sessSetup)
    {
        if(evp_ctx->qat_ctx)
        {
            qaeMemFree(evp_ctx->qat_ctx);
	    evp_ctx->qat_ctx = NULL;
	}
        if(sessSetup->hashSetupData.authModeSetupData.authKey)
	{
            qaeMemFree(sessSetup->hashSetupData.authModeSetupData.authKey);
	    sessSetup->hashSetupData.authModeSetupData.authKey = NULL;
	}
	if(evp_ctx->tls_virt_hdr)
	{
	    qaeMemFree(evp_ctx->tls_virt_hdr);
	    evp_ctx->tls_virt_hdr = NULL;
	}
        if(evp_ctx->srcBufferList.pPrivateMetaData)
        {
            qaeMemFree(evp_ctx->srcBufferList.pPrivateMetaData);
            evp_ctx->srcBufferList.pPrivateMetaData = NULL;
        }
        if(evp_ctx->dstBufferList.pPrivateMetaData)
        { 
            qaeMemFree(evp_ctx->dstBufferList.pPrivateMetaData);
            evp_ctx->dstBufferList.pPrivateMetaData = NULL;
        }
        if(evp_ctx->pIv)
        {
            qaeMemFree(evp_ctx->pIv);
            evp_ctx->pIv = NULL;
        
        }

	if(sessSetup->cipherSetupData.pCipherKey)
	{
	    OPENSSL_free(sessSetup->cipherSetupData.pCipherKey);
	    sessSetup->cipherSetupData.pCipherKey = NULL;
        }
      OPENSSL_free(sessSetup);

    }
    return 1;
}

/******************************************************************************
* function:
*         qat_digest_context_copy(EVP_MD_CTX *ctx_out,
*                                 const EVP_MD_CTX *ctx_in)
*
* @param ctx_out [OUT] - pointer to new ctx
* @param ctx_in  [IN]  - pointer to existing context
*
* @retval 0      function succeeded
* @retval 1      function failed
*
* description:
*    This function copies a context.  All buffers are also
*    copied.
******************************************************************************/
static int qat_digest_context_copy (EVP_MD_CTX *ctx_out,  
                                    const EVP_MD_CTX *ctx_in)
{
    qat_ctx *qat_out = NULL;
    qat_ctx *qat_in = NULL;
    qat_buffer *buff_in = NULL;
    qat_buffer *buff_out = NULL;
    int i = 0;

    if ((!ctx_in) || (!ctx_out))
    {
        WARN("[%s] --- ctx_in or ctx_out is NULL.\n", __func__);
        return 0;
    }

    DEBUG ("[%s] %p->%p\n", __func__, ctx_in->md_data, ctx_out->md_data);

    qat_out = (qat_ctx *) (ctx_out->md_data);
    qat_in = (qat_ctx *) (ctx_in->md_data);

    /*  If source context has not yet been initialised, there is nothing
     *  we can do except return success.
     */
    if (!qat_in)
    {
        return 1;
    }

    /*  If dest context has not been initialised, but source has, then
     *  consider this an error since we have nowhere to copy the context
     *  info into.  Return failure.
     */
    if (!qat_out)
    {
        WARN("[%s] --- qat_out or qat_in is NULL.\n", __func__);
        return 0;
    }

    if (qat_out->buff_count != 0)
    {
        WARN("[%s] --- ctx_out has buffers.\n", __func__);
    }

    buff_in = qat_in->first;
    qat_out->first = NULL;

    for (i = 0; i < qat_in->buff_count; i++)
   { 
      
        buff_out = OPENSSL_malloc (sizeof (qat_buffer));
        buff_out->data = copyAllocPinnedMemory ((void*) buff_in->data, buff_in->len);
        buff_out->len = buff_in->len;
        buff_out->next = NULL;

        if (qat_out->first)
        {
            qat_out->last->next = buff_out;
            qat_out->last = buff_out;
        }
        else
        {
            qat_out->first = 
            qat_out->last = buff_out;
        }

        buff_in = buff_in->next;
    }

    qat_out->buff_count = qat_in->buff_count;
    qat_out->buff_total = qat_in->buff_total;

    return 1;
}

/******************************************************************************
* function:
*   qat_sha1_init(EVP_MD_CTX *ctx)
*
*
* @param ctx [IN] - pointer to sha ctx
*
* description:
*    This function is rewrite of sha1_init() function in OpenSSL
*    The function is design to initialize the sha ctx structure and CpaCySymSession.
*    It is the first function called in SHA1 digest routine sequence
*    Function will return 1 if successful
******************************************************************************/
static int qat_sha1_init(EVP_MD_CTX * ctx)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_init(ctx);
}

/******************************************************************************
* function:
*   digest_init(EVP_MD_CTX *ctx)
*
*
* @param ctx [IN] - pointer to sha ctx
* @param digestType [in] - Enum digest type
*
* description:
*    This function is rewrite of sha/MD5_init() function in OpenSSL
*    The function is design to initialize the sha ctx structure and CpaCySymSession.
*    It is the first function called in SHA/MD5 digest routine sequence
*    Function will return 1 if successful
******************************************************************************/
static int digest_init(EVP_MD_CTX * ctx)
{

    DEBUG("[%s] ---- Digest init...\n\n", __func__);

    qat_ctx    *qat_context = NULL;
    qat_buffer *buff        = NULL;
    qat_buffer *buff_next   = NULL;
   
    /*  It does sometimes happen that we are asked to initialise a
     *  context before md_data has been allocated.  We politely refuse
     *  since we have nowhere to store our data but return success anyway.
     *  It will be initialised later on.
     */
    if (ctx->md_data == NULL)
    {
        DEBUG ("[%s] --- qat_context not allocated.\n", __func__);
        return 1;
    }
    
    qat_context = (qat_ctx *) ctx->md_data;  

    /* If this ctx has called Init before and  been updated 
     * with out a clean or final call we must free up the buffers
     * That were created in the update
     */ 
    if (qat_context->init == CTX_NOT_CLEAN)
    {
        DEBUG("[%s] ---- Init with unclean ctx\n", __func__);    

        buff = qat_context->first;
        
        int i;
        for (i = 0; i < qat_context->buff_count; i++)
        {
            qaeMemFree (buff->data);
            buff_next = buff->next;
            OPENSSL_free (buff);
            buff = buff_next;
        }

        qat_context->first = NULL;
        qat_context->last = NULL;
        qat_context->buff_count = 0;
        qat_context->buff_total = 0;
    }

    memset(ctx->md_data ,0x00, sizeof(qat_ctx));

    qat_context->init = 1;

    DEBUG("%s: ctx %p\n", __func__, qat_context->ctx);

    return 1;
}

/******************************************************************************
* function:
*         qat_sha1_update(EVP_MD_CTX *ctx,
*                         const void *data,
*                         size_t count)
*
* @param ctx   [IN] - pointer to sha ctx
* @param data  [IN] - pointer to chunks of inputdata
* @param count [IN] - message Length To Hash In Bytes
*
* description:
*   This function is rewrite of sha1_update() in OpenSSL,
*   It will be called repeatedly with chunks of target message to be
*   ddhashed before it pass to cpaCyRsaDecrypt() function.
*   The second function called in SHA1 digest routine sequence
*   and return 1 if successful
******************************************************************************/
static int qat_sha1_update(EVP_MD_CTX * ctx, const void *data, size_t count)
{ 
    DEBUG("[%s] --- called.\n", __func__);

    return digest_update(ctx,data,count);
}

/******************************************************************************
* function:
*         digest_update(EVP_MD_CTX *ctx,
*                         const void *data,
*                         size_t count)
*
* @param ctx   [IN] - pointer to sha ctx
* @param data  [IN] - pointer to chunks of inputdata
* @param count [IN] - message Length To Hash In Bytes
*
* description:
*   This function is rewrite of sha/MD5_update() in OpenSSL,
*   It will be called repeatedly with chunks of target message to be
*   hashed before it pass to cpaCyRsaDecrypt() function.
*   The second function called in SHA/MD5 digest routine sequence
*   and return 1 if successful
******************************************************************************/
static int digest_update(EVP_MD_CTX * ctx, const void *data, size_t count)
{
    DEBUG("[%s] --- called.\n", __func__);
   
    qat_ctx *qat_context = NULL;
    qat_buffer *buff;

    if (count == 0)
        return 1;

    if (zero_copy_memory_mode)
    {
        WARN("[%s] --- digest acceleration does not support zero copy.\n", __func__);
        return 0;
    }

    if ((!ctx) || (!data))
    {
        WARN("[%s] --- ctx or data is NULL.\n", __func__);
        return 0;
    }

    if (ctx->md_data == NULL)
    {
        WARN("[%s] --- qat_context not allocated.\n", __func__);
        return 0;
    }

    qat_context = (qat_ctx *) (ctx->md_data);
   
    if (qat_context->init == 0)
    {
        WARN("[%s] --- update called before init\n", __func__);
        return 0;
    }

    if (qat_context->buff_total + count > QAT_MAX_DIGEST_CHAIN_LENGTH)
    {
        WARN("[%s] --- Maximum digest chain length exceeded.\n", __func__);
        return 0;
    }
    
    buff = OPENSSL_malloc (sizeof (qat_buffer));
    if (!buff)
    {
        WARN("[%s] --- alloc failure.\n", __func__);
        return 0;
    }

    buff->data = copyAllocPinnedMemory ((void*) data, count);
    buff->len = count;
    buff->next = NULL;

    if (!buff->data)
    {    
        WARN("[%s] --- alloc failure.\n", __func__);
	OPENSSL_free (buff);
        return 0;
    }

    if (qat_context->first == NULL)
    {
        qat_context->first = buff;
        qat_context->last = buff;
    }
    else
    {
        qat_context->last->next = buff;
        qat_context->last = buff;
    }

    qat_context->buff_count++;
    qat_context->buff_total += count;
    qat_context->init = CTX_NOT_CLEAN;

    DEBUG("%s: added buffer len %d to chain, count now %d len %d\n", 
          __func__, (int) count, qat_context->buff_count,
          (int) qat_context->buff_total);

    return 1;
}

/******************************************************************************
* function:
*         qat_sha1_copy(EVP_MD_CTX *ctx_out,
*                        const EVP_MD_CTX *ctx_in)
*
* @param ctx_out [OUT] - pointer to new ctx
* @param ctx_in  [IN]  - pointer to existing context
*
* description:
*    This function copies a context and creates a new session
*    to be associated with this context.  All buffers are also
*    copied.
******************************************************************************/
static int qat_sha1_copy (EVP_MD_CTX *ctx_out, const EVP_MD_CTX *ctx_in)
{
    int sts = 1;

    if(ctx_in->md_data != NULL)
    {
        memset(ctx_out->md_data,0x00, sizeof(qat_ctx));
    }

    sts = qat_sha1_init (ctx_out);

    if (sts != 1)
        return sts;

    sts = qat_digest_context_copy (ctx_out, ctx_in);

    return sts;
}

/******************************************************************************
* function:
*   qat_sha1_final(EVP_MD_CTX *ctx,
*                  unsigned char *md)
*
* @param ctx [IN]  - pointer to sha ctx
* @param md  [OUT] - digest message output
*
* description:
*   This function is the rewrite of OpenSSL sha1_final() function.
*   It places the digested message in md.
*   The third function called in SHA1 digest routine sequence
*   and return 1 if successful
******************************************************************************/
static int qat_sha1_final(EVP_MD_CTX * ctx, unsigned char *md)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_final(ctx,md);
}

/******************************************************************************
* function:
*   digest_final(EVP_MD_CTX *ctx,
*                unsigned char *md,
*                int digestSize  )
*
* @param ctx [IN]  - pointer to sha ctx
* @param md  [OUT] - digest message output
*
* description:
*   This function is the rewrite of OpenSSL sha/MD5_final() function.
*   It places the digested message in md.
*   The third function called in SHA/MD5 digest routine sequence
*   and return 1 if successful
******************************************************************************/
static int digest_final(EVP_MD_CTX * ctx, unsigned char *md )
{

    CpaCySymSessionCtx pSessionCtx = NULL;
    CpaCySymOpData OpData = { 0, };
    CpaBufferList srcBufferList = { 0, };
    CpaFlatBuffer *srcFlatBuffer;
    CpaStatus sts = 0;
    Cpa32U metaSize = 0;
    void *srcPrivateMetaData = NULL;
    struct op_done opDone;
    qat_ctx *qat_context = NULL;
    qat_buffer *buff = NULL;
    qat_buffer *buff_next = NULL;
    int i;
    int success = 1;
    
    CpaCySymSessionSetupData sessionSetupData = { 0 };
    Cpa32U sessionCtxSize = 0;
    CpaInstanceHandle instanceHandle;

    DEBUG("[%s] --- called.\n", __func__);

    if ((!ctx) || (!md))
    {
        WARN("[%s] --- ctx or md is NULL.\n", __func__);
        return 0;
    }

    if (ctx->md_data == NULL)
    {
        WARN("[%s] --- qat_context not allocated.\n", __func__);
        return 0;
    }

    qat_context = (qat_ctx *) (ctx->md_data);

    sessionSetupData.sessionPriority = CPA_CY_PRIORITY_HIGH;
    sessionSetupData.symOperation = CPA_CY_SYM_OP_HASH;
    sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_PLAIN;
    sessionSetupData.verifyDigest = CPA_FALSE;

    sessionSetupData.hashSetupData.digestResultLenInBytes = ctx->digest->md_size;  

 
    switch(ctx->digest->md_size)

    {
        case SHA1_SIZE:
            sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA1;
           break;
        case SHA256_SIZE:
            sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA256;
           break;
        case SHA512_SIZE:
            sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA512;
           break;
        case MD5_SIZE:
            sessionSetupData.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_MD5;
          break;
        default:
          break;
    }

    /* Operation to perform */
    instanceHandle = get_next_inst();
    if ((sts = cpaCySymSessionCtxGetSize(instanceHandle,
                                         &sessionSetupData,
                                         &sessionCtxSize)) !=
        CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymSessionCtxGetSize failed, sts = %d.\n",
             __func__, sts);
        return 0;
    }

    pSessionCtx = (CpaCySymSessionCtx) qaePinnedMemAlloc (sessionCtxSize);
    if (pSessionCtx == NULL)
    {
        WARN("[%s] --- pSessionCtx malloc failed.\n", __func__);

        return 0;
    }

    if ((sts = cpaCySymInitSession
         (instanceHandle, qat_callbackFn, &sessionSetupData,
          pSessionCtx)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymInitSession failed, sts = %d.\n", __func__, sts);
        qaeMemFree (pSessionCtx);
        return 0;
    }

    qat_context->ctx = pSessionCtx;
    qat_context->instanceHandle = instanceHandle;

    pSessionCtx = qat_context->ctx;

    /* OpData structure setup */
    OpData.pDigestResult = qaePinnedMemAlloc (ctx->digest->md_size);

    if (!OpData.pDigestResult)
    {
        WARN("[%s] --- alloc failure.\n", __func__);
        return 0;
    }

    OpData.sessionCtx = pSessionCtx;
    OpData.packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
    OpData.hashStartSrcOffsetInBytes = 0;
    OpData.messageLenToHashInBytes = qat_context->buff_total;
    OpData.pAdditionalAuthData = NULL;

    /*  Allocate meta data and flat buffer array for as many buffers
     *  as we are holding in our context linked list.
     */
    if ((sts = cpaCyBufferListGetMetaSize(qat_context->instanceHandle,
                                          qat_context->buff_count, &metaSize)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyBufferListGetBufferSize failed sts=%d.\n",
             __func__, sts);
        return 0;
    }

    if (metaSize)
    {
        srcPrivateMetaData = qaePinnedMemAlloc (metaSize);

        if (!srcPrivateMetaData)
        {
            WARN("[%s] --- srcBufferList.pPrivateMetaData is NULL.\n",
                 __func__);
    	    qaeMemFree (OpData.pDigestResult);
            return 0;
        }
    }
    else
    {
        srcPrivateMetaData = NULL;
    }

    srcFlatBuffer = qaePinnedMemAlloc (qat_context->buff_count * sizeof (CpaFlatBuffer));
    if (srcFlatBuffer == NULL)
    {
        WARN("[%s] --- FlatBuffer malloc failed.\n", __func__);
        qaeMemFree (srcPrivateMetaData);
    	qaeMemFree (OpData.pDigestResult);
        return 0;
    }

    /*  Populate the pData and length elements of the flat buffer array
     *  from the context linked list of qat_buffers
     */
    buff = qat_context->first;

    for (i = 0; i < qat_context->buff_count; i++)
    {
        srcFlatBuffer[i].pData = buff->data;
        srcFlatBuffer[i].dataLenInBytes = (Cpa32U) buff->len;
        buff = buff->next;
    }

    /* Number of pointers */
    srcBufferList.numBuffers = qat_context->buff_count;
    ;
    /* Pointer to an unbounded array containing the number of CpaFlatBuffers
       defined by numBuffers */
    srcBufferList.pBuffers = srcFlatBuffer;
    /* This is an opaque field that is not read or modified internally. */
    srcBufferList.pUserData = NULL;

    srcBufferList.pPrivateMetaData = srcPrivateMetaData;

    initOpDone(&opDone);

    if ((sts = myPerformOp(qat_context->instanceHandle,
                           &opDone,
                           &OpData,
                           &srcBufferList,
                           &srcBufferList,
                           CPA_FALSE )) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymPerformOp failed sts=%d.\n", __func__, sts);
        success = 0;
    }
    else
    {
        waitForOpToComplete(&opDone);
        cleanupOpDone(&opDone);

        memcpy(md, OpData.pDigestResult, ctx->digest->md_size);
    }

    qaeMemFree (srcPrivateMetaData);
    qaeMemFree (srcFlatBuffer);
    qaeMemFree (OpData.pDigestResult);

    /*  Free the list of chained buffers
     */
    buff = qat_context->first;

    for (i = 0; i < qat_context->buff_count; i++)
    {        
        qaeMemFree (buff->data);
        buff_next = buff->next;
        OPENSSL_free (buff);
        buff = buff_next;
    }


    qat_context->first = NULL;
    qat_context->last = NULL;
    qat_context->buff_count = 0;
    qat_context->buff_total = 0;

    if ((sts =
         cpaCySymRemoveSession(qat_context->instanceHandle,
                               qat_context->ctx)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCySymRemoveSession failed, sts = %d.\n",
             __func__, sts);
       return 0;
    }

    qat_context->instanceHandle = NULL;

    if (pSessionCtx)
        qaeMemFree (pSessionCtx);

    qat_context->ctx = NULL;
    qat_context->init = 0;

    return success;
}

/******************************************************************************
* function:
*   qat_sha256_init(EVP_MD_CTX *ctx)
*
* @param ctx [IN] - pointer to sha ctx
*
* description:
*    This function is rewrite of sha256()_init in OpenSSL.
*    The function is design to initialize the sha ctx structure and
*    CpaCySymSession. It is the first function called in SHA256
*    digest routine sequence and return 1 if successful
******************************************************************************/
static int qat_sha256_init(EVP_MD_CTX * ctx)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_init(ctx);
}

/******************************************************************************
* function:
*         qat_sha256_update(EVP_MD_CTX *ctx,
*                           const void *data,
*                           size_t count)
*
* @param ctx   [IN] - pointer to sha ctx
* @param data  [IN] - pointer to chunks of inputdata
* @param count [IN] - message Length To Hash In Bytes
*
* description:
*    This function is rewrite of sha256_update() in OpenSSL
*    The function will be called repeatedly with chunks of target message to
*    be hashed before it pass to cpaCyRsaDecrypt() function.
*    It is the second function called in SHA256 digest routine sequence
*    and return 1 if successful
******************************************************************************/
static int qat_sha256_update(EVP_MD_CTX * ctx, const void *data, size_t count)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_update(ctx,data,count);
}

/******************************************************************************
* function:
*         qat_sha256_copy(EVP_MD_CTX *ctx_out,
*                        const EVP_MD_CTX *ctx_in)
*
* @param ctx_out [OUT] - pointer to new ctx
* @param ctx_in  [IN]  - pointer to existing context
*
* description:
*    This function copies a context and creates a new session
*    to be associated with this context.  All buffers are also
*    copied.
******************************************************************************/
static int qat_sha256_copy (EVP_MD_CTX *ctx_out, const EVP_MD_CTX *ctx_in)
{
    int sts = 1;

    if(ctx_in->md_data != NULL)
    {
        memset(ctx_out->md_data,0x00, sizeof(qat_ctx));
    }

    sts = qat_sha256_init (ctx_out);

    if (sts != 1)
        return sts;

    sts = qat_digest_context_copy (ctx_out, ctx_in);

    return sts;
}

/******************************************************************************
* function:
*         qat_sha256_final(EVP_MD_CTX *ctx,
*                          unsigned char *md)
*
* @param ctx [IN]  - pointer to sha ctx
* @param md  [OUT] - digest message output
*
* description:
*    This function is the rewrite of OpenSSL sha256_final() function.
*    It places the digested message in md.
*    The third function called in SHA256 digest routine sequence
*    and return 1 if successful
******************************************************************************/
static int qat_sha256_final(EVP_MD_CTX * ctx, unsigned char *md)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_final(ctx,md);
}

/******************************************************************************
* function:
*   qat_sha512_init(EVP_MD_CTX *ctx)
*
* @param ctx [IN] - pointer to sha ctx
*
* description:
*    This function is rewrite of sha512()_init in OpenSSL.
*    The function is design to initialize the sha ctx structure and
*    CpaCySymSession. It is the first function called in SHA512
*    digest routine sequence and return 1 if successful
******************************************************************************/
static int qat_sha512_init(EVP_MD_CTX * ctx)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_init(ctx);
}

/******************************************************************************
* function:
*         qat_sha512_update(EVP_MD_CTX *ctx,
*                           const void *data,
*                           size_t count)
*
* @param ctx   [IN] - pointer to sha ctx
* @param data  [IN] - pointer to chunks of inputdata
* @param count [IN] - message Length To Hash In Bytes
*
* description:
*    This function is rewrite of sha512_update() in OpenSSL
*    The function will be called repeatedly with chunks of target message to
*    be hashed before it pass to cpaCyRsaDecrypt() function.
*    It is the second function called in SHA512 digest routine sequence
*    and return 1 if successful
******************************************************************************/
static int qat_sha512_update(EVP_MD_CTX * ctx, const void *data, size_t count)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_update(ctx,data,count);
}

/******************************************************************************
* function:
*         qat_sha512_copy(EVP_MD_CTX *ctx_out,
*                        const EVP_MD_CTX *ctx_in)
*
* @param ctx_out [OUT] - pointer to new ctx
* @param ctx_in  [IN]  - pointer to existing context
*
* description:
*    This function copies a context and creates a new session
*    to be associated with this context.  All buffers are also
*    copied.
******************************************************************************/
static int qat_sha512_copy (EVP_MD_CTX *ctx_out, const EVP_MD_CTX *ctx_in)
{
    int sts = 1;

    if(ctx_in->md_data != NULL)
    {
        memset(ctx_out->md_data,0x00, sizeof(qat_ctx));
    }

    sts = qat_sha512_init (ctx_out);

    if (sts != 1)
        return sts;

    sts = qat_digest_context_copy (ctx_out, ctx_in);

    return sts;
}

/******************************************************************************
* function:
*         qat_sha512_final(EVP_MD_CTX *ctx,
*                          unsigned char *md)
*
* @param ctx [IN] - pointer to sha ctx
* @param md [OUT] - digest message output
*
* description:
*    This function is the rewrite of OpenSSL sha512_final() function.
*    It places the digested message in md.
*    The third function called in SHA512 digest routine sequence
*    and return 1 if successful
******************************************************************************/
static int qat_sha512_final(EVP_MD_CTX * ctx, unsigned char *md)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_final(ctx,md);
}

/******************************************************************************
* function:
*          qat_md5_init(EVP_MD_CTX *ctx)
*
* @param ctx [IN] - pointer to sha ctx
*
* description:
*    This function is rewrite of md5_init() in OpenSSL.
*    The function is design to initialize the sha ctx structure and
*    CpaCySymSession. It is the first function called in MD5
*    digest routine sequence and return 1 if successful
******************************************************************************/
static int qat_md5_init(EVP_MD_CTX * ctx)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_init(ctx);    
}

/******************************************************************************
* function:
*         qat_md5_update(EVP_MD_CTX *ctx,
*                        const void *data,
*                        size_t count)
*
* @param ctx   [IN] - pointer to sha ctx
* @param data  [IN] - pointer to chunks of inputdata
* @param count [IN] - message Length To Hash In Bytes
*
* description:
*    This function is rewrite of md5_update() in OpenSSL
*    The function will be called repeatedly with chunks of target message to
*    be hashed before it pass to cpaCyRsaDecrypt() function.
*    It is the second function called in MD5 digest routine sequence
*    and return 1 if successful
******************************************************************************/
static int qat_md5_update(EVP_MD_CTX * ctx, const void *data, size_t count)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_update(ctx,data,count); 
}

/******************************************************************************
* function:
*         qat_md5_copy(EVP_MD_CTX *ctx_out,
*                        const EVP_MD_CTX *ctx_in)
*
* @param ctx_out [OUT] - pointer to new ctx
* @param ctx_in  [IN]  - pointer to existing context
*
* description:
*    This function copies a context and creates a new session
*    to be associated with this context.  All buffers are also
*    copied.
******************************************************************************/
static int qat_md5_copy (EVP_MD_CTX *ctx_out, const EVP_MD_CTX *ctx_in)
{
    int sts = 1;
   
    if(ctx_in->md_data != NULL)
    {    
        memset(ctx_out->md_data,0x00, sizeof(qat_ctx));
    }    
    
    sts = qat_md5_init (ctx_out);

    if (sts != 1)
        return sts;

    sts = qat_digest_context_copy (ctx_out, ctx_in);
  
    return sts;
}

/******************************************************************************
* function:
*          qat_md5_final(EVP_MD_CTX *ctx,
*                        unsigned char *md)
*
* @param ctx [IN] - pointer to sha ctx
* @param md [OUT] - digest message output
*
* description:
*    This function is the rewrite of OpenSSL md5_final() function.
*    It places the digested message in md.
*    The third function called in MD5 digest routine sequence
*    and return 1 if successful
******************************************************************************/
static int qat_md5_final(EVP_MD_CTX * ctx, unsigned char *md)
{
    DEBUG("[%s] --- called.\n", __func__);

    return digest_final(ctx,md);
}

/******************************************************************************
* function:
*   int digest_cleanup(EVP_MD_CTX *ctx)
*
* @param ctx [IN] - pointer to sha ctx
*
* description:
*     This function is the rewrite of OpenSSL digest xxx_cleanup() function.
*     It design to set digested message in ctx to zeros if there is still values in it.
*     The last function called in the digest routine sequence
*     and return 1 if successful
******************************************************************************/
static int digest_cleanup(EVP_MD_CTX * ctx)
{
    qat_ctx *qat_context = NULL;

    qat_buffer *buff = NULL;
    qat_buffer *buff_next = NULL;

    if (!ctx)
    {
        WARN("[%s] --- ctx is NULL.\n", __func__);
        return 0;
    }

    DEBUG("%s: been called !\n", __func__);

    if (ctx->md_data == NULL)
    {
        DEBUG ("[%s] --- qat_context not allocated.\n", __func__);
        return 1;
    }

    qat_context = (qat_ctx *) (ctx->md_data);

    /*
     * After a ctx has been copied the old context may not call final, but
     * just clean. So we must check that the buffer count is 0. If not we 
     * must have buffers to free.       
    */    
   if(qat_context->buff_count != 0)
    {
        int i;
    
        buff = qat_context->first;
   
        for (i = 0; i < qat_context->buff_count; i++)
        {
            qaeMemFree (buff->data);
            buff_next = buff->next;
            OPENSSL_free (buff);
            buff = buff_next;
        }
    }
 
    qat_context->first = NULL;
    qat_context->last = NULL;
    qat_context->buff_count = 0;
    qat_context->buff_total = 0;
    qat_context->init =0;   
 
    return 1;

}

/******************************************************************************
* function:
*         qat_alloc_pad(unsigned char *in,
*                       int len,
*                       int rLen,
*                       int sign)
*
* @param in   [IN] - pointer to Flat Buffer
* @param len  [IN] - length of input data (hash)
* @param rLen [IN] - length of RSA
* @param sign [IN] - 1 for sign operation and 0 for decryption
*
* description:
*   This function is used to add PKCS#1 padding into input data buffer
*   before it pass to cpaCyRsaDecrypt() function.
*   The function returns a pointer to unsigned char buffer
******************************************************************************/
static unsigned char *qat_alloc_pad(unsigned char *in, int len,
                                    int rLen, int sign)
{
    int i = 0;
 
    /* out data buffer should have fix length */
    unsigned char *out = qaeMemAlloc(rLen, __FILE__, __LINE__);
 
    if (out == NULL)
    {
        WARN("[%s] --- out buffer malloc failed.\n", __func__);
        return NULL;
    }
 
    /* First two char are 0x00, 0x01 */
    out[0] = 0;
 
    if(sign)
    {
        out[1] = 1;
    }
    else
    {
        out[1] = 2;
    }
 
    /* Fill 0xff and end up with 0x00 in out buffer until the length of
       actual data space left */
    for (i = 2; i < (rLen - len - 1); i++)
    {
        out[i] = 0xff;
        out[i + 1] = 0x00;
    }
 
    /* shift actual data to the end of out buffer */
    memcpy((out + rLen - len), in, len);
 
    return out;
}

/******************************************************************************
* function:
*         qat_remove_pad(unsigned char *in,
*                        int len,
*                        int rLen,
*                        int sign)
*
* @param in   [IN] - pointer to Flat Buffer
* @param len  [IN] - length of output buffer
* @param rLen [IN] - length of RSA
* @param sign [IN] - 1 for sign operation and 0 for decryption
*
* description:
*   This function is used to remove PKCS#1 padding from outputBuffer
*   after cpaCyRsaEncrypt() function during RSA verify.
*   The function returns a unsigned char buffer pointer.
******************************************************************************/
static int qat_remove_pad(unsigned char *out, unsigned char *in, int len,
                          int rLen, int sign)
{
    int i = 0;
    int dLen = 0;
    int pLen = 0;
 
    if (sign)
    {
        /* First two char of padding should be 0x00, 0x01 for signing*/
        if(in[0] != 0x00 || in[1] != 0x01)
        {
            WARN("[%s] --- Padding format unknown!", __func__);
            return 1; // 1 is error condition
        }
    }
    else
    {
        /* First two char of padding should be 0x00, 0x02 for decryption */
        if(in[0] != 0x00 || in[1] != 0x02)
        {
            WARN("[%s] --- Padding format unknown!", __func__);
            return 1;
        }
    }
 
    /* While loop is design to reach the 0x00 value and count all the 0xFF
       value where filled by PKCS#1 padding */
    while (in[i + 2] != 0x00 && i < rLen)
           i++;
 
    /* padding length = 0x00 + 0x01 + length of 0xFF + 0x00 */
    pLen = 2 + i + 1;
    /* Actual data length = 128 - padding length */
    dLen = rLen - pLen;
 
    /* shift actual data to the beginning of out buffer */
    memcpy(out, in + pLen, len);
 
    return 0;
}

/******************************************************************************
* function:
*         qat_data_len(unsigned char *in
*                      int  rLen, int sign)
*
* @param in   [IN] - pointer to Flat Buffer
* @param rLen [IN] - length of RSA
* @param sign [IN] - 1 for sign operation and 0 for decryption
*
* description:
*   This function is used to calculate the length of actual data
*   and padding size inside of outputBuffer returned from cpaCyRsaEncrypt() function.
*   The function counts the padding length (i) and return the length
*   of actual data (dLen) contained in the outputBuffer
******************************************************************************/
static int qat_data_len(unsigned char *in, int rLen, int sign)
{
    /* first two bytes are 0x00, 0x01 */
    int i = 0;
    int dLen = 0;
    int pLen = 0;
 
    /* First two char of padding should be 0x00, 0x01 */
    if(sign)
    {
        /* First two char of padding should be 0x00, 0x01 */
        if(in[0] != 0x00 || in[1] != 0x01)
        {
            WARN("[%s] --- Padding format unknow!\n", __func__);
            return 0;
        }
    }
    else
    {
        /* First two char of padding should be 0x00, 0x02 for decryption */
        if(in[0] != 0x00 || in[1] != 0x02)
        {
            WARN("[%s] --- Pading format unknown!", __func__);
            return 0;
        }
    }
 
    /* while loop is design to reach the 0x00 value and count all the 0xFF
       value where filled by PKCS#1 padding */
    while (in[i + 2] != 0x00 && i < rLen)
           i++;
 
    /* padding length = 2 + length of 0xFF + 0x00 */
    pLen = 2 + i + 1;
    /* data length = 128 - padding length */
    dLen = rLen - pLen;
 
    return dLen;
}

/******************************************************************************
* function:
*         qat_BN_to_FB(CpaFlatBuffer *fb,
*                      BIGNUM *bn)
*
* @param fb [IN] - API flatbuffer structure pointer
* @param bn [IN] - Big Number pointer
*
* description:
*   This function is used to transform the big number format to the flat buffer
*   format. The function is used to deliver the RSA Public/Private key structure
*   from OpenSSL layer to API layer.
******************************************************************************/
static void qat_BN_to_FB(CpaFlatBuffer * fb, BIGNUM * bn)
{

    /* Memory allocate for flat buffer */
    fb->dataLenInBytes = (Cpa32U) BN_num_bytes(bn);
    fb->pData = qaePinnedMemAlloc (fb->dataLenInBytes);
    if (fb->pData == NULL)
    {
        WARN("[%s] --- FlatBuffer pData malloc failed.\n", __func__);
        return;
    }
    /* BN_bn2in() converts the absolute value of big number into big-endian
       form and stores it at output buffer. the output buffer must point to
       BN_num_bytes of memory */
    BN_bn2bin(bn, fb->pData);
}

/******************************************************************************
* function:
*         qat_rsa_priv_enc (int flen,
*                           const unsigned char *from,
*                           unsigned char *to,
*                           RSA *rsa,
*                           int padding)
*
* @param flen    [IN]  - length in bytes of input file (hash value)
* @param from    [IN]  - pointer to the input file
* @param to      [OUT] - pointer to output signature
* @param rsa     [IN]  - pointer to private key structure
* @param padding [IN]  - Padding scheme
*
* description:
*   This function is rewrite of OpenSSL RSA_priv_enc() function for RSA sign process.
*   All the inputs are pass form the above OpenSSL layer to the corresponding API
*   RSA sign function cpaCyRsaDecrypt().
*   The function returns the RSA signature output.
******************************************************************************/
static int
qat_rsa_priv_enc(int flen, const unsigned char *from, unsigned char *to,
                 RSA * rsa, int padding)
{
    DEBUG("[%s] --- called.\n", __func__);

    int outputLen = 0;
    int rsaLen = 0;
    CpaCyRsaPrivateKey cpaPrvKey;
    CpaCyRsaDecryptOpData *DecOpData = NULL;
    CpaFlatBuffer *outputBuffer = NULL;
    CpaStatus sts = 1;
    struct op_done opDone;
    CpaInstanceHandle instanceHandle;

    memset(&cpaPrvKey, 0, sizeof(CpaCyRsaPrivateKey));

    DEBUG("[%s] --- flen =%d, padding = %d \n", __func__, flen, padding);
    rsaLen = RSA_size(rsa);
    /* output signature should have same length as RSA(128) */
    outputLen = rsaLen;

    /* Padding check */
    if (padding != RSA_PKCS1_PADDING)
    {
        DEBUG("[%s] --- Unknown Padding!", __func__);
        goto exit;
    }

    /* Input data length check */
    /* The input message length should less than RSA size(128) and also have
       minimum space of PKCS1 padding(4 bytes) */
    if (flen > (rsaLen - 4) || flen == 0)
    {
        DEBUG("[%s] --- The input file length error !\n", __func__);
        goto exit;
    }

    /* output and input data MUST allocate memory for sign process */

    /* memory allocation for DecOpdata[IN] */
    DecOpData =
        (CpaCyRsaDecryptOpData *) qaePinnedMemAlloc (sizeof(CpaCyRsaDecryptOpData));
    if (DecOpData == NULL)
    {
        WARN("[%s] --- OpData malloc failed!\n", __func__);
        goto exit;
    }
    memset(DecOpData, 0, sizeof(CpaCyRsaDecryptOpData));

    cpaPrvKey.version = CPA_CY_RSA_VERSION_TWO_PRIME;

    /* Setup the private key rep type 2 structure */
    cpaPrvKey.privateKeyRepType = CPA_CY_RSA_PRIVATE_KEY_REP_TYPE_2;
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.prime1P, rsa->p);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.prime2Q, rsa->q);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.exponent1Dp, rsa->dmp1);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.exponent2Dq, rsa->dmq1);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.coefficientQInv, rsa->iqmp);

    /* Setup the DecOpData structure */
    DecOpData->pRecipientPrivateKey = &cpaPrvKey;
    DecOpData->inputData.dataLenInBytes = rsaLen;
    DecOpData->inputData.pData = qat_alloc_pad((Cpa8U *) from, flen, rsaLen, 1);
    if (DecOpData->inputData.pData == NULL)
    {
        WARN("[%s] --- InputData malloc failed!\n", __func__);
        goto exit;
    }

    /* Memory allocation for DecOpdata[IN] the size of outputBuffer should big
       enough to contain RSA_size */
    outputBuffer = (CpaFlatBuffer *) qaePinnedMemAlloc (rsaLen);
    if (outputBuffer == NULL)
    {
        WARN("[%s] --- OutputBuffer malloc failed!\n", __func__);
        goto exit;
    }

    if (zero_copy_memory_mode)
    {
        /* Assign outputBuffer to output pointer */
        outputBuffer->pData = (Cpa8U *) to;
        outputBuffer->dataLenInBytes = rsaLen;
    }
    else
    {
        outputBuffer->pData = (Cpa8U *) qaePinnedMemAlloc (rsaLen);
        outputBuffer->dataLenInBytes = rsaLen;
    }

    initOpDone(&opDone);

    instanceHandle = get_next_inst();

    /* cpaCyRsaDecrypt() is the function called for RSA verify in API, the
       DecOpData [IN] contains both private key value and input file (hash)
       value, the outputBuffer [OUT] stores the signature as the output
       message, the sts value return 0 if successful */
    do
    {
        sts = cpaCyRsaDecrypt(instanceHandle, qat_rsaCallbackFn, &opDone,
                              DecOpData, outputBuffer);
        if (sts == CPA_STATUS_RETRY)
        {
            qatPerformOpRetries++;
            pthread_yield();
        }
    }
    while (sts == CPA_STATUS_RETRY);
    if (sts != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyRsaDecrypt failed, sts=%d.\n", __func__, sts);
        goto exit;
    }
    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);

  exit:

    /* Free all the memory allocated in this function */
    if (DecOpData->inputData.pData)
        qaeMemFree (DecOpData->inputData.pData);

    if (cpaPrvKey.privateKeyRep2.prime1P.pData)
        qaeMemFree (cpaPrvKey.privateKeyRep2.prime1P.pData);

    if (cpaPrvKey.privateKeyRep2.prime2Q.pData)
        qaeMemFree (cpaPrvKey.privateKeyRep2.prime2Q.pData);

    if (cpaPrvKey.privateKeyRep2.exponent1Dp.pData)
        qaeMemFree (cpaPrvKey.privateKeyRep2.exponent1Dp.pData);

    if (cpaPrvKey.privateKeyRep2.exponent2Dq.pData)
        qaeMemFree (cpaPrvKey.privateKeyRep2.exponent2Dq.pData);

    if (cpaPrvKey.privateKeyRep2.coefficientQInv.pData)
        qaeMemFree (cpaPrvKey.privateKeyRep2.coefficientQInv.pData);

    if (DecOpData)
        qaeMemFree (DecOpData);

    if (!zero_copy_memory_mode)
        copyFreePinnedMemory (to, outputBuffer->pData, rsaLen);

    if (outputBuffer)
        qaeMemFree (outputBuffer);

    if (sts)
    {
        /* set output all 0xff if failed */
        DEBUG("[%s] --- cpaCyRsaDecrypt failed! \n", __func__);
        memset(to, 0xff, rsaLen);
    }
    else
        DEBUG("[%s] --- cpaCyRsaDecrypt finished! \n", __func__);

    /* Return 128 bytes message buffer */
    return outputLen;
}

/******************************************************************************
* function:
*         qat_rsa_priv_dec(int flen, const unsigned char *from,
*                          unsigned char *to, RSA * rsa, int padding)
*
* description:
*   Wrapper around the default OpenSSL RSA rsa_priv_dec() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
static int qat_rsa_priv_dec(int flen, const unsigned char *from,
                            unsigned char *to, RSA * rsa, int padding)
{
    DEBUG("[%s] --- called.\n", __func__);

    int dataLen = 0;
    int rsaLen = 0;
    CpaCyRsaPrivateKey cpaPrvKey;
    CpaCyRsaDecryptOpData *DecOpData = NULL;
    CpaFlatBuffer *outputBuffer = NULL;
    CpaStatus sts = 1;
    struct op_done opDone;
    CpaInstanceHandle instanceHandle;
 
    memset(&cpaPrvKey, 0, sizeof(CpaCyRsaPrivateKey));
 
    DEBUG("[%s] --- flen =%d, padding = %d \n", __func__, flen, padding);
    rsaLen = RSA_size(rsa);
 
    /* Padding check */
    if (padding != 1)
    {
        DEBUG("[%s] --- Unknown Padding!", __func__);
        goto exit;
    }
 
    /* output and input data MUST allocate memory for decryption process */
 
    /* memory allocation for DecOpdata[IN] */
    DecOpData =
        (CpaCyRsaDecryptOpData *)
            qaeMemAlloc(sizeof(CpaCyRsaDecryptOpData), __FILE__, __LINE__);
    if (DecOpData == NULL)
    {
        WARN("[%s] --- OpData malloc failed!\n", __func__);
        goto exit;
    }
    memset(DecOpData, 0, sizeof(CpaCyRsaDecryptOpData));
 
    cpaPrvKey.version = CPA_CY_RSA_VERSION_TWO_PRIME;
 
    /* Setup the private key rep type 2 structure */
    cpaPrvKey.privateKeyRepType = CPA_CY_RSA_PRIVATE_KEY_REP_TYPE_2;
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.prime1P, rsa->p);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.prime2Q, rsa->q);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.exponent1Dp, rsa->dmp1);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.exponent2Dq, rsa->dmq1);
    qat_BN_to_FB(&cpaPrvKey.privateKeyRep2.coefficientQInv, rsa->iqmp);

    /* Setup the DecOpData structure */
    DecOpData->pRecipientPrivateKey = &cpaPrvKey;
    DecOpData->inputData.dataLenInBytes = flen;
    DecOpData->inputData.pData =
        (Cpa8U *)qaeMemAlloc(flen, __FILE__, __LINE__);
    if (DecOpData->inputData.pData == NULL)
    {
        WARN("[%s] --- InputData malloc failed!\n", __func__);
        goto exit;
    }
    memcpy(DecOpData->inputData.pData, from, flen);
 
    /* Memory allocation for DecOpdata[IN] the size of outputBuffer should
       big enough to contain RSA_size */
    outputBuffer = (CpaFlatBuffer *) qaeMemAlloc(sizeof(CpaFlatBuffer), __FILE__, __LINE__);
    if (outputBuffer == NULL)
    {
        WARN("[%s] --- OutputBuffer malloc failed!\n", __func__);
        goto exit;
    }
 
    /* Assign outputBuffer to output pointer */
    //outputBuffer->pData = (Cpa8U *) to;
    outputBuffer->pData = qaeMemAlloc(rsaLen, __FILE__, __LINE__);
    if(outputBuffer->pData == NULL)
    {
        WARN("[%s] ---Output buffer allocation failed!\n", __func__);
        goto exit;
    }
 
    outputBuffer->dataLenInBytes = rsaLen;
 
    initOpDone(&opDone);
 
    instanceHandle = get_next_inst();
 
    /* cpaCyRsaDecrypt() is the function called for RSA verify in API, the
       DecOpData [IN] contains both private key value and input file (hash)
       value, the outputBuffer [OUT] stores the signature as the output
       message, the sts value return 0 if successful */
    if ((sts =
         cpaCyRsaDecrypt(instanceHandle, qat_rsaCallbackFn, &opDone,
                         DecOpData, outputBuffer)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyRsaDecrypt failed, sts=%d.\n", __func__,
              sts);
        goto exit;
    } 
    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);
 
    //Copy output to output buffer
    dataLen = qat_data_len(outputBuffer->pData, rsaLen, 0);
    if(qat_remove_pad(to, outputBuffer->pData, dataLen, rsaLen, 0) != 0)
    {
        WARN("[%s] --- pData remove padding detected, go to exit!\n",
              __func__);
        sts = 0;
        goto exit;
    }
      
    exit:
    /* Free all the memory allocated in this function */
    if (DecOpData->inputData.pData)
        qaeMemFree(DecOpData->inputData.pData);
 
    if (outputBuffer->pData)
        qaeMemFree(outputBuffer->pData);
 
    if (cpaPrvKey.privateKeyRep2.prime1P.pData)
        qaeMemFree(cpaPrvKey.privateKeyRep2.prime1P.pData);
 
    if (cpaPrvKey.privateKeyRep2.prime2Q.pData)
        qaeMemFree(cpaPrvKey.privateKeyRep2.prime2Q.pData);
 
    if (cpaPrvKey.privateKeyRep2.exponent1Dp.pData)
        qaeMemFree(cpaPrvKey.privateKeyRep2.exponent1Dp.pData);
 
    if (cpaPrvKey.privateKeyRep2.exponent2Dq.pData)
        qaeMemFree(cpaPrvKey.privateKeyRep2.exponent2Dq.pData);
 
    if (cpaPrvKey.privateKeyRep2.coefficientQInv.pData)
        qaeMemFree(cpaPrvKey.privateKeyRep2.coefficientQInv.pData);
 
    if (DecOpData)
        qaeMemFree(DecOpData);
 
    if (outputBuffer)
        qaeMemFree(outputBuffer);
 
    if (sts)
    {
        /* set output all 0xff if failed */
        DEBUG("[%s] --- cpaCyRsaDecrypt failed! \n", __func__);
        memset(to, 0xff, rsaLen);
    }
    else
        DEBUG("[%s] --- cpaCyRsaDecrypt finished! \n", __func__);
 
    return dataLen;
}

/******************************************************************************
* function:
*         qat_rsa_mod_exp(BIGNUM * r0, const BIGNUM * I, RSA * rsa,
*                         BN_CTX * ctx)
*
* description:
*   Wrapper around the default OpenSSL RSA rsa_mod_exp() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
static int qat_rsa_mod_exp(BIGNUM * r0, const BIGNUM * I, RSA * rsa,
                           BN_CTX * ctx)
{
    openssl_rsa_method = RSA_get_default_method();

    return openssl_rsa_method->rsa_mod_exp(r0, I, rsa, ctx);
}

/******************************************************************************
* function:
*         qat_bn_mod_exp(BIGNUM * r, const BIGNUM * a, const BIGNUM * p,
*                        const BIGNUM * m, BN_CTX * ctx, BN_MONT_CTX * m_ctx)
*
* description:
*   Wrapper around the default OpenSSL RSA rsa_bn_mod_exp() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
static int qat_bn_mod_exp(BIGNUM * r, const BIGNUM * a, const BIGNUM * p,
                          const BIGNUM * m, BN_CTX * ctx, BN_MONT_CTX * m_ctx)
{
    openssl_rsa_method = RSA_get_default_method();

    return openssl_rsa_method->bn_mod_exp(r, a, p, m, ctx, m_ctx);
}

/******************************************************************************
* function:
*         qat_rsa_pub_enc(int flen,const unsigned char *from,
*                         unsigned char *to,
*                         RSA *rsa,int padding)
*
* description:
*   Wrapper around the default OpenSSL RSA qat_rsa_pub_enc() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
static int qat_rsa_pub_enc(int flen, const unsigned char *from,
                           unsigned char *to, RSA * rsa, int padding)
{
    DEBUG("[%s] --- called.\n", __func__);
    int rsaLen = 0;
 
    CpaCyRsaPublicKey cpaPubKey;
    CpaCyRsaEncryptOpData *EncOpData = NULL;
    CpaFlatBuffer *outputBuffer = NULL;
    CpaStatus sts = 1;
    struct op_done opDone;
    unsigned char *pad_removed = NULL;
    CpaInstanceHandle instanceHandle;

    memset(&cpaPubKey, 0, sizeof(CpaCyRsaPublicKey));
 
    rsaLen = RSA_size(rsa);
 
    if (padding != 1)
    {
        WARN("[%s] --- Unknown Padding!", __func__);
        goto exit;
    }
 
    DEBUG("[%s] --- flen=%d padding=%d\n", __func__, flen, padding);
 
    /* Output and input data MUST allocate memory for RSA verify process */
    /* Memory allocation for EncOpData[IN] */
    EncOpData =
        (CpaCyRsaEncryptOpData *)
            qaeMemAlloc(sizeof(CpaCyRsaEncryptOpData), __FILE__, __LINE__);
    if (EncOpData == NULL)
    {
        WARN("[%s] --- OpData malloc failed!\n", __func__);
        goto exit;
    }
    /* Passing Public key from big number format to big endian order binary */
    qat_BN_to_FB(&cpaPubKey.modulusN, rsa->n);
    qat_BN_to_FB(&cpaPubKey.publicExponentE, rsa->e);
 
    /* Setup the Encrypt operation Data structure */
    EncOpData->pPublicKey = &cpaPubKey;
    if(flen > rsaLen - 11)
    {
        WARN("[%s] --- Message Invalid!", __func__);
        goto exit;
    }
 
    EncOpData->inputData.dataLenInBytes = rsaLen;
    EncOpData->inputData.pData = qat_alloc_pad((Cpa8U *) from, flen, rsaLen, 0);
    if(NULL == EncOpData->inputData.pData)
    {
        WARN("[%s] --- Padding failed!", __func__);
        goto exit;
    }     
 
    /* Memory allocation for outputBuffer[OUT] OutputBuffer size initialize
       as the size of rsa size */
    outputBuffer = (CpaFlatBuffer *) qaeMemAlloc(RSA_size(rsa), __FILE__, __LINE__);
    if (NULL == outputBuffer)
    {
        WARN("[%s] --- OutputBuffer malloc failed!\n", __func__);
        goto exit;
    }
 
    /* outputBuffer size should large enough to hold the Hash value but
       smaller than (RSA_size(rsa)-11) */
    outputBuffer->dataLenInBytes = rsaLen;
    outputBuffer->pData = qaeMemAlloc(rsaLen, __FILE__, __LINE__);
    if (NULL == outputBuffer->pData)
    {
        WARN("[%s] --- OutputBuffer pData malloc failed!\n", __func__);
        goto exit;
    }
 
    initOpDone(&opDone);
 
    instanceHandle = get_next_inst();
 
    /* The cpaCyRsaEncrypt() is the function called for RSA verify in API,
       the EnOpData [IN] contains both public key values and input
       signature values, the outputBuffer [OUT] stores the recovered digest
       as the output message, sts value return 0 if successful */
    if ((sts =
         cpaCyRsaEncrypt(instanceHandle, qat_rsaCallbackFn, &opDone,
                         EncOpData, outputBuffer)) != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyRsaEncrypt failed sts=%d.\n", __func__,
              sts);
        goto exit;
    }
    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);
 
    /* memory copy outputBuffer to to pointer where used for OpenSSL output
     */
    memcpy(to, outputBuffer->pData, outputBuffer->dataLenInBytes);
 
    exit:
    /* Free all the memory allocated in this function */
    if (EncOpData)
        qaeMemFree(EncOpData);
    if (outputBuffer->pData)
        qaeMemFree(outputBuffer->pData);
    if (pad_removed)
        qaeMemFree(pad_removed);
    if (outputBuffer)
        qaeMemFree(outputBuffer);
    if (cpaPubKey.modulusN.pData)
        qaeMemFree(cpaPubKey.modulusN.pData);
    if (cpaPubKey.modulusN.pData)
        qaeMemFree(cpaPubKey.publicExponentE.pData);
 
    /* setup output buffer if failed */
    if (sts)
    {
        /* set output all 0xff if failed */
        DEBUG("[%s] --- cpaCyRsaEncrypt failed! \n", __func__);
        memset(to, 0xff, rsaLen);
        return rsaLen;
    }
    else
    {
        DEBUG("[%s] --- cpaCyRsaEncrypt finished! \n", __func__);
 
        /* return outputLen bytes message buffer */
        return rsaLen;
    }
}

/******************************************************************************
* function:
*         qat_rsa_pub_dec(int flen,
*                         const unsigned char *from,
*                         unsigned char *to,
*                         RSA *rsa,
*                         int padding)
*
* @param flen    [IN]  - size in bytes of input signature
* @param from    [IN]  - pointer to the signature file
* @param to      [OUT] - pointer to output data
* @param rsa     [IN]  - pointer to public key structure
* @param padding [IN]  - Padding scheme
*
* description:
*   This function is rewrite of OpenSSL RSA_pub_dec() function for RSA verify process.
*   All the inputs are pass form the above OpenSSL layer to the corresponding API
*   RSA verify function cpaCyRsaEncrypt().
*   The function returns the RSA recovered message output.
******************************************************************************/
static int
qat_rsa_pub_dec(int flen, const unsigned char *from, unsigned char *to,
                RSA * rsa, int padding)
{
    DEBUG("[%s] --- called.\n", __func__);
    int outputLen = 0;
    int rsaLen = 0;

    CpaCyRsaPublicKey cpaPubKey;
    CpaCyRsaEncryptOpData *EncOpData = NULL;
    CpaFlatBuffer *outputBuffer = NULL;
    CpaStatus sts = 1;
    struct op_done opDone;
    unsigned char *pad_removed = NULL;
    CpaInstanceHandle instanceHandle;

    memset(&cpaPubKey, 0, sizeof(CpaCyRsaPublicKey));

    rsaLen = RSA_size(rsa);

    if (padding != RSA_PKCS1_PADDING)
    {
        WARN("[%s] --- Unknown Padding!", __func__);
        goto exit;
    }

    if (flen != rsaLen)
    { 
        WARN("[%s] --- The length of input signature incorrect! \n", __func__);
        goto exit;
    }

    DEBUG("[%s] --- flen=%d padding=%d\n", __func__, flen, padding);

    /* Output and input data MUST allocate memory for RSA verify process */
    /* Memory allocation for EncOpData[IN] */
    EncOpData =
        (CpaCyRsaEncryptOpData *) qaePinnedMemAlloc (sizeof(CpaCyRsaEncryptOpData));
    if (EncOpData == NULL)
    {
        WARN("[%s] --- OpData malloc failed!\n", __func__);
        goto exit;
    }
    /* Passing Public key from big number format to big endian order binary */
    qat_BN_to_FB(&cpaPubKey.modulusN, rsa->n);
    qat_BN_to_FB(&cpaPubKey.publicExponentE, rsa->e);

    /* Setup the Encrypt operation Data structure */
    EncOpData->pPublicKey = &cpaPubKey;

    if (zero_copy_memory_mode)
    {
        EncOpData->inputData.dataLenInBytes = flen;
        EncOpData->inputData.pData = (Cpa8U *) from;
    }
    else
    {
        EncOpData->inputData.dataLenInBytes = flen;
        EncOpData->inputData.pData = (Cpa8U *) copyAllocPinnedMemory ((void*) from, flen);
    }

    /* Memory allocation for outputBuffer[OUT] OutputBuffer size initialize as
       the size of rsa size */
    outputBuffer = (CpaFlatBuffer *) qaePinnedMemAlloc (RSA_size(rsa));
    if (outputBuffer == NULL)
    {
        WARN("[%s] --- OutputBuffer malloc failed!\n", __func__);
        goto exit;
    }

    /* outputBuffer size should large enough to hold the Hash value but smaller 
       than (RSA_size(rsa)-11) */
    outputBuffer->dataLenInBytes = rsaLen;
    outputBuffer->pData = qaePinnedMemAlloc (rsaLen);
    if (outputBuffer->pData == NULL)
    {
        WARN("[%s] --- OutputBuffer pData malloc failed!\n", __func__);
        goto exit;
    }

    initOpDone(&opDone);

    instanceHandle = get_next_inst();

    /* The cpaCyRsaEncrypt() is the function called for RSA verify in API, the
       EnOpData [IN] contains both public key values and input signature
       values, the outputBuffer [OUT] stores the recovered digest as the output 
       message, sts value return 0 if successful */
    do
    {
        sts = cpaCyRsaEncrypt(instanceHandle, qat_rsaCallbackFn, &opDone,
                              EncOpData, outputBuffer);
        if (sts == CPA_STATUS_RETRY)
        {
            qatPerformOpRetries++;
            pthread_yield();
        }
    }
    while (sts == CPA_STATUS_RETRY);
    if (sts != CPA_STATUS_SUCCESS)
    {
        WARN("[%s] --- cpaCyRsaEncrypt failed, sts=%d.\n", __func__, sts);
        goto exit;
    }
    waitForOpToComplete(&opDone);
    cleanupOpDone(&opDone);

    outputLen = qat_data_len(outputBuffer->pData, rsaLen, 1);
    if (outputLen == 0)
    {
        WARN("[%s] --- Unknown padding detected, go to exit!\n", __func__);
        sts = 0;
        goto exit;
    }
    /* remove the padding from outputBuffer */
    /* only RSA_PKCS1_PADDING scheme supported by qat engine */
    if (qat_remove_pad(to, outputBuffer->pData, outputBuffer->dataLenInBytes,
                       rsaLen, 1) != 0)
    {
        WARN("[%s] --- pData remove padding detected, go to exit!\n", __func__);
        sts = 0;
        goto exit;
    }

    exit:

    /* Free all the memory allocated in this function */
    if (!zero_copy_memory_mode)
        qaeMemFree (EncOpData->inputData.pData);
    if (EncOpData)
        qaeMemFree (EncOpData);
    if (outputBuffer->pData)
        qaeMemFree (outputBuffer->pData);
    if (pad_removed)
        qaeMemFree (pad_removed);

    if (outputBuffer)
        qaeMemFree (outputBuffer);
    if (cpaPubKey.modulusN.pData)
        qaeMemFree (cpaPubKey.modulusN.pData);
    if (cpaPubKey.modulusN.pData)
        qaeMemFree (cpaPubKey.publicExponentE.pData);

    /* setup output buffer if failed */
    if (sts)
    {
        /* set output all 0xff if failed */
        DEBUG("[%s] --- cpaCyRsaEncrypt failed! \n", __func__);
        memset(to, 0xff, rsaLen);
        return rsaLen;
    }
    else
    {
        DEBUG("[%s] --- cpaCyRsaEncrypt finished! \n", __func__);

        /* return outputLen bytes message buffer */
        return outputLen;
    }
}

/******************************************************************************
* function:
*         qat_dh_generate_key(DH * dh)
*
* description:
*   Wrapper around the default OpenSSL DH dh_generate_key function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
int qat_dh_generate_key(DH * dh)
{
    DEBUG("%s been called \n", __func__);
    openssl_dh_method = DH_OpenSSL();

    return openssl_dh_method->generate_key(dh);
}

/******************************************************************************
* function:
*         qat_dh_compute_key(unsigned char *key,
*                            const BIGNUM * pub_key, DH * dh)
*
* description:
*   Wrapper around the default OpenSSL DH dh_compute_key function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
int qat_dh_compute_key(unsigned char *key, const BIGNUM * pub_key, DH * dh)
{
    DEBUG("%s been called \n", __func__);
    openssl_dh_method = DH_OpenSSL();

    return openssl_dh_method->compute_key(key, pub_key, dh);
}

/******************************************************************************
* function:
*         qat_mod_exp(BIGNUM * r, const BIGNUM * a, const BIGNUM * p,
                      const BIGNUM * m, BN_CTX * ctx)
*
* @param r   [IN] - Result bignum of mod_exp
* @param a   [IN] - Base used for mod_exp
* @param p   [IN] - Exponent used for mod_exp
* @param m   [IN] - Modulus used for mod_exp
* @param ctx [IN] - EVP context.
*
* description:
*   Bignum modular exponentiation function used in DH and DSA.
*
******************************************************************************/
static int qat_mod_exp(BIGNUM * r, const BIGNUM * a, const BIGNUM * p,
                       const BIGNUM * m, BN_CTX * ctx)
{

    CpaCyLnModExpOpData opData;
    CpaFlatBuffer result = { 0, };
    CpaStatus status = 0;
    int retval = 1;
    CpaInstanceHandle instanceHandle;

    DEBUG("%s\n", __func__);
    qat_BN_to_FB(&opData.base, (BIGNUM *) a);
    qat_BN_to_FB(&opData.exponent, (BIGNUM *) p);
    qat_BN_to_FB(&opData.modulus, (BIGNUM *) m);

    result.dataLenInBytes = BN_num_bytes(m);
    result.pData = qaePinnedMemAlloc (result.dataLenInBytes);
    if (result.pData == NULL)
    {
        WARN("qaePinnedMemAlloc () failed for result.pData.\n");
        retval = 0;
        goto exit;
    }

    instanceHandle = get_next_inst();

    do
    {
        status = cpaCyLnModExp(instanceHandle, NULL, NULL, &opData, &result);
        if (status == CPA_STATUS_RETRY)
        {
            qatPerformOpRetries++;
            pthread_yield();
        }
    }
    while (status == CPA_STATUS_RETRY);

    if (CPA_STATUS_SUCCESS != status)
    {
        WARN("cpaCyLnModExp failed, status=%d\n", status);
        retval = 0;
        goto exit;
    }

    /* Convert the flatbuffer results back to a BN */
    BN_bin2bn(result.pData, result.dataLenInBytes, r);

  exit:

    if (opData.base.pData)
        qaeMemFree (opData.base.pData);
    if (opData.exponent.pData)
        qaeMemFree (opData.exponent.pData);
    if (opData.modulus.pData)
        qaeMemFree (opData.modulus.pData);
    if (result.pData)
        qaeMemFree (result.pData);

    return retval;
}

#if OPENSSL_VERSION_NUMBER < 0x01000000
/*
 * adapted from openssl-0.9.8w/fips/dsa/fips_dsa_ossl.c
 *
 * openssl-0.9.8w doesnt check if the engine provided DSA_METHOD doesn't
 * have a function for dsa_mod_exp and automatically fall back to software
 * (openssl-1.0.1c does) so we need to provide the software fall back.
*/
static int qat_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1, BIGNUM *p1,
		           BIGNUM *a2, BIGNUM *p2, BIGNUM *m, BN_CTX *ctx,
                           BN_MONT_CTX *in_mont)
{
     return BN_mod_exp2_mont(rr, a1, p1, a2, p2, m, ctx, in_mont);
}
#endif
/******************************************************************************
* function:
*         qat_mod_exp_dh(const DH * dh, BIGNUM * r, const BIGNUM * a,
*                        const BIGNUM * p, const BIGNUM * m, BN_CTX * ctx,
*                        BN_MONT_CTX * m_ctx)
*
* @param dh    [IN] - Pointer to a OpenSSL DH struct.
* @param r     [IN] - Result bignum of mod_exp
* @param a     [IN] - Base used for mod_exp
* @param p     [IN] - Exponent used for mod_exp
* @param m     [IN] - Modulus used for mod_exp
* @param ctx   [IN] - EVP context.
* @param m_ctx [IN] - EVP context for Montgomery multiplication.
*
* description:
*   Overridden modular exponentiation function used in DH.
*
******************************************************************************/
static int qat_mod_exp_dh(const DH * dh, BIGNUM * r, const BIGNUM * a,
                          const BIGNUM * p, const BIGNUM * m, BN_CTX * ctx,
                          BN_MONT_CTX * m_ctx)
{
    DEBUG("%s been called \n", __func__);
    return qat_mod_exp(r, a, p, m, ctx);
}

/******************************************************************************
* function:
*         qat_mod_exp_dsa(DSA * dsa, BIGNUM * r, BIGNUM * a, const BIGNUM * p,
                          const BIGNUM * m, BN_CTX * ctx, BN_MONT_CTX * m_ctx)
*
* @param dsa   [IN] - Pointer to a OpenSSL DSA struct.
* @param r     [IN] - Result bignum of mod_exp
* @param a     [IN] - Base used for mod_exp
* @param p     [IN] - Exponent used for mod_exp
* @param m     [IN] - Modulus used for mod_exp
* @param ctx   [IN] - EVP context.
* @param m_ctx [IN] - EVP context for Montgomery multiplication.
*
* description:
*   Overridden modular exponentiation function used in DSA.
*
******************************************************************************/
static int qat_mod_exp_dsa(DSA * dsa, BIGNUM * r, BIGNUM * a, const BIGNUM * p,
                           const BIGNUM * m, BN_CTX * ctx, BN_MONT_CTX * m_ctx)
{
    DEBUG("%s been called \n", __func__);
    return qat_mod_exp(r, a, p, m, ctx);

}

/******************************************************************************
* function:
*         qat_dsa_do_sign(const unsigned char *dgst, int dlen, DSA * dsa)
*
* description:
*   Wrapper around the default OpenSSL DSA dsa_do_sign() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
DSA_SIG *qat_dsa_do_sign(const unsigned char *dgst, int dlen, DSA * dsa)
{
    DEBUG("%s been called \n", __func__);

    openssl_dsa_method = DSA_OpenSSL();

    return openssl_dsa_method->dsa_do_sign(dgst, dlen, dsa);
}

/******************************************************************************
* function:
*         qat_dsa_sign_setup(DSA * dsa, BN_CTX * ctx_in, BIGNUM ** kinvp,
*                            BIGNUM ** rp)
*
* description:
*   Wrapper around the default OpenSSL DSA dsa_sign_setup() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
int qat_dsa_sign_setup(DSA * dsa, BN_CTX * ctx_in, BIGNUM ** kinvp,
                       BIGNUM ** rp)
{
    DEBUG("%s been called \n", __func__);
    openssl_dsa_method = DSA_OpenSSL();

    return openssl_dsa_method->dsa_sign_setup(dsa, ctx_in, kinvp, rp);
}

/******************************************************************************
* function:
*         qat_dsa_do_verify(const unsigned char *dgst, int dgst_len,
*                           DSA_SIG * sig, DSA * dsa)
*
* description:
*   Wrapper around the default OpenSSL DSA dsa_do_verify() function to avoid
*   a null function pointer.
*   See the OpenSSL documentation for parameters.
******************************************************************************/
static int qat_dsa_do_verify(const unsigned char *dgst, int dgst_len,
                             DSA_SIG * sig, DSA * dsa)
{
    DEBUG("%s been called \n", __func__);
    openssl_dsa_method = DSA_OpenSSL();

    return openssl_dsa_method->dsa_do_verify(dgst, dgst_len, sig, dsa);
}

/******************************************************************************
* function:
*         bind_qat(ENGINE *e,
*                  const char *id)
*
* @param e  [IN] - OpenSSL engine pointer
* @param id [IN] - engine id
*
* description:
*    Connect Qat engine to OpenSSL engine library
******************************************************************************/
static int bind_qat(ENGINE * e, const char *id)
{
    int ret = 0;

    DEBUG("[%s] id=%s\n", __func__, id);

    if (id && strcmp(id, engine_qat_id))
    {
        WARN("ENGINE_id defined already!\n");
        goto end;
    }

    if (!ENGINE_set_id(e, engine_qat_id))
    {
        WARN("ENGINE_set_id failed\n");
        goto end;
    }

    if (!ENGINE_set_name(e, engine_qat_name))
    {
        WARN("ENGINE_set_name failed\n");
        goto end;
    }

    /* set all the algos we are going to use. */
    if (!ENGINE_set_DSA(e, &qat_dsa_method))
    {
        WARN("ENGINE_set_DSA failed\n");
        goto end;
    }
    if (!ENGINE_set_RSA(e, &qat_rsa_method))
    {
        WARN("ENGINE_set_RSA failed\n");
        goto end;
    }

    if (!ENGINE_set_DH(e, &qat_dh_method))
    {
        WARN("ENGINE_set_DH failed\n");
        goto end;
    }

    if (!ENGINE_set_ciphers(e, qat_ciphers))
    {
        WARN("ENGINE_set_ciphers failed\n");
        goto end;
    }

#ifndef QAT_ZERO_COPY_MODE 
    if (!ENGINE_set_digests(e, qat_digests))
    {
        WARN("ENGINE_set_digests failed\n");
        goto end;
    }
#endif

    if (!ENGINE_set_destroy_function(e, qat_engine_destroy)
        || !ENGINE_set_init_function(e, qat_engine_init)
        || !ENGINE_set_finish_function(e, qat_engine_finish))
    {
        WARN("[%s] failed reg destroy, init or finish\n", __func__);

        goto end;
    }

    if ((ret = pthread_key_create(&qatInstanceForThread, NULL)) != 0)
    {
        fprintf(stderr, "pthread_key_create: %s\n", strerror(ret));
        goto end;
    }
    ret = 1;

  end:

    return ret;

}

#ifndef OPENSSL_NO_DYNAMIC_ENGINE
IMPLEMENT_DYNAMIC_BIND_FN(bind_qat) IMPLEMENT_DYNAMIC_CHECK_FN()
#endif

/******************************************************************************
* function:
*         qat_ciphers(ENGINE *e,
*                     const EVP_CIPHER **cipher,
*                     const int **nids,
*                     int nid)
*
* @param e      [IN] - OpenSSL engine pointer
* @param cipher [IN] - cipher structure pointer
* @param nids   [IN] - cipher function nids
* @param nid    [IN] - cipher operation id
*
* description:
*   Qat engine cipher operations registrar
******************************************************************************/
static int
qat_ciphers(ENGINE * e, const EVP_CIPHER ** cipher, const int **nids, int nid)
{

    int ok = 1;

    /* No specific cipher => return a list of supported nids ... */
    if (!cipher)
    {
        *nids = qat_cipher_nids;
        /* num ciphers supported (array/numelements -1) */
        return (sizeof(qat_cipher_nids) / sizeof(qat_cipher_nids[0]));
    }

    switch (nid)
    {
        case NID_aes_256_cbc:
            *cipher = &qat_aes_256_cbc;
            break;
        case NID_aes_192_cbc:
            *cipher = &qat_aes_192_cbc;
            break;
        case NID_aes_128_cbc:
            *cipher = &qat_aes_128_cbc;
            break;
        case NID_rc4:
            *cipher = &qat_rc4;
            break;
        case NID_des_ede3_cbc:
            *cipher = &qat_des_ede3_cbc;
            break;
        case NID_des_cbc:
            *cipher = &qat_des_cbc;
            break;
#ifdef NID_aes_128_cbc_hmac_sha1
        case NID_aes_128_cbc_hmac_sha1:
            *cipher = &qat_aes_128_cbc_hmac_sha1;
            break;
#endif
#ifdef NID_aes_256_cbc_hmac_sha1
        case NID_aes_256_cbc_hmac_sha1:
            *cipher = &qat_aes_256_cbc_hmac_sha1;
            break;
#endif
        default:
            ok = 0;
            *cipher = NULL;
    }

    return ok;
}

/******************************************************************************
* function:
*         qat_digests(ENGINE *e,
*                     const EVP_MD **digest,
*                     const int **nids,
*                     int nid)
*
* @param e      [IN] - OpenSSL engine pointer
* @param digest [IN] - digest structure pointer
* @param nids   [IN] - digest functions nids
* @param nid    [IN] - digest operation id
*
* description:
*   Qat engine digest operations registrar
******************************************************************************/
static int
qat_digests(ENGINE * e, const EVP_MD ** digest, const int **nids, int nid)
{
    int ok = 1;

    /* No specific cipher => return a list of supported nids ... */
    if (!digest)
    {
        *nids = qat_digest_nids;
        /* num digests supported (array/numelements -1) */
        return (sizeof(qat_digest_nids) / sizeof(qat_digest_nids[0]));
    }

    switch (nid)
    {
        case NID_sha1:
            /* printf("%s: saying we can do sha1\n", __func__); */
            *digest = &qat_sha1;
            break;
        case NID_sha256:
           /* printf("%s: saying we can do sha256\n", __func__); */
            *digest = &qat_sha256;
            break;
        case NID_sha512:
            /* printf("%s: saying we can do sha512\n", __func__); */
            *digest = &qat_sha512;
            break;
        case NID_md5:
            /* printf("%s: saying we can do md5\n", __func__); */
            *digest = &qat_md5;
            break;
        default:
            ok = 0;
            *digest = NULL;
    }

    return ok;
}

/* initialize Qat Engine if OPENSSL_NO_DYNAMIC_ENGINE*/
#ifdef OPENSSL_NO_DYNAMIC_ENGINE
static ENGINE *engine_qat(void)
{
    DEBUG("[%s] engine_qat\n", __func__);

    DEBUG("%s: About to set mem functions\n", __func__);

    if (access(QAT_DEV, F_OK) != 0)
    {
        perror("access");
        return NULL;
    }
#ifdef QAT_ZERO_COPY_MODE
    if (!CRYPTO_set_mem_ex_functions(qaeMemAlloc, qaeMemRealloc, qaeMemFree))
    {
        DEBUG("%s: CRYPTO_set_mem_functions failed\n", __func__);
        /* Don't abort. This may be tried from a few places and will only
           succeed the first time. */
    }
    else
    {
        DEBUG("%s: CRYPTO_set_mem_functions succeeded\n", __func__);
    }

    /*  If over-riding OPENSLL_malloc then buffers
     *  passed will already be pinned memory
     *  so we switch to zero copy mode
     */
    zero_copy_memory_mode = 1;
#endif

    ENGINE *ret = ENGINE_new();

    if (!ret)
        return NULL;

    if (!bind_qat(ret, engine_qat_id))
    {
        WARN("qat engine bind failed!\n");
        ENGINE_free(ret);
        return NULL;
    }

    return ret;
}

void ENGINE_load_qat(void)
{
    ENGINE *toadd = engine_qat();
    int err;

    DEBUG("[%s] engine_load_qat\n", __func__);

    if (!toadd)
        return;

    DEBUG("[%s] engine_load_qat adding\n", __func__);
    ENGINE_add(toadd);
    ENGINE_free(toadd);
    ERR_clear_error();
}

#endif

