#ifndef LIBCRYPTO_COMPAT_H
#define LIBCRYPTO_COMPAT_H

#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/ecdsa.h>
#include <openssl/dh.h>
#include <openssl/evp.h>

#define OpenSSL_version_num SSLeay
#define OpenSSL_version SSLeay_version
#define OPENSSL_VERSION SSLEAY_VERSION

#define OPENSSL_INIT_LOAD_CRYPTO_STRINGS	0x00000002L
#define OPENSSL_INIT_LOAD_CONFIG		0x00000040L
#define OPENSSL_INIT_ENGINE_RDRAND		0x00000200L
#define OPENSSL_INIT_ENGINE_DYNAMIC		0x00000400L
#define OPENSSL_INIT_ENGINE_CRYPTODEV		0x00001000L
#define OPENSSL_INIT_ENGINE_CAPI		0x00002000L
#define OPENSSL_INIT_ENGINE_PADLOCK		0x00004000L
#define OPENSSL_INIT_ENGINE_ALL_BUILTIN		\
		(OPENSSL_INIT_ENGINE_RDRAND | OPENSSL_INIT_ENGINE_DYNAMIC \
		 | OPENSSL_INIT_ENGINE_CRYPTODEV | OPENSSL_INIT_ENGINE_CAPI | \
		 OPENSSL_INIT_ENGINE_PADLOCK)
int OPENSSL_init_crypto(uint64_t opts, void *not_implemented);

int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d);
int RSA_set0_factors(RSA *r, BIGNUM *p, BIGNUM *q);
int RSA_set0_crt_params(RSA *r, BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp);
void RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d);
void RSA_get0_factors(const RSA *r, const BIGNUM **p, const BIGNUM **q);
void RSA_get0_crt_params(const RSA *r, const BIGNUM **dmp1, const BIGNUM **dmq1, const BIGNUM **iqmp);

void DSA_get0_pqg(const DSA *d, const BIGNUM **p, const BIGNUM **q, const BIGNUM **g);
int DSA_set0_pqg(DSA *d, BIGNUM *p, BIGNUM *q, BIGNUM *g);
void DSA_get0_key(const DSA *d, const BIGNUM **pub_key, const BIGNUM **priv_key);
int DSA_set0_key(DSA *d, BIGNUM *pub_key, BIGNUM *priv_key);

void DSA_SIG_get0(const DSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps);
int DSA_SIG_set0(DSA_SIG *sig, BIGNUM *r, BIGNUM *s);

void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps);
int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s);

void DH_get0_pqg(const DH *dh, const BIGNUM **p, const BIGNUM **q, const BIGNUM **g);
int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g);
void DH_get0_key(const DH *dh, const BIGNUM **pub_key, const BIGNUM **priv_key);
int DH_set0_key(DH *dh, BIGNUM *pub_key, BIGNUM *priv_key);
int DH_set_length(DH *dh, long length);

const unsigned char *EVP_CIPHER_CTX_iv(const EVP_CIPHER_CTX *ctx);
unsigned char *EVP_CIPHER_CTX_iv_noconst(EVP_CIPHER_CTX *ctx);
int EVP_MD_CTX_reset(EVP_MD_CTX *ctx);
EVP_MD_CTX *EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX *ctx);
#define EVP_CIPHER_impl_ctx_size(e) e->ctx_size
#define EVP_CIPHER_CTX_get_cipher_data(ctx) ctx->cipher_data

RSA_METHOD *RSA_meth_dup(const RSA_METHOD *meth);
int RSA_meth_set1_name(RSA_METHOD *meth, const char *name);
#define RSA_meth_get_finish(meth) meth->finish
int RSA_meth_set_priv_enc(RSA_METHOD *meth, int (*priv_enc) (int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding));
int RSA_meth_set_priv_dec(RSA_METHOD *meth, int (*priv_dec) (int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding));
int RSA_meth_set_finish(RSA_METHOD *meth, int (*finish) (RSA *rsa));
void RSA_meth_free(RSA_METHOD *meth);

int RSA_bits(const RSA *r);
int DSA_bits(const DSA *d);

RSA *EVP_PKEY_get0_RSA(EVP_PKEY *pkey);

EVP_CIPHER *EVP_CIPHER_meth_new(int cipher_type, int block_size, int key_len);
void EVP_CIPHER_meth_free(EVP_CIPHER *cipher);

int EVP_CIPHER_meth_set_iv_length(EVP_CIPHER *cipher, int iv_len);
int EVP_CIPHER_meth_set_flags(EVP_CIPHER *cipher, unsigned long flags);
int EVP_CIPHER_meth_set_init(EVP_CIPHER *cipher,
                             int (*init) (EVP_CIPHER_CTX *ctx,
                                          const unsigned char *key,
                                          const unsigned char *iv,
                                          int enc));
int EVP_CIPHER_meth_set_do_cipher(EVP_CIPHER *cipher,
                                  int (*do_cipher) (EVP_CIPHER_CTX *ctx,
                                                    unsigned char *out,
                                                    const unsigned char *in,
                                                    size_t inl));
int EVP_CIPHER_meth_set_cleanup(EVP_CIPHER *cipher,
                                int (*cleanup) (EVP_CIPHER_CTX *));
int EVP_CIPHER_meth_set_ctrl(EVP_CIPHER *cipher,
                             int (*ctrl) (EVP_CIPHER_CTX *, int type,
                                          int arg, void *ptr));

int (*EVP_CIPHER_meth_get_init(const EVP_CIPHER *cipher))(EVP_CIPHER_CTX *ctx,
                                                          const unsigned char *key,
                                                          const unsigned char *iv,
                                                          int enc);
int (*EVP_CIPHER_meth_get_do_cipher(const EVP_CIPHER *cipher))(EVP_CIPHER_CTX *ctx,
                                                               unsigned char *out,
                                                               const unsigned char *in,
                                                               size_t inl);
int (*EVP_CIPHER_meth_get_cleanup(const EVP_CIPHER *cipher))(EVP_CIPHER_CTX *);
int (*EVP_CIPHER_meth_get_ctrl(const EVP_CIPHER *cipher))(EVP_CIPHER_CTX *,
                                                          int type, int arg,
                                                          void *ptr);

#define EVP_CIPHER_CTX_reset(c)      EVP_CIPHER_CTX_init(c)

int EVP_CIPHER_CTX_encrypting(const EVP_CIPHER_CTX *ctx);

#endif /* OPENSSL_VERSION_NUMBER */

#endif /* LIBCRYPTO_COMPAT_H */

