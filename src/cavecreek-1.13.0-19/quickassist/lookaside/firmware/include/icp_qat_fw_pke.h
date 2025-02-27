/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or 
 *   redistributing this file, you may do so under either license.
 * 
 *   GPL LICENSE SUMMARY
 * 
 *   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
 * 
 *  version: QAT1.5.L.1.13.0-19
 */

/**
 * @file icp_qat_fw_pke.h
 * @defgroup icp_qat_fw_pke ICP QAT FW PKE Processing Definitions
 * @ingroup icp_qat_fw
 * $Revision$
 * @brief
 *      This file documents the external interfaces that the QAT FW running
 *      on the QAT Acceleration Engine provides to clients wanting to
 *      accelerate crypto assymetric applications
 */


#ifndef __ICP_QAT_FW_PKE__
#define __ICP_QAT_FW_PKE__


/*
****************************************************************************
* Include local header files
****************************************************************************
*/

#include "icp_qat_fw.h"

/**
 ***************************************************************************
 *
 * @ingroup icp_qat_fw_pke
 *
 * @brief
 *      PKE request message structure
 *
 *****************************************************************************/
typedef struct icp_qat_fw_pke_request_s
{
   icp_qat_fw_comn_req_hdr_t comn_hdr;
   /**< Common request header - CD Header/Param size must be zero */

   uint32_t flow_id;
   /** < Field used by Firmware to limit the number of stateful requests 
   * for a session being processed at a given point of time            */

   uint32_t functionalityId;
   /**< MMP functionality Id */

   icp_qat_fw_comn_req_mid_t comn_mid;
   /**< Common interface request fields */

   uint8_t  output_param_count;
   /**< Number of output large integers for request */

   uint8_t  input_param_count;
   /**< Number of input large integers for request */

   uint16_t resrvd;
   /** Reserved - must be zero **/

   uint32_t resrvd1;
   /**< Reserved - must be zero */

   icp_qat_fw_comn_req_ftr_t comn_ftr;
   /**< Common request footer */

} icp_qat_fw_pke_request_t;


/**
 *****************************************************************************
 *
 * @ingroup icp_qat_fw_pke
 *
 * @brief
 *      PKE response message structure
 *
 *****************************************************************************/
typedef struct icp_qat_fw_pke_response_s
{
   icp_qat_fw_comn_resp_hdr_t comn_resp;
   /**< Common response header */

   uint32_t flow_id;
   /**< Flow ID */

   uint32_t functionalityId;
   /**< MMP functionality Id */

   uint64_t input_params;
   /**< 64 bits pointer to a ::icp_qat_fw_mmp_input_param_t structure */

   uint64_t output_params;
   /**< 64 bits pointer to a ::icp_qat_fw_mmp_output_param_t structure */

   uint64_t content_desc_addr;
   /**< 64 bits MMP Library pointer */

   uint8_t  output_param_count;
   /**< Number of output large integers unmodified from request */

   uint8_t  input_param_count;
   /**< Number of input large integers unmodified from request */

   uint16_t resrvd;
   /** Reserved - must be zero **/

   uint32_t resrvd1;
   /**< Reserved - must be zero */

   icp_qat_fw_comn_req_ftr_t comn_ftr;
   /**< Common request footer; unmodifed from request */

} icp_qat_fw_pke_response_t;


#endif /* __ICP_QAT_FW_PKE__ */
