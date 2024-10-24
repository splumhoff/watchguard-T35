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

/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

#ifndef __TESTS_H
#define __TESTS_H

ENGINE * tests_initialise_engine(void);
void tests_run (int *count, int size, int type, ENGINE *e, int id, int print_output, int verify, int performance);
void tests_cleanup_engine (ENGINE *e);
void tests_hexdump (const char *title, const unsigned char *s,int l);

void tests_run_md5_msg (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_hmac_sha256 (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_sha1_msg   (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_sha256_msg (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_sha512_msg (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_aes256 (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_aes192 (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_aes128cbc_hmac_sha1 (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_aes128 (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_rc4 (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_des3 (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_des3_encrypt (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_des3_decrypt (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_des (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_rsa (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_rsa_sign (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_rsa_verify (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_dsa (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_dsa_sign_only (int count, int size, ENGINE *e, int print_output, int verify);
void tests_run_dsa_sign_verify (int count, int size, ENGINE *e, int print_output, int verify);

void tests_run_dh (int count, int size, ENGINE *e, int print_output, int verify);

#define TEST_DES		1
#define TEST_DES3		2
#define TEST_DES3_ENCRYPT	3
#define TEST_DES3_DECRYPT	4
#define TEST_AES128		5
#define TEST_AES192		6
#define TEST_AES256		7
#define TEST_RC4		8

/* Note, throughput test in main uses MAX_CIPHER_TEST_TYPE to determine if */
/* test id is a cipher or digest/PKE. Add ciphers above and digests below. */
#define MAX_CIPHER_TEST_TYPE    TEST_RC4

#define TEST_SHA1               9
#define TEST_SHA256             10
#define TEST_SHA512             11
#define TEST_MD5                12
#define TEST_RSA_SIGN           13
#define TEST_RSA_VERIFY         14
#define TEST_RSA_ENCRYPT        15
#define TEST_RSA_DECRYPT        16
#define TEST_DSA_SIGN           17
#define TEST_DSA_VERIFY         18
#define TEST_DH        			19
#define TEST_HMAC        		20
#define TEST_AES128_CBC_HMAC_SHA1 21
#define TEST_AES256_CBC_HMAC_SHA1 22

#define TEST_TYPE_MAX 22	 


#define DEBUG(...) 

#define QAT_DEV "/dev/qat_mem"

#endif

