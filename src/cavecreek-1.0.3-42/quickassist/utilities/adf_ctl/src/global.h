/***************************************************************************
 *
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
 *  version: QAT1.7.Upstream.L.1.0.3-42
 *
 ****************************************************************************/
#ifndef GLOBAL_H
#define GLOBAL_H

#include <bitset>
#include <cstdint>

typedef std::uint32_t u32;

extern "C" {
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif
#include <adf_cfg_user.h>
}

static const char* general_sec = ADF_GENERAL_SEC;
static const char* kernel_sec = ADF_KERNEL_SEC;
static const char* accelerator_sec = ADF_ACCEL_SEC;
static const char* num_cy_inst = ADF_NUM_CY;
static const char* num_dc_inst = ADF_NUM_DC;
static const char* num_process = "NumProcesses";
static const char* limit_dev_access = "LimitDevAccess";
static const char* derived_sec_name = "_INT_";
static const char* derived_sec_name_dev = "_DEV";
static const char* inst_cy = ADF_CY;
static const char* inst_dc = ADF_DC;
static const char* inst_name = "Name";
static const char* inst_is_polled = "IsPolled";
static const char* inst_affinity = ADF_ETRMGR_CORE_AFFINITY;
static const char* inst_bank = ADF_RING_BANK_NUM;
static const char* inst_cy_asym_tx = ADF_RING_ASYM_TX;
static const char* inst_cy_sym_tx = ADF_RING_SYM_TX;
static const char* inst_cy_asym_rx = ADF_RING_ASYM_RX;
static const char* inst_cy_sym_rx = ADF_RING_SYM_RX;
static const char* inst_cy_asym_req = ADF_RING_ASYM_SIZE;
static const char* inst_cy_sym_req = ADF_RING_SYM_SIZE;
static const int nr_cy_ring_pairs = 2;
static const char* inst_dc_rx = ADF_RING_DC_RX;
static const char* inst_dc_tx = ADF_RING_DC_TX;
static const char* inst_dc_req = ADF_RING_DC_SIZE;
static const int nr_dc_ring_pairs = 1;
static const char* config_ver = "ConfigVersion";
static const char* config_accel_bank = "Bank";
static const char* config_accel_coalesc = ADF_ETRMGR_COALESCING_ENABLED;
static const char* config_accel_coalesc_time = ADF_ETRMGR_COALESCE_TIMER;
static const char* config_accel_affinity = ADF_ETRMGR_CORE_AFFINITY;
static const char* config_accel_affinity_num_resp
    = ADF_ETRMGR_COALESCING_MSG_ENABLED;
static const int config_accel_def_coales = 1;
static const int config_accel_def_coales_timer = 10000;
static const int config_accel_def_coales_num_msg = 0;
static const int config_resp_poll = 1;
static const int config_resp_epoll = 2;
static const char* range_separator = "-";
static const char* path_to_config = "/etc/";
static const char* config_extension = ".conf";
static const char* up = "up";
static const char* dw = "down";
static const char* status = "status";
static const char* restart = "restart";
static const char* reset = "reset";
static const char* dev_param_name = "qat_dev";
static const char* config_file_dev_name = "_dev";
static const char* qat_ctl_file = "/dev/qat_adf_ctl";
static const int config_default_big_ring_size = 512;
static const int config_default_small_ring_size = 64;
static const int rings_per_bank = 16;
static const char* first_user_bundle = "FirstUserBundle";
static const int nr_proc_per_bundle = 4;
static const char* adf_uio_name = "UIO_%s_%02d_BUNDLE_%02d";
static const int max_nr_cpus = 256;
typedef std::bitset<max_nr_cpus> affinity_mask_t;

#endif
