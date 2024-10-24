/**
 * @file OsalCryptoInterface.c (linux kernel space)
 *
 * @brief Osal interface to linux crypto API.
 *
 * @par
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
 *  version: SXXXX.L.0.5.0-46
 */

#include "Osal.h"
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/internal/hash.h>
#include <crypto/sha.h>
#include <crypto/md5.h>

static struct crypto_shash *md5_tfm = NULL;
static struct crypto_shash *sha1_tfm = NULL;
static struct crypto_shash *sha224_tfm = NULL;
static struct crypto_shash *sha256_tfm = NULL;
static struct crypto_shash *sha384_tfm = NULL;
static struct crypto_shash *sha512_tfm = NULL;
static struct crypto_cipher *cipher_tfm = NULL;

static OsalLock encLock;

OSAL_STATUS
osalHashMD5(UINT8 *in,
            UINT8 *out)
{
    int rc = -1;
    if(!IS_ERR(md5_tfm))
    {
        struct md5_state *sctx = NULL;
        struct {
                struct shash_desc shash;
                char ctx[crypto_shash_descsize(md5_tfm)];
        } desc;

        desc.shash.tfm = md5_tfm;
        desc.shash.flags = 0x0;
        rc = crypto_shash_init(&desc.shash);
        if(rc)
        {
            printk("crypto_shash_init failed\n");
            return OSAL_FAIL;
        }
        rc = crypto_shash_update(&desc.shash, in, MD5_HMAC_BLOCK_SIZE);
        sctx = (struct md5_state*) desc.ctx;
        memcpy(out, sctx->hash, MD5_DIGEST_SIZE);
    }
    else
    {
        printk("md5 tfm not initialized\n");
    }
    return (rc == 0) ? OSAL_SUCCESS : OSAL_FAIL;
}

OSAL_STATUS
osalHashSHA1(UINT8 *in,
             UINT8 *out)
{
    int rc = -1;
    if(!IS_ERR(sha1_tfm))
    {
        struct sha1_state *sctx = NULL;
        struct {
               struct shash_desc shash;
               char ctx[crypto_shash_descsize(sha1_tfm)];
        } desc;

        desc.shash.tfm = sha1_tfm;
        desc.shash.flags = 0x0;
        rc = crypto_shash_init(&desc.shash);
        if(rc)
        {
            printk("crypto_shash_init failed\n");
            return OSAL_FAIL;
        }
        rc = crypto_shash_update(&desc.shash, in, SHA1_BLOCK_SIZE);
        sctx = (struct sha1_state*) desc.ctx;
        memcpy(out, sctx->state, SHA1_DIGEST_SIZE);
    }
    else
    {
        printk("sha1 tfm not initialized\n");
    }
    return (rc == 0) ? OSAL_SUCCESS : OSAL_FAIL;
}

OSAL_STATUS
osalHashSHA224(UINT8 *in,
               UINT8 *out)
{
    int rc = -1;
    if(!IS_ERR(sha224_tfm))
    {
        struct sha256_state *sctx = NULL;
        struct {
                struct shash_desc shash;
                char ctx[crypto_shash_descsize(sha224_tfm)];
        } desc;

        desc.shash.tfm = sha224_tfm;
        desc.shash.flags = 0x0;
        rc = crypto_shash_init(&desc.shash);
        if(rc)
        {
            printk("crypto_shash_init failed\n");
            return OSAL_FAIL;
        }
        rc = crypto_shash_update(&desc.shash, in, SHA224_BLOCK_SIZE);
        sctx = (struct sha256_state*) desc.ctx;
        memcpy(out, sctx->state, SHA256_DIGEST_SIZE);
    }
    else
    {
        printk("sha224 tfm not initialized\n");
    }
    return (rc == 0) ? OSAL_SUCCESS : OSAL_FAIL;
}

OSAL_STATUS
osalHashSHA256(UINT8 *in,
               UINT8 *out)
{
    int rc = -1;
    if(!IS_ERR(sha256_tfm))
    {
        struct sha256_state *sctx = NULL;
        struct {
                struct shash_desc shash;
                char ctx[crypto_shash_descsize(sha256_tfm)];
        } desc;

        desc.shash.tfm = sha256_tfm;
        desc.shash.flags = 0x0;
        rc = crypto_shash_init(&desc.shash);
        if(rc)
        {
            printk("crypto_shash_init failed\n");
            return OSAL_FAIL;
        }
        rc = crypto_shash_update(&desc.shash, in, SHA256_BLOCK_SIZE);
        sctx = (struct sha256_state*) desc.ctx;
        memcpy(out, sctx->state, SHA256_DIGEST_SIZE);
    }
    else
    {
        printk("sha256 tfm not initialized\n");
    }
    return (rc == 0) ? OSAL_SUCCESS : OSAL_FAIL;
}

OSAL_STATUS
osalHashSHA384(UINT8 *in,
               UINT8 *out)
{
    int rc = -1;
    if(!IS_ERR(sha384_tfm))
    {
        struct sha512_state *sctx = NULL;
        struct {
                struct shash_desc shash;
                char ctx[crypto_shash_descsize(sha384_tfm)];
        } desc;

        desc.shash.tfm = sha384_tfm;
        desc.shash.flags = 0x0;
        rc = crypto_shash_init(&desc.shash);
        if(rc)
        {
            printk("crypto_shash_init failed\n");
            return OSAL_FAIL;
        }
        rc = crypto_shash_update(&desc.shash, in, SHA384_BLOCK_SIZE);
        sctx = (struct sha512_state*) desc.ctx;
        memcpy(out, sctx->state, SHA512_DIGEST_SIZE);
    }
    else
    {
        printk("sha384 tfm not initialized\n");
    }
    return (rc == 0) ? OSAL_SUCCESS : OSAL_FAIL;
}

