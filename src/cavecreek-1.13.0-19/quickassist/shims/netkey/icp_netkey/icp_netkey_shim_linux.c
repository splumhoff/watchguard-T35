/******************************************************************************
 *
 * GPL LICENSE SUMMARY
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
 *  version: icp_qat_netkey.L.0.3.0-17
 *
 *****************************************************************************/

/*
 * icp_netkey_shim_linux.c
 *
 * This is an implementation of Linux Kernel Crypto API shim that uses
 * the Intel(R) Quick Assist API. The focus here is IPsec, (Netkey stack).
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <linux/scatterlist.h>
#include <linux/rtnetlink.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <crypto/authenc.h>
#include <crypto/aead.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <crypto/des.h>
#include <crypto/aes.h>
#include <crypto/md5.h>
#include <crypto/sha.h>
#include <crypto/algapi.h>
#include <crypto/rng.h>
/* Workaround for IXA00378497 - enc perf drop when append flag is set */
#include <crypto/scatterwalk.h>
#include <linux/sched.h>

#ifdef	CONFIG_WG_KERNEL_4_14
#include <crypto/skcipher.h>
#include <crypto/aead.h>
#include <crypto/internal/aead.h>
#endif

#include "cpa.h"
#include "cpa_cy_im.h"
#include "cpa_cy_sym.h"

// #define	SAMPLE_TEST		perform

#define	GCM_AAD_SIZE		(  8)
#define	GCM_NONCE_SIZE		(  4)
#define	GCM_ESP_IV_SIZE		(  8)
#define	GCM_IV_SIZE		(4+8)
#define	GCM_DIGEST_SIZE		( 16)

#define	GCM_HW			on
#define	DES_HW			on
#define	MD5_HW			on
#define	NULL_HW			on
#define	ABLK_HW			on
#define	AHASH_HW		on
#define	SHASH_HW		on

/* Define priority so that our algorithms take precedence over generic ones */
#define	CRA_PRIORITY		8086

#define	ICP_MAX_BLOCK_SIZE	AES_BLOCK_SIZE
#define	ICP_MAX_DIGEST_SIZE	SHA512_DIGEST_SIZE
#define	ICP_QAT_SRC_BUFF_ALIGN	8

/* This shim supports up to 8 buffers in a buffer list */
#define	ICP_MAX_NUM_BUFFERS	8

/* This is the max number of times we try to remove the session */
#define	ICP_RETRY_COUNT		10

/* This is the amount of time we wait between remove session re-tries */
#define	ICP_SESSION_REMOVE_TIME	2*HZ

//#define	PrintK   printk
#define	PrintK(...)

//#define	PrintHex(a,b)
#ifdef	CONFIG_WG_KERNEL_4_14
#define	PrintHex(a,b)	wg_dump_hex(a,b,"")
#else
#define	PrintHex(a,b)   wg_dump_hex_3_12(a,b,"")
static void inline      wg_dump_hex_3_12(const u8* buf, u32 len, const u8* tag)
{
  if (console_loglevel < 32) len = (len < 256) ? len : 256;

  if (buf)
  if (len > 0)
  print_hex_dump(KERN_CONT, tag, DUMP_PREFIX_OFFSET,
                 16, 1, buf, len, false);
}
#endif

static inline void* __xchg_ptr(void* x, volatile void** ptr)
{
  __asm__ __volatile__("xchg %0,%1"
                       :"=r" (x)
                       :"m" (*ptr), "0" (x)
                       :"memory");
  return x;
}

#define xchg_ptr(v,p) __xchg_ptr((void*)v,(volatile void**)p)

static int get_asize(const char* name)
{
  u8* type;

  if ((type = strstr(name, "sha")) != NULL) {
    u8  ch;
    int value = INT_MIN;

    type += 3;

    while ((ch = *type++) != 0)
    if ((ch >= '0') && (ch <= '9'))
      value = (value * 10) + (ch - '0');
    else
      break;

    if (value == 512) return SHA512_DIGEST_SIZE;
    if (value == 384) return SHA384_DIGEST_SIZE;
    if (value == 256) return SHA256_DIGEST_SIZE;
    if (value == 224) return SHA224_DIGEST_SIZE;
    if (value == 1)   return SHA1_DIGEST_SIZE;
  }
  else
  if ((type = strstr(name, "md5")) != NULL)
    return MD5_DIGEST_SIZE;

  return 0;
}

static inline int get_atype(int auth_len)
{
  if (likely(auth_len >= SHA512_DIGEST_SIZE))
    return CPA_CY_SYM_HASH_SHA512;
  if (likely(auth_len >= SHA384_DIGEST_SIZE))
    return CPA_CY_SYM_HASH_SHA384;
  if (likely(auth_len >= SHA256_DIGEST_SIZE))
    return CPA_CY_SYM_HASH_SHA256;
  if (likely(auth_len >= SHA224_DIGEST_SIZE))
    return CPA_CY_SYM_HASH_SHA224;
  if (likely(auth_len >= SHA1_DIGEST_SIZE))
    return CPA_CY_SYM_HASH_SHA1;
  if (likely(auth_len >= MD5_DIGEST_SIZE))
    return CPA_CY_SYM_HASH_MD5;

  return 0;
}

static inline int get_etype(int key_len, int iv_len)
{
  if (likely(iv_len >= 16))
    return CPA_CY_SYM_CIPHER_AES_CBC;

  if (likely(iv_len >= 8)) {
    if (likely(key_len >= 24))
      return CPA_CY_SYM_CIPHER_3DES_CBC;
    else
      return CPA_CY_SYM_CIPHER_DES_CBC;
  }

  return 0;
}

/* Structure for context created per SA */
struct icp_crypto_ctx {
  CpaInstanceHandle  instance;    /* qat instance assigned to this ctx */
  Cpa32U             authSize;    /* authentication  size */
  Cpa32U             metaSize;    /* meta data size */
  Cpa32U             sessionSize; /* session data size */
  Cpa32U             requestSize; /* request data size */
  struct mutex       mutexLock;   /* mutex lock on context */
  volatile
  CpaCySymSessionCtx encrypt_ctx; /* encrypt session */
  atomic_t           encrypt_key; /* flag if encrypt setkey */
  volatile
  CpaCySymSessionCtx decrypt_ctx; /* decrypt session */
  atomic_t           decrypt_key; /* flag if decrypt setkey */
  u8 salt[ICP_MAX_BLOCK_SIZE];    /* salt for iv generation */
  u32                gcm_mode;    /* flag  for GCM mode operations */
  u32                gcm_nonce;   /* nonce for GCM mode operations */
  atomic64_t         gcm_seqno;   /* seqno for GCM mode operations */
};

/* Structure for request ctx allocated per request */
struct icp_crypto_req_ctx {
  /* ptr to meta data memory required by src CpaBufferList */
  u8 *pMetaData;
  /* Src CpaBufferList */
  CpaBufferList srcList __attribute__ ((aligned(ICP_QAT_SRC_BUFF_ALIGN)));
  /* Flatbuffers to describe src data */
  CpaFlatBuffer srcFlatBuffers[ICP_MAX_NUM_BUFFERS];
  /* Workaround for IXA00378497 - enc performance drops if append flag set */
  /* Used if digest is scattered over multiple buffers */
  u8 digestBuffer[ICP_MAX_DIGEST_SIZE];
  CpaCySymOpData sym_op_data; /* QA API request */
  /* Workaround for IXA00378497 - enc performance drops if append flag set */
  u8 digestCpy; /* if set digest must copy from digestBuffer in callback */
  u8 *result;
  u32 authSize; /* authSize required in callback if digestCpy is set */
  u32 cryptlen;
  u32 assoclen;
  struct scatterlist *assoc;
  struct scatterlist *src;
  struct scatterlist *dst;
  struct crypto_async_request *async_req;
  u8  gcm_aad[AES_BLOCK_SIZE];
  u8  gcm_iv[AES_BLOCK_SIZE];
};

/* Global memory containing iv=0 */
static u8 pIvZero[ICP_MAX_BLOCK_SIZE] = { 0 }; /* iv=0 for geniv case */

/* Structure for storing info on instances used by QAT */
struct icp_qat_instances {
  CpaInstanceHandle *instances;  /* Pointer to array of instance handles */
  u32 ctr;   /* Used to allocate instances in a round robin fashion */
  u32 total; /* Total number of instances available */
};

/* Global instance info - set at module init */
struct icp_qat_instances qat_instances = { 0 };

static DEFINE_SPINLOCK(qatInstLock); /* to make instance allocation thread safe */

CpaInstanceHandle get_instance(void)
{
  CpaInstanceHandle cpaInst;

  spin_lock(&qatInstLock);
  cpaInst = qat_instances.instances[(qat_instances.ctr++) %
                                    qat_instances.total];
  spin_unlock(&qatInstLock);
  return cpaInst;
}

/*  Callback function */
static void symCallback(void *pCallback,
                        CpaStatus status,
                        const CpaCySymOp operationType,
                        void *pOpData, CpaBufferList * pDstBuffer,
                        CpaBoolean verifyResult)
{
  int                          res       = -EBADMSG;
  struct icp_crypto_req_ctx   *req_ctx   = (struct icp_crypto_req_ctx *)pCallback;
  struct crypto_async_request *async_req = req_ctx->async_req;

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: type %d status %d clen %d alen %d\n", __FUNCTION__,
         operationType, status, req_ctx->cryptlen, req_ctx->authSize);

  if (unlikely(CPA_STATUS_SUCCESS != status)) {
    printk(KERN_ERR "%s: symCallback failed, status = (%d)\n", __FUNCTION__,
           status);
  } else {
    static unsigned verifyFails = 0;

    if (likely(CPA_TRUE == verifyResult))
      /* set the result to success */
      res = verifyFails = 0;
    else
    if (((++verifyFails) & 15) == 0)
      printk(KERN_DEBUG "%s: verifyResult == CPA_FALSE (%d)\n", __FUNCTION__,
             verifyResult);

    /* Workaround for IXA00378497 - enc performance drops if append flag set */
    if (unlikely(req_ctx->digestCpy)) {

      scatterwalk_map_and_copy(req_ctx->digestBuffer,
                               req_ctx->src, req_ctx->cryptlen,
                               req_ctx->authSize, 1);
    }

    if (unlikely(console_loglevel & 16)) {
      printk(KERN_DEBUG "%s: dst   %p %5d\n", __FUNCTION__,
               sg_virt(req_ctx->src), req_ctx->cryptlen + req_ctx->authSize);
      PrintHex(sg_virt(req_ctx->src), req_ctx->src->length);
    }
  }

  async_req->complete(async_req, res);
}

/* Function used to convert scatterlist to bufferlist */
static int sl_to_bl(CpaBufferList *pList,
                    const struct scatterlist *const pSrc)
{
  struct scatterlist *pCurr = (struct scatterlist *)pSrc;

  while (pCurr) {

    if (unlikely(pList->numBuffers >= ICP_MAX_NUM_BUFFERS))
      break;

    pList->pBuffers[pList->numBuffers].pData = sg_virt(pCurr);
    pList->pBuffers[pList->numBuffers].dataLenInBytes =  pCurr->length;
    pList->numBuffers++;

    pCurr = sg_next(pCurr);
  }

  if (likely(pList->numBuffers < ICP_MAX_NUM_BUFFERS))
    return 0;

  printk(KERN_ERR "%s: numBuffers in BufferList has exceeded max "
         "value of %d\n", __FUNCTION__, ICP_MAX_NUM_BUFFERS);
  return -ENOMEM;
}

/* Initialise CpaBufferList based on pAssoc, iv and pSrc value */
static int icp_init_from_assoc_iv_src(struct icp_crypto_req_ctx *const opdata,
                                      struct scatterlist *const pAssoc,
                                      struct scatterlist *const pSrc,
                                      const  Cpa8U        const *iv,
                                      const  Cpa32U             iv_len)
{
  CpaBufferList *pBufflist    = &(opdata->srcList);

  pBufflist->pPrivateMetaData = opdata->pMetaData;
  pBufflist->pBuffers         = opdata->srcFlatBuffers;

  /* If pAssoc, iv and pSrc are described in 1 contiguous section of
     memory then we should create the CpaBufferList with 1 buffer to
     ensure the optimized path is chosen */
  /* iv is just after the assoc data */
  if (likely(iv))
  if (likely(pAssoc))
  if (likely((sg_virt(pAssoc) + pAssoc->length) == iv))
  if (likely((iv + iv_len) == sg_virt(pSrc))) /* src data is just after iv */
  if (likely(NULL == sg_next(pSrc))) {        /* src data in 1 sg */

    /* Can use optimized path for better performance */
    pBufflist->numBuffers                  = 1;
    pBufflist->pBuffers                    = opdata->srcFlatBuffers;
    pBufflist->pBuffers[0].pData           = sg_virt(pAssoc);
    pBufflist->pBuffers[0].dataLenInBytes  = pAssoc->length;
    pBufflist->pBuffers[0].dataLenInBytes += (iv_len + pSrc->length);

    return 0;
  }

  pBufflist->numBuffers = 0;

  /* up to 2 buffers will be intialised before calling
     sl_to_bl: pAssoc and pIV */

  if (likely(pAssoc)) {
    pBufflist->pBuffers[0].pData          = sg_virt(pAssoc);
    pBufflist->pBuffers[0].dataLenInBytes = pAssoc->length;
    pBufflist->numBuffers++;
  }

  if (likely(iv)) {
    pBufflist->pBuffers[1].pData          = (Cpa8U *)iv;
    pBufflist->pBuffers[1].dataLenInBytes = iv_len;
    pBufflist->numBuffers++;
  }

  return sl_to_bl(pBufflist, pSrc);
}

