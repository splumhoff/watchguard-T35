/**
 * $Id:
 *
 * @file md5.c
 *
 * @brief md5 hashing functions for the usertool library 
 *
 * Copyright &copy; 2005, WatchGuard Technologies, Inc.
 * All Rights Reserved
 *
 * @date Tue Feb 12 2014
 *
 * @par Function:
 *
 *     md5 hashing functions.
 *
 */

#include <common.h>

#if 0
#ifdef DEBUG
#include <stdio.h>
#endif
#endif

#include "md5.h"

static void MD5Transform(u_int32_t buf[4], u_int32_t in[16]);

#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define WGbyteReverse(buf, len)   /* Nothing */
#else
static void WGbyteReverse(unsigned char *buf, u_int32_t longs);

/**
 * @brief This function reverses byte order of data.
 *
 * @param buf - data to reverse
 *
 * @param longs - length of buffer
 *
 * @retval none
 *
 * Note: this code is harmless on little-endian machines. 
 */
static void WGbyteReverse(unsigned char *buf, u_int32_t longs)
{
#ifdef DEBUG
    fprintf(stderr,"WGbyteReverse() lib/wgcrypto/md5.c\n");
#endif

    u_int32_t t;

    do {
        t = (u_int32_t) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
            ((unsigned) buf[1] << 8 | buf[0]);
        *(u_int32_t *) buf = t;
        buf += 4;
    } while (--longs);
}
#endif


/**
 * @brief Function performs md5 hash by calling three helper functions
 *
 * @param md5 - 16 byte digest
 *
 * @param buf - input buffer
 *
 * @param buflen - length of buf
 *
 * @retval none
 * 
 */
void wgut_md5buf(unsigned char *md5, void *buf, size_t buflen) {
    wgut_MD5_CTX ctx;
  
    wgut_MD5Init(&ctx);
    wgut_MD5Update(&ctx, buf, buflen);
    wgut_MD5Final(md5, &ctx);
}

static char *hextab = "0123456789abcdef";

/*
 * @brief Function converts hex string to.
 * NOTE: caller must ensure that 'ascii' is at least 32 bytes long !!!
 *
 * @param ascii - buffer with ascii
 *
 * @param md5buf - buffer with hex
 *
 * @param n - length
 *
 * @retval none
 * 
 */
void wgut_HEX2ASCII(unsigned char *ascii, const unsigned char *md5buf, int n) {
    int i = 0;
    int l = n - 1;
    int l2 = n + n - 1;

    for(i=0; i < n; i++) {
        ascii[l2-(i*2)]   = hextab[  md5buf[l - i] & 0x0f      ];
        ascii[l2-(i*2)-1] = hextab[ (md5buf[l - i] & 0xf0) >> 4];   
    }
}

static unsigned char chrtab[0x100] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/*
 * @brief Function converts an ascii string to hex.
 * NOTE: caller must ensure that 'ascii' is at least 32 bytes long !!!
 *
 * @param md5buf - buffer with hex
 *
 * @param ascii - buffer with ascii
 *
 * @param length - length
 *
 * @retval none
 * 
 */
void wgut_ASCII2HEX(unsigned char *md5buf, const unsigned char *ascii, int length) {
    int i = 0;
    
    for (i = 0; i < length; i++) {
        md5buf[i] = (chrtab[ (int)(ascii[i*2]) ] << 4) | 
            chrtab[(int)(ascii[i*2+1])];
    }

    return;
}

/**
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 *
 * @param ctx - wgut_MD5_CTX
 *
 * @retval none
 * 
 */
void wgut_MD5Init(wgut_MD5_CTX *ctx) {
  memset(ctx, 0, sizeof(wgut_MD5_CTX));
  ctx->buf[0] = 0x67452301;
  ctx->buf[1] = 0xefcdab89;
  ctx->buf[2] = 0x98badcfe;
  ctx->buf[3] = 0x10325476;
  
  ctx->bits[0] = 0;
  ctx->bits[1] = 0;
}


