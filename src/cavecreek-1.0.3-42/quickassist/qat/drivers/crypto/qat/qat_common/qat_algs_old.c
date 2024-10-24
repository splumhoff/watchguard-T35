/*
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY
  Copyright(c) 2014 Intel Corporation.
  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
  qat-linux@intel.com

  BSD LICENSE
  Copyright(c) 2014 Intel Corporation.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifdef QAT_AEAD_OLD_SUPPORTED
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/internal/aead.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <crypto/algapi.h>
#include <crypto/authenc.h>
#include <crypto/rng.h>
#include <linux/dma-mapping.h>
#include "adf_accel_devices.h"
#include "adf_transport.h"
#include "adf_common_drv.h"
#include "qat_crypto.h"
#include "icp_qat_hw.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_la.h"

#ifdef	CONFIG_WG_PLATFORM

#define	CONFIG_WG_PLATFORM_GCM		1
#define	CONFIG_WG_PLATFORM_SHA2		1
#define	CONFIG_WG_PLATFORM_MODE0	1

#define	PrintK(...)
#define	PrintHex(...)

#ifdef	CONFIG_WG_PLATFORM_GCM

#if	CONFIG_WG_PLATFORM_GCM > 1

#define	static

#undef	PrintK
#undef	PrintHex

#define	PrintK		if (unlikely(console_loglevel & 16)) printk
#define	PrintHex(a,b)	if (unlikely(console_loglevel & 32)) wg_dump_hex((u8*)(a),(b),"")

#endif

#define	GCM_NONCE_SIZE		( 4)
#define	GCM_IV_SIZE		( 8)
#define	GCM_DIGEST_SIZE		(16)
#define	GCM_AAD_PAD_SIZE	(16)

#define	GCM_STATE1_SZ		(16)
#define	GCM_STATE2_SZ		(40)
#define	GCM_PARAMS_SZ		((int)(sizeof(struct icp_qat_hw_auth_setup) + \
				 GCM_STATE1_SZ + GCM_STATE2_SZ + \
				 sizeof(struct icp_qat_hw_cipher_config)))

#define	QAT_AES_HW_CONFIG_GCM_ENC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CTR_MODE, alg, \
				       ICP_QAT_HW_CIPHER_NO_CONVERT, \
				       ICP_QAT_HW_CIPHER_ENCRYPT)

#define	QAT_AES_HW_CONFIG_GCM_DEC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CTR_MODE, alg, \
				       ICP_QAT_HW_CIPHER_NO_CONVERT, \
				       ICP_QAT_HW_CIPHER_ENCRYPT)

static	struct crypto_cipher* gcm_aes_cipher;

#if	CONFIG_WG_PLATFORM_GCM > 8

char	GCMKey[] = {
	0x47, 0x43, 0x4D, 0x31, 0x36, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0X00, 0x00, 0x00, 0x00
};

char	GCMAad[] = {
	0x07, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x01
};

char	GCMIv[]  = {
	0x4E, 0x4F, 0x4E, 0x45, 0x93, 0x2F, 0xA9, 0x4F,
	0x4A, 0xF6, 0xDB, 0xB6
};

char	GCMSrc[] = {
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

char	GCMDst[] = {
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
	// Tag
	0x58, 0xbd, 0x2a, 0x75, 0x04, 0x01, 0xbc, 0xfd,
	0x08, 0x30, 0x98, 0x93, 0x14, 0x94, 0x9f, 0x30
};

#endif	// CONFIG_WG_PLATFORM_GCM > 8

#endif	// CONFIG_WG_PLATFORM_GCM

#endif	// CONFIG_WG_PLATFORM

#define	QAT_AES_HW_CONFIG_CBC_ENC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CBC_MODE, alg, \
				       ICP_QAT_HW_CIPHER_NO_CONVERT, \
				       ICP_QAT_HW_CIPHER_ENCRYPT)

#define	QAT_AES_HW_CONFIG_CBC_DEC(alg) \
	ICP_QAT_HW_CIPHER_CONFIG_BUILD(ICP_QAT_HW_CIPHER_CBC_MODE, alg, \
				       ICP_QAT_HW_CIPHER_KEY_CONVERT, \
				       ICP_QAT_HW_CIPHER_DECRYPT)

static DEFINE_MUTEX(algs_lock);
static unsigned int active_devs;

struct qat_alg_buf {
	uint32_t len;
	uint32_t resrvd;
	uint64_t addr;
} __packed;

struct qat_alg_buf_list {
	uint64_t resrvd;
	uint32_t num_bufs;
	uint32_t num_mapped_bufs;
	struct qat_alg_buf bufers[];
} __packed __aligned(64);

/* Common content descriptor */
struct qat_alg_cd {
	union {
		struct qat_enc { /* Encrypt content desc */
			struct icp_qat_hw_cipher_algo_blk cipher;
			struct icp_qat_hw_auth_algo_blk hash;
		} qat_enc_cd;
		struct qat_dec { /* Decrytp content desc */
			struct icp_qat_hw_auth_algo_blk hash;
			struct icp_qat_hw_cipher_algo_blk cipher;
		} qat_dec_cd;
	};
} __aligned(64);

struct qat_alg_aead_ctx {
	struct qat_alg_cd *enc_cd;
	struct qat_alg_cd *dec_cd;
	dma_addr_t enc_cd_paddr;
	dma_addr_t dec_cd_paddr;
	struct icp_qat_fw_la_bulk_req enc_fw_req;
	struct icp_qat_fw_la_bulk_req dec_fw_req;
	struct crypto_shash *hash_tfm;
	enum icp_qat_hw_auth_algo qat_hash_alg;
	struct qat_crypto_instance *inst;
	struct crypto_tfm *tfm;
	uint8_t salt[AES_BLOCK_SIZE];
	spinlock_t lock;	/* protects qat_alg_aead_ctx struct */
#ifdef	CONFIG_WG_PLATFORM_GCM
	u32	   gcm_keylen;	/* keylen for GCM mode operations */
	u32	   gcm_nonce;	/* nonce  for GCM mode operations */
	atomic64_t gcm_seqno;	/* seqno  for GCM mode operations */
#endif
};

struct qat_alg_ablkcipher_ctx {
	struct icp_qat_hw_cipher_algo_blk *enc_cd;
	struct icp_qat_hw_cipher_algo_blk *dec_cd;
	dma_addr_t enc_cd_paddr;
	dma_addr_t dec_cd_paddr;
	struct icp_qat_fw_la_bulk_req enc_fw_req;
	struct icp_qat_fw_la_bulk_req dec_fw_req;
	struct qat_crypto_instance *inst;
	struct crypto_tfm *tfm;
	spinlock_t lock;	/* protects qat_alg_ablkcipher_ctx struct */
};

static int qat_get_inter_state_size(enum icp_qat_hw_auth_algo qat_hash_alg)
{
	switch (qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		return ICP_QAT_HW_SHA1_STATE1_SZ;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		return ICP_QAT_HW_SHA256_STATE1_SZ;
#ifdef	CONFIG_WG_PLATFORM_SHA2 // WG:JB SHA384
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		return ICP_QAT_HW_SHA384_STATE1_SZ;
#endif
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		return ICP_QAT_HW_SHA512_STATE1_SZ;
	default:
		return -EFAULT;
	};
	return -EFAULT;
}

static int qat_alg_do_precomputes(struct icp_qat_hw_auth_algo_blk *hash,
				  struct qat_alg_aead_ctx *ctx,
				  const uint8_t *auth_key,
				  unsigned int auth_keylen)
{
	SHASH_DESC_ON_STACK(shash, ctx->hash_tfm);
	struct sha1_state sha1;
	struct sha256_state sha256;
	struct sha512_state sha512;

	int block_size = crypto_shash_blocksize(ctx->hash_tfm);
	int digest_size = crypto_shash_digestsize(ctx->hash_tfm);
	char ipad[block_size];
	char opad[block_size];
	__be32 *hash_state_out;
	__be64 *hash512_state_out;
	int i, offset;

#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen) {
		char gcm_zeros[AES_BLOCK_SIZE] = {0};

		hash->sha.inner_setup.auth_counter.counter = 0;

		crypto_cipher_setkey(gcm_aes_cipher, auth_key, auth_keylen);
		crypto_cipher_encrypt_one(gcm_aes_cipher,
					  &hash->sha.state1[GCM_STATE1_SZ],
					  gcm_zeros);

		return 0;
	}
#endif

#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	if (unlikely(auth_keylen == 0))
	if (unlikely(wg_fips_sha >  0)) {
		auth_keylen = wg_fips_sha_len;
		auth_key    = wg_fips_sha_key;
	}
#endif

	memset(ipad, 0, block_size);
	memset(opad, 0, block_size);
	shash->tfm = ctx->hash_tfm;
	shash->flags = 0x0;

	if (auth_keylen > block_size) {
		int ret = crypto_shash_digest(shash, auth_key,
					      auth_keylen, ipad);
		if (ret)
			return ret;

		memcpy(opad, ipad, digest_size);
	} else {
		memcpy(ipad, auth_key, auth_keylen);
		memcpy(opad, auth_key, auth_keylen);
	}

	for (i = 0; i < block_size; i++) {
		char *ipad_ptr = ipad + i;
		char *opad_ptr = opad + i;
		*ipad_ptr ^= 0x36;
		*opad_ptr ^= 0x5C;
	}

	if (crypto_shash_init(shash))
		return -EFAULT;

	if (crypto_shash_update(shash, ipad, block_size))
		return -EFAULT;

	hash_state_out = (__be32 *)hash->sha.state1;
	hash512_state_out = (__be64 *)hash_state_out;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		if (crypto_shash_export(shash, &sha1))
			return -EFAULT;
		for (i = 0; i < digest_size >> 2; i++, hash_state_out++)
			*hash_state_out = cpu_to_be32(*(sha1.state + i));
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		if (crypto_shash_export(shash, &sha256))
			return -EFAULT;
		for (i = 0; i < digest_size >> 2; i++, hash_state_out++)
			*hash_state_out = cpu_to_be32(*(sha256.state + i));
		break;
#ifdef	CONFIG_WG_PLATFORM_SHA2 // WG:JB SHA384
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
#endif
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		if (crypto_shash_export(shash, &sha512))
			return -EFAULT;
		for (i = 0; i < digest_size >> 3; i++, hash512_state_out++)
			*hash512_state_out = cpu_to_be64(*(sha512.state + i));
		break;
	default:
		return -EFAULT;
	}

	if (crypto_shash_init(shash))
		return -EFAULT;

	if (crypto_shash_update(shash, opad, block_size))
		return -EFAULT;

	offset = round_up(qat_get_inter_state_size(ctx->qat_hash_alg), 8);
	hash_state_out = (__be32 *)(hash->sha.state1 + offset);
	hash512_state_out = (__be64 *)hash_state_out;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		if (crypto_shash_export(shash, &sha1))
			return -EFAULT;
		for (i = 0; i < digest_size >> 2; i++, hash_state_out++)
			*hash_state_out = cpu_to_be32(*(sha1.state + i));
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		if (crypto_shash_export(shash, &sha256))
			return -EFAULT;
		for (i = 0; i < digest_size >> 2; i++, hash_state_out++)
			*hash_state_out = cpu_to_be32(*(sha256.state + i));
		break;