/* Perform sym operation */
static int perform_sym_op(struct crypto_async_request   *async_req,
                          struct icp_crypto_ctx         *ctx,
                          struct icp_crypto_req_ctx     *pReqCtx,
                          Cpa8U  *iv, const Cpa32U       iv_len,
                          const  CpaCySymCipherDirection cipherDirection,
                          CpaBoolean                     geniv)
{
  CpaStatus          status;
  Cpa32U             messageLen;
  CpaCySymSessionCtx pSessionCtx;
  struct scatterlist *pAssoc = pReqCtx->assoc;
  struct scatterlist *pSrc   = pReqCtx->src;

  if (unlikely(console_loglevel & 16)) if (0)
  printk(KERN_DEBUG "%s: assoc %d ivlen %d cryptlen %d\n", __FUNCTION__,
         pReqCtx->assoclen, iv_len, pReqCtx->cryptlen);

  /* Workaround for IXA00378497 - enc performance drops if append flag set */

  if (cipherDirection == CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT)
    pSessionCtx = ctx->encrypt_ctx;
  else
    pSessionCtx = ctx->decrypt_ctx;

  pReqCtx->async_req = async_req;

  /* space for meta data after icp_crypto_req_ctx*/
  pReqCtx->pMetaData = ((u8 *) pReqCtx) + sizeof(struct icp_crypto_req_ctx);

  /* Describe input SGLs as one CpaBufferList */
#ifdef	CONFIG_WG_KERNEL_4_14
  pReqCtx->authSize  = ctx->authSize;
  pReqCtx->cryptlen -= iv_len;

  if (unlikely(wg_fips_iv))
  if (unlikely(ctx->authSize == 0))
  if (atomic_read(&ctx->encrypt_key) != 0) {
    pReqCtx->assoclen -= iv_len;
    pReqCtx->cryptlen += iv_len;
  }

  if (likely(!pAssoc))
  if (likely(pReqCtx->assoclen > 0)) {
    struct scatterlist assocTmp;
    memcpy(pAssoc = &assocTmp, pSrc, sizeof(assocTmp));

    if (CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT == cipherDirection) {
      if (likely(!ctx->gcm_mode)) {
        pReqCtx->assoclen -= iv_len;
        pReqCtx->cryptlen += iv_len;
      }
      memcpy(iv, sg_virt(pAssoc) + pReqCtx->assoclen,     iv_len);
    } else {
      memcpy(    sg_virt(pAssoc) + pReqCtx->assoclen, iv, iv_len);
    }

    iv         = sg_virt(pAssoc) + pReqCtx->assoclen;
  }

  if (likely(pAssoc)) pAssoc->length = pReqCtx->assoclen;

  pSrc->offset += (pReqCtx->assoclen + iv_len);
  pSrc->length  =  pReqCtx->cryptlen + ctx->authSize;;

  if (CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT == cipherDirection) geniv = iv_len;
#endif

  if (unlikely(icp_init_from_assoc_iv_src(pReqCtx, pAssoc, pSrc, iv, iv_len)))
    return -ENOMEM;

  /* Workaround for IXA00378497 - enc performance drops if append flag set */
  pReqCtx->digestCpy = 0;

  /* Setup CpaCySymOpData */
  pReqCtx->sym_op_data.packetType                      = CPA_CY_SYM_PACKET_TYPE_FULL;
  pReqCtx->sym_op_data.sessionCtx                      = pSessionCtx;

  pReqCtx->authSize                                    = ctx->authSize;

  if (!ctx->gcm_mode) {

    pReqCtx->sym_op_data.ivLenInBytes                  = iv_len;
    pReqCtx->sym_op_data.pAdditionalAuthData           = NULL;

    pReqCtx->sym_op_data.hashStartSrcOffsetInBytes     = 0;
    pReqCtx->sym_op_data.cryptoStartSrcOffsetInBytes   = pReqCtx->assoclen;
    pReqCtx->sym_op_data.messageLenToHashInBytes       = pReqCtx->assoclen;

  } else {

    memcpy(&pReqCtx->gcm_iv[0], &ctx->gcm_nonce, GCM_NONCE_SIZE);
    pReqCtx->sym_op_data.ivLenInBytes                  = GCM_IV_SIZE;
    pReqCtx->sym_op_data.pIv                           = pReqCtx->gcm_iv;

    memcpy(pReqCtx->gcm_aad, sg_virt(pAssoc), pReqCtx->assoclen);
    pReqCtx->sym_op_data.pAdditionalAuthData           = pReqCtx->gcm_aad;

    pReqCtx->sym_op_data.hashStartSrcOffsetInBytes     = 0;
    pReqCtx->sym_op_data.cryptoStartSrcOffsetInBytes   = pReqCtx->assoclen + iv_len;
    pReqCtx->sym_op_data.messageLenToHashInBytes       = 0;
    pReqCtx->sym_op_data.messageLenToCipherInBytes     = pReqCtx->cryptlen;
  }

  messageLen = pReqCtx->cryptlen;

  if (CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT == cipherDirection) {

    geniv = geniv ? iv_len : 0;

    /* Generating IV - set iv=0 and encrypt an extra block */
    if (!ctx->gcm_mode) {
      pReqCtx->sym_op_data.pIv                       = geniv ? pIvZero : iv;
      pReqCtx->sym_op_data.messageLenToCipherInBytes = (messageLen + geniv);
      pReqCtx->sym_op_data.messageLenToHashInBytes  += (messageLen + geniv);
    } else {
      u64 seqno64  =  atomic64_inc_return(&ctx->gcm_seqno);
      memcpy(iv,                         &seqno64, iv_len);
      memcpy(&pReqCtx->gcm_iv[GCM_NONCE_SIZE], iv, iv_len);
    }

    if (unlikely(wg_fips_iv)) {
      memcpy(iv,  wg_fips_iv, iv_len);

      if (!ctx->gcm_mode) {
        pReqCtx->sym_op_data.pIv                          = iv;
        pReqCtx->sym_op_data.cryptoStartSrcOffsetInBytes += iv_len;
        pReqCtx->sym_op_data.messageLenToCipherInBytes   -= iv_len;
      } else
        memcpy(&pReqCtx->gcm_iv[GCM_NONCE_SIZE], iv, iv_len);

      wg_fips_iv = NULL;
    }

  } else

  if (CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT == cipherDirection) {

    messageLen -= ctx->authSize;

    /* Use given iv */
    if (!ctx->gcm_mode) {
      pReqCtx->sym_op_data.pIv                          = iv;
      pReqCtx->sym_op_data.cryptoStartSrcOffsetInBytes += iv_len;
      pReqCtx->sym_op_data.messageLenToCipherInBytes    = messageLen;
      pReqCtx->sym_op_data.messageLenToHashInBytes     += (iv_len + messageLen);
    } else {
      pReqCtx->sym_op_data.messageLenToCipherInBytes   -= ctx->authSize;
      memcpy(&pReqCtx->gcm_iv[GCM_NONCE_SIZE], iv, iv_len);
    }

  }

  if (unlikely(console_loglevel & 16)) {
    printk(KERN_DEBUG "%s: coff %2d clen %2d hlen %2d assoc %2d giv %2d\n",
	   __FUNCTION__,
           pReqCtx->sym_op_data.cryptoStartSrcOffsetInBytes,
           pReqCtx->sym_op_data.messageLenToCipherInBytes,
           pReqCtx->sym_op_data.messageLenToHashInBytes,
           pReqCtx->assoclen, geniv);

    if (pAssoc) {
      printk(KERN_DEBUG "%s: assoc %p %5d\n", __FUNCTION__,
               sg_virt(pAssoc), pReqCtx->assoclen);
      PrintHex(sg_virt(pAssoc), pReqCtx->assoclen);
    }

    if (iv) {
      printk(KERN_DEBUG "%s: iv    %p %5d\n", __FUNCTION__,
               iv, iv_len);
      PrintHex(iv, iv_len);
    }

    if (ctx->gcm_mode) {
      printk(KERN_DEBUG "%s: salt  %p %5d\n", __FUNCTION__,
               pReqCtx->sym_op_data.pIv, GCM_IV_SIZE);
      PrintHex(pReqCtx->sym_op_data.pIv, GCM_IV_SIZE);
    }

    {
      printk(KERN_DEBUG "%s: src   %p %5d\n", __FUNCTION__,
               sg_virt(pReqCtx->src), pReqCtx->sym_op_data.messageLenToCipherInBytes);
      PrintHex(sg_virt(pReqCtx->src), pReqCtx->src->length);
    }
  }

  /* Workaround for IXA00378497 - enc performance drops if append flag set */
  /* The pDigestResult needs to be set even when digestIsAppended is true,
   * otherwise the performance test will not complete IXA00373239 */
  pReqCtx->sym_op_data.pDigestResult = pReqCtx->result;

  /* Check for multiple buffers */
  if (unlikely(pReqCtx->srcList.numBuffers > 1)) {
    u32                 offset = 0;
    struct scatterlist *pCurr  = pReqCtx->src;

    /* Find digest location */
    for (;;) {
      if (!pCurr) {
        printk(KERN_EMERG "%s: Null data len %d off %d num %d\n",
               __FUNCTION__, messageLen, offset, pReqCtx->srcList.numBuffers);
        return -EFAULT;
      }
      if (messageLen < offset + pCurr->length)
        break;

      offset += pCurr->length;
      pCurr = sg_next(pCurr);
    }

    /* Ensure digest is in 1 buffer */
    if (messageLen + ctx->authSize >
        offset + pCurr->length) {

      pReqCtx->sym_op_data.pDigestResult =  pReqCtx->digestBuffer;
      if (CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT == cipherDirection) {
        /* need to copy here */
        scatterwalk_map_and_copy(pReqCtx->digestBuffer,
                                 pReqCtx->src, messageLen,
                                 ctx->authSize,
                                 0);
      } else {
        pReqCtx->digestCpy = 1;
        pReqCtx->authSize  = ctx->authSize;
      }
    } else {
      pReqCtx->sym_op_data.pDigestResult  = sg_virt(pCurr);
      pReqCtx->sym_op_data.pDigestResult += (messageLen - offset);
    }

  } else {
    if (likely(!pReqCtx->sym_op_data.pDigestResult))
      pReqCtx->sym_op_data.pDigestResult = sg_virt(pReqCtx->src) + messageLen;
  }

  if (unlikely(console_loglevel & 16)) {
    printk(KERN_DEBUG "%s: flat  %p %5d\n", __FUNCTION__,
             pReqCtx->srcList.pBuffers[0].pData, pReqCtx->srcList.pBuffers[0].dataLenInBytes);
    PrintHex(pReqCtx->srcList.pBuffers[0].pData, pReqCtx->srcList.pBuffers[0].dataLenInBytes);
  }

  if (likely(pReqCtx->dst == pReqCtx->src)) {
    status = cpaCySymPerformOp(ctx->instance, (void *)pReqCtx,
                               &(pReqCtx->sym_op_data),
                               &(pReqCtx->srcList),
                               &(pReqCtx->srcList),
                               NULL);
    if (CPA_STATUS_RETRY == status) {
      return -EBUSY;
    }
    if (CPA_STATUS_SUCCESS != status) {
      printk(KERN_ERR "%s: cpaCySymPerformOp failed. (status = (%d))\n",
             __FUNCTION__, status);
      return -EINVAL;
    }
  } else {
    printk(KERN_ERR "%s: Out of place processing not supported\n",
           __FUNCTION__);
    return -EINVAL;
  }

  return -EINPROGRESS;
}

/* This function is used to remove encrypt/decrypt session */
static void icp_crypto_free(struct icp_crypto_ctx *ctx,
                            volatile CpaCySymSessionCtx *pSession,
                            atomic_t *pSetKey)
{
  int                count;
  CpaStatus          status;
  CpaCySymSessionCtx session;

  session = xchg_ptr(NULL, pSession);

  if (!session) return;

  status = CPA_STATUS_SUCCESS;

  /* Free session */

  if (atomic_dec_return(pSetKey) < 0)
    atomic_set(pSetKey, 0);
  else
  for (count = ICP_RETRY_COUNT;
       (status = cpaCySymRemoveSession(ctx->instance, session))
         == CPA_STATUS_RETRY;) {
    /* Count exhausted? */
    if (--count <= 0) break;

    /* Wait a while before retry */
    set_current_state((long)TASK_INTERRUPTIBLE);
    schedule_timeout(ICP_SESSION_REMOVE_TIME);
  }

  if (CPA_STATUS_SUCCESS != status)
    printk(KERN_ERR "%s: Failed to remove session\n", __FUNCTION__);

  kfree(session);
}

/* This function is used to remove encrypt/decrypt session */
static void icp_crypto_exit(struct crypto_tfm *tfm)
{
  struct icp_crypto_ctx *ctx = (struct icp_crypto_ctx *)crypto_tfm_ctx(tfm);

  mutex_lock(&ctx->mutexLock);

  icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
  icp_crypto_free(ctx, &ctx->decrypt_ctx, &ctx->decrypt_key);

  mutex_unlock(&ctx->mutexLock);
}

/* This function is used to set up icp_crypto_ctx */
static int icp_crypto_init(struct crypto_tfm *tfm, unsigned int *pReqSize)
{
  CpaStatus                status;
  CpaCySymSessionSetupData Setup = { 0 };
  struct icp_crypto_ctx   *ctx   = crypto_tfm_ctx(tfm);

  /* Set up the mutex */
  mutex_init(&ctx->mutexLock);

  /* Clear setkey flags */
  atomic_set(&ctx->encrypt_key, 0);
  atomic_set(&ctx->decrypt_key, 0);

  /* Get instance */
  ctx->instance = get_instance();

  /* get meta data size */
  ctx->metaSize = 0;
  status = cpaCyBufferListGetMetaSize(ctx->instance, ICP_MAX_NUM_BUFFERS,
                                      &ctx->metaSize);
  if (CPA_STATUS_SUCCESS != status) {
    printk(KERN_ERR "%s: Failed to get meta size\n", __FUNCTION__);
    return -EINVAL;
  }

  ctx->requestSize = sizeof(struct icp_crypto_req_ctx) + ctx->metaSize;
  if (pReqSize) *pReqSize = ctx->requestSize;

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: ctx %p size %d\n", __FUNCTION__,
         ctx, ctx->requestSize);

  /* alloc session memory */
  /* Note: for current driver the session size does not depend on
   *  the setup data */
  ctx->sessionSize = 0;
  status = cpaCySymSessionCtxGetSize(ctx->instance, &Setup,
                                     &ctx->sessionSize);
  if (CPA_STATUS_SUCCESS != status) {
    printk(KERN_ERR "%s: Failed to get session size\n", __FUNCTION__);
    return -EINVAL;
  }

  return 0;
}