/**
 * @brief Update context to reflect the concatenation of another buffer full
 * of bytes.
 *
 * @param ctx - wgut_MD5_CTX
 *
 * @param buf - input buffer
 *
 * @param len - buffer length
 *
 * @retval none
 * 
 */
void wgut_MD5Update(wgut_MD5_CTX *ctx, unsigned char *buf, size_t len) {
    size_t t;

    if (len == 0)
        return;

  /* Update bitcount */  
    t = ctx->bits[0];
    if ((ctx->bits[0] = (t + (len << 3))&0xffffffffUL) < t)
        ctx->bits[1]++; 	/* Carry from low to high */
    ctx->bits[1] += len >> 29;
  
    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */
  
    /* Handle any leading odd-sized chunks */
    if (t) {
        unsigned char *p = (unsigned char *) ctx->in + t;
    
        t = 64 - t;
        if (len < t) {
            memmove(p, buf, len);
            return;
        }

        memmove(p, buf, t);
        WGbyteReverse((unsigned char*)ctx->in, 16);
        MD5Transform(ctx->buf, ctx->in);
        buf += t;
        len -= t;
    }
    
    /* Process data in 64-byte chunks */
    while (len >= 64) {
        memmove(ctx->in, buf, 64);
        WGbyteReverse((unsigned char*)ctx->in, 16);
        MD5Transform(ctx->buf, ctx->in);
        buf += 64;
        len -= 64;
    }
  
    /* Handle any remaining bytes of data. */
  
    memmove(ctx->in, buf, len);
}

/**
 * @brief Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 *
 * @param digest - md5 digest
 *
 * @param ctx - wgut_MD5_CTX
 *
 * @retval none
 * 
 */
void wgut_MD5Final(unsigned char digest[16], wgut_MD5_CTX *ctx) {
  unsigned count;
  unsigned char *p;

  /* Compute number of bytes mod 64 */
  count = (ctx->bits[0] >> 3) & 0x3F;

  /* Set the first char of padding to 0x80.  This is safe since there is
     always at least one byte free */
  p = (unsigned char*)ctx->in + count;
  *p++ = 0x80;

  /* Bytes of padding needed to make 64 bytes */
  count = 64 - 1 - count;

  /* Pad out to 56 mod 64 */
  if (count < 8) {
    /* Two lots of padding:  Pad the first block to 64 bytes */
    memset(p, 0, count);
    WGbyteReverse((unsigned char*)ctx->in, 16);
    MD5Transform(ctx->buf, ctx->in);

    /* Now fill the next block with 56 bytes */
    memset(ctx->in, 0, 56);
  } else {
    /* Pad block to 56 bytes */
    memset(p, 0, count - 8);
  }
  WGbyteReverse((unsigned char*)ctx->in, 14);

  /* Append length in bits and transform */
  ctx->in[14] = ctx->bits[0];
  ctx->in[15] = ctx->bits[1];

  MD5Transform(ctx->buf, ctx->in);
  WGbyteReverse((unsigned char *) ctx->buf, 4);
  memmove(digest, ctx->buf, 16);
  memset(ctx, 0, sizeof(*ctx));        /* In case it's sensitive */
}


/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + (data),  w = w<<s | (w&0xffffffff)>>(32-s),  w += x )

/*
 * @brief The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 *
 * @param buf - md5 hash
 *
 * @param in - new data
 *
 * @retval none
 * 
 */
static void MD5Transform(u_int32_t buf[4], u_int32_t in[16]) {
  register unsigned long a, b, c, d;

  a = buf[0];
  b = buf[1];
  c = buf[2];
  d = buf[3];

  MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
  MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
  MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
  MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
  MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
  MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
  MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
  MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
  MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
  MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
  MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
  MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
  MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
  MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
  MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
  MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
  MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
  MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
  MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
  MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
  MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
  MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}