#ifdef	CONFIG_WG_PLATFORM_SHA2 // WG:JB SHA384
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
#endif
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		if (crypto_shash_export(shash, &sha512))
			return -EFAULT;
		for (i = 0; i < digest_size >> 3; i++, hash512_state_out++)
			*hash512_state_out = cpu_to_be64(*(sha512.state + i));
		break;
	default:
		return -EFAULT;
	}
	memzero_explicit(ipad, block_size);
	memzero_explicit(opad, block_size);
	return 0;
}

static void qat_alg_init_common_hdr(struct icp_qat_fw_comn_req_hdr *header)
{
	header->hdr_flags =
		ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(ICP_QAT_FW_COMN_REQ_FLAG_SET);
	header->service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_LA;
	header->comn_req_flags =
		ICP_QAT_FW_COMN_FLAGS_BUILD(QAT_COMN_CD_FLD_TYPE_64BIT_ADR,
					    QAT_COMN_PTR_TYPE_SGL);
	ICP_QAT_FW_LA_PARTIAL_SET(header->serv_specif_flags,
				  ICP_QAT_FW_LA_PARTIAL_NONE);
#ifdef	CONFIG_WG_PLATFORM_MODE0 // WG:JB FIPS algos
	if (unlikely(wg_fips_sha_mode0)) {
	ICP_QAT_FW_LA_CIPH_AUTH_CFG_OFFSET_FLAG_SET(header->serv_specif_flags,
						    ICP_QAT_FW_CIPH_AUTH_CFG_OFFSET_IN_SHRAM_CP);
	} else
#endif
	ICP_QAT_FW_LA_CIPH_IV_FLD_FLAG_SET(header->serv_specif_flags,
					   ICP_QAT_FW_CIPH_IV_16BYTE_DATA);
	ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
				ICP_QAT_FW_LA_NO_PROTO);
	ICP_QAT_FW_LA_UPDATE_STATE_SET(header->serv_specif_flags,
				       ICP_QAT_FW_LA_NO_UPDATE_STATE);
}

#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos

static void qat_alg_aeadcipher_init_com(struct icp_qat_fw_la_bulk_req *req,
					struct icp_qat_hw_cipher_algo_blk *cd,
					const uint8_t *key, unsigned int keylen)
{
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req->comn_hdr;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cd_ctrl = (void *)&req->cd_ctrl;

	memcpy(cd->aes.key, key, keylen);
	qat_alg_init_common_hdr(header);
	header->service_cmd_id = ICP_QAT_FW_LA_CMD_CIPHER;
	cd_pars->u.s.content_desc_params_sz =
		sizeof(struct icp_qat_hw_cipher_algo_blk) >> 3;

	/* Cipher CD config setup */
	cd_ctrl->cipher_key_sz = keylen >> 3;
	cd_ctrl->cipher_state_sz = AES_BLOCK_SIZE >> 3;
	cd_ctrl->cipher_cfg_offset = 0;
	ICP_QAT_FW_COMN_CURR_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
	ICP_QAT_FW_COMN_NEXT_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_DRAM_WR);
}

static void qat_alg_aeadcipher_init_enc(struct qat_alg_aead_ctx *ctx,
					int alg, const uint8_t *key,
					unsigned int keylen)
{
	struct qat_enc *enc_ctx = &ctx->enc_cd->qat_enc_cd;
	struct icp_qat_hw_cipher_algo_blk *enc_cd = &enc_ctx->cipher;
	struct icp_qat_fw_la_bulk_req *req = &ctx->enc_fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;


	PrintK(KERN_DEBUG "%s: alg %d keylen %d\n", __FUNCTION__, alg, keylen);

	qat_alg_aeadcipher_init_com(req, enc_cd, key, keylen);
	cd_pars->u.s.content_desc_addr = ctx->enc_cd_paddr;
	enc_cd->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_ENC(alg);
}

static void qat_alg_aeadcipher_init_dec(struct qat_alg_aead_ctx *ctx,
					int alg, const uint8_t *key,
					unsigned int keylen)
{
	// Use 'enc' here since we need struct icp_qat_hw_cipher_algo_blk
	// to come first for aeadcipher operations given there is no hash.
	struct qat_enc *dec_ctx = &ctx->dec_cd->qat_enc_cd;
	struct icp_qat_hw_cipher_algo_blk *dec_cd = &dec_ctx->cipher;
	struct icp_qat_fw_la_bulk_req *req = &ctx->dec_fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;

	PrintK(KERN_DEBUG "%s: alg %d keylen %d\n", __FUNCTION__, alg, keylen);

	qat_alg_aeadcipher_init_com(req, dec_cd, key, keylen);
	cd_pars->u.s.content_desc_addr = ctx->dec_cd_paddr;
	dec_cd->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_DEC(alg);
}

#endif

static int qat_alg_aead_init_enc_session(struct qat_alg_aead_ctx *ctx,
					 int alg,
					 struct crypto_authenc_keys *keys)
{
	struct crypto_aead *aead_tfm = __crypto_aead_cast(ctx->tfm);
	unsigned int digestsize = crypto_aead_crt(aead_tfm)->authsize;
	struct qat_enc *enc_ctx = &ctx->enc_cd->qat_enc_cd;
	struct icp_qat_hw_cipher_algo_blk *cipher = &enc_ctx->cipher;
	struct icp_qat_hw_auth_algo_blk *hash =
		(struct icp_qat_hw_auth_algo_blk *)((char *)enc_ctx +
		sizeof(struct icp_qat_hw_auth_setup) + keys->enckeylen);
	struct icp_qat_fw_la_bulk_req *req_tmpl = &ctx->enc_fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req_tmpl->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req_tmpl->comn_hdr;
	void *ptr = &req_tmpl->cd_ctrl;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cipher_cd_ctrl = ptr;
	struct icp_qat_fw_auth_cd_ctrl_hdr *hash_cd_ctrl = ptr;

	/* CD setup */
#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen) {
	hash = (struct icp_qat_hw_auth_algo_blk *)((char *)enc_ctx +
		sizeof(struct icp_qat_hw_cipher_config) + keys->enckeylen);
	cipher->aes.cipher_config.val = QAT_AES_HW_CONFIG_GCM_ENC(alg);
	} else
#endif
	cipher->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_ENC(alg);
	memcpy(cipher->aes.key, keys->enckey, keys->enckeylen);
#ifdef	CONFIG_WG_PLATFORM_MODE0 // WG:JB FIPS algos
	if (unlikely(wg_fips_sha_mode0)) {

	hash = (struct icp_qat_hw_auth_algo_blk *)cipher;
	hash->sha.inner_setup.auth_config.config =
		ICP_QAT_HW_AUTH_CONFIG_BUILD(ICP_QAT_HW_AUTH_MODE0,
					     ctx->qat_hash_alg,
					     digestsize = wg_fips_sha / 8);
	} else
#endif
	hash->sha.inner_setup.auth_config.config =
		ICP_QAT_HW_AUTH_CONFIG_BUILD(ICP_QAT_HW_AUTH_MODE1,
					     ctx->qat_hash_alg, digestsize);
	hash->sha.inner_setup.auth_counter.counter =
		cpu_to_be32(crypto_shash_blocksize(ctx->hash_tfm));

	if (qat_alg_do_precomputes(hash, ctx, keys->authkey, keys->authkeylen))
		return -EFAULT;

	/* Request setup */
	qat_alg_init_common_hdr(header);
#ifdef	CONFIG_WG_PLATFORM_MODE0 // WG:JB FIPS algos
	if (unlikely(wg_fips_sha_mode0))
	header->service_cmd_id = ICP_QAT_FW_LA_CMD_AUTH;
	else
#endif
	header->service_cmd_id = ICP_QAT_FW_LA_CMD_CIPHER_HASH;

	ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
	ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
				   ICP_QAT_FW_LA_RET_AUTH_RES);
	ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
				   ICP_QAT_FW_LA_NO_CMP_AUTH_RES);
	cd_pars->u.s.content_desc_addr = ctx->enc_cd_paddr;
	cd_pars->u.s.content_desc_params_sz = sizeof(struct qat_alg_cd) >> 3;

	/* Cipher CD config setup */
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	if (likely(alg != 0)) {
#endif
	cipher_cd_ctrl->cipher_key_sz = keys->enckeylen >> 3;
	cipher_cd_ctrl->cipher_state_sz = AES_BLOCK_SIZE >> 3;
	cipher_cd_ctrl->cipher_cfg_offset = 0;
	ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	}
#endif
	ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl, ICP_QAT_FW_SLICE_AUTH);
	/* Auth CD config setup */
	hash_cd_ctrl->hash_cfg_offset = ((char *)hash - (char *)cipher) >> 3;
	hash_cd_ctrl->hash_flags = ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED;
	hash_cd_ctrl->inner_res_sz = digestsize;
	hash_cd_ctrl->final_sz = digestsize;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		hash_cd_ctrl->inner_state1_sz =
			round_up(ICP_QAT_HW_SHA1_STATE1_SZ, 8);
		hash_cd_ctrl->inner_state2_sz =
			round_up(ICP_QAT_HW_SHA1_STATE2_SZ, 8);
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_SHA256_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA256_STATE2_SZ;
		break;
#ifdef	CONFIG_WG_PLATFORM_SHA2 // WG:JB SHA384
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_SHA384_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA384_STATE2_SZ;
		break;
#endif
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_SHA512_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA512_STATE2_SZ;
		break;
#ifdef	CONFIG_WG_PLATFORM_GCM
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
		hash_cd_ctrl->inner_state1_sz = GCM_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = GCM_STATE2_SZ;
		hash_cd_ctrl->inner_res_sz    = GCM_DIGEST_SIZE;
		hash_cd_ctrl->final_sz	      = GCM_DIGEST_SIZE;

		cd_pars->u.s.content_desc_params_sz = (sizeof(struct icp_qat_hw_auth_setup) +
						       GCM_STATE1_SZ + GCM_STATE2_SZ +
						       sizeof(struct icp_qat_hw_cipher_config) +
						       keys->enckeylen) >> 3;

		ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
		ICP_QAT_FW_LA_GCM_PROTO);
		ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(header->serv_specif_flags,
		ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);

		PrintK(KERN_DEBUG "%s: cipher_algo_blk   %p %3d\n", __FUNCTION__, cipher,
			 (int)sizeof(struct icp_qat_hw_cipher_config) + keys->enckeylen);
		PrintHex(cipher,
			 (int)sizeof(struct icp_qat_hw_cipher_config) + keys->enckeylen);

		PrintK(KERN_DEBUG "%s: auth_algo_blk     %p %3d\n", __FUNCTION__, hash,
			 (int)sizeof(struct icp_qat_hw_auth_setup) + GCM_STATE1_SZ + GCM_STATE2_SZ);
		PrintHex(hash,
			 (int)sizeof(struct icp_qat_hw_auth_setup) + GCM_STATE1_SZ + GCM_STATE2_SZ);

		break;
#endif
	default:
		break;
	}