/* This function is used to set up icp_crypto_ctx */
#ifdef	CONFIG_WG_KERNEL_4_14
static void aead_exit_tfm(struct crypto_aead *tfm)
{
  return icp_crypto_exit(&tfm->base);
}
static int  aead_init_tfm(struct crypto_aead *tfm)
{
  return icp_crypto_init(&tfm->base, &tfm->reqsize);
}
#else
static int  aead_init_tfm(struct crypto_tfm *tfm)
{
  return icp_crypto_init(tfm, &tfm->crt_aead.reqsize);
}
#endif

/* This function initialises encrypt and decrypt sessions with the key */
static int aead_setkey(struct crypto_aead *aead_tfm,
                const Cpa8U * key, unsigned int keylen)
{
  int op    = 0;
  int chain = 0;

  struct rtattr                   *pRta   = (void *)key;
  struct crypto_authenc_key_param *pParam = NULL;
  struct icp_crypto_ctx           *ctx    = crypto_aead_ctx(aead_tfm);
  struct crypto_tfm               *tfm    = crypto_aead_tfm(aead_tfm);
  const  char                     *name   = crypto_tfm_alg_name(tfm);

  unsigned int enckeylen  = 0;
  unsigned int authkeylen = 0;
  unsigned int iv_len     = crypto_aead_ivsize(aead_tfm);

  CpaStatus                status;
  CpaCySymCipherAlgorithm  cipherAlgorithm;
  CpaCySymHashAlgorithm    hashAlgorithm;
  CpaCySymSessionSetupData Setup = { 0 };

  icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
  icp_crypto_free(ctx, &ctx->decrypt_ctx, &ctx->decrypt_key);

  ctx->encrypt_ctx = kzalloc(ctx->sessionSize, GFP_KERNEL);
  if (NULL == ctx->encrypt_ctx) {
    printk(KERN_ERR "%s: Failed to alloc encrypt session\n", __FUNCTION__);
    return -ENOMEM;
  }

  ctx->decrypt_ctx = kzalloc(ctx->sessionSize, GFP_KERNEL);
  if (NULL == ctx->decrypt_ctx) {
    printk(KERN_ERR "%s: Failed to alloc decrypt session\n", __FUNCTION__);
    kfree(ctx->encrypt_ctx);
    ctx->encrypt_ctx = NULL;
    return -ENOMEM;
  }

  /* Check for GCM */
  if ((ctx->gcm_mode = (strstr(name, "gcm") != NULL))) {

    if ((enckeylen = keylen) < GCM_NONCE_SIZE)
      return -EINVAL;

    memcpy(&ctx->gcm_nonce, &key[enckeylen -= GCM_NONCE_SIZE], GCM_NONCE_SIZE);

    cipherAlgorithm = CPA_CY_SYM_CIPHER_AES_GCM;
    hashAlgorithm   = CPA_CY_SYM_HASH_AES_GCM;

    ctx->authSize   = GCM_DIGEST_SIZE;

    if (unlikely(console_loglevel & 16)) {
      printk(KERN_DEBUG "%s: ctx %p authsize %d name %s\n",
             __FUNCTION__, ctx, ctx->authSize, name);
      printk(KERN_DEBUG "%s: ctx %p ivlen %2d key %p klen %2d nonce %d\n",
             __FUNCTION__, ctx, iv_len, key, keylen, GCM_NONCE_SIZE);
      PrintHex((u8*)&ctx->gcm_nonce, GCM_NONCE_SIZE);
    }

  }
  else
  {

  /* The key we receive is the concatenation of:
     - flags (length is RTA_ALIGN(pRta->rta_len))
     - authentication key (length can be computed)
     - cipher key (length is given by the flags)
  */
  if (!RTA_OK(pRta, keylen)) {
    crypto_aead_set_flags(aead_tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
    printk(KERN_ERR "%s: Bad key\n", __FUNCTION__);
    return -EINVAL;
  }

  if (pRta->rta_type != CRYPTO_AUTHENC_KEYA_PARAM) {
    crypto_aead_set_flags(aead_tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
    printk(KERN_ERR "%s: Bad key rta_type\n", __FUNCTION__);
    return -EINVAL;
  }

  if (RTA_PAYLOAD(pRta) < sizeof(*pParam)) {
    crypto_aead_set_flags(aead_tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
    printk(KERN_ERR "%s: Bad key rta_payload\n", __FUNCTION__);
    return -EINVAL;
  }

  pParam     = RTA_DATA(pRta);

  enckeylen  = be32_to_cpu(pParam->enckeylen);
  key       += RTA_ALIGN(pRta->rta_len);
  keylen    -= RTA_ALIGN(pRta->rta_len);
  authkeylen = keylen - enckeylen;

  if (unlikely(console_loglevel & 16)) {
  printk(KERN_DEBUG "%s: ctx %p authsize %d asize %d name %s\n",
         __FUNCTION__, ctx, ctx->authSize, get_asize(name), name);
  printk(KERN_DEBUG "%s: ctx %p ivlen %2d key %p elen %2d alen %2d\n",
         __FUNCTION__, ctx, iv_len, key, enckeylen, authkeylen);
  }

  if (keylen < enckeylen) {
    crypto_aead_set_flags(aead_tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
    printk(KERN_ERR "%s: Bad key, enckeylen is greater than keylen\n",
           __FUNCTION__);
    return -EINVAL;
  }

  cipherAlgorithm = get_etype(enckeylen, iv_len);
  hashAlgorithm   = get_atype(get_asize(name));

  }

  if (likely((cipherAlgorithm > 0) && (hashAlgorithm > 0)))
    op = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
  else
  if (likely(cipherAlgorithm > 0))
    op = CPA_CY_SYM_OP_CIPHER;
  else
  if (likely(hashAlgorithm > 0))
    op = CPA_CY_SYM_OP_HASH;
  else
    return 0;

  if (likely(op == CPA_CY_SYM_OP_ALGORITHM_CHAINING))
    chain = -1;

  if (unlikely(console_loglevel & 16)) {
  printk(KERN_DEBUG "%s: ctx %p ealg %d aalg %d chain %d\n",
         __FUNCTION__, ctx, cipherAlgorithm, hashAlgorithm, chain);
  PrintHex(key,           authkeylen);
  PrintHex(key+authkeylen, enckeylen);
  }

  /* Get salt */
  if (iv_len > 0) {
    int res = crypto_rng_get_bytes(crypto_default_rng, ctx->salt, iv_len);

    if (res != 0) {
      printk(KERN_ERR "%s: crypto_rng_get_bytes error\n", __FUNCTION__);
      return res;
    }

    memcpy(&ctx->gcm_seqno, &ctx->salt, GCM_ESP_IV_SIZE);
  }

  /* Initialise sessions */

  Setup.sessionPriority = CPA_CY_PRIORITY_HIGH;

  Setup.symOperation = op;

  if (likely(cipherAlgorithm > 0)) {
    Setup.cipherSetupData.cipherAlgorithm = cipherAlgorithm;
    Setup.cipherSetupData.pCipherKey = (Cpa8U *) key + authkeylen;
    Setup.cipherSetupData.cipherKeyLenInBytes = enckeylen;
  }

  if (likely(hashAlgorithm > 0)) {
    Setup.hashSetupData.hashAlgorithm = hashAlgorithm;
    Setup.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
    if (hashAlgorithm == CPA_CY_SYM_HASH_AES_GCM) {
    Setup.hashSetupData.authModeSetupData.aadLenInBytes = GCM_AAD_SIZE;
    if (unlikely(wg_fips_aad_len)) {
    Setup.hashSetupData.authModeSetupData.aadLenInBytes = wg_fips_aad_len;
    wg_fips_aad_len = 0;
    }
    } else {
    Setup.hashSetupData.authModeSetupData.authKey = (Cpa8U *)key;
    Setup.hashSetupData.authModeSetupData.authKeyLenInBytes = authkeylen;
    }
    Setup.hashSetupData.digestResultLenInBytes = ctx->authSize;
  }

  /* encrypt first */
  Setup.cipherSetupData.cipherDirection = CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;
  Setup.algChainOrder = CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH & chain;

/* Workaround for IXA00378497 - enc performance drop when append flag is set */
/* Using the same append config for encrypt/decrypt */
  Setup.digestIsAppended = CPA_FALSE;
  Setup.verifyDigest     = CPA_FALSE;

  /* Lock the context */
  mutex_lock(&ctx->mutexLock);

  /* Initialize the cipher enc session */
  status = cpaCySymInitSession(ctx->instance, symCallback,
                               &Setup, ctx->encrypt_ctx);
  if (CPA_STATUS_SUCCESS != status) {
    /* Unlock the context */
    icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
    mutex_unlock(&ctx->mutexLock);

    printk(KERN_ERR "%s: Failed to initialize encryption session\n",
           __FUNCTION__);
    return -EINVAL;
  }

  /* Set that encrypt key is set up */
  atomic_set(&ctx->encrypt_key, 1);

  /* now setup the decrypt session */
  Setup.cipherSetupData.cipherDirection = CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;
  Setup.algChainOrder = CPA_CY_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER & chain;

  Setup.verifyDigest = CPA_TRUE;

  /* Initialize the cipher decrypt session */
  status = cpaCySymInitSession(ctx->instance, symCallback,
                               &Setup, ctx->decrypt_ctx);
  if (CPA_STATUS_SUCCESS != status) {
    /* Unlock the context */
    icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
    icp_crypto_free(ctx, &ctx->decrypt_ctx, &ctx->decrypt_key);
    mutex_unlock(&ctx->mutexLock);

    printk(KERN_ERR "%s: Failed to initialize decryption session\n",
           __FUNCTION__);
    return -EINVAL;
  }

  /* Set that decrypt key is set up */
  atomic_set(&ctx->decrypt_key, 1);

  /* Unlock the context */
  mutex_unlock(&ctx->mutexLock);

  return 0;
}

/* This function set the authsize */
static int aead_setauthsize(struct crypto_aead *tfm, unsigned int authsize)
{
  struct icp_crypto_ctx *ctx = (struct icp_crypto_ctx *)crypto_aead_ctx(tfm);

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: tfm %p ctx %p authsize %2d\n", __FUNCTION__,
         tfm, ctx, authsize);

  ctx->authSize = authsize;

  return 0;
}

/* This function initiates encryption/decryption */
static int aead_perform(struct aead_request *req, u8 *iv,
                        int direction, int giv)
{
  struct crypto_aead        *aead_tfm = crypto_aead_reqtfm(req);
  struct icp_crypto_ctx     *ctx      = crypto_aead_ctx(aead_tfm);
  struct icp_crypto_req_ctx *req_ctx  = aead_request_ctx(req);

  req_ctx->cryptlen = req->cryptlen;
  req_ctx->assoclen = req->assoclen;
#ifdef	CONFIG_WG_KERNEL_4_14
  req_ctx->assoc    = NULL;
#else
  req_ctx->assoc    = req->assoc;
#endif
  req_ctx->src      = req->src;
  req_ctx->dst      = req->dst;
  req_ctx->result   = NULL;

  return perform_sym_op(&req->base, ctx, req_ctx,
                        iv, crypto_aead_ivsize(aead_tfm),
                        direction, giv);
}

/* This function initiates decryption */
static int aead_encrypt(struct aead_request *req)
{
  return aead_perform(req, req->iv,
                      CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT, 0);
}

/* This function initiates decryption */
static int aead_decrypt(struct aead_request *req)
{
  return aead_perform(req, req->iv,
                      CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT, 0);
}

#ifndef	CONFIG_WG_KERNEL_4_14
/* Generate IV encryption */
static int aead_givencrypt(struct aead_givcrypt_request *giv_req)
{
  struct aead_request *req     = &(giv_req->areq);
  struct crypto_aead *aead_tfm = crypto_aead_reqtfm(req);
  struct icp_crypto_ctx *ctx   = crypto_aead_ctx(aead_tfm);
  int iv_len                   = crypto_aead_ivsize(aead_tfm);
  u64 seq                      = cpu_to_be64(giv_req->seq);

  memcpy(giv_req->giv, ctx->salt, iv_len);
  memcpy(giv_req->giv + iv_len - sizeof(seq), &seq, sizeof(seq));

  return aead_perform(req, giv_req->giv,
                      CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT, 1);
}
#endif

#ifdef	CONFIG_WG_KERNEL_4_14
static struct icp_aead {
  struct aead_alg aead;
} icp_aeads[] = {
#ifdef	GCM_HW
{
  .aead = { .base = {
    .cra_name		= "rfc4106(gcm(aes))",
    .cra_driver_name	= "rfc4106_gcm_aes_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= GCM_ESP_IV_SIZE,
    .maxauthsize	= GCM_DIGEST_SIZE,
  },
},
#endif
#ifdef	NULL_HW
{
  .aead = { .base = {
    .cra_name		= "authenc(digest_null,cbc(cipher_null))",
    .cra_driver_name	= "authenc_digest_null_cbc_cipher_null_icp",
    .cra_blocksize	= 1,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= 000*000,
    .maxauthsize	= 000*000,
  },
},
#endif
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha1),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= 000*000,
    .maxauthsize	= SHA1_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha256),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= 000*000,
    .maxauthsize	= SHA256_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha384),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= 000*000,
    .maxauthsize	= SHA384_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha512),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= 000*000,
    .maxauthsize	= SHA512_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(digest_null,cbc(des3_ede))",
    .cra_driver_name	= "authenc_digest_null_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES3_EDE_BLOCK_SIZE,
    .maxauthsize	= 000*000,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(digest_null,cbc(aes))",
    .cra_driver_name	= "authenc_digest_null_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= AES_BLOCK_SIZE,
    .maxauthsize	= 000*000,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha1),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES3_EDE_BLOCK_SIZE,
    .maxauthsize	= SHA1_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha256),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES3_EDE_BLOCK_SIZE,
    .maxauthsize	= SHA256_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha384),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES3_EDE_BLOCK_SIZE,
    .maxauthsize	= SHA384_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha512),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES3_EDE_BLOCK_SIZE,
    .maxauthsize	= SHA512_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha1),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= AES_BLOCK_SIZE,
    .maxauthsize	= SHA1_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha256),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= AES_BLOCK_SIZE,
    .maxauthsize	= SHA256_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha384),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= AES_BLOCK_SIZE,
    .maxauthsize	= SHA384_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha512),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= AES_BLOCK_SIZE,
    .maxauthsize	= SHA512_DIGEST_SIZE,
  },
},
#ifdef	MD5_HW
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(md5),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= 000*000,
    .maxauthsize	= MD5_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(md5),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES3_EDE_BLOCK_SIZE,
    .maxauthsize	= MD5_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(md5),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= AES_BLOCK_SIZE,
    .maxauthsize	= MD5_DIGEST_SIZE,
  },
},
#endif
#ifdef	DES_HW
{
  .aead = { .base = {
    .cra_name		= "authenc(digest_null,cbc(des))",
    .cra_driver_name	= "authenc_digest_null_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES_BLOCK_SIZE,
    .maxauthsize	= 000*000
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha1),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES_BLOCK_SIZE,
    .maxauthsize	= SHA1_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha256),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES_BLOCK_SIZE,
    .maxauthsize	= SHA256_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha384),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES_BLOCK_SIZE,
    .maxauthsize	= SHA384_DIGEST_SIZE,
  },
},
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(sha512),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES_BLOCK_SIZE,
    .maxauthsize	= SHA512_DIGEST_SIZE,
  },
},
#ifdef	MD5_HW
{
  .aead = { .base = {
    .cra_name		= "authenc(hmac(md5),cbc(des))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    },
    .ivsize		= DES_BLOCK_SIZE,
    .maxauthsize	= MD5_DIGEST_SIZE,
  },
},
#endif
#endif
};
#else
static struct icp_aead {
  struct crypto_alg crypto;
} icp_aeads[] = {
#ifdef	GCM_HW
{
  .crypto = {
    .cra_name		= "rfc4106(gcm(aes))",
    .cra_driver_name	= "rfc4106_gcm_aes_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= GCM_ESP_IV_SIZE,
        .maxauthsize	= GCM_DIGEST_SIZE,
      }
    }
  },
},
#endif
#ifdef	NULL_HW
{
  .crypto = {
    .cra_name		= "authenc(digest_null,cbc(cipher_null))",
    .cra_driver_name	= "authenc_digest_null_cbc_cipher_null_icp",
    .cra_blocksize	= 1,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= 000*000,
        .maxauthsize	= 000*000,
      }
    }
  },
},
#endif
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha1),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= 000*000,
        .maxauthsize	= SHA1_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha256),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= 000*000,
        .maxauthsize	= SHA256_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha384),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= 000*000,
        .maxauthsize	= SHA384_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha512),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= 000*000,
        .maxauthsize	= SHA512_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(digest_null,cbc(des3_ede))",
    .cra_driver_name	= "authenc_digest_null_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .maxauthsize	= 000*000,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(digest_null,cbc(aes))",
    .cra_driver_name	= "authenc_digest_null_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= AES_BLOCK_SIZE,
        .maxauthsize	= 000*000,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha1),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .maxauthsize	= SHA1_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha256),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .maxauthsize	= SHA256_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha384),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .maxauthsize	= SHA384_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha512),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .maxauthsize	= SHA512_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha1),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= AES_BLOCK_SIZE,
        .maxauthsize	= SHA1_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha256),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= AES_BLOCK_SIZE,
        .maxauthsize	= SHA256_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha384),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= AES_BLOCK_SIZE,
        .maxauthsize	= SHA384_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha512),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= AES_BLOCK_SIZE,
        .maxauthsize	= SHA512_DIGEST_SIZE,
      }
    }
  },
},
#ifdef	MD5_HW
{
  .crypto = {
    .cra_name		= "authenc(hmac(md5),cbc(cipher_null))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_cipher_null_icp",
    .cra_blocksize	= 4,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= 000*000,
        .maxauthsize	= MD5_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(md5),cbc(des3_ede))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .maxauthsize	= MD5_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(md5),cbc(aes))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= AES_BLOCK_SIZE,
        .maxauthsize	= MD5_DIGEST_SIZE,
      }
    }
  },
},
#endif
#ifdef	DES_HW
{
  .crypto = {
    .cra_name		= "authenc(digest_null,cbc(des))",
    .cra_driver_name	= "authenc_digest_null_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES_BLOCK_SIZE,
        .maxauthsize	= 000*000
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha1),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha1_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES_BLOCK_SIZE,
        .maxauthsize	= SHA1_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha256),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha256_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES_BLOCK_SIZE,
        .maxauthsize	= SHA256_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha384),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha384_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES_BLOCK_SIZE,
        .maxauthsize	= SHA384_DIGEST_SIZE,
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "authenc(hmac(sha512),cbc(des))",
    .cra_driver_name	= "authenc_hmac_sha512_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES_BLOCK_SIZE,
        .maxauthsize	= SHA512_DIGEST_SIZE,
      }
    }
  },
},
#ifdef	MD5_HW
{
  .crypto = {
    .cra_name		= "authenc(hmac(md5),cbc(des))",
    .cra_driver_name	= "authenc_hmac_md5_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_AEAD,
    .cra_type		= &crypto_aead_type,
    .cra_u		= { .aead = {
        .ivsize		= DES_BLOCK_SIZE,
        .maxauthsize	= MD5_DIGEST_SIZE,
      }
    }
  },
},
#endif
#endif
};
#endif

