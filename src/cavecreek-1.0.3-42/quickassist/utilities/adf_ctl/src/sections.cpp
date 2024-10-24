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
#include "sections.h"

#include <boost/lexical_cast.hpp>

#include "bundle.h"
#include "dev_config.h"
#include "device.h"
#include "global.h"
#include "utils.h"

extern dev_config* cfg;

section_general::section_general(const std::string& name)
    : config_section(name)
    , cy_sym_req(0)
    , cy_asym_req(0)
    , dc_req(0)
{
}

section_general::~section_general() {}

void section_general::process_section(int dev)
{
    /* Verify config version
     * and remove the entry from general section */
    int version = atoi(entries[config_ver].c_str());
    if (2 != version)
        throw std::runtime_error("Only version 2 config file supported");
    entries.erase(entries.find(config_ver));

    /* Find default values from concurrent requests
     * and remove the entries from the general section */
    auto val = entries.find(std::string(inst_cy) + inst_cy_sym_req);
    if (val != entries.end())
    {
        cy_sym_req = atoi(val->second.c_str());
        entries.erase(val);
    }
    else
    {
        cy_sym_req = config_default_big_ring_size;
    }
    val = entries.find(std::string(inst_cy) + inst_cy_asym_req);
    if (val != entries.end())
    {
        cy_asym_req = atoi(val->second.c_str());
        entries.erase(val);
    }
    else
    {
        cy_asym_req = config_default_small_ring_size;
    }
    val = entries.find(std::string(num_dc_inst) + inst_dc_req);
    if (val != entries.end())
    {
        dc_req = atoi(val->second.c_str());
        entries.erase(val);
    }
    else
    {
        dc_req = config_default_big_ring_size;
    }
}

section_kernel::section_kernel(const std::string& name)
    : config_section(name)
{
}

section_kernel::~section_kernel() {}

void section_kernel::process_section(int dev)
{
    /* In kernel section just populate rings for instances.
     * Process number is always 0 */
    create_rings_entries_for_cy_instances(0);
    create_rings_entries_for_dc_instances(0);
}

int section_accelerator::accel_num = -1;

section_accelerator::section_accelerator(const std::string& name)
    : config_section(name)
{
    accel_num++;
    accel_inst = accel_num;
    config_section::name += boost::lexical_cast<std::string>(accel_inst);
}

section_accelerator::~section_accelerator() { accel_num--; }

void section_accelerator::process_section(int dev)
{
    /* Find global settings for coalescing. Use defaults if not found */
    int config_accel_coales = config_accel_def_coales;
    int config_accel_coales_timer = config_accel_def_coales_timer;
    int config_accel_coales_num_msg = config_accel_def_coales_num_msg;
    auto coalesc_cfg = cfg->gen->get_entries().find(config_accel_coalesc);
    if (coalesc_cfg != cfg->gen->get_entries().end())
    {
        config_accel_coales = atoi((*coalesc_cfg).second.c_str());
    }
    coalesc_cfg = cfg->gen->get_entries().find(config_accel_coalesc_time);
    if (coalesc_cfg != cfg->gen->get_entries().end())
    {
        config_accel_coales_timer = atoi((*coalesc_cfg).second.c_str());
    }
    coalesc_cfg = cfg->gen->get_entries().find(config_accel_affinity_num_resp);
    if (coalesc_cfg != cfg->gen->get_entries().end())
    {
        config_accel_coales_num_msg = atoi((*coalesc_cfg).second.c_str());
    }
    for (unsigned int i = 0; i < cfg->dev_info->banks_per_accel; i++)
    {
        /* Populate accelerator bank entries */
        std::string accel_name = config_accel_bank;
        accel_name += boost::lexical_cast<std::string>(i);
        std::string cfg_entry = accel_name + config_accel_coalesc;
        entries[cfg_entry]
            = boost::lexical_cast<std::string>(config_accel_coales);
        cfg_entry = accel_name + config_accel_coalesc_time;
        entries[cfg_entry]
            = boost::lexical_cast<std::string>(config_accel_coales_timer);
        cfg_entry = accel_name + config_accel_affinity_num_resp;
        entries[cfg_entry]
            = boost::lexical_cast<std::string>(config_accel_coales_num_msg);
        /* Configure Cpu affinity for bank */
        int cpu = utils::get_single_core_from_affinity(
            cfg->dev->bundles[i]->affinity_mask);
        cfg_entry = accel_name + config_accel_affinity;
        entries[cfg_entry] = boost::lexical_cast<std::string>(cpu);
    }
}