#ifdef	CONFIG_WG_PLATFORM_MODE0 // WG:JB FIPS algos
	if (unlikely(wg_fips_sha_mode0)) {

		hash_cd_ctrl->resrvd1		  = 0;
		hash_cd_ctrl->inner_state2_sz	  = 0;

		hash_cd_ctrl->inner_state2_offset = (hash_cd_ctrl->inner_state1_sz >> 3);

		switch (wg_fips_sha_mode0) {
		case 256: hash_cd_ctrl->hash_cfg_offset = 54; break;
		case 384: hash_cd_ctrl->hash_cfg_offset = 60; break;
		case 512: hash_cd_ctrl->hash_cfg_offset = 70; break;
		default:  hash_cd_ctrl->hash_cfg_offset = 41;
		}
	}
#endif
	hash_cd_ctrl->inner_state2_offset = hash_cd_ctrl->hash_cfg_offset +
			((sizeof(struct icp_qat_hw_auth_setup) +
			 round_up(hash_cd_ctrl->inner_state1_sz, 8)) >> 3);
	ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl, ICP_QAT_FW_SLICE_AUTH);
	ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl, ICP_QAT_FW_SLICE_DRAM_WR);
	return 0;
}

static int qat_alg_aead_init_dec_session(struct qat_alg_aead_ctx *ctx,
					 int alg,
					 struct crypto_authenc_keys *keys)
{
	struct crypto_aead *aead_tfm = __crypto_aead_cast(ctx->tfm);
	unsigned int digestsize = crypto_aead_crt(aead_tfm)->authsize;
	struct qat_dec *dec_ctx = &ctx->dec_cd->qat_dec_cd;
	struct icp_qat_hw_auth_algo_blk *hash = &dec_ctx->hash;
	struct icp_qat_hw_cipher_algo_blk *cipher =
		(struct icp_qat_hw_cipher_algo_blk *)((char *)dec_ctx +
		sizeof(struct icp_qat_hw_auth_setup) +
		roundup(crypto_shash_digestsize(ctx->hash_tfm), 8) * 2);
	struct icp_qat_fw_la_bulk_req *req_tmpl = &ctx->dec_fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req_tmpl->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req_tmpl->comn_hdr;
	void *ptr = &req_tmpl->cd_ctrl;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cipher_cd_ctrl = ptr;
	struct icp_qat_fw_auth_cd_ctrl_hdr *hash_cd_ctrl = ptr;
	struct icp_qat_fw_la_auth_req_params *auth_param =
		(struct icp_qat_fw_la_auth_req_params *)
		((char *)&req_tmpl->serv_specif_rqpars +
		sizeof(struct icp_qat_fw_la_cipher_req_params));

	/* CD setup */
#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen) {
	cipher = (struct icp_qat_hw_cipher_algo_blk *)((char *)dec_ctx +
		sizeof(struct icp_qat_hw_auth_setup) +
		GCM_STATE1_SZ + GCM_STATE2_SZ);
	cipher->aes.cipher_config.val = QAT_AES_HW_CONFIG_GCM_DEC(alg);
	} else
#endif
	cipher->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_DEC(alg);
	memcpy(cipher->aes.key, keys->enckey, keys->enckeylen);
	hash->sha.inner_setup.auth_config.config =
		ICP_QAT_HW_AUTH_CONFIG_BUILD(ICP_QAT_HW_AUTH_MODE1,
					     ctx->qat_hash_alg,
					     digestsize);
	hash->sha.inner_setup.auth_counter.counter =
		cpu_to_be32(crypto_shash_blocksize(ctx->hash_tfm));

	if (qat_alg_do_precomputes(hash, ctx, keys->authkey, keys->authkeylen))
		return -EFAULT;

	/* Request setup */
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	if (likely(alg != 0))
#endif
	qat_alg_init_common_hdr(header);
	header->service_cmd_id = ICP_QAT_FW_LA_CMD_HASH_CIPHER;
	ICP_QAT_FW_LA_DIGEST_IN_BUFFER_SET(header->serv_specif_flags,
					   ICP_QAT_FW_LA_DIGEST_IN_BUFFER);
	ICP_QAT_FW_LA_RET_AUTH_SET(header->serv_specif_flags,
				   ICP_QAT_FW_LA_NO_RET_AUTH_RES);
	ICP_QAT_FW_LA_CMP_AUTH_SET(header->serv_specif_flags,
				   ICP_QAT_FW_LA_CMP_AUTH_RES);
	cd_pars->u.s.content_desc_addr = ctx->dec_cd_paddr;
	cd_pars->u.s.content_desc_params_sz = sizeof(struct qat_alg_cd) >> 3;

	/* Cipher CD config setup */
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	if (likely(alg != 0)) {
#endif
	cipher_cd_ctrl->cipher_key_sz = keys->enckeylen >> 3;
	cipher_cd_ctrl->cipher_state_sz = AES_BLOCK_SIZE >> 3;
	cipher_cd_ctrl->cipher_cfg_offset =
		(sizeof(struct icp_qat_hw_auth_setup) +
		 roundup(crypto_shash_digestsize(ctx->hash_tfm), 8) * 2) >> 3;
	ICP_QAT_FW_COMN_CURR_ID_SET(cipher_cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
	ICP_QAT_FW_COMN_NEXT_ID_SET(cipher_cd_ctrl, ICP_QAT_FW_SLICE_DRAM_WR);
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	}
#endif

	/* Auth CD config setup */
	hash_cd_ctrl->hash_cfg_offset = 0;
	hash_cd_ctrl->hash_flags = ICP_QAT_FW_AUTH_HDR_FLAG_NO_NESTED;
	hash_cd_ctrl->inner_res_sz = digestsize;
	hash_cd_ctrl->final_sz = digestsize;

	switch (ctx->qat_hash_alg) {
	case ICP_QAT_HW_AUTH_ALGO_SHA1:
		hash_cd_ctrl->inner_state1_sz =
			round_up(ICP_QAT_HW_SHA1_STATE1_SZ, 8);
		hash_cd_ctrl->inner_state2_sz =
			round_up(ICP_QAT_HW_SHA1_STATE2_SZ, 8);
		break;
	case ICP_QAT_HW_AUTH_ALGO_SHA256:
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_SHA256_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA256_STATE2_SZ;
		break;
#ifdef	CONFIG_WG_PLATFORM_SHA2 // WG:JB SHA384
	case ICP_QAT_HW_AUTH_ALGO_SHA384:
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_SHA384_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA384_STATE2_SZ;
		break;
#endif
	case ICP_QAT_HW_AUTH_ALGO_SHA512:
		hash_cd_ctrl->inner_state1_sz = ICP_QAT_HW_SHA512_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = ICP_QAT_HW_SHA512_STATE2_SZ;
		break;
#ifdef	CONFIG_WG_PLATFORM_GCM
	case ICP_QAT_HW_AUTH_ALGO_GALOIS_128:
		hash_cd_ctrl->inner_state1_sz = GCM_STATE1_SZ;
		hash_cd_ctrl->inner_state2_sz = GCM_STATE2_SZ;
		hash_cd_ctrl->inner_res_sz    = GCM_DIGEST_SIZE;
		hash_cd_ctrl->final_sz	      = GCM_DIGEST_SIZE;

		cipher_cd_ctrl->cipher_cfg_offset   = (sizeof(struct icp_qat_hw_auth_setup) +
						       GCM_STATE1_SZ + GCM_STATE2_SZ) >> 3;

		cd_pars->u.s.content_desc_params_sz = (sizeof(struct icp_qat_hw_auth_setup) +
						       GCM_STATE1_SZ + GCM_STATE2_SZ +
						       sizeof(struct icp_qat_hw_cipher_config) +
						       keys->enckeylen) >> 3;

		ICP_QAT_FW_LA_PROTO_SET(header->serv_specif_flags,
		ICP_QAT_FW_LA_GCM_PROTO);
		ICP_QAT_FW_LA_GCM_IV_LEN_FLAG_SET(header->serv_specif_flags,
		ICP_QAT_FW_LA_GCM_IV_LEN_12_OCTETS);

		PrintK(KERN_DEBUG "%s: cipher_algo_blk %p %5d\n", __FUNCTION__, cipher,
			 (int)sizeof(struct icp_qat_hw_cipher_config) + keys->enckeylen);
		PrintHex(cipher,
			 (int)sizeof(struct icp_qat_hw_cipher_config) + keys->enckeylen);

		PrintK(KERN_DEBUG "%s: auth_algo_blk   %p %5d\n", __FUNCTION__, hash,
			 (int)sizeof(struct icp_qat_hw_auth_setup) + GCM_STATE1_SZ + GCM_STATE2_SZ);
		PrintHex(hash,
			 (int)sizeof(struct icp_qat_hw_auth_setup) + GCM_STATE1_SZ + GCM_STATE2_SZ);

		break;
#endif
	default:
		break;
	}

	hash_cd_ctrl->inner_state2_offset = hash_cd_ctrl->hash_cfg_offset +
			((sizeof(struct icp_qat_hw_auth_setup) +
			 round_up(hash_cd_ctrl->inner_state1_sz, 8)) >> 3);
	auth_param->auth_res_sz = digestsize;
	ICP_QAT_FW_COMN_CURR_ID_SET(hash_cd_ctrl, ICP_QAT_FW_SLICE_AUTH);
	ICP_QAT_FW_COMN_NEXT_ID_SET(hash_cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
	return 0;
}

static void qat_alg_ablkcipher_init_com(struct qat_alg_ablkcipher_ctx *ctx,
					struct icp_qat_fw_la_bulk_req *req,
					struct icp_qat_hw_cipher_algo_blk *cd,
					const uint8_t *key, unsigned int keylen)
{
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;
	struct icp_qat_fw_comn_req_hdr *header = &req->comn_hdr;
	struct icp_qat_fw_cipher_cd_ctrl_hdr *cd_ctrl = (void *)&req->cd_ctrl;

	memcpy(cd->aes.key, key, keylen);
	qat_alg_init_common_hdr(header);
	header->service_cmd_id = ICP_QAT_FW_LA_CMD_CIPHER;
	cd_pars->u.s.content_desc_params_sz =
		sizeof(struct icp_qat_hw_cipher_algo_blk) >> 3;

	/* Cipher CD config setup */
	cd_ctrl->cipher_key_sz = keylen >> 3;
	cd_ctrl->cipher_state_sz = AES_BLOCK_SIZE >> 3;
	cd_ctrl->cipher_cfg_offset = 0;
	ICP_QAT_FW_COMN_CURR_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_CIPHER);
	ICP_QAT_FW_COMN_NEXT_ID_SET(cd_ctrl, ICP_QAT_FW_SLICE_DRAM_WR);
}

static void qat_alg_ablkcipher_init_enc(struct qat_alg_ablkcipher_ctx *ctx,
					int alg, const uint8_t *key,
					unsigned int keylen)
{
	struct icp_qat_hw_cipher_algo_blk *enc_cd = ctx->enc_cd;
	struct icp_qat_fw_la_bulk_req *req = &ctx->enc_fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;

	qat_alg_ablkcipher_init_com(ctx, req, enc_cd, key, keylen);
	cd_pars->u.s.content_desc_addr = ctx->enc_cd_paddr;
	enc_cd->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_ENC(alg);
}