#ifdef	ABLK_HW

static int  ablk_setkey(struct crypto_ablkcipher *ablk_tfm,
                        const Cpa8U * key, unsigned int keylen)
{
  CpaStatus                status;
  CpaCySymCipherAlgorithm  cipherAlgorithm;
  CpaCySymSessionSetupData Setup = { 0 };
  struct icp_crypto_ctx   *ctx   = crypto_ablkcipher_ctx(ablk_tfm);

  unsigned int iv_len = crypto_ablkcipher_ivsize(ablk_tfm);

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: ctx %p ivlen %2d key %p elen %2d\n",
         __FUNCTION__, ctx, iv_len, key, keylen);

  icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
  icp_crypto_free(ctx, &ctx->decrypt_ctx, &ctx->decrypt_key);

  ctx->encrypt_ctx = kzalloc(ctx->sessionSize, GFP_KERNEL);
  if (NULL == ctx->encrypt_ctx) {
    printk(KERN_ERR "%s: Failed to alloc encrypt session\n", __FUNCTION__);
    return -ENOMEM;
  }

  ctx->decrypt_ctx = kzalloc(ctx->sessionSize, GFP_KERNEL);
  if (NULL == ctx->decrypt_ctx) {
    printk(KERN_ERR "%s: Failed to alloc decrypt session\n", __FUNCTION__);
    kfree(ctx->encrypt_ctx);
    ctx->encrypt_ctx = NULL;
    return -ENOMEM;
  }

  /* Get salt */
  if (iv_len > 0) {
    int res = crypto_rng_get_bytes(crypto_default_rng, ctx->salt, iv_len);

    if (res != 0) {
      printk(KERN_ERR "%s: crypto_rng_get_bytes error\n", __FUNCTION__);
      return res;
    }
  }

  cipherAlgorithm = get_etype(keylen, iv_len);

  /* Initialise sessions */

  Setup.sessionPriority                     = CPA_CY_PRIORITY_HIGH;
  Setup.symOperation                        = CPA_CY_SYM_OP_CIPHER;
  Setup.cipherSetupData.cipherAlgorithm     = cipherAlgorithm;
  Setup.cipherSetupData.pCipherKey          = (Cpa8U *) key;
  Setup.cipherSetupData.cipherKeyLenInBytes = keylen;

  /* encrypt first */
  Setup.cipherSetupData.cipherDirection = CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;

/* Workaround for IXA00378497 - enc performance drop when append flag is set */
/* Using the same append config for encrypt/decrypt */
  Setup.digestIsAppended = CPA_FALSE;
  Setup.verifyDigest     = CPA_FALSE;

  /* Lock the context */
  mutex_lock(&ctx->mutexLock);

  /* Initialize the cipher enc session */
  status = cpaCySymInitSession(ctx->instance, symCallback,
                               &Setup, ctx->encrypt_ctx);
  if (CPA_STATUS_SUCCESS != status) {
    /* Unlock the context */
    icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
    mutex_unlock(&ctx->mutexLock);

    printk(KERN_ERR "%s: Failed to initialize encryption session\n",
           __FUNCTION__);
    return -EINVAL;
  }

  /* Set that encrypt key is set up */
  atomic_set(&ctx->encrypt_key, 1);

  /* now setup the decrypt session */
  Setup.cipherSetupData.cipherDirection = CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;

  /* Initialize the cipher decrypt session */
  status = cpaCySymInitSession(ctx->instance, symCallback,
                               &Setup, ctx->decrypt_ctx);
  if (CPA_STATUS_SUCCESS != status) {
    /* Unlock the context */
    icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
    icp_crypto_free(ctx, &ctx->decrypt_ctx, &ctx->decrypt_key);
    mutex_unlock(&ctx->mutexLock);

    printk(KERN_ERR "%s: Failed to initialize decryption session\n",
           __FUNCTION__);
    return -EINVAL;
  }

  /* Set that decrypt key is set up */
  atomic_set(&ctx->decrypt_key, 1);

  /* Unlock the context */
  mutex_unlock(&ctx->mutexLock);

  return 0;
}

static int ablk_init_tfm(struct crypto_tfm *tfm)
{
  return icp_crypto_init(tfm, &tfm->crt_ablkcipher.reqsize);
}

static int ablk_perform(struct ablkcipher_request *req, int direction)
{
  struct crypto_ablkcipher  *tfm     = crypto_ablkcipher_reqtfm(req);
  struct icp_crypto_ctx     *ctx     = crypto_ablkcipher_ctx(tfm);
  struct icp_crypto_req_ctx *req_ctx = ablkcipher_request_ctx(req);

  req_ctx->cryptlen = req->nbytes;
  req_ctx->assoclen = 0;
  req_ctx->assoc    = NULL;
  req_ctx->src      = req->src;
  req_ctx->dst      = req->dst;
  req_ctx->result   = NULL;

  return perform_sym_op(&req->base, ctx, req_ctx, NULL,
                        crypto_ablkcipher_ivsize(tfm),
                        direction, 0);
}

static int ablk_encrypt(struct ablkcipher_request *req)
{
  return ablk_perform(req, CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT);
}

static int ablk_decrypt(struct ablkcipher_request *req)
{
  return ablk_perform(req, CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT);
}

static struct icp_ablk {
  struct crypto_alg crypto;
} icp_ablks[] = {
{
  .crypto = {
    .cra_name		= "ablk(cbc(des3_ede))",
    .cra_driver_name	= "ablk_cbc_des3_ede_icp",
    .cra_blocksize	= DES3_EDE_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER,
    .cra_type		= &crypto_ablkcipher_type,
    .cra_u		= { .ablkcipher = {
        .min_keysize	= DES3_EDE_KEY_SIZE,
        .max_keysize	= DES3_EDE_KEY_SIZE,
        .ivsize		= DES3_EDE_BLOCK_SIZE,
        .geniv		= "eseqiv",
      }
    }
  },
},
{
  .crypto = {
    .cra_name		= "ablk(cbc(aes))",
    .cra_driver_name	= "ablk_cbc_aes_icp",
    .cra_blocksize	= AES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER,
    .cra_type		= &crypto_ablkcipher_type,
    .cra_u		= { .ablkcipher = {
        .min_keysize	= AES_MIN_KEY_SIZE,
        .max_keysize	= AES_MAX_KEY_SIZE,
        .ivsize		= AES_BLOCK_SIZE,
        .geniv		= "eseqiv",
      }
    }
  },
},
#ifdef	DES_HW
{
  .crypto = {
    .cra_name		= "ablk(cbc(des))",
    .cra_driver_name	= "ablk_cbc_des_icp",
    .cra_blocksize	= DES_BLOCK_SIZE,
    .cra_flags		= CRYPTO_ALG_TYPE_ABLKCIPHER,
    .cra_type		= &crypto_ablkcipher_type,
    .cra_u		= { .ablkcipher = {
        .min_keysize	= DES_KEY_SIZE,
        .max_keysize	= DES_KEY_SIZE,
        .ivsize		= DES_BLOCK_SIZE,
  .geniv		= "eseqiv",
      }
    }
  },
},
#endif
};

#endif

#ifdef	AHASH_HW

