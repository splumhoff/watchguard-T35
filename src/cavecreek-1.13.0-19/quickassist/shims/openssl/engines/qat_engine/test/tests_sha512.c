
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

#include "tests.h"

/******************************************************************************
* function:
*   tests_run_sha512_msg (int count,
*                         int size, 
*                         ENGINE *e
*                         int print_output,
*                         int verify)
*
* @param count        [IN] - number of iterations
* @param size         [IN] - input data size 
* @param e            [IN] - OpenSSL engine pointer
* @param print_output [IN] - print hex output flag
* @param verify       [IN] - verify flag
*
* Description:
*   This function is designed to test the QAT engine with variable message sizes
*   using the SHA512 algorithm. The higher level EVP interface function EVP_Digest()
*   is used inside of test application.
*   This is a boundary test, the application should return the expected digest hash value.
*   In verify mode an input size of 1024 bytes is used to generate a comparison digest.
******************************************************************************/
void tests_run_sha512_msg(int count, int size, ENGINE * e, int print_output,
                          int verify)
{
    int i = 0;
    int inLen = size;
    int ret = 0;

    /* Use default input size in verify mode. */
    if (verify)
        inLen = 1024;

    unsigned char md[SHA512_DIGEST_LENGTH];
    unsigned char *inData = OPENSSL_malloc(inLen);

    unsigned char expected[] = {
        0xDC, 0x71, 0xA7, 0xA5, 0x44, 0x38, 0xA3, 0xAF,
        0x5D, 0x2C, 0x6A, 0x56, 0xD8, 0xB1, 0x22, 0xD5,
        0xD5, 0xC5, 0xF8, 0xC5, 0x8F, 0x0D, 0x9C, 0x1D,
        0x7D, 0x31, 0xCF, 0x11, 0x9A, 0x70, 0x22, 0x77,
        0xCD, 0xEE, 0x88, 0xE8, 0xB8, 0xFF, 0xBD, 0x7E,
        0xCA, 0xBE, 0x44, 0x73, 0x90, 0xF1, 0xDA, 0x1E,
        0xBC, 0x22, 0xEA, 0x22, 0xF8, 0x01, 0x08, 0xB7,
        0xB5, 0x22, 0xF8, 0x01, 0x55, 0xF1, 0xDD, 0xE6,
    };

    if (inData == NULL)
    {
        printf("*** [%s] --- inData malloc failed! \n", __func__);
        exit(EXIT_FAILURE);
    }

    /* Setup the input and output data. */
    memset(inData, 0xaa, inLen);
    memset(md, 0x00, SHA512_DIGEST_LENGTH);

    for (i = 0; i < count; i++)
    {
        DEBUG("\n----- SHA512 digest msg ----- \n\n");

        ret = EVP_Digest(inData,    /* Input data pointer.  */
                         inLen, /* Input data length.  */
                         md,    /* Output hash pointer.  */
                         NULL, EVP_sha512(),    /* Hash algorithm indicator. */
                         e      /* Engine indicator.  */
            );

        if (ret != 1 || verify)
        {
            /* Compare the digest results with the expected results. */
            if (memcmp(md, expected, SHA512_DIGEST_LENGTH))
            {
                printf("# FAIL verify for SHA512.\n");

                tests_hexdump("SHA512 actual  :", md, SHA512_DIGEST_LENGTH);
                tests_hexdump("SHA512 expected:", expected,
                              SHA512_DIGEST_LENGTH);
            }
            else
            {
                printf("# PASS verify for SHA512.\n");
            }
        }

        if (print_output)
            tests_hexdump("SHA512 digest text:", md, SHA512_DIGEST_LENGTH);
    }

    if (inData)
        OPENSSL_free(inData);
}
