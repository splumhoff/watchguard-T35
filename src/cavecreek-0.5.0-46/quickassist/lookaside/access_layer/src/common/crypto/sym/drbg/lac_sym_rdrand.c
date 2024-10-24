/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or 
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
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
 *   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
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
 *  version: SXXXX.L.0.5.0-46
 *
 *****************************************************************************/

/**
 *****************************************************************************
 * @file lac_sym_rdrand.c
 *
 * @ingroup LacSym_Drbg
 *
 * @description
 *     Implementation of rdrand to generate random bits
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa_types.h"
#include "cpa.h"

#ifndef KERNEL_SPACE
#include <cpuid.h>
#endif

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/
#include "lac_common.h"
#include "Osal.h"

/*
*******************************************************************************
* Define RDRAND specific macros
*******************************************************************************
*/

#define LAC_RDRAND_RETRIES 10
/* Max number of retries for rdrand */

#define LAC_RDRAND_BITOFFSET 30
/* RDRAND support is determined from the 30th bit in ecx register */

#define LAC_RDRAND_BUFFER_LENGTH (sizeof(LAC_ARCH_UINT))
/* Length in bytes of the random data buffer received upon invoking
 * the rdrand instruction
 */

#define LAC_SIZE_OF_VENDOR_ID 13
/* Length of string to store cpu vendor ID */

#define LAC_DECODE_VND_ID_BYTES(string, reg)\
        *(string + 0) = reg & 0xFF;         \
        *(string + 1) = (reg >> 8)  & 0xFF; \
        *(string + 2) = (reg >> 16) & 0xFF; \
        *(string + 3) = (reg >> 24) & 0xFF; \
/* Determine cpu vendorID from cpu registers */

#ifdef KERNEL_SPACE
#define CPUID(level, pEax, pEbx, pEcx, pEdx) __cpuid(pEax, pEbx, pEcx, pEdx)
#else
#define CPUID(level, pEax, pEbx, pEcx, pEdx) \
                  __get_cpuid(level, pEax, pEbx, pEcx, pEdx)
#endif
/* Determine cpu registers info by querying cpuid */

/*
*******************************************************************************
* Functions
*******************************************************************************
*/

/**
 * @ingroup LacSym_DrbgRdrand
 *      Determine if rdrand bit is set in ecx register
 */
STATIC CpaStatus
LacDrbg_RdrandIsBitSet(unsigned int ecx)
{
    CpaStatus status = CPA_STATUS_SUCCESS;
    if (!(0x1 & (ecx >> LAC_RDRAND_BITOFFSET)))
    {
        status = CPA_STATUS_FAIL;
    }
    return status;
}

/**
 * @ingroup LacSym_DrbgRdrand
 *      Determine support for rdrand execution
 */
CpaStatus
LacDrbg_RdrandIsSupported(void)
{
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    char vendor_id[LAC_SIZE_OF_VENDOR_ID] = {0};
    CpaStatus status = CPA_STATUS_SUCCESS;

    /* Get Vendor ID */
    eax = 0;
    CPUID(eax, &eax, &ebx, &ecx, &edx);

    LAC_DECODE_VND_ID_BYTES(vendor_id+0, ebx);
    LAC_DECODE_VND_ID_BYTES(vendor_id+4, edx);
    LAC_DECODE_VND_ID_BYTES(vendor_id+8, ecx);

    vendor_id[LAC_SIZE_OF_VENDOR_ID-1] = '\0';
    if (!strncmp(vendor_id, "GenuineIntel", LAC_SIZE_OF_VENDOR_ID-1))
    {
        eax = 1;   /* Get Feature Bits */
        CPUID(eax, &eax, &ebx, &ecx, &edx);

        if(CPA_STATUS_SUCCESS != LacDrbg_RdrandIsBitSet(ecx))
        {
            LAC_LOG_ERROR("Vendor = Intel. No RDRAND support.\n");
            status = CPA_STATUS_UNSUPPORTED;
        }
    }
    else
    {
        LAC_LOG_ERROR("Vendor = Non-Intel. No RDRAND support.\n");
        status = CPA_STATUS_UNSUPPORTED;
    }

    return status;
}