static void ahash_dump(const char* func, struct ahash_request* areq)
{
  if (likely(areq)) {
    PrintK(KERN_DEBUG "%s: areq %p base %p nbytes %d result %p\n", func,
           areq, &areq->base, areq->nbytes, areq->result);
    PrintK(KERN_DEBUG "%s: complete %p data %p\n", func,
           areq->base.complete, areq->base.data);
  }
  else
    printk(KERN_EMERG "%s: areq %p\n", func, areq);
}

static int  ahash_init_tfm(struct crypto_tfm *tfm)
{
  return icp_crypto_init(tfm, NULL);
}

static int  ahash_setkey(struct crypto_ahash *ahash_tfm, const u8 *key,
                         unsigned int keylen)
{
  CpaStatus                status;
  CpaCySymHashAlgorithm    hashAlgorithm;
  CpaCySymSessionSetupData Setup = { 0 };
  struct icp_crypto_ctx   *ctx   = crypto_ahash_ctx(ahash_tfm);
  struct crypto_tfm       *tfm   = crypto_ahash_tfm(ahash_tfm);
  const  char             *name  = crypto_tfm_alg_name(tfm);

  /* Set request size */
  ahash_tfm->reqsize    = ctx->requestSize;

  /* Set authsize */
  ctx->authSize         = get_asize(name);

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: ctx %p key %p len %2d auth %2d req %2d '%s'\n",
         __FUNCTION__, ctx, key, keylen,
         ctx->authSize, ctx->requestSize, name);

  hashAlgorithm         = get_atype(ctx->authSize);

  Setup.sessionPriority = CPA_CY_PRIORITY_HIGH;
  Setup.symOperation    = CPA_CY_SYM_OP_HASH;

  Setup.hashSetupData.digestResultLenInBytes = ctx->authSize;
  Setup.hashSetupData.hashAlgorithm          = hashAlgorithm;
  Setup.hashSetupData.authModeSetupData.authKeyLenInBytes = keylen;

  if (likely(keylen > 0)) {
    Setup.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
    Setup.hashSetupData.authModeSetupData.authKey = (Cpa8U *)key;
  } else {
    Setup.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_PLAIN;
    Setup.hashSetupData.authModeSetupData.authKey = (Cpa8U *)"";
  }

/* Workaround for IXA00378497 - enc performance drop when append flag is set */
/* Using the same append config for encrypt/decrypt */
  Setup.digestIsAppended = CPA_FALSE;
  Setup.verifyDigest     = CPA_FALSE;

  icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);

  ctx->encrypt_ctx = kzalloc(ctx->sessionSize, GFP_KERNEL);
  if (NULL == ctx->encrypt_ctx) {
    printk(KERN_ERR "%s: Failed to alloc encrypt session\n", __FUNCTION__);
    return -ENOMEM;
  }

  /* Lock the context */
  mutex_lock(&ctx->mutexLock);

  /* Initialize the cipher enc session */
  status = cpaCySymInitSession(ctx->instance, symCallback,
                               &Setup, ctx->encrypt_ctx);
  if (CPA_STATUS_SUCCESS != status) {
    /* Unlock the context */
    icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
    mutex_unlock(&ctx->mutexLock);

    printk(KERN_ERR "%s: Failed to initialize encryption session\n",
           __FUNCTION__);
    return -EINVAL;
  }

  /* Set that encrypt key is set up */
  atomic_set(&ctx->encrypt_key, 1);

  /* Unlock the context */
  mutex_unlock(&ctx->mutexLock);

  return 0;
}

static int ahash_init(struct ahash_request *areq)
{
  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: areq %p\n", __FUNCTION__, areq);

  ahash_dump(__FUNCTION__, areq);

  return 0;
}

static int ahash_process_req(struct ahash_request *areq, unsigned int nbytes)
{
  struct crypto_ahash       *tfm     = crypto_ahash_reqtfm(areq);
  struct icp_crypto_ctx     *ctx     = crypto_ahash_ctx(tfm);
  struct icp_crypto_req_ctx *req_ctx = ahash_request_ctx(areq);

  if (unlikely(console_loglevel & 16))
    printk(KERN_DEBUG "%s: ctx %p len %d\n", __FUNCTION__, ctx, nbytes);

  req_ctx->cryptlen = nbytes;
  req_ctx->assoclen = 0;
  req_ctx->assoc    = NULL;
  req_ctx->src      = areq->src;
  req_ctx->dst      = areq->src;
  req_ctx->result   = areq->result;

  return perform_sym_op(&areq->base, ctx, req_ctx, NULL, 0,
                        CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT, 0);
}

static int ahash_digest(struct ahash_request *areq)
{
  ahash_dump(__FUNCTION__, areq);

  return ahash_process_req(areq, areq->nbytes);
}

static int ahash_update(struct ahash_request *areq)
{
  ahash_dump(__FUNCTION__, areq);

  return ahash_process_req(areq, areq->nbytes);
}

static int ahash_final(struct ahash_request *areq)
{
  ahash_dump(__FUNCTION__, areq);

  return ahash_process_req(areq, 0);
}

static int ahash_finup(struct ahash_request *areq)
{
  ahash_dump(__FUNCTION__, areq);

  return ahash_process_req(areq, areq->nbytes);
}

static struct icp_ahash {
  struct ahash_alg ahash;
} icp_ahashs[] = {
{
  .ahash = {
    .init		= ahash_init,
    .update		= ahash_update,
    .final		= ahash_final,
    .finup		= ahash_finup,
    .digest		= ahash_digest,
    .setkey		= ahash_setkey,
    .halg.digestsize    = SHA1_DIGEST_SIZE,
    .halg.statesize     = SHA1_BLOCK_SIZE*2,
    .halg.base = {
      .cra_name		= "auth(sha1)",
      .cra_driver_name	= "auth_sha1_icp",
      .cra_blocksize	= SHA1_BLOCK_SIZE,
      .cra_flags	= CRYPTO_ALG_TYPE_AHASH,
      .cra_type		= &crypto_ahash_type,
    }
  }
},
{
  .ahash = {
    .init		= ahash_init,
    .update		= ahash_update,
    .final		= ahash_final,
    .finup		= ahash_finup,
    .digest		= ahash_digest,
    .setkey		= ahash_setkey,
    .halg.digestsize    = SHA256_DIGEST_SIZE,
    .halg.statesize     = SHA256_BLOCK_SIZE*2,
    .halg.base = {
      .cra_name		= "auth(sha256)",
      .cra_driver_name	= "auth_sha256_icp",
      .cra_blocksize	= SHA256_BLOCK_SIZE,
      .cra_flags	= CRYPTO_ALG_TYPE_AHASH,
      .cra_type		= &crypto_ahash_type,
    }
  }
},
{
  .ahash = {
    .init		= ahash_init,
    .update		= ahash_update,
    .final		= ahash_final,
    .finup		= ahash_finup,
    .digest		= ahash_digest,
    .setkey		= ahash_setkey,
    .halg.digestsize    = SHA384_DIGEST_SIZE,
    .halg.statesize     = SHA384_BLOCK_SIZE*2,
    .halg.base = {
      .cra_name		= "auth(sha384)",
      .cra_driver_name	= "auth_sha384_icp",
      .cra_blocksize	= SHA384_BLOCK_SIZE,
      .cra_flags	= CRYPTO_ALG_TYPE_AHASH,
      .cra_type		= &crypto_ahash_type,
    }
  }
},
{
  .ahash = {
    .init		= ahash_init,
    .update		= ahash_update,
    .final		= ahash_final,
    .finup		= ahash_finup,
    .digest		= ahash_digest,
    .setkey		= ahash_setkey,
    .halg.digestsize    = SHA512_DIGEST_SIZE,
    .halg.statesize     = SHA512_BLOCK_SIZE*2,
    .halg.base = {
      .cra_name		= "auth(sha512)",
      .cra_driver_name	= "auth_sha512_icp",
      .cra_blocksize	= SHA512_BLOCK_SIZE,
      .cra_flags	= CRYPTO_ALG_TYPE_AHASH,
      .cra_type		= &crypto_ahash_type,
    }
  }
}
};

#endif

#ifdef	SHASH_HW

static int  sha_hw_init_tfm(struct crypto_tfm *tfm)
{
  return icp_crypto_init(tfm, NULL);
}

static int sha_hw_setkey(struct crypto_shash *shash_tfm, const u8 *key,
                          unsigned int keylen)
{
  CpaStatus                status;
  CpaCySymSessionSetupData Setup = { 0 };
  struct icp_crypto_ctx   *ctx   = crypto_shash_ctx(shash_tfm);

  /* Set request size */
  shash_tfm->descsize   = ctx->requestSize;

  /* Set authsize */
  ctx->authSize         = SHA1_DIGEST_SIZE;

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: ctx %p key %p len %2d auth %2d req %2d\n",
         __FUNCTION__, ctx, key, keylen, ctx->authSize, ctx->requestSize);

  Setup.sessionPriority = CPA_CY_PRIORITY_HIGH;
  Setup.symOperation    = CPA_CY_SYM_OP_HASH;

  Setup.hashSetupData.hashAlgorithm = CPA_CY_SYM_HASH_SHA1;
  Setup.hashSetupData.hashMode      = CPA_CY_SYM_HASH_MODE_PLAIN;

  Setup.hashSetupData.digestResultLenInBytes = ctx->authSize;

  Setup.hashSetupData.authModeSetupData.authKey           = (Cpa8U *)"";
  Setup.hashSetupData.authModeSetupData.authKeyLenInBytes = 0;

/* Workaround for IXA00378497 - enc performance drop when append flag is set */
/* Using the same append config for encrypt/decrypt */
  Setup.digestIsAppended = CPA_FALSE;
  Setup.verifyDigest     = CPA_FALSE;

  icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);

  ctx->encrypt_ctx = kzalloc(ctx->sessionSize, GFP_KERNEL);
  if (NULL == ctx->encrypt_ctx) {
    printk(KERN_ERR "%s: Failed to alloc encrypt session\n", __FUNCTION__);
    return -ENOMEM;
  }

  /* Lock the context */
  mutex_lock(&ctx->mutexLock);

  /* Initialize the cipher enc session */
  status = cpaCySymInitSession(ctx->instance, symCallback,
                               &Setup, ctx->encrypt_ctx);
  if (CPA_STATUS_SUCCESS != status) {
    /* Unlock the context */
    icp_crypto_free(ctx, &ctx->encrypt_ctx, &ctx->encrypt_key);
    mutex_unlock(&ctx->mutexLock);

    printk(KERN_ERR "%s: Failed to initialize encryption session\n",
           __FUNCTION__);
    return -EINVAL;
  }

  /* Set that encrypt key is set up */
  atomic_set(&ctx->encrypt_key, 1);

  /* Unlock the context */
  mutex_unlock(&ctx->mutexLock);

  return 0;
}

static int sha_hw_init(struct shash_desc *desc)
{
  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: desc %p\n", __FUNCTION__, desc);

  return 0;
}

static void sha_hw_cb(struct crypto_async_request *async_req, int status)
{
  async_req->flags |= 1;
}

static int sha_hw_update(struct shash_desc *desc, const u8 *data,
        unsigned int len)
{
  int    err;
  struct crypto_shash        *shash_tfm = desc->tfm;
  struct icp_crypto_ctx      *ctx       = crypto_shash_ctx(shash_tfm);
  struct icp_crypto_req_ctx  *req_ctx   = shash_desc_ctx(desc);
  struct crypto_async_request async_req = { .complete = sha_hw_cb, };
  struct scatterlist          sg;

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: desc %p len %d\n", __FUNCTION__, desc, len);

  sg_init_one(&sg, data, len + SHA1_DIGEST_SIZE);

  async_req.tfm     = &shash_tfm->base;
  async_req.flags   = 0;

  req_ctx->cryptlen = len;
  req_ctx->assoclen = 0;
  req_ctx->assoc    = NULL;
  req_ctx->src      = &sg;
  req_ctx->dst      = &sg;
  req_ctx->result   = NULL;

  err = perform_sym_op(&async_req, ctx, req_ctx, NULL, 0,
                       CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT, 0);

  if (!err) while (async_req.flags == 0) udelay(1);

  return err;
}

static int sha_hw_final(struct shash_desc *desc, u8 *out)
{
  int    err = 0;
  struct icp_crypto_req_ctx *req_ctx = shash_desc_ctx(desc);

  /* Check for zero length hash buffer */
  if (unlikely(req_ctx->cryptlen == 0)) err = sha_hw_update(desc, out, 0);

  memcpy(out, req_ctx->digestBuffer, SHA1_DIGEST_SIZE);

  /* Wipe context */
  memset(req_ctx->digestBuffer, 0, SHA1_DIGEST_SIZE);

  return err;
}

#ifdef	EXPORT_IMPORT

static int sha_hw_export(struct shash_desc *desc, void *out)
{
  struct sha1_state *sctx = shash_desc_ctx(desc);

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: desc %p\n", __FUNCTION__, desc);

  memcpy(out, sctx, sizeof(*sctx));
  return 0;
}

static int sha_hw_import(struct shash_desc *desc, const void *in)
{
  struct sha1_state *sctx = shash_desc_ctx(desc);

  if (unlikely(console_loglevel & 16))
  printk(KERN_DEBUG "%s: desc %p\n", __FUNCTION__, desc);

  memcpy(sctx, in, sizeof(*sctx));
  return 0;
}

#endif

struct icp_shash {
  struct shash_alg shash;
} icp_shashs[] = {
{
  .shash = {
    .init		= sha_hw_init,
    .update		= sha_hw_update,
    .final		= sha_hw_final,
#ifdef	EXPORT_IMPORT
    .export		= sha_hw_export,
    .import		= sha_hw_import,
#endif
    .setkey		= sha_hw_setkey,
    .statesize		= sizeof(struct sha1_state),
    .digestsize		= SHA1_DIGEST_SIZE,
    .base = {
      .cra_name		= "hash(sha1)",
      .cra_driver_name	= "hash_sha1_icp",
      .cra_blocksize	= SHA1_BLOCK_SIZE,
      .cra_flags	= CRYPTO_ALG_TYPE_SHASH,
    }
  }
}
};

