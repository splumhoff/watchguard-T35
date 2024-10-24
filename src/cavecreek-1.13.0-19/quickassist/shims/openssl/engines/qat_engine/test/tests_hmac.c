
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
#include <openssl/hmac.h>
#include <openssl/engine.h>
#include <openssl/des.h>
#include <openssl/rand.h>

#include "tests.h"

static unsigned char key[] = 
{
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 
	0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 
	0x0b, 0x0b, 0x0b, 0x0b, 
};
        
		
static unsigned char data[]= 
{ 
	0x48, 0x69, 0x20, 0x54, 0x68, 0x65, 0x72, 0x65,
};

static unsigned char expected[] = 
{
	0x49, 0x2c, 0xe0, 0x20, 0xfe, 0x25, 0x34, 0xa5, 
	0x78, 0x9d, 0xc3, 0x84, 0x88, 0x06, 0xc7, 0x8f, 
	0x4f, 0x67, 0x11, 0x39, 0x7f, 0x08, 0xe7, 0xe7, 
	0xa1, 0x2c, 0xa5, 0xa4, 0x48, 0x3c, 0x8a, 0xa6,
};


/******************************************************************************
* function:
*   tests_run_hmac_sha256 (int count,
*                      int size,
*                      ENGINE *e
*                      int print_output,
*                      int verify)
*
* @param count        [IN] - number of iterations
* @param size         [IN] - input data size
* @param e            [IN] - OpenSSL engine pointer
* @param print_output [IN] - print hex output flag
* @param verify       [IN] - verify flag
*
* Description:
*   This function is design to verify HMAC-SHA256 using QAT engine. 
******************************************************************************/
void tests_run_hmac_sha256(int count, int size, ENGINE * e, int print_output,
                       int verify)
{
        unsigned char* result;
        unsigned int result_len = 32;
	int ret = 1;
        int i = 0;
        HMAC_CTX *ctx = NULL ;

    	ctx = (HMAC_CTX *) OPENSSL_malloc(sizeof(HMAC_CTX));
        result = OPENSSL_malloc(result_len);

        HMAC_CTX_init(ctx);
        HMAC_Init_ex(ctx, key, 16, EVP_sha256(), e);
        HMAC_Update(ctx, data, 8);
        HMAC_Final(ctx, result, &result_len);
	HMAC_CTX_cleanup(ctx);

	if(verify)
	{

        	for (i=0; i!=result_len; i++)
        	{
                	if (expected[i]!=result[i])
                	{
                        	printf("Got %02X instead of %02X at byte %d!\n", result[i], expected[i], i);
       				ret = 0; 
                        	break;
                	}
        	}
	}

	if (ret)
                printf("# PASS verify for HMAC.\n");

	
	if(ctx)
	    OPENSSL_free(ctx);
	if(result)
	    OPENSSL_free(result);
}
