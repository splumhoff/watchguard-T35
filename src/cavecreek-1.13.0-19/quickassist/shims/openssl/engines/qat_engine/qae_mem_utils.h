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

/*****************************************************************************
 * @file qae_mem_utils.h
 *
 * This file provides linux kernel memory allocation for quick assist API
 *
 *****************************************************************************/

#ifndef __QAE_MEM_UTILS_H
#define __QAE_MEM_UTILS_H

#include "cpa.h"

/*define types which need to vary between 32 and 64 bit*/
#ifdef __x86_64__
#define QAE_UINT  Cpa64U
#define QAE_INT   Cpa64S
#else
#define QAE_UINT  Cpa32U
#define QAE_INT  Cpa32S
#endif

#define qaePinnedMemAlloc(m) qaeMemAlloc(m,__FILE__,__LINE__)
#define qaePinnedMemRealloc(m,s) qaeMemAlloc(m,s,__FILE__,__LINE__)

/*****************************************************************************
 * function:
 *         qaeMemAlloc(size_t memsize);
 *
 * @description
 *      allocates memsize bytes of memory
 *
 * @param[in] memsize, the amount of memory in bytes to be allocated
 *
 * @retval pointer to the allocated memory
 *
 *****************************************************************************/
void *qaeMemAlloc(size_t memsize, const char *file, int line);

/*****************************************************************************
 * function:
 *         qaeMemRealloc(void *ptr, size_t memsize)
 *
 * @description
 *      re-allocates memsize bytes of memory
 *
 * @param[in] pointer to existing memory
 * @param[in] memsize, the amount of memory in bytes to be allocated
 *
 * @retval pointer to the allocated memory
 *
 *****************************************************************************/
void *qaeMemRealloc(void *ptr, size_t memsize, const char *file, int line);

/*****************************************************************************
 * function:
 *         qaeMemFree(void *ptr)
 *
 * @description
 *      frees memory allocated by the qaeMemAlloc function
 *
 *
 * @param[in] pointer to the memory to be freed
 *
 * @retval none
 *
 *****************************************************************************/
void qaeMemFree(void *ptr);


/*****************************************************************************
 * function:
 *         qaeMemV2P(void *v)
 *
 * @description
 * 	find the physical address of a block of memory referred to by virtual
 * 	address v in the current process's address map
 *
 *
 * @param[in] ptr, virtual pointer to the memory
 *
 * @retval the physical address of the memory referred to by ptr
 *
 *****************************************************************************/
CpaPhysicalAddr qaeMemV2P (void *v);

/******************************************************************************
* function:
*         setMyVirtualToPhysical(CpaVirtualToPhysical fp)
*
* @param CpaVirtualToPhysical [IN] - Function pointer to translation function
*
* description:
*   External API to allow users to specify their own virtual to physical
*   address translation function.
*
******************************************************************************/
void setMyVirtualToPhysical(CpaVirtualToPhysical fp);

#endif