/**
 * @ingroup LacSym_DrbgRdrand
 *      Invoke rdrand instruction to generate 32bit/64bit
 *      random bits
 *  Note:
 *      pBytes should not be NULL
 *      Function returns 0 upon success
 */
STATIC LAC_ARCH_UINT
LacDrbg_RdrandGet (LAC_ARCH_UINT *pBytes)
{
    LAC_ARCH_UINT value = 0, flag = 0;
    asm (
         "1:\n"
    #ifdef __x86_64__
         ".byte 0x48             # rex.W 64 bit register prefix\n"
    #endif
         ".byte 0x0f, 0xc7       # rdrand opcode\n"
         ".byte 0xf2             # register rdx/edx\n"
         " cmovc %4,%3           # set count = 0 if succeed"
         "                       #(force last iteration)\n"
         " cmovc %4,%1           # set flag = 0 if succeed(cf=1)\n"
         " sub   $1,%3           # decrement count\n"
         " jnc 1b                # loop again if count was not 0 before sub\n"
         : "=d&" (value), "=a&" (flag)
         : "1" ((LAC_ARCH_UINT)1), "r" ((LAC_ARCH_UINT)LAC_RDRAND_RETRIES),
         "r" ((LAC_ARCH_UINT)0)
       );
    *pBytes = value;
    return flag;
}

/**
 * @ingroup LacSym_DrbgRdrand
 *      Call rdrand and fill the data buffer with
 *      buffers of random bytes specified by numOfRdrandBuffers
 */
STATIC CpaStatus
LacDrbg_RdrandGetBuffers(Cpa32U numOfRdrandBuffers, Cpa8U *pData)
{
    do
    {
        if(CPA_STATUS_SUCCESS != LacDrbg_RdrandGet((LAC_ARCH_UINT*)pData))
        {
            LAC_LOG_ERROR1("Failure to get random after %d retries\n",
                LAC_RDRAND_RETRIES);
            return CPA_STATUS_FAIL;
        }
        pData += LAC_RDRAND_BUFFER_LENGTH;
        numOfRdrandBuffers--;
    }while(numOfRdrandBuffers > 0);

    return CPA_STATUS_SUCCESS;
}

/**
 * @ingroup LacSym_DrbgRdrand
 *      Call rdrand and fill the data buffer with
 *      random bytes specified by lengthInBytes
 */
CpaStatus
LacDrbg_RdrandGetBytes(Cpa32U lengthInBytes, Cpa8U *pData)
{
    Cpa32U numOfBuffers = 0;
    Cpa32U remainingBytes = 0;
    LAC_ARCH_UINT tempData = 0;
    Cpa8U *pRemBytesLocation = NULL;

#ifdef ICP_PARAM_CHECK
    LAC_CHECK_NULL_PARAM(pData);
#endif

    numOfBuffers = lengthInBytes/LAC_RDRAND_BUFFER_LENGTH;
    remainingBytes  = lengthInBytes%LAC_RDRAND_BUFFER_LENGTH;

    if(numOfBuffers)
    {
        if(CPA_STATUS_SUCCESS != LacDrbg_RdrandGetBuffers(numOfBuffers,pData))
        {
            osalMemSet(pData, 0,lengthInBytes);
            return CPA_STATUS_FAIL;
        }
    }

    if(remainingBytes)
    {
        if(CPA_STATUS_SUCCESS != LacDrbg_RdrandGet(&tempData))
        {
            LAC_LOG_ERROR1("Failure to get random after %d retries\n",
                LAC_RDRAND_RETRIES);
            osalMemSet(pData, 0,lengthInBytes);
            return CPA_STATUS_FAIL;
        }
        pRemBytesLocation = pData + (lengthInBytes - remainingBytes);
        osalMemCopy(pRemBytesLocation, &tempData, remainingBytes);
    }

    return CPA_STATUS_SUCCESS;
}