static void qat_alg_ablkcipher_init_dec(struct qat_alg_ablkcipher_ctx *ctx,
					int alg, const uint8_t *key,
					unsigned int keylen)
{
	struct icp_qat_hw_cipher_algo_blk *dec_cd = ctx->dec_cd;
	struct icp_qat_fw_la_bulk_req *req = &ctx->dec_fw_req;
	struct icp_qat_fw_comn_req_hdr_cd_pars *cd_pars = &req->cd_pars;

	qat_alg_ablkcipher_init_com(ctx, req, dec_cd, key, keylen);
	cd_pars->u.s.content_desc_addr = ctx->dec_cd_paddr;
	dec_cd->aes.cipher_config.val = QAT_AES_HW_CONFIG_CBC_DEC(alg);
}

static int qat_alg_validate_key(int key_len, int *alg)
{
	switch (key_len) {
	case AES_KEYSIZE_128:
		*alg = ICP_QAT_HW_CIPHER_ALGO_AES128;
		break;
	case AES_KEYSIZE_192:
		*alg = ICP_QAT_HW_CIPHER_ALGO_AES192;
		break;
	case AES_KEYSIZE_256:
		*alg = ICP_QAT_HW_CIPHER_ALGO_AES256;
		break;
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	case 0:
		*alg = ICP_QAT_HW_CIPHER_ALGO_NULL;
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

static int qat_alg_aead_init_sessions(struct qat_alg_aead_ctx *ctx,
				      const uint8_t *key, unsigned int keylen)
{
	struct crypto_authenc_keys keys;
	int alg;

	if (crypto_rng_get_bytes(crypto_default_rng, ctx->salt, AES_BLOCK_SIZE))
		return -EFAULT;

#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen == GCM_NONCE_SIZE) {

		ctx->gcm_keylen = keylen;

		keylen -= GCM_NONCE_SIZE;

		memcpy((void*)&ctx->gcm_nonce, &key[keylen],  GCM_NONCE_SIZE);
		memset((void*)&key[keylen],		  0,  GCM_NONCE_SIZE);
		memcpy((void*)&ctx->gcm_seqno, &ctx->salt[0], GCM_IV_SIZE);

		if (keylen == 0) key = NULL;

		keys.enckey    = keys.authkey    = key;
		keys.enckeylen = keys.authkeylen = keylen;

		PrintK(KERN_DEBUG "%s: gcmkey %p %5d\n", __FUNCTION__, key, keylen);
		PrintHex(key, keylen);
	}
	else
#endif
	if (crypto_authenc_extractkeys(&keys, key, keylen))
		goto bad_key;

	if (qat_alg_validate_key(keys.enckeylen, &alg))
		goto bad_key;

#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	if (unlikely(!ctx->qat_hash_alg)) if (alg) goto nohash;
#endif

	if (qat_alg_aead_init_enc_session(ctx, alg, &keys))
		goto error;

	if (qat_alg_aead_init_dec_session(ctx, alg, &keys))
		goto error;

	return 0;

#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
nohash:
	qat_alg_aeadcipher_init_enc(ctx, alg, keys.enckey, keys.enckeylen);
	qat_alg_aeadcipher_init_dec(ctx, alg, keys.enckey, keys.enckeylen);
	return 0;
#endif

bad_key:
	crypto_tfm_set_flags(ctx->tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
	return -EINVAL;
error:
	return -EFAULT;
}

static int qat_alg_ablkcipher_init_sessions(struct qat_alg_ablkcipher_ctx *ctx,
					    const uint8_t *key,
					    unsigned int keylen)
{
	int alg;

	if (qat_alg_validate_key(keylen, &alg))
		goto bad_key;

	qat_alg_ablkcipher_init_enc(ctx, alg, key, keylen);
	qat_alg_ablkcipher_init_dec(ctx, alg, key, keylen);
	return 0;
bad_key:
	crypto_tfm_set_flags(ctx->tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
	return -EINVAL;
}

static int qat_alg_aead_setkey(struct crypto_aead *tfm, const uint8_t *key,
			       unsigned int keylen)
{
	struct qat_alg_aead_ctx *ctx = crypto_aead_ctx(tfm);
	struct device *dev;

#ifdef	CONFIG_WG_PLATFORM_GCM
	PrintK(KERN_DEBUG "%s: keylen %d gcm %d\n", __FUNCTION__,
	       keylen, ctx->gcm_keylen);
#endif

	spin_lock(&ctx->lock);
	if (ctx->enc_cd) {
		/* rekeying */
		dev = &GET_DEV(ctx->inst->accel_dev);
		memset(ctx->enc_cd, 0, sizeof(*ctx->enc_cd));
		memset(ctx->dec_cd, 0, sizeof(*ctx->dec_cd));
		memset(&ctx->enc_fw_req, 0, sizeof(ctx->enc_fw_req));
		memset(&ctx->dec_fw_req, 0, sizeof(ctx->dec_fw_req));
	} else {
		/* new key */
		int node = get_current_node();
		struct qat_crypto_instance *inst =
				qat_crypto_get_instance_node(node);
		if (!inst) {
			spin_unlock(&ctx->lock);
			return -EINVAL;
		}

		dev = &GET_DEV(inst->accel_dev);
		ctx->inst = inst;
		ctx->enc_cd = dma_zalloc_coherent(dev, sizeof(*ctx->enc_cd),
						  &ctx->enc_cd_paddr,
						  GFP_ATOMIC);
		if (!ctx->enc_cd) {
			spin_unlock(&ctx->lock);
			return -ENOMEM;
		}
		ctx->dec_cd = dma_zalloc_coherent(dev, sizeof(*ctx->dec_cd),
						  &ctx->dec_cd_paddr,
						  GFP_ATOMIC);
		if (!ctx->dec_cd) {
			spin_unlock(&ctx->lock);
			goto out_free_enc;
		}
	}
	spin_unlock(&ctx->lock);
	if (qat_alg_aead_init_sessions(ctx, key, keylen))
		goto out_free_all;

	return 0;

out_free_all:
	memset(ctx->dec_cd, 0, sizeof(struct qat_alg_cd));
	dma_free_coherent(dev, sizeof(struct qat_alg_cd),
			  ctx->dec_cd, ctx->dec_cd_paddr);
	ctx->dec_cd = NULL;
out_free_enc:
	memset(ctx->enc_cd, 0, sizeof(struct qat_alg_cd));
	dma_free_coherent(dev, sizeof(struct qat_alg_cd),
			  ctx->enc_cd, ctx->enc_cd_paddr);
	ctx->enc_cd = NULL;
	return -ENOMEM;
}

static void qat_alg_free_bufl(struct qat_crypto_instance *inst,
			      struct qat_crypto_request *qat_req)
{
	struct device *dev = &GET_DEV(inst->accel_dev);
	struct qat_alg_buf_list *bl = qat_req->buf.bl;
	struct qat_alg_buf_list *blout = qat_req->buf.blout;
	dma_addr_t blp = qat_req->buf.blp;
	dma_addr_t blpout = qat_req->buf.bloutp;
	size_t sz = qat_req->buf.sz;
	size_t sz_out = qat_req->buf.sz_out;
	int i;

	for (i = 0; i < bl->num_bufs; i++)
		dma_unmap_single(dev, bl->bufers[i].addr,
				 bl->bufers[i].len, DMA_BIDIRECTIONAL);

	dma_unmap_single(dev, blp, sz, DMA_TO_DEVICE);
	kfree(bl);
	if (blp != blpout) {
		/* If out of place operation dma unmap only data */
		int bufless = blout->num_bufs - blout->num_mapped_bufs;

		for (i = bufless; i < blout->num_bufs; i++) {
			dma_unmap_single(dev, blout->bufers[i].addr,
					 blout->bufers[i].len,
					 DMA_BIDIRECTIONAL);
		}
		dma_unmap_single(dev, blpout, sz_out, DMA_TO_DEVICE);
		kfree(blout);
	}
}

static int qat_alg_sgl_to_bufl(struct qat_crypto_instance *inst,
			       struct scatterlist *assoc, int assoclen,
			       struct scatterlist *sgl,
			       struct scatterlist *sglout, uint8_t *iv,
			       uint8_t ivlen,
			       struct qat_crypto_request *qat_req)
{
	struct device *dev = &GET_DEV(inst->accel_dev);
	int i, bufs = 0, sg_nctr = 0;
	int n = sg_nents(sgl), assoc_n = sg_nents(assoc);
	struct qat_alg_buf_list *bufl;
	struct qat_alg_buf_list *buflout = NULL;
	dma_addr_t blp;
	dma_addr_t bloutp = 0;
	struct scatterlist *sg;
	size_t sz_out = 0, sz = sizeof(struct qat_alg_buf_list) +
			((1 + n + assoc_n) * sizeof(struct qat_alg_buf));

	if (unlikely(!n))
		return -EINVAL;

	bufl = kzalloc_node(sz, GFP_ATOMIC,
			    dev_to_node(&GET_DEV(inst->accel_dev)));
	if (unlikely(!bufl))
		return -ENOMEM;

	blp = dma_map_single(dev, bufl, sz, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(dev, blp)))
		goto err;

	for_each_sg(assoc, sg, assoc_n, i) {
		if (!sg->length)
			continue;

		if (!(assoclen > 0))
			break;

		bufl->bufers[bufs].addr =
			dma_map_single(dev, sg_virt(sg),
				       min_t(int, assoclen, sg->length),
				       DMA_BIDIRECTIONAL);
		bufl->bufers[bufs].len = min_t(int, assoclen, sg->length);
		if (unlikely(dma_mapping_error(dev, bufl->bufers[bufs].addr)))
			goto err;
		bufs++;
		assoclen -= sg->length;
	}

	if (ivlen) {
		bufl->bufers[bufs].addr = dma_map_single(dev, iv, ivlen,
							 DMA_BIDIRECTIONAL);
		bufl->bufers[bufs].len = ivlen;
		if (unlikely(dma_mapping_error(dev, bufl->bufers[bufs].addr)))
			goto err;
		bufs++;
	}

	for_each_sg(sgl, sg, n, i) {
		int y = sg_nctr + bufs;

		if (!sg->length)
			continue;

		bufl->bufers[y].addr = dma_map_single(dev, sg_virt(sg),
						      sg->length,
						      DMA_BIDIRECTIONAL);
		bufl->bufers[y].len = sg->length;
		if (unlikely(dma_mapping_error(dev, bufl->bufers[y].addr)))
			goto err;
		sg_nctr++;
	}
	bufl->num_bufs = sg_nctr + bufs;
	qat_req->buf.bl = bufl;
	qat_req->buf.blp = blp;
	qat_req->buf.sz = sz;
	/* Handle out of place operation */
	if (sgl != sglout) {
		struct qat_alg_buf *bufers;

		n = sg_nents(sglout);
		sz_out = sizeof(struct qat_alg_buf_list) +
			((1 + n + assoc_n) * sizeof(struct qat_alg_buf));
		sg_nctr = 0;
		buflout = kzalloc_node(sz_out, GFP_ATOMIC,
				       dev_to_node(&GET_DEV(inst->accel_dev)));
		if (unlikely(!buflout))
			goto err;
		bloutp = dma_map_single(dev, buflout, sz_out, DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(dev, bloutp)))
			goto err;
		bufers = buflout->bufers;
		/* For out of place operation dma map only data and
		 * reuse assoc mapping and iv */
		for (i = 0; i < bufs; i++) {
			bufers[i].len = bufl->bufers[i].len;
			bufers[i].addr = bufl->bufers[i].addr;
		}
		for_each_sg(sglout, sg, n, i) {
			int y = sg_nctr + bufs;

			if (!sg->length)
				continue;

			bufers[y].addr = dma_map_single(dev, sg_virt(sg),
							sg->length,
							DMA_BIDIRECTIONAL);
			if (unlikely(dma_mapping_error(dev, bufers[y].addr)))
				goto err;
			bufers[y].len = sg->length;
			sg_nctr++;
		}
		buflout->num_bufs = sg_nctr + bufs;
		buflout->num_mapped_bufs = sg_nctr;
		qat_req->buf.blout = buflout;
		qat_req->buf.bloutp = bloutp;
		qat_req->buf.sz_out = sz_out;
	} else {
		/* Otherwise set the src and dst to the same address */
		qat_req->buf.bloutp = qat_req->buf.blp;
		qat_req->buf.sz_out = 0;
	}
	return 0;
err:
	dev_err(dev, "Failed to map buf for dma\n");
	sg_nctr = 0;
	for (i = 0; i < n + bufs; i++)
		if (!dma_mapping_error(dev, bufl->bufers[i].addr))
			dma_unmap_single(dev, bufl->bufers[i].addr,
					 bufl->bufers[i].len,
					 DMA_BIDIRECTIONAL);

	if (!dma_mapping_error(dev, blp))
		dma_unmap_single(dev, blp, sz, DMA_TO_DEVICE);
	kfree(bufl);
	if (sgl != sglout && buflout) {
		n = sg_nents(sglout);
		for (i = bufs; i < n + bufs; i++)
			if (!dma_mapping_error(dev, buflout->bufers[i].addr))
				dma_unmap_single(dev, buflout->bufers[i].addr,
						 buflout->bufers[i].len,
						 DMA_BIDIRECTIONAL);
		if (!dma_mapping_error(dev, bloutp))
			dma_unmap_single(dev, bloutp, sz_out, DMA_TO_DEVICE);
		kfree(buflout);
	}
	return -ENOMEM;
}

static void qat_aead_alg_callback(struct icp_qat_fw_la_resp *qat_resp,
				  struct qat_crypto_request *qat_req)
{
	struct qat_alg_aead_ctx *ctx = qat_req->aead_ctx;
	struct qat_crypto_instance *inst = ctx->inst;
	struct aead_request *areq = qat_req->aead_req;
	uint8_t stat_filed = qat_resp->comn_resp.comn_status;
	int res = 0, qat_res = ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(stat_filed);

	qat_alg_free_bufl(inst, qat_req);
	if (unlikely(qat_res != ICP_QAT_FW_COMN_STATUS_FLAG_OK))
		res = -EBADMSG;
#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen) {

		if (unlikely(wg_fips_aad_len & 7)) {
			u8* aad = sg_virt(areq->assoc);
			int j = ((-areq->assoclen) & 7);
			memmove(aad, aad - j, areq->assoclen);
		}

		memcpy(sg_virt(areq->assoc) + areq->assoclen, &ctx->gcm_seqno, GCM_IV_SIZE);

		PrintK(KERN_DEBUG "%s: result %p   %5d+%2d\n", __FUNCTION__,
			 sg_virt(areq->dst), areq->cryptlen,  GCM_DIGEST_SIZE);
		PrintHex(sg_virt(areq->dst), areq->cryptlen + GCM_DIGEST_SIZE);
#if	CONFIG_WG_PLATFORM_GCM > 8
		PrintK(KERN_DEBUG "%s: expect\n", __FUNCTION__);
		PrintHex(GCMDst, sizeof(GCMDst));
#endif
	}
#endif
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	else
	if (unlikely(wg_fips_sha > 0))
	if (unlikely(res == -EBADMSG)) {
		wg_fips_sha_err++;
		printk(KERN_ERR "%s: %s Auth Errors %d\n", __FUNCTION__,
		       ctx->tfm->__crt_alg->cra_name, wg_fips_sha_err);
	}
#endif
	areq->base.complete(&areq->base, res);
}

