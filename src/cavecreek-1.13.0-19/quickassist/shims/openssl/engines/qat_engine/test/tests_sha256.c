
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
*   tests_run_sha256__msg (int count,
*                          int size, 
*                          ENGINE *e
*                          int print_output,
*                          int verify)
*
* @param count        [IN] - number of iterations
* @param size         [IN] - input data size 
* @param e            [IN] - OpenSSL engine pointer
* @param print_output [IN] - print hex output flag
* @param verify       [IN] - verify flag
*
* description:
*   This function is designed to test the QAT engine with variable message sizes
*   using the SHA256 algorithm. The higher level EVP interface function EVP_Digest()
*   is used inside of test application.
*   This is a boundary test, the application should return the expected digest hash value.
*   In verify mode a input size of 1024 bytes is used to generate a comparison digest.
******************************************************************************/
void tests_run_sha256_msg(int count, int size, ENGINE * e, int print_output,
                          int verify)
{
    int i = 0;
    int inLen = size;
    int ret = 0;

    /* Use default input size in verify mode. */
    if (verify)
        inLen = 1024;

    unsigned char md[SHA256_DIGEST_LENGTH];
    unsigned char *inData = OPENSSL_malloc(inLen);

    unsigned char expected[] = {
        0x0E, 0xC7, 0x76, 0x47, 0xB0, 0x18, 0x96, 0x7B,
        0x6E, 0x56, 0x57, 0x55, 0x35, 0xA7, 0x8E, 0x12,
        0xC4, 0x8D, 0x35, 0xD2, 0x75, 0x50, 0xA8, 0xA8,
        0x9E, 0x1D, 0x39, 0xA7, 0xBE, 0xA8, 0x97, 0x88,
    };

    if (inData == NULL)
    {
        printf("*** [%s] --- inData malloc failed! \n", __func__);
        exit(EXIT_FAILURE);
    }

    /* Setup the input and output data. */
    memset(inData, 0xaa, inLen);
    memset(md, 0x00, SHA256_DIGEST_LENGTH);

    for (i = 0; i < count; i++)
    {
        DEBUG("\n----- SHA256 digest msg ----- \n\n");

        ret = EVP_Digest(inData,    /* Input data pointer.  */
                         inLen, /* Input data length.  */
                         md,    /* Output hash pointer.  */
                         NULL, EVP_sha256(),    /* Hash algorithm indicator */
                         e      /* Engine indicator.  */
            );

        if (ret != 1 || verify)
        {
            /* Compare the digested results with the expected results */
            if (memcmp(md, expected, SHA256_DIGEST_LENGTH))
            {
                printf("# FAIL verify for SHA256.\n");

                tests_hexdump("SHA256 actual  :", md, SHA256_DIGEST_LENGTH);
                tests_hexdump("SHA256 expected:", expected,
                              SHA256_DIGEST_LENGTH);
            }
            else
            {
                printf("# PASS verify for SHA256.\n");
            }
        }

        if (print_output)
            tests_hexdump("SHA256 digest text:", md, SHA256_DIGEST_LENGTH);
    }

    if (inData)
        OPENSSL_free(inData);
}