OSAL_STATUS
osalHashSHA512(UINT8 *in,
               UINT8 *out)
{
    int rc = -1;
    if(!IS_ERR(sha512_tfm))
    {
        struct sha512_state *sctx = NULL;
        struct {
                struct shash_desc shash;
                char ctx[crypto_shash_descsize(sha512_tfm)];
        } desc;

        desc.shash.tfm = sha512_tfm;
        desc.shash.flags = 0x0;
        rc = crypto_shash_init(&desc.shash);
        if(rc)
        {
            printk("crypto_shash_init failed\n");
            return OSAL_FAIL;
        }
        rc = crypto_shash_update(&desc.shash, in, SHA512_BLOCK_SIZE);
        sctx = (struct sha512_state*) desc.ctx;
        memcpy(out, sctx->state, SHA512_DIGEST_SIZE);
    }
    else
    {
        printk("sha512 tfm not initialized\n");
    }
    return (rc == 0) ? OSAL_SUCCESS : OSAL_FAIL;
}

OSAL_STATUS
osalAESEncrypt(UINT8 *key,
               UINT32 keyLenInBytes,
               UINT8 *in,
               UINT8 *out)
{
    OSAL_STATUS ret=OSAL_SUCCESS;

    if(IS_ERR(cipher_tfm))
    {
        printk("aes tfm not initialized\n");
        return OSAL_FAIL;
    }
    else
    {
        ret = osalLock(&encLock);
        if (OSAL_FAIL == ret)
        {
            printk("osalAESEncrypt: osalLock failed\n");
            return OSAL_FAIL;
        }
        crypto_cipher_setkey(cipher_tfm, key, keyLenInBytes);
        crypto_cipher_encrypt_one(cipher_tfm, out, in);
        ret = osalUnlock(&encLock);
        if (OSAL_FAIL == ret)
        {
            printk("osalAESEncrypt: osalUnlock failed\n");
            return OSAL_FAIL;
        }
    }
    return OSAL_SUCCESS;
}


OSAL_STATUS
osalCryptoInterfaceInit(void)
{
    OSAL_STATUS ret=OSAL_SUCCESS;

    md5_tfm = crypto_alloc_shash("md5", 0, 0);
    if(IS_ERR(md5_tfm))
    {
        printk("crypto_alloc_shash md5 failed\n");
        return OSAL_FAIL;
    }

    sha1_tfm = crypto_alloc_shash("sha1", 0, 0);
    if(IS_ERR(sha1_tfm))
    {
        printk("crypto_alloc_shash sha1 failed\n");
        return OSAL_FAIL;
    }
    sha224_tfm = crypto_alloc_shash("sha224", 0, 0);
    if(IS_ERR(sha224_tfm))
    {
        printk("crypto_alloc_shash sha224 failed\n");
        return OSAL_FAIL;
    }
    sha256_tfm = crypto_alloc_shash("sha256", 0, 0);
    if(IS_ERR(sha256_tfm))
    {
        printk("crypto_alloc_shash sha256 failed\n");
        return OSAL_FAIL;
    }

    sha384_tfm = crypto_alloc_shash("sha384", 0, 0);
    if(IS_ERR(sha384_tfm))
    {
        printk("crypto_alloc_shash sha384 failed\n");
        return OSAL_FAIL;
    }

    sha512_tfm = crypto_alloc_shash("sha512", 0, 0);
    if(IS_ERR(sha512_tfm))
    {
        printk("crypto_alloc_shash sha512 failed\n");
        return OSAL_FAIL;
    }

    cipher_tfm = crypto_alloc_cipher("aes", 0, 0);
    if(IS_ERR(cipher_tfm))
    {
        printk("crypto_alloc_cipher aes failed\n");
        return OSAL_FAIL;
    }

    ret = osalLockInit(&encLock, TYPE_IGNORE);
    if (OSAL_FAIL == ret)
    {
        printk("osalCryptoInterfaceInit: osalLockInit failed\n");
        return OSAL_FAIL;
    }

    return OSAL_SUCCESS;
}

void
osalCryptoInterfaceExit(void)
{
    osalLockDestroy(&encLock);

    if(!IS_ERR(md5_tfm))
    {
        crypto_free_shash(md5_tfm);
    }
    if(!IS_ERR(sha1_tfm))
    {
        crypto_free_shash(sha1_tfm);
    }
    if(!IS_ERR(sha224_tfm))
    {
        crypto_free_shash(sha224_tfm);
    }
    if(!IS_ERR(sha256_tfm))
    {
        crypto_free_shash(sha256_tfm);
    }
    if(!IS_ERR(sha384_tfm))
    {
        crypto_free_shash(sha384_tfm);
    }
    if(!IS_ERR(sha512_tfm))
    {
        crypto_free_shash(sha512_tfm);
    }
    if(!IS_ERR(cipher_tfm))
    {
        crypto_free_cipher(cipher_tfm);
    }
}