static void qat_ablkcipher_alg_callback(struct icp_qat_fw_la_resp *qat_resp,
					struct qat_crypto_request *qat_req)
{
	struct qat_alg_ablkcipher_ctx *ctx = qat_req->ablkcipher_ctx;
	struct qat_crypto_instance *inst = ctx->inst;
	struct ablkcipher_request *areq = qat_req->ablkcipher_req;
	uint8_t stat_filed = qat_resp->comn_resp.comn_status;
	int res = 0, qat_res = ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(stat_filed);

	qat_alg_free_bufl(inst, qat_req);
	if (unlikely(qat_res != ICP_QAT_FW_COMN_STATUS_FLAG_OK))
		res = -EINVAL;
	areq->base.complete(&areq->base, res);
}

void qat_alg_callback(void *resp)
{
	struct icp_qat_fw_la_resp *qat_resp = resp;
	struct qat_crypto_request *qat_req =
				(void *)(__force long)qat_resp->opaque_data;

	qat_req->cb(qat_resp, qat_req);
}

static int qat_alg_aead_dec(struct aead_request *areq)
{
	struct crypto_aead *aead_tfm = crypto_aead_reqtfm(areq);
	struct crypto_tfm *tfm = crypto_aead_tfm(aead_tfm);
	struct qat_alg_aead_ctx *ctx = crypto_tfm_ctx(tfm);
	struct qat_crypto_request *qat_req = aead_request_ctx(areq);
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	struct icp_qat_fw_la_bulk_req *msg;
	int digst_size = crypto_aead_crt(aead_tfm)->authsize;
	int ret, ctr = 0;

#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen)
	ret = qat_alg_sgl_to_bufl(ctx->inst, NULL, 0,
				  areq->src, areq->dst, sg_virt(areq->assoc),
				  areq->assoclen + GCM_IV_SIZE, qat_req);
	else
#endif
	ret = qat_alg_sgl_to_bufl(ctx->inst, areq->assoc, areq->assoclen,
				  areq->src, areq->dst, areq->iv,
				  AES_BLOCK_SIZE, qat_req);
	if (unlikely(ret))
		return ret;

	msg = &qat_req->req;
	*msg = ctx->dec_fw_req;
	qat_req->aead_ctx = ctx;
	qat_req->aead_req = areq;
	qat_req->cb = qat_aead_alg_callback;
	qat_req->req.comn_mid.opaque_data = (uint64_t)(__force long)qat_req;
	qat_req->req.comn_mid.src_data_addr = qat_req->buf.blp;
	qat_req->req.comn_mid.dest_data_addr = qat_req->buf.bloutp;
	cipher_param = (void *)&qat_req->req.serv_specif_rqpars;
	cipher_param->cipher_length = areq->cryptlen - digst_size;
	cipher_param->cipher_offset = areq->assoclen + AES_BLOCK_SIZE;
	memcpy(cipher_param->u.cipher_IV_array, areq->iv, AES_BLOCK_SIZE);
	auth_param = (void *)((uint8_t *)cipher_param + sizeof(*cipher_param));
	auth_param->auth_off = 0;
	auth_param->auth_len = areq->assoclen +
				cipher_param->cipher_length + AES_BLOCK_SIZE;
#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen) {
		u8* aad = sg_virt(areq->assoc);
		struct icp_qat_hw_auth_algo_blk *hash =
		(struct icp_qat_hw_auth_algo_blk *)((char *)ctx->dec_cd);

		hash->sha.state1[GCM_STATE1_SZ + AES_BLOCK_SIZE + 3] = areq->assoclen;

		cipher_param->cipher_length = areq->cryptlen - GCM_DIGEST_SIZE;
		cipher_param->cipher_offset = areq->assoclen + GCM_IV_SIZE;

		auth_param->auth_len	    = areq->cryptlen - GCM_DIGEST_SIZE;
		auth_param->auth_off	    = areq->assoclen + GCM_IV_SIZE;

		auth_param->u1.aad_adr	    = qat_req->buf.bl->bufers[0].addr;
		auth_param->u2.aad_sz	    = round_up(areq->assoclen, GCM_AAD_PAD_SIZE);
		auth_param->hash_state_sz   = round_up(areq->assoclen, GCM_STATE1_SZ) >> 3;
		auth_param->auth_res_sz	    = GCM_DIGEST_SIZE;

		memcpy(&cipher_param->u.cipher_IV_array[0],
		       &ctx->gcm_nonce,	      GCM_NONCE_SIZE);
		memcpy(&cipher_param->u.cipher_IV_array[GCM_NONCE_SIZE/sizeof(u32)],
		       areq->iv,	      GCM_IV_SIZE);

		if (unlikely(wg_fips_aad_len & 7)) {
			int j = ((-areq->assoclen) & 7);
			memmove(aad - j, aad, areq->assoclen);
			aad -= j;
			auth_param->u1.aad_adr -= j;
		}

		memset(aad + areq->assoclen, 0, auth_param->u2.aad_sz - areq->assoclen);
	}
#endif

#ifdef	CONFIG_WG_PLATFORM_GCM
#if	CONFIG_WG_PLATFORM_GCM > 1
	PrintK(KERN_DEBUG "%s:\n",		    __FUNCTION__);
	PrintK(KERN_DEBUG "%s:\n",		    __FUNCTION__);

	PrintK(KERN_DEBUG "%s: coff %d clen %d aoff %d alen %d\n", __FUNCTION__,
	       cipher_param->cipher_offset, cipher_param->cipher_length,
	       auth_param->auth_off,	    auth_param->auth_len);

	PrintK(KERN_DEBUG "%s: cipher_params %p\n", __FUNCTION__, cipher_param);
	PrintK(KERN_DEBUG "%s: auth_params   %p\n", __FUNCTION__, auth_param);

	if (ctx->gcm_keylen) {
		u8* aad = sg_virt(areq->src) - ((-areq->assoclen) & 7);

		PrintK(KERN_DEBUG "%s: aad+pad %p %3d+%1d\n", __FUNCTION__,
			 aad, areq->assoclen,	auth_param->u2.aad_sz - areq->assoclen);
		PrintHex(aad,			auth_param->u2.aad_sz);

		PrintK(KERN_DEBUG "%s: iv      %p %3d+%d\n", __FUNCTION__,
			 &cipher_param->u.cipher_IV_array[0], GCM_NONCE_SIZE + GCM_IV_SIZE,  4);
		PrintHex(&cipher_param->u.cipher_IV_array[0], GCM_NONCE_SIZE + GCM_IV_SIZE);

		PrintK(KERN_DEBUG "%s: src     %p %5d\n", __FUNCTION__,
			 sg_virt(areq->src), areq->assoclen + GCM_IV_SIZE + areq->cryptlen);
		PrintHex(sg_virt(areq->src), areq->assoclen + GCM_IV_SIZE + areq->cryptlen);

		PrintK(KERN_DEBUG "%s: dec_cd  %p %5d\n", __FUNCTION__,
			 ctx->dec_cd, GCM_PARAMS_SZ + (ctx->gcm_keylen & -8));
		PrintHex(ctx->dec_cd, GCM_PARAMS_SZ + (ctx->gcm_keylen & -8));
	} else {
		PrintK(KERN_DEBUG "%s: assoc   %p %5d\n", __FUNCTION__,
			 sg_virt(areq->assoc), areq->assoclen);
		PrintHex(sg_virt(areq->assoc), areq->assoclen);

		PrintK(KERN_DEBUG "%s: iv      %p %5d\n", __FUNCTION__,
			 &cipher_param->u.cipher_IV_array[0], AES_BLOCK_SIZE);
		PrintHex(&cipher_param->u.cipher_IV_array[0], AES_BLOCK_SIZE);

		PrintK(KERN_DEBUG "%s: src     %p %5d\n", __FUNCTION__,
			 sg_virt(areq->src), auth_param->auth_len + digst_size);
		PrintHex(sg_virt(areq->src), auth_param->auth_len + digst_size);

		PrintK(KERN_DEBUG "%s: dec_cd  %p %5d\n", __FUNCTION__,
			 ctx->dec_cd, (int)sizeof(struct qat_alg_cd));
		PrintHex(ctx->dec_cd, (int)sizeof(struct qat_alg_cd));
	}

