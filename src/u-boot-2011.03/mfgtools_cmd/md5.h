/**
 *
 * @file md5.h
 *
 * @brief md5 checksum
 *
 * @author Maury Miller
 * Copyright &copy; 2005, WatchGuard Technologies, Inc.
 * All Rights Reserved
 *
 * @date Tue Nov 8 2005
 *
 * @par Function:
 *
 *      Header file for md5 functions.
 *        
 */

#ifndef WGUT_MD5_H_
#define WGUT_MD5_H_

// #include <endian.h>
#include <linux/types.h>

#define MD5HEX_LEN   16
#define MD5ASCII_LEN 32

typedef struct {
    u_int32_t buf[4];
    u_int32_t bits[2];
    u_int32_t in[64];
} wgut_MD5_CTX;

/*
** md5buf()
**
** INPUT: md5 == pointer to 16-byte buffer to hold final MD5
**        buf == pointer to memory location to sum
**        len == size of memory 
**
** OUTPUT: md5 will be calculated and stored in *md5
**
*/
void wgut_md5buf(unsigned char *md5, void *buf, size_t buflen);


/*
 * convert ASCII to hex 
*/
extern void wgut_ASCII2HEX(unsigned char *md5buf, const unsigned char *ascii, int length);

#define wgut_ascii2hex wgut_ASCII2HEX

#define ASCII2MD5(md5buf, ascii) \
    wgut_ASCII2HEX((md5buf), (ascii), MD5HEX_LEN)
    
/* convert 16-byte hex to ASCII
 *
 * NOTE: No NULL-terminator is written to 'ascii' -- you must terminate
 * after the call if you wish!!!
*/
void wgut_HEX2ASCII(unsigned char *ascii, const unsigned char *md5buf, int n);

#define wgut_hex2ascii wgut_HEX2ASCII


#define MD52ASCII(ascii, md5buf) \
    wgut_HEX2ASCII((ascii), (md5buf), MD5HEX_LEN)

/*
 * NOTE: this string is built from right-to-left - so, it is safe
 * to use a single buffer for dual-purpose, ie:
 *
 *    wgut_MD5_CTX md5;
 *    char buf[33];  // note: room for NULL-terminator
 *    wgut_MD5Init(&md5);
 *    wgut_MD5Update(&md5, foobar, foobar_length);
 *    wgut_MD5Final(buf, &md5);
 *    MD52ASCII(buf, buf);
 *    buf[32] = 0;
 */

void wgut_MD5Init(wgut_MD5_CTX *ctx);
void wgut_MD5Update(wgut_MD5_CTX *ctx, unsigned char *buf, size_t len);
void wgut_MD5Final(unsigned char digest[16], wgut_MD5_CTX *ctx);

#endif /* WGUT_MD5_H_ */
