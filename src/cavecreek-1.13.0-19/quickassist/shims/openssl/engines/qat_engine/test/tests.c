
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
#include <openssl/sha.h>
#include <openssl/engine.h>
#include <openssl/des.h>
#include <openssl/rand.h>

#define _GNU_SOURCE
#define __USE_GNU

#include <pthread.h>

#include "tests.h"


static __inline__ unsigned long long rdtsc(void)
{
    unsigned long a, d;

    asm volatile ("rdtsc":"=a" (a), "=d"(d));

    return (((unsigned long long)a) | (((unsigned long long)d) << 32));
}


/******************************************************************************
* function:
*   tests_initialise_engine (void)
*
* description:
*   QAT engine initialise functions, load up the QAT engine and set as the default engine in OpenSSL.
******************************************************************************/

ENGINE *tests_initialise_engine(void)
{
    /* loading qat engine */
    ENGINE *e;

    DEBUG("Loading Qat Engine ! \n");
    e = ENGINE_by_id("qat");

    if (!e)
    {
        printf("Engine load failed, using default engine !\n");
        return NULL;
    }

    if (!ENGINE_init(e))
    {

        ENGINE_free(e);
        printf("Engine initialise failed ! using default engine\n");
        return NULL;
    }

    /* Set QAT engine as the default engine */
    ENGINE_set_default(e, ENGINE_METHOD_ALL);

    return e;
}

/******************************************************************************
* function:
*   tests_cleanup_engine (ENGINE *e)
*
* @param e [IN] - OpenSSL engine pointer
*
* description:
*   QAT engine clean up function.
******************************************************************************/
void tests_cleanup_engine(ENGINE * e)
{
    if (e)
    {
        /* Release the functional reference from ENGINE_init() */
        ENGINE_finish(e);
        /* Release the structural reference from ENGINE_by_id() */
        ENGINE_free(e);
    }
    DEBUG("QAT Engine Freed ! \n");
}

/******************************************************************************
* function:
*   tests_hexdump (const char *title,
          const unsigned char *s,
                          int l)
*
* @param title [IN] - hex dump title
* @param s [IN] - input pointer
* @param l [IN] - length of input
*
* description:
*   hex dump function.
******************************************************************************/
void tests_hexdump(const char *title, const unsigned char *s, int l)
{
    int i = 0;

    printf("%s", title);

    for (i = 0; i < l; i++)
    {
        if ((i % 8) == 0)
            printf("\n        ");

        printf("0x%02X, ", s[i]);
    }

    printf("\n\n");
}

/******************************************************************************
* function:
*   tests_run (int count,
*              int type
*              ENGINE *e
*              int id
*              int print_output
*              int verify
*              int performance)
*
* @param count        [IN] - Number of iteration count
* @param type         [IN] - Testing type
* @param size         [IN] - testing data size
* @param e            [IN] - OpenSSL engine pointer
* @param id           [IN] - Thread ID
* @param print_output [IN] - Print hex out flag
* @param verfiy       [IN] - Verify output flag
* @param performance  [IN] - performance output flag
*
* description:
*   select which application to run based on user input
******************************************************************************/

void tests_run(int *count, int type, int size, ENGINE * e, int id,
               int print_output, int verify, int performance)
{

    if (performance)
    {
        printf("\n|-----------------------------------------------------|\n");
        printf("|----------Thread ID %d, running in progress-----------|\n",
               id);
        printf("|-----------------------------------------------------|\n");
    }

    switch (type)
    {

        case TEST_HMAC:

/* HMAC test application */
            tests_run_hmac_sha256(*count, size, e, print_output, verify);
            break;

        case TEST_MD5:

/* MD5 test application */
            tests_run_md5_msg(*count, size, e, print_output, verify);
            break;

        case TEST_SHA1:

/* SHA1 test application */
            tests_run_sha1_msg(*count, size, e, print_output, verify);
            break;

        case TEST_SHA256:

/* SHA256 hash test application */
            tests_run_sha256_msg(*count, size, e, print_output, verify);
            break;

        case TEST_SHA512:

/* SHA512 hash test application */
            tests_run_sha512_msg(*count, size, e, print_output, verify);
            break;

        case TEST_DES3:

/* DES3 cipher test application, input message length 14 bytes */
            tests_run_des3(*count, size, e, print_output, verify);
            break;

        case TEST_DES3_ENCRYPT:

/* DES3 cipher test application, input message length 14 bytes */
            tests_run_des3_encrypt(*count, size, e, print_output, verify);
            break;

        case TEST_DES3_DECRYPT:

/* DES3 cipher test application, input message length 14 bytes */
            tests_run_des3_decrypt(*count, size, e, print_output, verify);
            break;

        case TEST_DES:

/* DES cipher test application, input message length 14 bytes */
            tests_run_des(*count, size, e, print_output, verify);
            break;

        case TEST_AES256:

/* AES256 cipher test application, input message length 14 bytes */
            tests_run_aes256(*count, size, e, print_output, verify);
            break;

        case TEST_AES192:

/* AES192 cipher test application, input message length 14 bytes */
            tests_run_aes192(*count, size, e, print_output, verify);
            break;

        case TEST_AES128:

/* AES128 cipher test application, input message len bytes */
            tests_run_aes128(*count, size, e, print_output, verify);
            break;

        case TEST_AES128_CBC_HMAC_SHA1:
      	    tests_run_aes_cbc_hmac_sha1(*count, size, e, print_output, verify, TEST_AES128_CBC_HMAC_SHA1);
	    break;

        case TEST_AES256_CBC_HMAC_SHA1:
	    tests_run_aes_cbc_hmac_sha1(*count, size, e, print_output, verify, TEST_AES256_CBC_HMAC_SHA1);
	    break;

	case TEST_RC4:

/* RC4 cipher test application, input message length 14 bytes */
            tests_run_rc4(*count, size, e, print_output, verify);
            break;

        case TEST_DSA_SIGN:

/* DSA sign & verify test application, input message length 124 bytes */
            tests_run_dsa_sign_only(*count, size, e, print_output, verify);
            break;

        case TEST_DSA_VERIFY:

/* DSA sign & verify test application, input message length 124 bytes */
            tests_run_dsa_sign_verify(*count, size, e, print_output, verify);
            break;

        case TEST_RSA_SIGN:

/* RSA sign & verify test application, input message length 124 bytes */
            tests_run_rsa_sign(*count, size, e, print_output, verify);
            break;

        case TEST_RSA_VERIFY:

/* RSA sign & verify test application, input message length 124 bytes */
            tests_run_rsa_verify(*count, size, e, print_output, verify);
            break;
       
        case TEST_RSA_ENCRYPT:

/* RSA encrypt test application*/
            tests_run_rsa_encrypt(*count, size, e, print_output, verify);
            break;

        case TEST_RSA_DECRYPT:

/* RSA decrypt test application */
            tests_run_rsa_decrypt(*count, size, e, print_output, verify);
            break;
       
	case TEST_DH:

/* DH test application, input message length 124 bytes */
            tests_run_dh(*count, size, e, print_output, verify);
            break;

        default:
            fprintf(stderr, "Unknown test type %d\n", type);
            exit(EXIT_FAILURE);
            break;
    }

    if (performance)
    {
        printf("\n|-----------------------------------------------------|\n");
        printf("|----------Thread ID %3d finished---------------------|\n",
               id);
        printf("|-----------------------------------------------------|\n");
    }

}