#if	CONFIG_WG_PLATFORM_GCM > 2
	PrintK(KERN_DEBUG "%s: req     %p %5d\n", __FUNCTION__,
		&qat_req->req, (int)sizeof(struct icp_qat_fw_la_bulk_req));

	PrintK(KERN_DEBUG "%s: comn_hdr\n", __FUNCTION__);
	PrintHex(&qat_req->req.comn_hdr, sizeof(struct icp_qat_fw_comn_req_hdr));
	PrintK(KERN_DEBUG "%s: cd_pars\n",  __FUNCTION__);
	PrintHex(&qat_req->req.cd_pars,  sizeof(struct icp_qat_fw_comn_req_hdr_cd_pars));
	PrintK(KERN_DEBUG "%s: comn_mid\n", __FUNCTION__);
	PrintHex(&qat_req->req.comn_mid, sizeof(struct icp_qat_fw_comn_req_mid));
	PrintK(KERN_DEBUG "%s: serv_specif_rqpars\n",   __FUNCTION__);
	PrintHex(&qat_req->req.serv_specif_rqpars, sizeof(struct icp_qat_fw_comn_req_rqpars));
	PrintK(KERN_DEBUG "%s: cd_ctrl\n",  __FUNCTION__);
	PrintHex(&qat_req->req.cd_ctrl,  sizeof(struct icp_qat_fw_comn_req_cd_ctrl));
#endif
#endif
#endif

	do {
		ret = adf_send_message(ctx->inst->sym_tx, (uint32_t *)msg);
	} while (ret == -EAGAIN && ctr++ < 10);

	if (ret == -EAGAIN) {
		qat_alg_free_bufl(ctx->inst, qat_req);
		return -EBUSY;
	}
	return -EINPROGRESS;
}

static int qat_alg_aead_enc_internal(struct aead_request *areq, uint8_t *iv,
				     int enc_iv)
{
	struct crypto_aead *aead_tfm = crypto_aead_reqtfm(areq);
	struct crypto_tfm *tfm = crypto_aead_tfm(aead_tfm);
	struct qat_alg_aead_ctx *ctx = crypto_tfm_ctx(tfm);
	struct qat_crypto_request *qat_req = aead_request_ctx(areq);
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_auth_req_params *auth_param;
	struct icp_qat_fw_la_bulk_req *msg;
	int ret, ctr = 0;
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	int ivlen = tfm->crt_u.aead.ivsize;

	if (unlikely(ivlen == 0)) {

	areq->src->offset -= areq->assoclen;
	areq->src->length += areq->assoclen;

	if (unlikely(wg_fips_iv)) {
		memcpy(iv - areq->assoclen, wg_fips_iv, AES_BLOCK_SIZE + areq->assoclen);
		wg_fips_iv = NULL;
	}

	ret = qat_alg_sgl_to_bufl(ctx->inst, NULL, 0,
				  areq->src, areq->dst, NULL, 0, qat_req);

	} else {

	if (unlikely(wg_fips_iv)) {
		memcpy(iv, wg_fips_iv, ivlen);
		wg_fips_iv = NULL;
		enc_iv = 0;
	}
#endif

#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen)
	ret = qat_alg_sgl_to_bufl(ctx->inst, NULL, 0,
				  areq->src, areq->dst, sg_virt(areq->assoc),
				  areq->assoclen + GCM_IV_SIZE, qat_req);
	else
#endif
	ret = qat_alg_sgl_to_bufl(ctx->inst, areq->assoc, areq->assoclen,
				  areq->src, areq->dst, iv, AES_BLOCK_SIZE,
				  qat_req);
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	}
#endif
	if (unlikely(ret))
		return ret;

	msg = &qat_req->req;
	*msg = ctx->enc_fw_req;
	qat_req->aead_ctx = ctx;
	qat_req->aead_req = areq;
	qat_req->cb = qat_aead_alg_callback;
	qat_req->req.comn_mid.opaque_data = (uint64_t)(__force long)qat_req;
	qat_req->req.comn_mid.src_data_addr = qat_req->buf.blp;
	qat_req->req.comn_mid.dest_data_addr = qat_req->buf.bloutp;
	cipher_param = (void *)&qat_req->req.serv_specif_rqpars;
	auth_param = (void *)((uint8_t *)cipher_param + sizeof(*cipher_param));

#ifdef	CONFIG_WG_PLATFORM_GCM
	if (ctx->gcm_keylen) {
		u8* aad = sg_virt(areq->assoc);
		struct icp_qat_hw_auth_algo_blk *hash =
		(struct icp_qat_hw_auth_algo_blk *)((char *)ctx->enc_cd +
		sizeof(struct icp_qat_hw_cipher_config) + (ctx->gcm_keylen & -8));

		hash->sha.state1[GCM_STATE1_SZ + AES_BLOCK_SIZE + 3] = areq->assoclen;

		cipher_param->cipher_length = areq->cryptlen;
		cipher_param->cipher_offset = areq->assoclen + GCM_IV_SIZE;

		auth_param->auth_len	    = areq->cryptlen;
		auth_param->auth_off	    = areq->assoclen + GCM_IV_SIZE;

		auth_param->u1.aad_adr	    = qat_req->buf.bl->bufers[0].addr;
		auth_param->u2.aad_sz	    = round_up(areq->assoclen, GCM_AAD_PAD_SIZE);
		auth_param->hash_state_sz   = round_up(areq->assoclen, GCM_STATE1_SZ) >> 3;

		memcpy(&cipher_param->u.cipher_IV_array[0],
		       &ctx->gcm_nonce,	      GCM_NONCE_SIZE);
		memcpy(&cipher_param->u.cipher_IV_array[GCM_NONCE_SIZE/sizeof(u32)],
		       iv,		      GCM_IV_SIZE);

		if (unlikely(wg_fips_aad_len & 7)) {
			int j = ((-areq->assoclen) & 7);
			memmove(aad - j, aad, areq->assoclen);
			aad -= j;
			auth_param->u1.aad_adr -= j;
		}

		memset(aad + areq->assoclen, 0, auth_param->u2.aad_sz - areq->assoclen);

#if	CONFIG_WG_PLATFORM_GCM > 8
		{
		__be64  seq;

		memcpy(&seq,			       &GCMIv[GCM_NONCE_SIZE],	sizeof(u64));
		atomic64_set(&ctx->gcm_seqno, seq);

		memcpy(cipher_param->u.cipher_IV_array,	GCMIv,			sizeof(GCMIv));
		memcpy(sg_virt(areq->assoc),		GCMAad,			sizeof(GCMAad));
		memcpy(sg_virt(areq->src),		GCMSrc,			sizeof(GCMSrc));
		memset(sg_virt(areq->src) + areq->cryptlen, 0xEE, GCM_DIGEST_SIZE + 8);
		}
#endif
	}
#endif
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	else
	if (unlikely(ivlen == 0)) {

		cipher_param->cipher_offset = 0;
		cipher_param->cipher_length = 0;

		auth_param->auth_off = 0;
		auth_param->auth_len = areq->assoclen + areq->cryptlen;
#ifdef	CONFIG_WG_PLATFORM_MODE0 // WG:JB FIPS algos
		if (unlikely(wg_fips_sha_mode0)) {
			auth_param->auth_len -= (wg_fips_sha / 8);
			memset(&msg->cd_pars, 0, sizeof(msg->cd_pars));
			wg_fips_sha_mode0 = 0;
		}
#endif
	} else {
#endif
	if (enc_iv) {
		cipher_param->cipher_length = areq->cryptlen + AES_BLOCK_SIZE;
		cipher_param->cipher_offset = areq->assoclen;
	} else {
		memcpy(cipher_param->u.cipher_IV_array, iv, AES_BLOCK_SIZE);
		cipher_param->cipher_length = areq->cryptlen;
		cipher_param->cipher_offset = areq->assoclen + AES_BLOCK_SIZE;
	}
	auth_param->auth_off = 0;
	auth_param->auth_len = areq->assoclen + areq->cryptlen + AES_BLOCK_SIZE;
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	}
#endif

#ifdef	CONFIG_WG_PLATFORM_GCM
#if	CONFIG_WG_PLATFORM_GCM > 1
	PrintK(KERN_DEBUG "%s:\n",		    __FUNCTION__);
	PrintK(KERN_DEBUG "%s:\n",		    __FUNCTION__);

	PrintK(KERN_DEBUG "%s: coff %d clen %d aoff %d alen %d\n", __FUNCTION__,
	       cipher_param->cipher_offset, cipher_param->cipher_length,
	       auth_param->auth_off,	    auth_param->auth_len);

	PrintK(KERN_DEBUG "%s: cipher_params %p\n", __FUNCTION__, cipher_param);
	PrintK(KERN_DEBUG "%s: auth_params   %p\n", __FUNCTION__, auth_param);

	if (ctx->gcm_keylen) {
		u8* aad = sg_virt(areq->src) - ((-areq->assoclen) & 7);

		PrintK(KERN_DEBUG "%s: aad+pad %p %3d+%1d\n", __FUNCTION__,
			 aad, areq->assoclen,	auth_param->u2.aad_sz - areq->assoclen);
		PrintHex(aad,			auth_param->u2.aad_sz);

		PrintK(KERN_DEBUG "%s: iv      %p %3d+%d\n", __FUNCTION__,
			 &cipher_param->u.cipher_IV_array[0], GCM_NONCE_SIZE + GCM_IV_SIZE,  4);
		PrintHex(&cipher_param->u.cipher_IV_array[0], GCM_NONCE_SIZE + GCM_IV_SIZE);

		PrintK(KERN_DEBUG "%s: src     %p %5d\n", __FUNCTION__,
			 sg_virt(areq->src), areq->assoclen + GCM_IV_SIZE + areq->cryptlen);
		PrintHex(sg_virt(areq->src), areq->assoclen + GCM_IV_SIZE + areq->cryptlen);

		PrintK(KERN_DEBUG "%s: enc_cd  %p %5d\n", __FUNCTION__,
			 ctx->enc_cd, GCM_PARAMS_SZ + (ctx->gcm_keylen & -8));
		PrintHex(ctx->enc_cd, GCM_PARAMS_SZ + (ctx->gcm_keylen & -8));
	} else {
		PrintK(KERN_DEBUG "%s: assoc   %p %5d\n", __FUNCTION__,
			 sg_virt(areq->assoc), areq->assoclen);
		PrintHex(sg_virt(areq->assoc), areq->assoclen);

		PrintK(KERN_DEBUG "%s: iv      %p %5d\n", __FUNCTION__,
			 &cipher_param->u.cipher_IV_array[0], AES_BLOCK_SIZE);
		PrintHex(&cipher_param->u.cipher_IV_array[0], AES_BLOCK_SIZE);

		PrintK(KERN_DEBUG "%s: src     %p %5d\n", __FUNCTION__,
			 sg_virt(areq->src), auth_param->auth_len);
		PrintHex(sg_virt(areq->src), auth_param->auth_len);

		PrintK(KERN_DEBUG "%s: enc_cd  %p %5d\n", __FUNCTION__,
			 ctx->enc_cd, (int)sizeof(struct qat_alg_cd));
		PrintHex(ctx->enc_cd, (int)sizeof(struct qat_alg_cd));
	}

#if	CONFIG_WG_PLATFORM_GCM > 2
	PrintK(KERN_DEBUG "%s: req     %p %5d\n", __FUNCTION__,
		&qat_req->req, (int)sizeof(struct icp_qat_fw_la_bulk_req));

	PrintK(KERN_DEBUG "%s: comn_hdr\n", __FUNCTION__);
	PrintHex(&qat_req->req.comn_hdr, sizeof(struct icp_qat_fw_comn_req_hdr));
	PrintK(KERN_DEBUG "%s: cd_pars\n",  __FUNCTION__);
	PrintHex(&qat_req->req.cd_pars,  sizeof(struct icp_qat_fw_comn_req_hdr_cd_pars));
	PrintK(KERN_DEBUG "%s: comn_mid\n", __FUNCTION__);
	PrintHex(&qat_req->req.comn_mid, sizeof(struct icp_qat_fw_comn_req_mid));
	PrintK(KERN_DEBUG "%s: serv_specif_rqpars\n",   __FUNCTION__);
	PrintHex(&qat_req->req.serv_specif_rqpars, sizeof(struct icp_qat_fw_comn_req_rqpars));
	PrintK(KERN_DEBUG "%s: cd_ctrl\n",  __FUNCTION__);
	PrintHex(&qat_req->req.cd_ctrl,  sizeof(struct icp_qat_fw_comn_req_cd_ctrl));
#endif
#endif
#endif

	do {
		ret = adf_send_message(ctx->inst->sym_tx, (uint32_t *)msg);
	} while (ret == -EAGAIN && ctr++ < 10);

	if (ret == -EAGAIN) {
		qat_alg_free_bufl(ctx->inst, qat_req);
		return -EBUSY;
	}
	return -EINPROGRESS;
}

