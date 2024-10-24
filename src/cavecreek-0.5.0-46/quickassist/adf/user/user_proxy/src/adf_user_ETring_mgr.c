/*****************************************************************************
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
/*****************************************************************************
 * @file adf_ETring_mgr.c
 *
 * @description
 * User space implementation of ETring transport functions
 *****************************************************************************/
#include "cpa.h"
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_platform.h"
#include "adf_platform.h"
#include "icp_adf_transport.h"
#include "adf_transport_ctrl.h"
#include "icp_adf_cfg.h"
#include "adf_cfg.h"
#include "adf_dev_ring_ctl.h"


/*
 * Put messages onto the ET Ring
 */
CpaStatus adf_user_put_msgs( adf_dev_ring_handle_t *pRingHandle,
                             Cpa32U *inBuf,
                             Cpa32U bufLen)
{
    icp_accel_dev_t *accel_dev = NULL;
    Cpa32U* targetAddr = NULL;
    Cpa8U* csr_base_addr = NULL;

    ICP_CHECK_FOR_NULL_PARAM(pRingHandle);
    ICP_CHECK_FOR_NULL_PARAM(pRingHandle->accel_dev);

    accel_dev = (icp_accel_dev_t*)pRingHandle->accel_dev;
    csr_base_addr = ((Cpa8U*)accel_dev->virtConfigSpace);

    ICP_MUTEX_LOCK(pRingHandle->user_lock);
    /* Check if there is enough space in the ring */
    if (pRingHandle->space < ICP_ET_RING_MIN_FREE_SPACE)
    {
        /* read hardware for ring information of Head */
        pRingHandle->head = READ_CSR_RING_HEAD(pRingHandle->bank_num,
                                            pRingHandle->ring_num);

        if (pRingHandle->head > pRingHandle->tail)
        {
            pRingHandle->space = pRingHandle->head - pRingHandle->tail;
        }
        else
        {
            pRingHandle->space =
              pRingHandle->ring_size -
              (pRingHandle->tail - pRingHandle->head);
        }
        /* Check again after updating the shadow copy of the head pointer
         * that could be updated be the AE claster hardware in the meantime
         */
        if (unlikely(pRingHandle->space < ICP_ET_RING_MIN_FREE_SPACE))
        {
            ICP_MUTEX_UNLOCK(pRingHandle->user_lock);
            return CPA_STATUS_RETRY;
        }
    }

    /*
     * We have enough space - copy the message to the ring
     */
    targetAddr = (Cpa32U*)
                     (((UARCH_INT) pRingHandle->ring_virt_addr)
                     + pRingHandle->tail);

    icp_adf_memcpy(targetAddr, inBuf);

    /* Update shadow copy values */
    pRingHandle->tail = modulo((pRingHandle->tail + ICP_ET_DEFAULT_MSG_SIZE),
                                 pRingHandle->modulo);
    pRingHandle->space -= ICP_ET_DEFAULT_MSG_SIZE;

    /* and the config space of the device */

    WRITE_CSR_RING_TAIL(pRingHandle->bank_num,
                        pRingHandle->ring_num,
                        pRingHandle->tail);

    ICP_MUTEX_UNLOCK(pRingHandle->user_lock);
    return CPA_STATUS_SUCCESS;
}


/*
 * Notifies the transport handle in question.
 */
