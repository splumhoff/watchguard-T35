/*
 * Portions Copyrighted by Freescale Semiconductor, Inc., 2010-2011
 */

#ifndef _RSA_H
#define _RSA_H

void a_exp_b_mod_c(Integer *x, Integer *e, Integer *m, Integer *r);
extern void Integer__Encode(Integer *in, u8 *bt, size_t outputLen,
                           int signedness);
void rsa_public_verif(unsigned char *signature, u8 *to,
                               unsigned char *rsa_pub_key, int key_len);

#endif
