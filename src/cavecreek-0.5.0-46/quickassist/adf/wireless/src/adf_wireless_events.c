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

/******************************************************************************
 * @file  adf_wireless_events.c
 *
 * @description
 *      This file contains the function definitions for wireless event handlers.
 *
 *****************************************************************************/
#include "cpa.h"
#include "icp_platform.h"
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "adf_cfg.h"
#include "adf_platform.h"
#include "adf_wireless.h"
#include "Osal.h"

/*
 * adf_wirelessEventInit event handler
 * Forward the init message to wireless data plain
 */
CpaStatus adf_wirelessEventInit(icp_accel_dev_t* accel_dev)
{
    CpaStatus stat = CPA_STATUS_SUCCESS;
    ICP_CHECK_FOR_NULL_PARAM(accel_dev);

    stat = adf_wirelessCreateInstance(accel_dev);
    if( CPA_STATUS_SUCCESS != stat )
    {
        ADF_ERROR("Failed to create instances\n");
        return stat;
    }
    return stat;
}


/*
 * adf_wirelessEventShutdown event handler
 * Forward the shutdown message to wireless data plain
 */
CpaStatus adf_wirelessEventShutdown(icp_accel_dev_t* accel_dev)
{
    CpaStatus stat = CPA_STATUS_SUCCESS;
    ICP_CHECK_FOR_NULL_PARAM(accel_dev);
    stat = adf_wirelessDeleteInstance(accel_dev);
    if (stat != CPA_STATUS_SUCCESS)
    {
        ADF_ERROR("adf_wirelessDeleteInstance failed\n");
        return stat;
    }
    return stat;
}

