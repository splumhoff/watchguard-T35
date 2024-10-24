/*****************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
 *  version: QAT1.7.Upstream.L.1.0.3-42
 *
 *****************************************************************************/

/*****************************************************************************
 * @file uio_user_arbiter.h
 *
 * @description
 *      This file contains the arbiter related interfaces
 *
 *****************************************************************************/
#ifndef ADF_UIO_USER_ARBITER_H
#define ADF_UIO_USER_ARBITER_H

#include <adf_platform_common.h>
#include <icp_platform.h>

#define ICP_ARB_REG_SLOT                      0x1000
#define ICP_ARB_RINGSRVARBEN_OFFSET_START     0x19C

#define READ_CSR_ARB_RINGSRVARBEN(csr_base_addr, index)                      \
    ICP_ADF_CSR_RD(csr_base_addr, ICP_ARB_RINGSRVARBEN_OFFSET_START +        \
            ICP_ARB_REG_SLOT * index )

#define WRITE_CSR_ARB_RINGSRVARBEN(csr_base_addr, index, value)              \
    ICP_ADF_CSR_WR(csr_base_addr, ICP_ARB_RINGSRVARBEN_OFFSET_START +        \
            ICP_ARB_REG_SLOT * index, value)

static __inline__ void adf_update_ring_arb_enable(adf_dev_ring_handle_t *ring)
{
    int32_t status;
    pthread_mutex_t *mutex =
        *(pthread_mutex_t **)(ring->bank_data->user_bank_lock);

    /* Lock the register to enable/disable arbiter */
    status = pthread_mutex_lock(mutex);
    if(status)
    {
        ADF_ERROR("Failed to lock bank with error %d\n", status);
        return;
    }

    WRITE_CSR_ARB_RINGSRVARBEN(ring->csr_addr, 0,
                               ring->bank_data->ring_mask & 0xFF);
    pthread_mutex_unlock(mutex);
}

#define adf_update_ring_arb_disable adf_update_ring_arb_enable

#endif /*ADF_UIO_USER_ARBITER_H*/
