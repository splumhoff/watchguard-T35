/*
 * Portions Copyrighted by Freescale Semiconductor, Inc., 2010-2011
 */

#include "fsl_secboot_types.h"
#include "rsa.h"

/*
 * Decodes the string to integer.
 * This  function  takes the string formatted RSA keys and decode
 * to the internal INTEGER object representation.
 * Following are the arguments
 * bt - keys/sign buffer in string format
 * len  - length of the buffer
 * s - signedness if Unsigned or Signed
 * in - Integer Object
 */
static void Integer__Decode(unsigned char *bt, int len, enum Signedness s,
				Integer *in)
{
       int sign;
       int i;
       u8 b;
       b = *bt;                /* Peek */

       sign = ((s == SIGNED) && (b & 0x80)) ? NEGATIVE : POSITIVE;

       while (len > 0 && (sign == POSITIVE ? b == 0 : b == 0xff)) {
		++bt;           /* Skip */
		len--;
		b = *bt;        /*Peek */
       }

       for (i = len; i > 0; i--) {
		b = *bt++;      /* Get */
		in->reg[(i - 1) / (WORD_SIZE)] |=
			(u32) b << ((i - 1) % (WORD_SIZE)) * 8;
       }
}

/*
 * RSA Signature verification.
 * This  function  recovers the encoded message from the signature
 * using the signer's public key rsa. "to" must point to a memory
 * section large enough to hold the encoded message.
 * Following are the arguments
 * sign - pointer to signature which is to be verified
 * rsa_pub_key - pointer to RSA public key
 * klen - size of RSA public key in bytes
 * to - pointer to the memory where recovered message is stored
 */
void rsa_public_verif(unsigned char *sign, u8 *to, u8 *rsa_pub_key,
			int klen)
{
       int i;
       RSA rsa;

       Integer signInt;
       Integer result;

       result.reg_size = signInt.reg_size = rsa.E.reg_size = rsa.N.reg_size =
		klen / 8;

       for (i = 0; i < 4 * KEY_SIZE_WORDS; i++) {
		rsa.N.reg[i] = 0;
		rsa.E.reg[i] = 0;
		signInt.reg[i] = 0;
		result.reg[i] = 0;
       }

       /* convert to integer format */
       Integer__Decode(rsa_pub_key, klen / 2, UNSIGNED, &rsa.N);
       Integer__Decode(rsa_pub_key + klen / 2, klen / 2, UNSIGNED, &rsa.E);
       Integer__Decode(sign, klen / 2, UNSIGNED, &signInt);

       /* apply the RSA result = sign exp E mod N */
       a_exp_b_mod_c(&signInt, &rsa.E, &rsa.N, &result);

       /* encode the result */
       Integer__Encode(&result, to, klen / 2, UNSIGNED);
}