#endif

int __init icp_algos_init(void)
{
  int j, err;

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ "\n\n", __FUNCTION__);

#ifdef	SHASH_HW
  for (j = 0; j < ARRAY_SIZE(icp_shashs); j++) {
    struct crypto_alg *cra = &icp_shashs[j].shash.base;

    //shash
    cra->cra_flags    |= CRYPTO_ALG_ASYNC;
    cra->cra_ctxsize   = sizeof(struct icp_crypto_ctx);
    cra->cra_module    = THIS_MODULE;
    cra->cra_alignmask = 0;
    cra->cra_priority  = CRA_PRIORITY;
    cra->cra_init      = sha_hw_init_tfm;
    cra->cra_exit      = icp_crypto_exit;

    INIT_LIST_HEAD(&cra->cra_list);

    printk(KERN_INFO "%s: Insert shash '%s'\n", __FUNCTION__, cra->cra_name);
    if ((err = crypto_register_shash(&icp_shashs[j].shash)) != 0) {
    printk(KERN_EMERG "%s: Error %4d registering '%s'\n",
           __FUNCTION__, err, cra->cra_name);
    cra->cra_priority  = 0;
    }
  }
#endif

#ifdef	AHASH_HW

  for (j = 0; j < ARRAY_SIZE(icp_ahashs); j++) {
    struct crypto_alg *cra = &icp_ahashs[j].ahash.halg.base;

    //ahash
    cra->cra_flags    |= CRYPTO_ALG_ASYNC;
    cra->cra_ctxsize   = sizeof(struct icp_crypto_ctx);
    cra->cra_module    = THIS_MODULE;
    cra->cra_alignmask = 0;
    cra->cra_priority  = CRA_PRIORITY;
    cra->cra_init      = ahash_init_tfm;
    cra->cra_exit      = icp_crypto_exit;

    INIT_LIST_HEAD(&cra->cra_list);

    printk(KERN_INFO "%s: Insert ahash '%s'\n", __FUNCTION__, cra->cra_name);
    if ((err = crypto_register_ahash(&icp_ahashs[j].ahash)) != 0) {
    printk(KERN_EMERG "%s: Error %4d registering '%s'\n",
           __FUNCTION__, err, cra->cra_name);
    cra->cra_priority  = 0;
    }
  }
#endif

#ifdef	ABLK_HW
  for (j = 0; j < ARRAY_SIZE(icp_ablks); j++) {
    struct crypto_alg* cra = &icp_ablks[j].crypto;

    cra->cra_flags    |= CRYPTO_ALG_ASYNC;
    cra->cra_ctxsize   = sizeof(struct icp_crypto_ctx);
    cra->cra_module    = THIS_MODULE;
    cra->cra_alignmask = 0;
    cra->cra_init      = ablk_init_tfm;
    cra->cra_exit      = icp_crypto_exit;
    cra->cra_priority  = CRA_PRIORITY;

    INIT_LIST_HEAD(&cra->cra_list);

    //ablk
    cra->cra_ablkcipher.setkey  = ablk_setkey;
    cra->cra_ablkcipher.encrypt = ablk_encrypt;
    cra->cra_ablkcipher.decrypt = ablk_decrypt;

    printk(KERN_INFO "%s: Insert ablk  '%s'\n", __FUNCTION__, cra->cra_name);
    if ((err = crypto_register_alg(&icp_ablks[j].crypto)) != 0) {
    printk(KERN_EMERG "%s: Error %4d registering '%s'\n",
           __FUNCTION__, err, cra->cra_name);
    cra->cra_priority  = 0;
    }
  }
#endif

  for (j = 0; j < ARRAY_SIZE(icp_aeads); j++) {
#ifdef	CONFIG_WG_KERNEL_4_14
    struct aead_alg*  aead = &icp_aeads[j].aead;
    struct crypto_alg* cra = &aead->base;

    cra->cra_flags    |= CRYPTO_ALG_ASYNC;
    cra->cra_ctxsize   = sizeof(struct icp_crypto_ctx);
    cra->cra_module    = THIS_MODULE;
    cra->cra_alignmask = 0;
    cra->cra_priority  = CRA_PRIORITY;

    INIT_LIST_HEAD(&cra->cra_list);

    //aead
    aead->setkey                = aead_setkey;
    aead->setauthsize           = aead_setauthsize;
    aead->encrypt               = aead_encrypt;
    aead->decrypt               = aead_decrypt;
    aead->init                  = aead_init_tfm;
    aead->exit                  = aead_exit_tfm;

    printk(KERN_INFO "%s: Insert aead  '%s'\n", __FUNCTION__, cra->cra_name);

    if ((err = crypto_register_aead(&icp_aeads[j].aead)) != 0) {
    printk(KERN_EMERG "%s: Error %4d registering '%s'\n",
           __FUNCTION__, err, cra->cra_name);
    cra->cra_priority  = 0;
    }
#else
    struct crypto_alg* cra = &icp_aeads[j].crypto;

    cra->cra_flags    |= CRYPTO_ALG_ASYNC;
    cra->cra_ctxsize   = sizeof(struct icp_crypto_ctx);
    cra->cra_module    = THIS_MODULE;
    cra->cra_alignmask = 0;
    cra->cra_init      = aead_init_tfm;
    cra->cra_exit      = icp_crypto_exit;
    cra->cra_priority  = CRA_PRIORITY;

    INIT_LIST_HEAD(&cra->cra_list);

    //aead
    cra->cra_aead.setkey	= aead_setkey;
    cra->cra_aead.setauthsize   = aead_setauthsize;
    cra->cra_aead.encrypt       = aead_encrypt;
    cra->cra_aead.decrypt       = aead_decrypt;
    cra->cra_aead.givencrypt    = aead_givencrypt;

    printk(KERN_INFO "%s: Insert aead  '%s'\n", __FUNCTION__, cra->cra_name);

    if ((err = crypto_register_alg(&icp_aeads[j].crypto)) != 0) {
    printk(KERN_EMERG "%s: Error %4d registering '%s'\n",
           __FUNCTION__, err, cra->cra_name);
    cra->cra_priority  = 0;
    }
#endif
  }

  return 0;
}

void __exit icp_algos_exit(void)
{
  int j;

  printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ "\n\n", __FUNCTION__);

  for (j = 0; j < ARRAY_SIZE(icp_aeads); j++)
#ifdef	CONFIG_WG_KERNEL_4_14
  if (icp_aeads[j].aead.base.cra_priority > 0) {
    struct aead_alg*  aead = &icp_aeads[j].aead;
    struct crypto_alg* cra = &aead->base;
    printk(KERN_INFO "%s: Remove aead  '%s'\n", __FUNCTION__, cra->cra_name);
    crypto_unregister_aead(&icp_aeads[j].aead);
#else
  if (icp_aeads[j].crypto.cra_priority > 0) {
    struct crypto_alg* cra = &icp_aeads[j].crypto;
    printk(KERN_INFO "%s: Remove aead  '%s'\n", __FUNCTION__, cra->cra_name);
    crypto_unregister_alg(&icp_aeads[j].crypto);
#endif
  }
#ifdef	ABLK_HW
  for (j = 0; j < ARRAY_SIZE(icp_ablks); j++)
  if (icp_ablks[j].crypto.cra_priority > 0) {
    struct crypto_alg* cra = &icp_ablks[j].crypto;
    printk(KERN_INFO "%s: Remove ablk  '%s'\n", __FUNCTION__, cra->cra_name);
    crypto_unregister_alg(&icp_ablks[j].crypto);
  }
#endif

#ifdef	AHASH_HW
  for (j = 0; j < ARRAY_SIZE(icp_ahashs); j++)
  if (icp_ahashs[j].ahash.halg.base.cra_priority > 0) {
    struct crypto_alg *cra = &icp_ahashs[j].ahash.halg.base;
    printk(KERN_INFO "%s: Remove ahash '%s'\n", __FUNCTION__, cra->cra_name);
    crypto_unregister_ahash(&icp_ahashs[j].ahash);
  }
#endif

#ifdef	SHASH_HW
  for (j = 0; j < ARRAY_SIZE(icp_shashs); j++)
  if (icp_shashs[j].shash.base.cra_priority > 0) {
    struct crypto_alg *cra = &icp_shashs[j].shash.base;
    printk(KERN_INFO "%s: Remove shash '%s'\n", __FUNCTION__, cra->cra_name);
    crypto_unregister_shash(&icp_shashs[j].shash);
  }
#endif
}

/* This function gets the QA instances and registers our module with LKCF */
static int __init icp_netkey_init(void)
{
  int       j;
  int       retval;
  int       startCtr;
  Cpa16U    num    = 0;
  CpaStatus status = CPA_STATUS_SUCCESS;

  /* QA API crypto instances are found at module init and are assumed to
     stay static while this module is active. */
  if (cpaCyGetNumInstances(&num) != CPA_STATUS_SUCCESS) {
    printk(KERN_ERR "%s: Could not get number of Quick Assist"
           "crypto instances\n", __FUNCTION__);
    retval = -EINVAL;
    goto out;
  }

  /* Allocate memory to store global array of QA instances */
  qat_instances.instances = kmalloc(sizeof(CpaInstanceHandle) * num,
                                    GFP_KERNEL);
  if (NULL == qat_instances.instances) {
    printk(KERN_ERR "%s: Failed to alloc instance memory\n", __FUNCTION__);
    retval = -ENOMEM;
    goto out;
  }

  if (cpaCyGetInstances(num, qat_instances.instances) != CPA_STATUS_SUCCESS) {
    printk(KERN_ERR "%s: Could not get Quick Assist crypto instance\n",
           __FUNCTION__);
    retval = -EINVAL;
    goto free_instances;
  }

  qat_instances.ctr   = 0;
  qat_instances.total = num;

  /* Start all instances */
  for (startCtr = 0; startCtr < num; startCtr++) {
    status = cpaCyStartInstance(qat_instances.instances[startCtr]);
    if (CPA_STATUS_SUCCESS != status) {
      printk(KERN_ERR "%s: Failed to start instance %d\n", __FUNCTION__,
             startCtr);
      retval = -EINVAL;
      goto stop_instances;
    }

    printk(KERN_INFO "%s: Instance %3d [%p] started\n",
           __FUNCTION__, startCtr, qat_instances.instances[startCtr]);
  }

#ifdef	SAMPLE_TEST
  {
    CpaStatus
    SampleTest(void);
    SampleTest();
  }
#endif

  /* Get default rng */
  retval = crypto_get_default_rng();
  if (retval != 0) {
    printk(KERN_ERR "%s: Failed to get default rng\n", __FUNCTION__);
    goto stop_instances;
  }

  /* Initialize algorithm drivers */
  return icp_algos_init();

 stop_instances:
  for (j = 0; j < startCtr; j++)
    cpaCyStopInstance(qat_instances.instances[j]);

 free_instances:
  kfree(qat_instances.instances);

 out:
  return retval;
}

/* This function calls driver_exit functions and frees instance memory */
static void __exit icp_netkey_exit(void)
{
  int j;

  /* Un-register algos */
  icp_algos_exit();

  /* Free default rng */
  crypto_put_default_rng();

  /* Stop all instances */
  for (j = 0; j < qat_instances.total; j++)
    cpaCyStopInstance(qat_instances.instances[j]);

  /* Free resources */
  kfree(qat_instances.instances);
}