static int qat_alg_aead_enc(struct aead_request *areq)
{
	return qat_alg_aead_enc_internal(areq, areq->iv, 0);
}

static int qat_alg_aead_genivenc(struct aead_givcrypt_request *req)
{
	struct crypto_aead *aead_tfm = crypto_aead_reqtfm(&req->areq);
	struct crypto_tfm *tfm = crypto_aead_tfm(aead_tfm);
	struct qat_alg_aead_ctx *ctx = crypto_tfm_ctx(tfm);
	__be64 seq;

	memcpy(req->giv, ctx->salt, AES_BLOCK_SIZE);
	seq = cpu_to_be64(req->seq);
	memcpy(req->giv + AES_BLOCK_SIZE - sizeof(uint64_t),
	       &seq, sizeof(uint64_t));
	return qat_alg_aead_enc_internal(&req->areq, req->giv, 1);
}

#ifdef	CONFIG_WG_PLATFORM_GCM
static int qat_alg_aead_gcm_genivenc(struct aead_givcrypt_request *req)
{
	struct crypto_aead *aead_tfm = crypto_aead_reqtfm(&req->areq);
	struct crypto_tfm *tfm = crypto_aead_tfm(aead_tfm);
	struct qat_alg_aead_ctx *ctx = crypto_tfm_ctx(tfm);
	__be64 seq = atomic64_inc_return(&ctx->gcm_seqno);

	memcpy(req->giv, &seq, sizeof(uint64_t));
	return qat_alg_aead_enc_internal(&req->areq, req->giv, 1);
}

#endif

static int qat_alg_ablkcipher_setkey(struct crypto_ablkcipher *tfm,
				     const uint8_t *key,
				     unsigned int keylen)
{
	struct qat_alg_ablkcipher_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	struct device *dev;

	spin_lock(&ctx->lock);
	if (ctx->enc_cd) {
		/* rekeying */
		dev = &GET_DEV(ctx->inst->accel_dev);
		memset(ctx->enc_cd, 0, sizeof(*ctx->enc_cd));
		memset(ctx->dec_cd, 0, sizeof(*ctx->dec_cd));
		memset(&ctx->enc_fw_req, 0, sizeof(ctx->enc_fw_req));
		memset(&ctx->dec_fw_req, 0, sizeof(ctx->dec_fw_req));
	} else {
		/* new key */
		int node = get_current_node();
		struct qat_crypto_instance *inst =
				qat_crypto_get_instance_node(node);
		if (!inst) {
			spin_unlock(&ctx->lock);
			return -EINVAL;
		}

		dev = &GET_DEV(inst->accel_dev);
		ctx->inst = inst;
		ctx->enc_cd = dma_zalloc_coherent(dev, sizeof(*ctx->enc_cd),
						  &ctx->enc_cd_paddr,
						  GFP_ATOMIC);
		if (!ctx->enc_cd) {
			spin_unlock(&ctx->lock);
			return -ENOMEM;
		}
		ctx->dec_cd = dma_zalloc_coherent(dev, sizeof(*ctx->dec_cd),
						  &ctx->dec_cd_paddr,
						  GFP_ATOMIC);
		if (!ctx->dec_cd) {
			spin_unlock(&ctx->lock);
			goto out_free_enc;
		}
	}
	spin_unlock(&ctx->lock);
	if (qat_alg_ablkcipher_init_sessions(ctx, key, keylen))
		goto out_free_all;

	return 0;

out_free_all:
	memset(ctx->dec_cd, 0, sizeof(*ctx->dec_cd));
	dma_free_coherent(dev, sizeof(*ctx->dec_cd),
			  ctx->dec_cd, ctx->dec_cd_paddr);
	ctx->dec_cd = NULL;
out_free_enc:
	memset(ctx->enc_cd, 0, sizeof(*ctx->enc_cd));
	dma_free_coherent(dev, sizeof(*ctx->enc_cd),
			  ctx->enc_cd, ctx->enc_cd_paddr);
	ctx->enc_cd = NULL;
	return -ENOMEM;
}

static int qat_alg_ablkcipher_encrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *atfm = crypto_ablkcipher_reqtfm(req);
	struct crypto_tfm *tfm = crypto_ablkcipher_tfm(atfm);
	struct qat_alg_ablkcipher_ctx *ctx = crypto_tfm_ctx(tfm);
	struct qat_crypto_request *qat_req = ablkcipher_request_ctx(req);
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_bulk_req *msg;
	int ret, ctr = 0;

#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
	if (unlikely(wg_fips_iv)) {
		memcpy(req->info, wg_fips_iv, AES_BLOCK_SIZE);
		wg_fips_iv = NULL;
	}
#endif

	ret = qat_alg_sgl_to_bufl(ctx->inst, NULL, 0, req->src, req->dst,
				  NULL, 0, qat_req);
	if (unlikely(ret))
		return ret;

	msg = &qat_req->req;
	*msg = ctx->enc_fw_req;
	qat_req->ablkcipher_ctx = ctx;
	qat_req->ablkcipher_req = req;
	qat_req->cb = qat_ablkcipher_alg_callback;
	qat_req->req.comn_mid.opaque_data = (uint64_t)(__force long)qat_req;
	qat_req->req.comn_mid.src_data_addr = qat_req->buf.blp;
	qat_req->req.comn_mid.dest_data_addr = qat_req->buf.bloutp;
	cipher_param = (void *)&qat_req->req.serv_specif_rqpars;
	cipher_param->cipher_length = req->nbytes;
	cipher_param->cipher_offset = 0;
	memcpy(cipher_param->u.cipher_IV_array, req->info, AES_BLOCK_SIZE);
	do {
		ret = adf_send_message(ctx->inst->sym_tx, (uint32_t *)msg);
	} while (ret == -EAGAIN && ctr++ < 10);

	if (ret == -EAGAIN) {
		qat_alg_free_bufl(ctx->inst, qat_req);
		return -EBUSY;
	}
	return -EINPROGRESS;
}

static int qat_alg_ablkcipher_decrypt(struct ablkcipher_request *req)
{
	struct crypto_ablkcipher *atfm = crypto_ablkcipher_reqtfm(req);
	struct crypto_tfm *tfm = crypto_ablkcipher_tfm(atfm);
	struct qat_alg_ablkcipher_ctx *ctx = crypto_tfm_ctx(tfm);
	struct qat_crypto_request *qat_req = ablkcipher_request_ctx(req);
	struct icp_qat_fw_la_cipher_req_params *cipher_param;
	struct icp_qat_fw_la_bulk_req *msg;
	int ret, ctr = 0;

	ret = qat_alg_sgl_to_bufl(ctx->inst, NULL, 0, req->src, req->dst,
				  NULL, 0, qat_req);
	if (unlikely(ret))
		return ret;

	msg = &qat_req->req;
	*msg = ctx->dec_fw_req;
	qat_req->ablkcipher_ctx = ctx;
	qat_req->ablkcipher_req = req;
	qat_req->cb = qat_ablkcipher_alg_callback;
	qat_req->req.comn_mid.opaque_data = (uint64_t)(__force long)qat_req;
	qat_req->req.comn_mid.src_data_addr = qat_req->buf.blp;
	qat_req->req.comn_mid.dest_data_addr = qat_req->buf.bloutp;
	cipher_param = (void *)&qat_req->req.serv_specif_rqpars;
	cipher_param->cipher_length = req->nbytes;
	cipher_param->cipher_offset = 0;
	memcpy(cipher_param->u.cipher_IV_array, req->info, AES_BLOCK_SIZE);
	do {
		ret = adf_send_message(ctx->inst->sym_tx, (uint32_t *)msg);
	} while (ret == -EAGAIN && ctr++ < 10);

	if (ret == -EAGAIN) {
		qat_alg_free_bufl(ctx->inst, qat_req);
		return -EBUSY;
	}
	return -EINPROGRESS;
}

