
/***************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2007,2008,2009,2010,2011 Intel Corporation. All rights reserved.
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
 *   BSD LICENSE
 *
 *   Copyright(c) 2007,2008,2009, 2010,2011 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rc4.h>
#include <openssl/engine.h>
#include <openssl/rand.h>

#include "tests.h"

/******************************************************************************
* function:
*         run_rc4 (int count,
*                  ENGINE *e
*                  int print_output
*                  int verify
*                  int len)
*
* @param count        [IN] - number of iterations
* @param e            [IN] - OpenSLL engine pointer
* @param print_output [IN] - print hex output flag
* @param verify       [IN] - verify flag
* @param len          [IN] - the length of input message
*
* description:
*   This function is designed to test the RC4 cipher during encryption and
*   decryption using the QAT engine.
*
*   The higher level EVP interface wrapper functions EVP_EncryptInit_ex(),
*   EVP_EncryptUpdate() and EVP_EncryptFinal_ex() are used for the cipher
*   routines inside of test application.
*
******************************************************************************/
static void
run_rc4(int count, ENGINE * e, int print_output, int verify, int len)
{
    int i = 0;
    int ret = 0;
    int cipherLen = 0;
    int update_len = 0;
    int DoutLen = 0;
    int DinLen = 0;
    EVP_CIPHER_CTX *ctx = NULL;
    EVP_CIPHER_CTX *dctx = NULL;

    unsigned char *input = OPENSSL_malloc(len);
    unsigned char *outcipher = OPENSSL_malloc(len);
    unsigned char *Doutput = OPENSSL_malloc(len);
    unsigned char *incipher = outcipher;

    /* 16 bytes/128bit key. */
    const unsigned char key[] = {
        0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
        0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    };

    if (outcipher == NULL || Doutput == NULL)
    {
        printf("[%s] --- Initial malloc failed ! \n", __func__);
        exit(EXIT_FAILURE);
    }

    /* Setup input plaintext */
    for (i = 0; i < len; i++)
        input[i] = i % 16;

    /* initialise ctx for encryption */
    ctx = OPENSSL_malloc(sizeof(EVP_CIPHER_CTX));
    if (ctx == NULL)
    {
        printf("[%s] --- OPENSSL_malloc() failed for ctx\n", __func__);
        exit(EXIT_FAILURE);
    }
    EVP_CIPHER_CTX_init(ctx);

    DEBUG("\n----- Encrypt RC4 cipher ----- \n\n");

    if (print_output)
        tests_hexdump("RC4 input msg:", input, len);

    ret = EVP_EncryptInit_ex(ctx,   /* EVP_CIPHER_CTX */
                             EVP_rc4(), /* EVP_CIPHER type */
                             e, /* Engine indicator */
                             key,   /* Key */
                             NULL   /* IV not used for RC4 */
        );

    if (ret != 1)
    {
        printf("[%s] --- EVP_EncryptInit_ex(, ret = %d!\n", __func__, ret);
        goto err;
    }

    for (i = 0; i < count; i++)
    {

        ret = EVP_EncryptUpdate(ctx,    /* EVP_CIPHER_CTX */
                                outcipher,  /* Output cipher pointer */
                                &cipherLen, /* Output cipher length pointer */
                                input,  /* Input text */
                                len /* Input text length */
            );

        if (ret != 1)
        {
            printf("[%s] --- Error in EVP_EncryptUpdate, ret = %d!\n", __func__,
                   ret);
            goto err;
        }
    }

    DEBUG("Textlen   is %d \n", len);
    DEBUG("cipherLen is %d \n", cipherLen);
    DEBUG("Calling EVP_CipherFinal! \n");

    ret = EVP_EncryptFinal_ex(ctx,  /* EVP_CIPHER_CTX */
                              outcipher,    /* Output buffer */
                              &update_len   /* Number of bytes of output */
        );

    if (ret != 1)
    {
        printf("[%s] --- Error in evp_EncryptFinal, ret = %d!\n", __func__,
               ret);
        goto err;
    }

    if (print_output)
    {
        tests_hexdump("RC4 Ciphertext:", outcipher, cipherLen);
        tests_hexdump("RC4 Key:", key, sizeof(key));
    }

    EVP_CIPHER_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
    ctx = NULL;

        /*---------- Decryption ----------*/

    /* Initialise ctx for decryption */
    dctx = OPENSSL_malloc(sizeof(EVP_CIPHER_CTX));
    if (dctx == NULL)
    {
        printf("[%s] --- OPENSSL_malloc() failed for dctx\n", __func__);
        exit(EXIT_FAILURE);
    }
    EVP_CIPHER_CTX_init(dctx);

    /* REset the update_len value for decryption */
    update_len = 0;

    /* Input length should same as output cipher length from encryption */
    DinLen = cipherLen;

    ret = EVP_DecryptInit_ex(dctx,  /* EVP_CIPHER_CTX */
                             EVP_rc4(), /* EVP_CIPHER type */
                             e, /* Engine indicator */
                             key,   /* Key */
                             NULL   /* IV not used for RC4 */
        );

    if (ret != 1)
    {
        printf("[%s] --- Error in EVP_DecryptInit_ex, ret = %d!\n", __func__,
               ret);
        goto err;
    }

    for (i = 0; i < count; i++)
    {

        ret = EVP_DecryptUpdate(dctx,   /* EVP_CIPHER_CTX */
                                Doutput,    /* Output text pointer */
                                &DoutLen,   /* Output text length pointer */
                                incipher,   /* Input cipher for decryption */
                                DinLen  /* Input cipher length */
            );

        if (ret != 1)
        {
            printf("[%s] --- Error in EVP_DecryptUpdate, ret = %d!\n", __func__,
                   ret);
            goto err;
        }
    }

    DEBUG("DoutLen is %d \n", DoutLen);

    ret = EVP_DecryptFinal_ex(dctx, /* EVP_CIPHER_CTX */
                              Doutput,  /* Output buffer */
                              &update_len   /* Number of bytes of output */
        );

    if (ret != 1)
    {
        printf("[%s] --- Error in EVP_DecryptFinal, ret = %d!\n", __func__,
               ret);
        goto err;
    }

    if (print_output)
        tests_hexdump("RC4 Decrypted msg:", Doutput, DoutLen);

    /* Compare and verify the decrypt and encrypt message */
    if (verify)
    {
        if (memcmp(Doutput, input, len))
        {
            printf("# FAIL verify for RC4.\n");
            tests_hexdump("RC4 actual  :", Doutput, DoutLen);
            tests_hexdump("RC4 expected:", input, len);
        }
        else
        {
            printf("# PASS verify for RC4.\n");
        }
    }

    EVP_CIPHER_CTX_cleanup(dctx);
    OPENSSL_free(dctx);
    dctx = NULL;

    if (input)
        OPENSSL_free(input);
    if (outcipher)
        OPENSSL_free(outcipher);
    if (Doutput)
        OPENSSL_free(Doutput);
    return;

  err:
    if (ctx != NULL)
    {
        EVP_CIPHER_CTX_cleanup(ctx);
        OPENSSL_free(ctx);
    }
    if (dctx != NULL) ;
    {
        EVP_CIPHER_CTX_cleanup(dctx);
        OPENSSL_free(dctx);
    }
    exit(EXIT_FAILURE);
}

/******************************************************************************
* function:
*      tests_run_rc4 (int count,
*                     int size,
*		   engine *e	
*                     int print_output
*                     int verify)
*
* @param count [IN] - number of iterations
* @param size [IN] - the length of input message
* @param e [IN] - OpenSLL engine pointer
* @param print_output [IN] - print hex output flag
* @param verify [IN] - verify flag
*
* description:
*	specify a test case
*
******************************************************************************/
void
tests_run_rc4(int count, int size, ENGINE * e, int print_output, int verify)
{
    run_rc4(count, e, print_output, verify, size);
}