module_init(icp_netkey_init);
module_exit(icp_netkey_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LCKF Driver for Intel Quick Assist crypto acceleration");
MODULE_AUTHOR("Intel Corporation");


// SAMPLE TEST CODE FOME INTEL


#ifdef	SAMPLE_TEST

/*
 * Attach the code below to the end of this module and call SampleTest from
 * the module's init code to run a simple start up test of the algo whose
 * test code is included below.
 */

/* Kernel space utils */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <asm/io.h>

#ifdef __x86_64__
#define SAMPLE_ADDR_LEN uint64_t
#else
#define SAMPLE_ADDR_LEN uint32_t
#endif

/* Threads */
typedef struct task_struct *sampleThread;

/* Printing */
/**< Prints the name of the function and the arguments only if gDebugParam is 
 * TRUE.
 */
#define PRINT_DBG(args...)              \
    do {                                \
        if (TRUE == gDebugParam) {      \
            printk("%s(): ", __func__); \
            printk(args);               \
        }                               \
    } while (0)

/**< Prints the name of the function and the arguments */
    #define PRINT_ERR(args...)          \
    do {                                \
            printk("%s(): ", __func__); \
            printk(args);               \
    } while (0)

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Completion definitions
 *
 ******************************************************************************/
#define COMPLETION_STRUCT completion

#define COMPLETION_INIT(c) init_completion(c)

#define COMPLETION_WAIT(c, timeout)                         \
    wait_for_completion_interruptible_timeout(c, timeout)

#define COMPLETE(c) complete(c)

#define COMPLETION_DESTROY(s)

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function and associated macro sleeps for ms milliseconds 
 *
 * @param[in] ms    sleep time in ms 
 *
 * @retval none 
 *
 ******************************************************************************/
static __inline CpaStatus 
sampleCodeSleep(Cpa32U ms)
{
    if(ms != 0)
    {
        set_current_state((long) TASK_INTERRUPTIBLE);
        schedule_timeout((ms * HZ)/ 1000);
    }
    else
    {
       schedule();
    }

    return CPA_STATUS_SUCCESS;
}

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Macro from the sampleCodeSleep function
 *
 ******************************************************************************/
#define OS_SLEEP(ms) \
    sampleCodeSleep((ms))

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function and associated macro allocates the memory for the given
 *      size and stores the address of the memory allocated in the pointer.
 *      Memory allocated by this function is NOT gauranteed to be physically
 *      contiguous. 
 *
 * @param[out] ppMemAddr    address of pointer where address will be stored
 * @param[in] sizeBytes     the size of the memory to be allocated
 *
 * @retval CPA_STATUS_RESOURCE  Macro failed to allocate Memory
 * @retval CPA_STATUS_SUCCESS   Macro executed successfully
 *
 ******************************************************************************/
static __inline CpaStatus
Mem_OsMemAlloc(void **ppMemAddr,
                  Cpa32U sizeBytes)
{
    *ppMemAddr = kmalloc(sizeBytes, GFP_KERNEL);
    if (NULL == *ppMemAddr)
    {
        return CPA_STATUS_RESOURCE;
    }
    return CPA_STATUS_SUCCESS;
}

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function and associated macro allocates the memory for the given
 *      size for the given alignment and stores the address of the memory 
 *      allocated in the pointer. Memory allocated by this function is
 *      gauranteed to be physically contiguous.
 *
 * @param[out] ppMemAddr    address of pointer where address will be stored
 * @param[in] sizeBytes     the size of the memory to be allocated
 * @param[in] alignement    the alignment of the memory to be allocated (non-zero)
 *
 * @retval CPA_STATUS_RESOURCE  Macro failed to allocate Memory
 * @retval CPA_STATUS_SUCCESS   Macro executed successfully
 *
 ******************************************************************************/
static __inline CpaStatus
Mem_Alloc_Contig(void **ppMemAddr,
                  Cpa32U sizeBytes, Cpa32U alignment)
{
    void * pAlloc = NULL;
    uint32_t align = 0; 

    pAlloc = kmalloc_node((sizeBytes+alignment+sizeof(void*)), GFP_KERNEL, 0);
    if (NULL == pAlloc)
    {
        return CPA_STATUS_RESOURCE;
    }

    *ppMemAddr = pAlloc + sizeof(void*);

    align = ((SAMPLE_ADDR_LEN)(*ppMemAddr))%alignment;

    *ppMemAddr += (alignment-align);
    *(SAMPLE_ADDR_LEN *)(*ppMemAddr - sizeof(void*)) = (SAMPLE_ADDR_LEN)pAlloc; 

    return CPA_STATUS_SUCCESS;
}

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Macro from the Mem_OsMemAlloc function
 *
 ******************************************************************************/
#define OS_MALLOC(ppMemAddr, sizeBytes) \
    Mem_OsMemAlloc((void *)(ppMemAddr), (sizeBytes))

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Macro from the Mem_Alloc_Contig function
 *
 ******************************************************************************/
#define PHYS_CONTIG_ALLOC(ppMemAddr, sizeBytes) \
    Mem_Alloc_Contig((void *)(ppMemAddr), (sizeBytes), 1)

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *     Algined version of PHYS_CONTIG_ALLOC() macro 
 *
 ******************************************************************************/
#define PHYS_CONTIG_ALLOC_ALIGNED(ppMemAddr, sizeBytes, alignment) \
    Mem_Alloc_Contig((void *)(ppMemAddr), (sizeBytes), (alignment))

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function and associated macro frees the memory at the given address
 *      and resets the pointer to NULL. The memory must have been allocated by 
 *      the function Mem_OsMemAlloc()
 *
 * @param[out] ppMemAddr    address of pointer where mem address is stored.
 *                          If pointer is NULL, the function will exit silently
 *
 * @retval void
 *
 ******************************************************************************/
static __inline void
Mem_OsMemFree(void **ppMemAddr)
{
    if (NULL != *ppMemAddr)
    {
       kfree(*ppMemAddr);
       *ppMemAddr = NULL;
    }
}

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function and associated macro frees the memory at the given address
 *      and resets the pointer to NULL. The memory must have been allocated by 
 *      the function Mem_Alloc_Contig().
 *
 * @param[out] ppMemAddr    address of pointer where mem address is stored.
 *                          If pointer is NULL, the function will exit silently
 *
 * @retval void
 *
 ******************************************************************************/
static __inline void
Mem_Free_Contig(void **ppMemAddr)
{
    void * pAlloc = NULL;
    if (NULL != *ppMemAddr)
    {
       pAlloc = (void *)(*((SAMPLE_ADDR_LEN *)(*ppMemAddr - sizeof(void*))));
       kfree(pAlloc);
       *ppMemAddr = NULL;
    }
}

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Macro from the Mem_OsMemFree function
 *
 ******************************************************************************/
#define OS_FREE(pMemAddr) \
    Mem_OsMemFree((void *)&pMemAddr)

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      Macro from the Mem_Free_Contig function
 *
 ******************************************************************************/
#define PHYS_CONTIG_FREE(pMemAddr) \
    Mem_Free_Contig((void *)&pMemAddr)

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function returns the physical address for a given virtual address.
 *      In case of error 0 is returned.
 *
 * @param[in] virtAddr     Virtual address 
 *
 * @retval CpaPhysicalAddr Physical address or 0 in case of error 
 *
 ******************************************************************************/

static __inline CpaPhysicalAddr
sampleVirtToPhys(void * virtAddr)
{
    return (CpaPhysicalAddr) virt_to_phys(virtAddr);
}

/**
 *******************************************************************************
 * @ingroup sampleUtils
 *      This function creates a thread 
 *
 ******************************************************************************/

static __inline CpaStatus
sampleThreadCreate(sampleThread * thread, void* funct, void* args)
{
   *thread = kthread_create(funct, args, "SAMPLE_THREAD");
   return CPA_STATUS_SUCCESS;
}

static __inline void 
sampleThreadExit(void)
{
}

void
sampleCyGetInstance(CpaInstanceHandle* pCyInstHandle);

void
sampleCyStartPolling(CpaInstanceHandle cyInstHandle);

void
sampleCyStopPolling(void);

void
sampleDcGetInstance(CpaInstanceHandle* pDcInstHandle);

void
sampleDcStartPolling(CpaInstanceHandle dcInstHandle);

void
sampleDcStopPolling(void);

// SAMPLE COMMON CODE

/*
 * Maximum number of instances to query from the API
 */
#define MAX_INSTANCES 1

static sampleThread gPollingThread;
static int gPollingCy = 0;
static int gDebugParam = TRUE;

/*
 * This function returns a handle to an instance of the cryptographic
 */

void
sampleCyGetInstance(CpaInstanceHandle* pCyInstHandle)
{
    CpaInstanceHandle cyInstHandles[MAX_INSTANCES];
    Cpa16U numInstances = 0;
    CpaStatus status = CPA_STATUS_SUCCESS;

    *pCyInstHandle = NULL;

    status = cpaCyGetNumInstances(&numInstances);
    if ((status == CPA_STATUS_SUCCESS) && (numInstances > 0))
    {
        status = cpaCyGetInstances(MAX_INSTANCES, cyInstHandles);
        if (status == CPA_STATUS_SUCCESS)
        {
            *pCyInstHandle = cyInstHandles[0];
        }
    }
}

/* 
 * This function polls a crypto instance.
 *
 */
static void
sal_polling(CpaInstanceHandle cyInstHandle)
{
   gPollingCy = 1;
   while(gPollingCy)
   {
       void icp_sal_CyPollInstance(CpaInstanceHandle, int);    
       icp_sal_CyPollInstance(cyInstHandle, 0);
       OS_SLEEP(10);
   }

   sampleThreadExit();
}

/*
 * This function checks the instance info. If the instance is
 * required to be polled then it starts a polling thread.
 */
void
sampleCyStartPolling(CpaInstanceHandle cyInstHandle)
{
   CpaInstanceInfo2 info2 = {0};
   CpaStatus status = CPA_STATUS_SUCCESS;

   status = cpaCyInstanceGetInfo2(cyInstHandle, &info2);
   if((status == CPA_STATUS_SUCCESS) && (info2.isPolled == CPA_TRUE))
   {
      /* Start thread to poll instance */
      sampleThreadCreate(&gPollingThread, sal_polling, cyInstHandle);
   }

}

/*
 * This function stops the polling of a crypto instance. 
 */
void
sampleCyStopPolling(void)
{
   gPollingCy = 0;
}

// SAMPLE GCM CODE

#if	0

static Cpa8U sampleKey[] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
        0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22
};

static Cpa8U sampleIv[] = {
        0xca, 0xfe, 0xca, 0xfe, 0xca, 0xfe, 0xca, 0xfe,
        0xca, 0xfe, 0xca, 0xfe
};

static Cpa8U sampleAddAuthData[] = {
        0xde, 0xad, 0xde, 0xad, 0xde, 0xad, 0xde, 0xad,
        0xde, 0xad, 0xde, 0xad, 0xde, 0xad, 0xde, 0xad,
        0xde, 0xad, 0xde, 0xad
};

static Cpa8U samplePayload[] = {
        0x79, 0x84, 0x86, 0x44, 0x68, 0x45, 0x15, 0x61,
        0x86, 0x54, 0x66, 0x56, 0x54, 0x54, 0x31, 0x54,
        0x64, 0x64, 0x68, 0x45, 0x15, 0x15, 0x61, 0x61,
        0x51, 0x51, 0x51, 0x51, 0x51, 0x56, 0x14, 0x11,
        0x72, 0x13, 0x51, 0x82, 0x84, 0x56, 0x74, 0x53,
        0x45, 0x34, 0x65, 0x15, 0x46, 0x14, 0x67, 0x55,
        0x16, 0x14, 0x67, 0x54, 0x65, 0x47, 0x14, 0x67,
        0x46, 0x74, 0x65, 0x46
};

static Cpa8U expectedOutput[] = {
        0x59, 0x85, 0x02, 0x97, 0xE0, 0x4D, 0xFC, 0x5C,
        0x03, 0xCC, 0x83, 0x64, 0xCE, 0x28, 0x0B, 0x95,
        0x78, 0xEC, 0x93, 0x40, 0xA1, 0x8D, 0x21, 0xC5,
        0x48, 0x6A, 0x39, 0xBA, 0x4F, 0x4B, 0x8C, 0x95,
        0x6F, 0x8C, 0xF6, 0x9C, 0xD0, 0xA5, 0x8D, 0x67,
        0xA1, 0x32, 0x11, 0xE7, 0x2E, 0xF6, 0x63, 0xAF,
        0xDE, 0xD4, 0x7D, 0xEC, 0x15, 0x01, 0x58, 0xCB,
        0xE3, 0x7B, 0xC6, 0x94,

        /* Tag */

        0x5D, 0x10, 0x3F, 0xC7, 0x22, 0xC7, 0x21, 0x29,
        0x95, 0x10, 0xF9, 0x9E, 0x81, 0xE7, 0x8A, 0x07
};

#else

static Cpa8U sampleKey[] = {
        0x47, 0x43, 0x4D, 0x31, 0x36, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0X00, 0x00, 0x00, 0x00
};

static Cpa8U sampleIv[] = {
        0x4E, 0x4F, 0x4E, 0x45, 0x93, 0x2F, 0xA9, 0x4F,
        0x4A, 0xF6, 0xDB, 0xB6
};

static Cpa8U sampleAddAuthData[] = {
        0x07, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x01
};

static Cpa8U samplePayload[] = {
        0x45, 0x00, 0x00, 0x54, 0x92, 0x43, 0x40, 0x00,
        0x40, 0x01, 0x30, 0x10, 0xc0, 0xa8, 0xfb, 0x01,

        0xc0, 0xa8, 0xfc, 0x02, 0x08, 0x00, 0x57, 0xD4,
        0xDE, 0x0D, 0x00, 0x01, 0x7A, 0x04, 0x8F, 0x5A,

        0x00, 0x00, 0x00, 0x00, 0xF7, 0xEA, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x11, 0x12, 0x13,

        0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,

        0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
        0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33,

        0x34, 0x35, 0x36, 0x37, 0x01, 0x02, 0x02, 0x04
};

static Cpa8U expectedOutput[] = {
        0xb3, 0x13, 0xfe, 0x1e, 0xcf, 0x0b, 0xba, 0x74,
        0xb3, 0xeb, 0x28, 0xc5, 0x20, 0xd2, 0x3e, 0xca,

        0x6b, 0xdc, 0x48, 0x61, 0x05, 0x8c, 0xc5, 0x2f,
        0x9f, 0xaa, 0xd0, 0x10, 0xa4, 0xeb, 0xc8, 0xfd,

        0xe3, 0x85, 0x7b, 0x68, 0x5b, 0x18, 0xe0, 0x43,
        0x00, 0x21, 0x06, 0xcc, 0xa3, 0x7e, 0xbc, 0xc6,

        0x7d, 0x2f, 0xbe, 0xda, 0x0c, 0x50, 0x73, 0xd0,
        0xf6, 0x86, 0x8b, 0xcf, 0xe1, 0x26, 0xa6, 0xb2,

        0x46, 0x61, 0x54, 0xcc, 0xc9, 0x84, 0xd3, 0xec,
        0xbf, 0x3d, 0xae, 0x35, 0x97, 0xeb, 0x32, 0xe5,

        0x30, 0x04, 0xd9, 0xcc, 0x2e, 0x78, 0x02, 0xc9,

        /* Tag */

        0x58, 0xbd, 0x2a, 0x75, 0x04, 0x01, 0xbc, 0xfd,
        0x08, 0x30, 0x98, 0x93, 0x14, 0x94, 0x9f, 0x30
};

#endif

#define TAG_LENGTH	(sizeof(expectedOutput) - sizeof(samplePayload))

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
 * of a decrypted packet, etc.  In this example, the function checks
 * verifyResult returned and sets the complete variable to indicate
 * it has been called.
 */