static int qat_alg_aead_init(struct crypto_tfm *tfm,
			     enum icp_qat_hw_auth_algo hash,
			     const char *hash_name)
{
	struct qat_alg_aead_ctx *ctx = crypto_tfm_ctx(tfm);

#ifdef	CONFIG_WG_PLATFORM_GCM
	ctx->gcm_keylen = (hash == ICP_QAT_HW_AUTH_ALGO_GALOIS_128) ? GCM_NONCE_SIZE : 0;
	PrintK(KERN_DEBUG "%s: fips %d mode0 %d gcm %d hash %d %s\n", __FUNCTION__,
	       wg_fips_sha, wg_fips_sha_mode0, ctx->gcm_keylen, hash, hash_name);
#endif
#ifdef	CONFIG_WG_PLATFORM_FIPS
	if (hash == ICP_QAT_HW_AUTH_ALGO_NULL)
	ctx->hash_tfm = NULL;
	else {
	ctx->hash_tfm = crypto_alloc_shash(hash_name, 0, 0);
	if (IS_ERR(ctx->hash_tfm))
		return -EFAULT;
	}
#else
	ctx->hash_tfm = crypto_alloc_shash(hash_name, 0, 0);
	if (IS_ERR(ctx->hash_tfm))
		return -EFAULT;
#endif
	spin_lock_init(&ctx->lock);
	ctx->qat_hash_alg = hash;
	crypto_aead_set_reqsize(__crypto_aead_cast(tfm),
		sizeof(struct aead_request) +
		sizeof(struct qat_crypto_request));
	ctx->tfm = tfm;
	return 0;
}

static int qat_alg_aead_sha1_init(struct crypto_tfm *tfm)
{
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA1, "sha1");
}

static int qat_alg_aead_sha256_init(struct crypto_tfm *tfm)
{
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA256, "sha256");
}

#ifdef	CONFIG_WG_PLATFORM_C3XXX_SHA384
static int qat_alg_aead_sha384_init(struct crypto_tfm *tfm)
{
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA384, "sha384");
}
#endif

static int qat_alg_aead_sha512_init(struct crypto_tfm *tfm)
{
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA512, "sha512");
}

#ifdef	CONFIG_WG_PLATFORM_GCM
static int qat_alg_aead_ghash_init(struct crypto_tfm *tfm)
{
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_GALOIS_128, "ghash");
}
#endif

#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB FIPS algos
static int qat_alg_aead_null_init(struct crypto_tfm *tfm)
{
	if (wg_fips_sha >= 512)
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA512, "sha512");
	if (wg_fips_sha >= 384)
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA384, "sha384");
	if (wg_fips_sha >= 256)
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA256, "sha256");
	if (wg_fips_sha >= 160)
	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_SHA1,   "sha1");

	return qat_alg_aead_init(tfm, ICP_QAT_HW_AUTH_ALGO_NULL,   "");
}
#endif

static void qat_alg_aead_exit(struct crypto_tfm *tfm)
{
	struct qat_alg_aead_ctx *ctx = crypto_tfm_ctx(tfm);
	struct qat_crypto_instance *inst = ctx->inst;
	struct device *dev;

	if (!IS_ERR(ctx->hash_tfm))
		crypto_free_shash(ctx->hash_tfm);

	if (!inst)
		return;

	dev = &GET_DEV(inst->accel_dev);
	if (ctx->enc_cd) {
		memset(ctx->enc_cd, 0, sizeof(struct qat_alg_cd));
		dma_free_coherent(dev, sizeof(struct qat_alg_cd),
				  ctx->enc_cd, ctx->enc_cd_paddr);
	}
	if (ctx->dec_cd) {
		memset(ctx->dec_cd, 0, sizeof(struct qat_alg_cd));
		dma_free_coherent(dev, sizeof(struct qat_alg_cd),
				  ctx->dec_cd, ctx->dec_cd_paddr);
	}
	qat_crypto_put_instance(inst);
}

static int qat_alg_ablkcipher_init(struct crypto_tfm *tfm)
{
	struct qat_alg_ablkcipher_ctx *ctx = crypto_tfm_ctx(tfm);

	spin_lock_init(&ctx->lock);
	tfm->crt_ablkcipher.reqsize = sizeof(struct ablkcipher_request) +
					sizeof(struct qat_crypto_request);
	ctx->tfm = tfm;
	return 0;
}

static void qat_alg_ablkcipher_exit(struct crypto_tfm *tfm)
{
	struct qat_alg_ablkcipher_ctx *ctx = crypto_tfm_ctx(tfm);
	struct qat_crypto_instance *inst = ctx->inst;
	struct device *dev;

	if (!inst)
		return;

	dev = &GET_DEV(inst->accel_dev);
	if (ctx->enc_cd) {
		memset(ctx->enc_cd, 0,
		       sizeof(struct icp_qat_hw_cipher_algo_blk));
		dma_free_coherent(dev,
				  sizeof(struct icp_qat_hw_cipher_algo_blk),
				  ctx->enc_cd, ctx->enc_cd_paddr);
	}
	if (ctx->dec_cd) {
		memset(ctx->dec_cd, 0,
		       sizeof(struct icp_qat_hw_cipher_algo_blk));
		dma_free_coherent(dev,
				  sizeof(struct icp_qat_hw_cipher_algo_blk),
				  ctx->dec_cd, ctx->dec_cd_paddr);
	}
	qat_crypto_put_instance(inst);
}

static struct crypto_alg qat_algs[] = { {
	.cra_name = "authenc(hmac(sha1),cbc(aes))",
	.cra_driver_name = "qat_aes_cbc_hmac_sha1",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha1_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA1_DIGEST_SIZE,
		},
	},
}, {
	.cra_name = "authenc(hmac(sha256),cbc(aes))",
	.cra_driver_name = "qat_aes_cbc_hmac_sha256",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha256_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA256_DIGEST_SIZE,
		},
	},
}, {
	.cra_name = "authenc(hmac(sha512),cbc(aes))",
	.cra_driver_name = "qat_aes_cbc_hmac_sha512",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha512_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA512_DIGEST_SIZE,
		},
	},
}, {
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB Extra WG algos
#ifdef	CONFIG_WG_PLATFORM_C3XXX_SHA384
	.cra_name = "authenc(hmac(sha384),cbc(aes))",
	.cra_driver_name = "qat_aes_cbc_hmac_sha384",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha384_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = SHA384_DIGEST_SIZE,
		},
	},
}, {
#endif
	.cra_name = "authenc(digest_null,cbc(cipher_null))",
	.cra_driver_name = "qat_cipher_null_cbc_digest_null",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = 1,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_null_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = 000*000,
			.maxauthsize = 000*000,
		},
	},
}, {
	.cra_name = "authenc(hmac(sha1),cbc(cipher_null))",
	.cra_driver_name = "qat_cipher_null_cbc_hmac_sha1",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = 4,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha1_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = 000*000,
			.maxauthsize = SHA1_DIGEST_SIZE,
		},
	},
}, {
	.cra_name = "authenc(hmac(sha256),cbc(cipher_null))",
	.cra_driver_name = "qat_cipher_null_cbc_hmac_sha256",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = 4,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha256_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = 000*000,
			.maxauthsize = SHA256_DIGEST_SIZE,
		},
	},
}, {
#ifdef	CONFIG_WG_PLATFORM_C3XXX_SHA384
	.cra_name = "authenc(hmac(sha384),cbc(cipher_null))",
	.cra_driver_name = "qat_cipher_null_cbc_hmac_sha384",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = 4,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha384_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = 000*000,
			.maxauthsize = SHA384_DIGEST_SIZE,
		},
	},
}, {
#endif
	.cra_name = "authenc(hmac(sha512),cbc(cipher_null))",
	.cra_driver_name = "qat_cipher_null_cbc_hmac_sha512",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = 4,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_sha512_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = 000*000,
			.maxauthsize = SHA512_DIGEST_SIZE,
		},
	},
}, {
	.cra_name = "authenc(digest_null,cbc(aes))",
	.cra_driver_name = "qat_aes_cbc_digest_null",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_null_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_genivenc,
			.ivsize = AES_BLOCK_SIZE,
			.maxauthsize = 000*000,
		},
	},
}, {
#endif
#ifdef	CONFIG_WG_PLATFORM_GCM
	.cra_name = "rfc4106(gcm(aes))",
	.cra_driver_name = "qat_rfc4106_gcm_aes",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC,
	.cra_blocksize = 4,
	.cra_ctxsize = sizeof(struct qat_alg_aead_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_aead_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_aead_ghash_init,
	.cra_exit = qat_alg_aead_exit,
	.cra_u = {
		.aead = {
			.setkey = qat_alg_aead_setkey,
			.decrypt = qat_alg_aead_dec,
			.encrypt = qat_alg_aead_enc,
			.givencrypt = qat_alg_aead_gcm_genivenc,
			.ivsize = GCM_IV_SIZE,
			.maxauthsize = GCM_DIGEST_SIZE,
		},
	},
}, {
#endif
#ifdef	CONFIG_WG_PLATFORM_FIPS // WG:JB Extra WG algos
	.cra_name = "ablk(cbc(aes))",
	.cra_driver_name = "qat_ablk_aes_cbc",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_ablkcipher_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_ablkcipher_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_ablkcipher_init,
	.cra_exit = qat_alg_ablkcipher_exit,
	.cra_u = {
		.ablkcipher = {
			.setkey = qat_alg_ablkcipher_setkey,
			.decrypt = qat_alg_ablkcipher_decrypt,
			.encrypt = qat_alg_ablkcipher_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
}, {
#endif
	.cra_name = "cbc(aes)",
	.cra_driver_name = "qat_aes_cbc",
	.cra_priority = 4001,
	.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_ctxsize = sizeof(struct qat_alg_ablkcipher_ctx),
	.cra_alignmask = 0,
	.cra_type = &crypto_ablkcipher_type,
	.cra_module = THIS_MODULE,
	.cra_init = qat_alg_ablkcipher_init,
	.cra_exit = qat_alg_ablkcipher_exit,
	.cra_u = {
		.ablkcipher = {
			.setkey = qat_alg_ablkcipher_setkey,
			.decrypt = qat_alg_ablkcipher_decrypt,
			.encrypt = qat_alg_ablkcipher_encrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
		},
	},
} };

int qat_algs_register(void)
{
	int ret = 0;

#ifdef	CONFIG_WG_PLATFORM
	printk(KERN_INFO "\n%s: Built " __DATE__ " " __TIME__ "\n\n", __FUNCTION__);
#endif

#ifdef	CONFIG_WG_PLATFORM_GCM
	gcm_aes_cipher = crypto_alloc_cipher("aes", 0, 0);
	if (IS_ERR(gcm_aes_cipher)) return -ENOENT;
#endif

	mutex_lock(&algs_lock);
	if (++active_devs == 1) {
		int i;

		crypto_get_default_rng();
		for (i = 0; i < ARRAY_SIZE(qat_algs); i++)
			qat_algs[i].cra_flags =
				(qat_algs[i].cra_type == &crypto_aead_type) ?
				CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC :
				CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC;

		ret = crypto_register_algs(qat_algs, ARRAY_SIZE(qat_algs));
	}
	mutex_unlock(&algs_lock);
	return ret;
}

void qat_algs_unregister(void)
{
	mutex_lock(&algs_lock);
	if (--active_devs == 0) {
		crypto_unregister_algs(qat_algs, ARRAY_SIZE(qat_algs));
		crypto_put_default_rng();
	}
	mutex_unlock(&algs_lock);
}
#endif