CpaStatus adf_user_notify_msgs( adf_dev_ring_handle_t *pRingHandle )
{
    /*
     * Algorithm:
     * (1) Read Ring Tail CSR
     * (2) Invoke CB for each message on the ring
     * (3) Advance Ring Head CSR by amount of data removed from ring
     *
     * Note:
     * - No locking is performed since it is assumed that this function
     *   is being called for a single thread only.
     * - It is assumed that the ring contains at least one message.
     *   N.B. This means that this method should not be used in its current
     *   form for polling.
     * - It is assumed that the size of the ring is an integer multiple
     *   of the size of the messages on the ring.
     */

    Cpa32U *msg = NULL;
    Cpa8U* csr_base_addr = NULL;
    icp_accel_dev_t *accel_dev = NULL;
    Cpa32U empty_stat = 0;

    ICP_CHECK_FOR_NULL_PARAM(pRingHandle);
    ICP_CHECK_FOR_NULL_PARAM(pRingHandle->accel_dev);

    accel_dev = (icp_accel_dev_t*) pRingHandle->accel_dev;
    ICP_CHECK_FOR_NULL_PARAM(accel_dev->virtConfigSpace);
    csr_base_addr = ((Cpa8U*)accel_dev->virtConfigSpace);

    /*
     * Step (1): Read Ring Tail CSR
     */
    pRingHandle->tail = READ_CSR_RING_TAIL(pRingHandle->bank_num,
                                        pRingHandle->ring_num);

    if (unlikely(pRingHandle->head == pRingHandle->tail))
    {
        empty_stat = READ_CSR_E_STAT(pRingHandle->bank_num);
        if(unlikely (empty_stat & ( 1 << pRingHandle->ring_num )))
        {
            return CPA_STATUS_SUCCESS;
        }
    }
    /*
     * Step (2): Invoke callback for each message on the ring.
     */
    do
    {
        /* Setup the pointer to the message on the ring */
        msg = (Cpa32U*)
              (((UARCH_INT)pRingHandle->ring_virt_addr)
               + pRingHandle->head);
        /* Invoke the callback for the message */
        pRingHandle->callback((icp_comms_trans_handle *)pRingHandle, msg);
        /* Advance the head offset and handle wraparound */
        pRingHandle->head = modulo((pRingHandle->head + ICP_ET_DEFAULT_MSG_SIZE),
                                    pRingHandle->modulo);
    } while (pRingHandle->head != pRingHandle->tail);

    /*
     * Step (3): Update the head CSR
     */
    WRITE_CSR_RING_HEAD(pRingHandle->bank_num,
                        pRingHandle->ring_num,
                        pRingHandle->head);

    /* enable interrupts */
    if(pRingHandle->bank_data->timed_coalesc_enabled)
    {
        WRITE_CSR_INT_COL_EN(pRingHandle->bank_data->bank_number,
                          pRingHandle->bank_data->interrupt_mask);

    }
    if(!pRingHandle->bank_data->timed_coalesc_enabled ||
        pRingHandle->bank_data->number_msg_coalesc_enabled)
    {
        WRITE_CSR_INT_EN(pRingHandle->bank_data->bank_number,
                           pRingHandle->bank_data->interrupt_mask);
    }
    return CPA_STATUS_SUCCESS;
}



/*
 * Notify function used for polling. Messages are read until the ring is
 * empty or the response quota has been fulfilled.
 * If the response quota is zero, messages are read until the ring is drained.
 */
CpaStatus adf_user_notify_msgs_poll( adf_dev_ring_handle_t *pRingHandle )
{
    /*
     * Algorithm:
     * (1) Read Ring Tail CSR
     * (2) Invoke CB for each message on the ring
     * (3) Advance Ring Head CSR by amount of data removed from ring
     *
     */
    icp_accel_dev_t *accel_dev=NULL;
    Cpa32U *msg = NULL;
    Cpa32U* csr_base_addr = NULL;
    Cpa32U msg_counter=0, response_quota=ICP_NO_RESPONSE_QUOTA;

    accel_dev = (icp_accel_dev_t*) pRingHandle->accel_dev;
    csr_base_addr = ((Cpa32U*)accel_dev->virtConfigSpace);

    response_quota = pRingHandle->ringResponseQuota;
    if(response_quota == 0)
    {
        response_quota = ICP_NO_RESPONSE_QUOTA;
    }

    /*
     * Step (1): Read Ring Tail CSR
     */
    pRingHandle->tail = READ_CSR_RING_TAIL(pRingHandle->bank_num,
                                           pRingHandle->ring_num);

    /*
     * Step (2): Invoke callback for each message on the ring.
     */
    do
    {
        /* Setup the pointer to the message on the ring */
        msg = (Cpa32U*)
              (((UARCH_INT)pRingHandle->ring_virt_addr)
               + pRingHandle->head);
          /* Invoke the callback for the message */
        pRingHandle->callback((icp_comms_trans_handle *)pRingHandle, msg);
        /* Advance the head offset and handle wraparound */
        pRingHandle->head = modulo((pRingHandle->head + ICP_ET_DEFAULT_MSG_SIZE),
                                    pRingHandle->modulo);
        msg_counter++;

    } while ((pRingHandle->head != pRingHandle->tail) &&
             (msg_counter < response_quota));

    /*
     * Step (3): Update the head CSR
     */
    WRITE_CSR_RING_HEAD(pRingHandle->bank_num,
                        pRingHandle->ring_num,
                        pRingHandle->head);
    return CPA_STATUS_SUCCESS;
}