static void
symcCallbackGCM(void *pCallbackTag,
        CpaStatus status,
        const CpaCySymOp operationType,
        void *pOpData,
        CpaBufferList *pDstBuffer,
        CpaBoolean verifyResult)
{
    PRINT_DBG("Callback called with status = %d.\n", status);

    if(CPA_FALSE == verifyResult)
    {
       PRINT_ERR("Error verifyResult failed\n");
    }

    if (NULL != pCallbackTag)
    {
        /** indicate that the function has been called */
        COMPLETE((struct COMPLETION_STRUCT *)pCallbackTag);
    }
}

/*
 * Perform a simple GCM test
 */

static CpaStatus
PerformOpGCM(CpaInstanceHandle cyInstHandle, CpaCySymSessionCtx sessionCtx, int dir)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    Cpa8U  *pBufferMeta = NULL;
    Cpa32U bufferMetaSize = 0;
    CpaBufferList *pBufferList = NULL;
    CpaFlatBuffer *pFlatBuffer = NULL;
    CpaCySymOpData *pOpData = NULL;
    Cpa32U bufferSize = sizeof(samplePayload) + TAG_LENGTH;
    Cpa32U aadBuffSize = 0;
    Cpa32U numBuffers = 1;  /* only using 1 buffer in this case */
    /* allocate memory for bufferlist and array of flat buffers in a contiguous
     * area and carve it up to reduce number of memory allocations required. */
    Cpa32U bufferListMemSize = sizeof(CpaBufferList) +
        (numBuffers * sizeof(CpaFlatBuffer));
    Cpa8U  *pSrcBuffer = NULL;
    Cpa8U  *pIvBuffer = NULL;
    Cpa8U  *pAadBuffer = NULL;

    /* The following variables are allocated on the stack because we block
     * until the callback comes back. If a non-blocking approach was to be
     * used then these variables should be dynamically allocated */
    struct COMPLETION_STRUCT complete;

    /* get meta information size */
    PRINT_DBG("cpaCyBufferListGetMetaSize\n");
    status = cpaCyBufferListGetMetaSize( cyInstHandle,
                numBuffers, &bufferMetaSize);

    if (CPA_STATUS_SUCCESS == status)
    {
        status = PHYS_CONTIG_ALLOC(&pBufferMeta, bufferMetaSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        status = OS_MALLOC(&pBufferList, bufferListMemSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        status = PHYS_CONTIG_ALLOC(&pSrcBuffer, bufferSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* increment by sizeof(CpaBufferList) to get at the
         * array of flatbuffers */
        pFlatBuffer = (CpaFlatBuffer *) (pBufferList + 1);

        pBufferList->pBuffers = pFlatBuffer;
        pBufferList->numBuffers = 1;
        pBufferList->pPrivateMetaData = pBufferMeta;

        pFlatBuffer->dataLenInBytes = bufferSize;
        pFlatBuffer->pData = pSrcBuffer;

        /* copy source into buffer */
        if(CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT == dir)
        {
           memcpy(pSrcBuffer, samplePayload, sizeof(samplePayload));
        }
        else
        {
           memcpy(pSrcBuffer, expectedOutput, sizeof(expectedOutput));
        }
        /* Allocate memory to store IV. For GCM this is the block J0
         * (size equal to AES block size). If iv is 12 bytes the
         * implementation will construct the J0 block given the iv.
         * If iv is not 12 bytes then the user must contruct the J0
         * block and give this as the iv. In both cases space for J0
         * must be allocated. */
        status = PHYS_CONTIG_ALLOC(&pIvBuffer, AES_BLOCK_SIZE);
    }
    if (CPA_STATUS_SUCCESS == status)
    {
        /* Allocate memory for AAD. For GCM this memory will hold the
         * additional authentication data and any padding to ensure total
         * size is a multiple of the AES block size
         */
        aadBuffSize = sizeof(sampleAddAuthData);
        if(aadBuffSize%AES_BLOCK_SIZE)
        {
           aadBuffSize += AES_BLOCK_SIZE-(aadBuffSize%AES_BLOCK_SIZE);
        }
        status = PHYS_CONTIG_ALLOC(&pAadBuffer, aadBuffSize);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        memcpy(pAadBuffer, sampleAddAuthData, sizeof(sampleAddAuthData));
        status = OS_MALLOC(&pOpData, sizeof(CpaCySymOpData));
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        if(12 == sizeof(sampleIv))
        {
           pOpData->sessionCtx = sessionCtx;
           pOpData->packetType = CPA_CY_SYM_PACKET_TYPE_FULL;
           pOpData->pIv = pIvBuffer;
           /* In this example iv is 12 bytes. The implementation
            * will use the iv to generation the J0 block
            */
           memcpy(pIvBuffer, sampleIv, sizeof(sampleIv));
           pOpData->ivLenInBytes = sizeof(sampleIv);
           pOpData->cryptoStartSrcOffsetInBytes = 0;
           pOpData->messageLenToCipherInBytes = sizeof(samplePayload);
           /* For GCM hash offset and length are not required */
           pOpData->pAdditionalAuthData = pAadBuffer;
        }
        else
        {
           /* Need to generate J0 block see SP800-38D */
        }
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /** initialisation for callback; the "complete" variable is used by the
         * callback function to indicate it has been called*/
        COMPLETION_INIT(&complete);

        PRINT_DBG("cpaCySymPerformOp\n");

        /** Perform symmetric operation */
        status = cpaCySymPerformOp(cyInstHandle,
                (void *)&complete, /* data sent as is to the callback function*/
                pOpData,           /* operational data struct */
                pBufferList,       /* source buffer list */
                pBufferList,       /* same src & dst for an in-place operation*/
                NULL);             /* pVerifyResult not required in async mode */

        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("cpaCySymPerformOp failed. (status = %d)\n", status);
        }

        if (CPA_STATUS_SUCCESS == status)
        {
            /** wait until the completion of the operation*/
            if (!COMPLETION_WAIT(&complete, 5000))
            {
                PRINT_ERR("timeout or interruption in cpaCySymPerformOp\n");
                status = CPA_STATUS_FAIL;
            }
        }

        if (CPA_STATUS_SUCCESS == status)
        {
            if(CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT == dir)
            {
               if (0 == memcmp(pSrcBuffer, expectedOutput, bufferSize))
               {
                   PRINT_DBG("Output matches expected output GCM encrypt\n");
               }
               else
               {
                   PRINT_ERR("Output does not match expected output GCM encrypt\n");
                   PrintHex(pSrcBuffer, bufferSize);
                   status = CPA_STATUS_FAIL;
               }
            }
            else
            {
               if (0 == memcmp(pSrcBuffer, samplePayload, sizeof(samplePayload)))
               {
                   PRINT_DBG("Output matches expected output GCM decrypt\n");
               }
               else
               {
                   PRINT_ERR("Output does not match expected output GCM decrypt\n");
                   PrintHex(pSrcBuffer, sizeof(samplePayload));
                   status = CPA_STATUS_FAIL;
               }
            }
        }
    }




    /* at this stage, the callback function has returned, so it is sure that
     * the structures won't be needed any more*/
    PHYS_CONTIG_FREE(pSrcBuffer);
    PHYS_CONTIG_FREE(pIvBuffer);
    PHYS_CONTIG_FREE(pAadBuffer);
    OS_FREE(pBufferList);
    PHYS_CONTIG_FREE(pBufferMeta);
    OS_FREE(pOpData);

    COMPLETION_DESTROY(&complete);

    return status;
}

/*
 * Set up a simple GCM test
 */

CpaStatus
SampleTestGCM(void)
{
    CpaStatus status = CPA_STATUS_FAIL;
    CpaCySymSessionCtx sessionCtx = NULL;
    Cpa32U sessionCtxSize = 0;
    CpaInstanceHandle cyInstHandle = NULL;
    CpaCySymSessionSetupData sessionSetupData = {0};
    CpaCySymStats64 symStats = {0};

    printk("\n\n");

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

    if(CPA_STATUS_SUCCESS == status)
    {
       /*
        * Set the address translation function for the instance
        */
        status = cpaCySetAddressTranslation(cyInstHandle, sampleVirtToPhys);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
       /*
        * If the instance is polled start the polling thread. Note that
        * how the polling is done is implementation-dependant.
        */
        sampleCyStartPolling(cyInstHandle);

        PRINT_DBG("Authenticated Encryption\n");

        /* populate symmetric session data structure */
        sessionSetupData.sessionPriority =  CPA_CY_PRIORITY_NORMAL;
        sessionSetupData.symOperation = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
        sessionSetupData.algChainOrder =
                                    CPA_CY_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH;

        sessionSetupData.cipherSetupData.cipherAlgorithm =
                                    CPA_CY_SYM_CIPHER_AES_GCM;
        sessionSetupData.cipherSetupData.pCipherKey = sampleKey;
        sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                                    sizeof(sampleKey);
        sessionSetupData.cipherSetupData.cipherDirection =
                                    CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT;

        sessionSetupData.hashSetupData.hashAlgorithm =  CPA_CY_SYM_HASH_AES_GCM;
        sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
        sessionSetupData.hashSetupData.digestResultLenInBytes = TAG_LENGTH;
        /* For GCM authKey and authKeyLen are not required this information
           is provided by the cipherKey in cipherSetupData */
        sessionSetupData.hashSetupData.authModeSetupData.aadLenInBytes =
                                                 sizeof(sampleAddAuthData);
        /* Tag follows immediately after the region to hash */
        sessionSetupData.digestIsAppended = CPA_TRUE;
        /* digestVerify is not required to be set. For GCM authenticated
           encryption this value is understood to be CPA_FALSE */

        /* Determine size of session context to allocate */
        PRINT_DBG("cpaCySymSessionCtxGetSize GCM encrypt\n");
        status = cpaCySymSessionCtxGetSize(cyInstHandle,
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
        PRINT_DBG("cpaCySymInitSession GCM encrypt\n");
        status = cpaCySymInitSession(cyInstHandle,
                    symcCallbackGCM, &sessionSetupData, sessionCtx);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        CpaStatus sessionStatus = CPA_STATUS_SUCCESS;

        /* Perform algchaining operation */
        status = PerformOpGCM(cyInstHandle, sessionCtx,
                              CPA_CY_SYM_CIPHER_DIRECTION_ENCRYPT);

        /* Remove the session - session init has already succeeded */
        PRINT_DBG("cpaCySymRemoveSession GCM encrypt\n");
        sessionStatus = cpaCySymRemoveSession(
                            cyInstHandle, sessionCtx);

        /* maintain status of remove session only when status of all operations
         * before it are successful. */
        if (CPA_STATUS_SUCCESS == status)
        {
            status = sessionStatus;
        }
    }

    if(CPA_STATUS_SUCCESS == status)
    {
        PRINT_DBG("Authenticated Decryption\n");

        /* populate symmetric session data structure */
        sessionSetupData.sessionPriority =  CPA_CY_PRIORITY_NORMAL;
        sessionSetupData.symOperation = CPA_CY_SYM_OP_ALGORITHM_CHAINING;
        sessionSetupData.algChainOrder =
                                    CPA_CY_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER;

        sessionSetupData.cipherSetupData.cipherAlgorithm =
                                    CPA_CY_SYM_CIPHER_AES_GCM;
        sessionSetupData.cipherSetupData.pCipherKey = sampleKey;
        sessionSetupData.cipherSetupData.cipherKeyLenInBytes =
                                    sizeof(sampleKey);
        sessionSetupData.cipherSetupData.cipherDirection =
                                    CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT;

        sessionSetupData.hashSetupData.hashAlgorithm =  CPA_CY_SYM_HASH_AES_GCM;
        sessionSetupData.hashSetupData.hashMode = CPA_CY_SYM_HASH_MODE_AUTH;
        sessionSetupData.hashSetupData.digestResultLenInBytes = TAG_LENGTH;

        /* For GCM authKey and authKeyLen are not required this information
           is provided by the cipherKey in cipherSetupData */
        sessionSetupData.hashSetupData.authModeSetupData.aadLenInBytes =
                                                 sizeof(sampleAddAuthData);
        /* Tag follows immediately after the region to hash */
        sessionSetupData.digestIsAppended = CPA_TRUE;
        /* digestVerify is not required to be set. For GCM authenticated
           decryption this value is understood to be CPA_TRUE */

    }
    if (CPA_STATUS_SUCCESS == status)
    {
        /* Initialize the session */
        PRINT_DBG("cpaCySymInitSession GCM Decrypt\n");
        status = cpaCySymInitSession(cyInstHandle,
                    symcCallbackGCM, &sessionSetupData, sessionCtx);
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        CpaStatus sessionStatus = CPA_STATUS_SUCCESS;

        /* Perform algchaining operation */
        status = PerformOpGCM(cyInstHandle, sessionCtx,
                              CPA_CY_SYM_CIPHER_DIRECTION_DECRYPT);

        /* Remove the session - session init has already succeeded */
        PRINT_DBG("cpaCySymRemoveSession GCM decrypt\n");
        sessionStatus = cpaCySymRemoveSession(
                            cyInstHandle, sessionCtx);

        /* maintain status of remove session only when status of all operations
         * before it are successful. */
        if (CPA_STATUS_SUCCESS == status)
        {
            status = sessionStatus;
        }
    }

    if (CPA_STATUS_SUCCESS == status)
    {
        /* Query symmetric statistics */
        status = cpaCySymQueryStats64(cyInstHandle, &symStats);

        if (CPA_STATUS_SUCCESS != status)
        {
            PRINT_ERR("cpaCySymQueryStats failed, status = %d\n", status);
        }
        else
        {
            PRINT_DBG("Number of symmetric operation completed: %llu\n",
                    (unsigned long long)symStats.numSymOpCompleted);
        }
    }

    /* Clean up */

    /* Free session Context */
    PHYS_CONTIG_FREE(sessionCtx);

    /* Stop the polling thread */
    sampleCyStopPolling();


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

/*
 * Generic test wrapper, change to call specific test as needed.
 */

CpaStatus
SampleTest(void)
{
  return SampleTestGCM();
}

#endif	// SAMPLE_TEST
