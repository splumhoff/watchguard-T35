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
#include "config_section.h"

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <vector>

#include "dev_config.h"
#include "device.h"
#include "global.h"
#include "sections.h"
#include "utils.h"

extern dev_config* cfg;

config_section::config_section(const std::string& name)
    : name(name)
{
}

config_section::~config_section() {}

config_section& config_section::
operator+=(std::pair<std::string, std::string> entry)
{
    entries[entry.first] = entry.second;
    return *this;
}

bool config_section::has_same_name(config_section* a, config_section* b) throw()
{
    return (a->get_name() == b->get_name());
}

std::map<std::string, std::string>& config_section::get_entries() throw()
{
    return entries;
}

std::string config_section::get_name() const throw() { return name; }

void config_section::process_section(int dev)
{
    std::string modified_name;
    int num_processes = 0;
    int limit_dev_acc = 0;
    auto itr = entries.find(num_process);
    if (itr != entries.end())
    {
        num_processes = atoi(itr->second.c_str());
        entries.erase(itr);
    }
    itr = entries.find(limit_dev_access);
    if (itr != entries.end())
    {
        limit_dev_acc = atoi(itr->second.c_str());
        entries.erase(itr);
    }
    /* Derive new config section based on number of processes
        * entry in this section */
    for (int i = 1; i < num_processes; i++)
    {
        if (limit_dev_acc)
        {
            modified_name = name + derived_sec_name_dev
                + boost::lexical_cast<std::string>(dev) + derived_sec_name
                + boost::lexical_cast<std::string>(i);
        }
        else
        {
            modified_name
                = name + derived_sec_name + boost::lexical_cast<std::string>(i);
        }
        config_section* s = new config_section(modified_name);
        s->entries.insert(entries.begin(), entries.end());
        cfg->sections_derived.push_back(s);
        /* Process new created sestions */
        s->process_derived(i);
    }

    const char* limit_dev_acc_str;
    if (limit_dev_acc)
    {
        limit_dev_acc_str = "1";
        modified_name = name + derived_sec_name_dev
            + boost::lexical_cast<std::string>(dev) + derived_sec_name;
    }
    else
    {
        modified_name = name + derived_sec_name;
        limit_dev_acc_str = "0";
    }
    modified_name += "0";
    /* Keep the original section with limit_dev_access
        * entry for later */
    config_section* s = new config_section(name);
    *s += std::make_pair(limit_dev_access, limit_dev_acc_str);
    cfg->sections_derived.push_back(s);

    name = modified_name;
    if (num_processes > 0)
    {
        /* Populate ring entries for this section */
        create_rings_entries_for_cy_instances(0);
        create_rings_entries_for_dc_instances(0);
    }
}

void config_section::populate_cpu_list_from_value(
    std::vector<unsigned int>& cpus,
    const std::string& value)
{
    unsigned long affinity_value = std::stoul(value);

    if (!cpus.empty() && affinity_value <= cpus.back())
        throw std::runtime_error(
            "Affinity entry invalid: Value smaller than previous");

    if (affinity_value > 255)
        throw std::runtime_error(
            "Invalid instance affinity entry. Value out of range.");

    cpus.push_back(affinity_value);
}

void config_section::populate_cpu_list_from_range(
    std::vector<unsigned int>& cpus,
    const std::string& cpu_range)
{
    unsigned int start = atoi(cpu_range.c_str());
    int index = cpu_range.find(range_separator);
    unsigned int end = atoi(cpu_range.c_str() + index + 1);

    if (start >= end || start > 256 || end > 256)
        throw std::runtime_error("Invalid instance affinity entry");

    if (!cpus.empty() && start <= cpus.back())
        throw std::runtime_error(
            "Affinity entry invalid: Value smaller than previous");

    for (unsigned int i = start; i < end + 1; i++)
        cpus.push_back(i);
}

void config_section::populate_cpu_list(std::vector<unsigned int>& cpus,
                                       const std::string& cpu_list)
{
    /*
        * Supported formats are
        * N
        * N,M with N < M
        * N-M,X-Y with N < M < X < Y
        * N-M,X-Y,W,Z with N < M < X < Y < W < Z
        * Constraint: all numbers must be greater than the previous.
        * Any other formats are considered malformatted and will be discarded
        */
    boost::char_separator<char> sep(",");
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    tokenizer tokens(cpu_list, sep);

    for (tokenizer::iterator tok_iter = tokens.begin();
         tok_iter != tokens.end();
         ++tok_iter)
    {

        if ((*tok_iter).find_first_of(range_separator) != std::string::npos)
            populate_cpu_list_from_range(cpus, *tok_iter);
        else
            populate_cpu_list_from_value(cpus, *tok_iter);
    }
}

void config_section::populate_service_instance(const std::string& name,
                                               service_instance& inst)
{
    inst.name = entries[name + inst_name];
    inst.polling_mode
        = boost::lexical_cast<int>(entries[name + inst_is_polled]);
    inst.affinity_mask.reset();
    inst.affinity_mask.set(
        boost::lexical_cast<int>(entries[name + inst_affinity]));
}

void config_section::process_derived(int process_num)
{
    /* Populate ring entries for this derived section */
    create_rings_entries_for_cy_instances(process_num);
    create_rings_entries_for_dc_instances(process_num);
}

int config_section::get_core_number_for_instance(std::string& inst_name,
                                                 int process_num)
{
    /* Instance Core number is configured as a list:
     * Cy0CoreAffinity = 4,6,8
     * and the actuall number for an instance is derived base
     * on process_number. */
    std::string core_str = inst_name + inst_affinity;
    std::string core_val = entries[core_str];
    std::vector<int> core_list;

    boost::char_separator<char> sep(",");
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    tokenizer tokens(core_val, sep);

    for (tokenizer::iterator tok_iter = tokens.begin();
         tok_iter != tokens.end();
         ++tok_iter)
    {

        if ((*tok_iter).find_first_of(range_separator) != std::string::npos)
        {

            unsigned int start = atoi((*tok_iter).c_str());
            int index = (*tok_iter).find(range_separator);
            unsigned int end = atoi((*tok_iter).c_str() + index + 1);

            for (unsigned int i = start; i < end + 1; i++)
                core_list.push_back(i);
        }
        else
        {
            core_list.push_back(stoul((*tok_iter)));
        }
    }

    if (core_list.empty())
    {
        throw std::runtime_error("Not defined any affinity for instance "
                                 + inst_name);
    }
    int core_num = core_list.at(process_num % core_list.size());
    return core_num;
}

void config_section::create_rings_entries_for_dc_instances(int process_num)
{
    int nr_inst = 0;
    /* Find number of Dc instances entry */
    auto pos = entries.find(num_dc_inst);
    if (pos != entries.end())
        nr_inst = atoi((*pos).second.c_str());
    for (int x = 0; x < nr_inst; x++)
    {
        /* Set the common part of the instance name  Dc<inst> */
        std::string inst = inst_dc + boost::lexical_cast<std::string>(x);
        /* Find out the acctual core affinity
         * for this process instance and update configuration */
        int core_num = get_core_number_for_instance(inst, process_num);
        entries[inst + inst_affinity]
            = boost::lexical_cast<std::string>(core_num);
        compression_instance dc_inst;
        /* Read instance parameters and set the instance with them */
        populate_service_instance(std::string(inst_dc)
                                      + boost::lexical_cast<std::string>(x),
                                  dc_inst);
        /* Get rings for this instance */
        cfg->dev->get_ring_pairs(dc_inst, name);
        /* Add conf entry for cd bank number */
        std::string ring_cfg = inst + inst_bank;
        entries[ring_cfg] = boost::lexical_cast<std::string>(dc_inst.bundle);
        /* Add conf entry for dc tx ring number */
        ring_cfg = inst + inst_dc_tx;
        entries[ring_cfg]
            = boost::lexical_cast<std::string>(dc_inst.tx_rings[0]);
        /* Add conf entry for dc rx ring number */
        ring_cfg = inst + inst_dc_rx;
        entries[ring_cfg]
            = boost::lexical_cast<std::string>(dc_inst.rx_rings[0]);
        /* Add number of concurrent requests based on global
         * config in general sectioin or local section entry */
        ring_cfg = inst + inst_dc_req;
        int requests = atoi(entries[ring_cfg].c_str());
        if (!requests)
            requests = cfg->gen->dc_req;
        entries[ring_cfg] = boost::lexical_cast<std::string>(requests);
    }
}

void config_section::create_rings_entries_for_cy_instances(int process_num)
{
    int nr_inst = 0;
    /* Find number of Cy instances entry */
    auto pos = entries.find(num_cy_inst);
    if (pos != entries.end())
        nr_inst = atoi((*pos).second.c_str());
    for (int x = 0; x < nr_inst; x++)
    {
        std::string inst = inst_cy + boost::lexical_cast<std::string>(x);
        /* Find out the acctual core affinity
         * for this process instance and update configuration */
        int core_num = get_core_number_for_instance(inst, process_num);
        entries[inst + inst_affinity]
            = boost::lexical_cast<std::string>(core_num);
        crypto_instance cy_inst;
        /* Read instance parameters and set the instance with them */
        populate_service_instance(std::string(inst_cy)
                                      + boost::lexical_cast<std::string>(x),
                                  cy_inst);
        /* Get rings for this instance */
        cfg->dev->get_ring_pairs(cy_inst, name);
        /* Add conf entry for cy bank number */
        std::string ring_cfg = inst + inst_bank;
        entries[ring_cfg] = boost::lexical_cast<std::string>(cy_inst.bundle);
        /* Add conf entry for cy sym tx hi ring number */
        ring_cfg = inst + inst_cy_asym_tx;
        entries[ring_cfg]
            = boost::lexical_cast<std::string>(cy_inst.tx_rings[0]);
        /* Add conf entry for cy sym tx lo ring number */
        ring_cfg = inst + inst_cy_sym_tx;
        entries[ring_cfg]
            = boost::lexical_cast<std::string>(cy_inst.tx_rings[1]);
        /* Add conf entry for cy sym rx hi ring number */
        ring_cfg = inst + inst_cy_asym_rx;
        entries[ring_cfg]
            = boost::lexical_cast<std::string>(cy_inst.rx_rings[0]);
        /* Add conf entry for cy sym rx lo ring number */
        ring_cfg = inst + inst_cy_sym_rx;
        entries[ring_cfg]
            = boost::lexical_cast<std::string>(cy_inst.rx_rings[1]);
        /* Add number of sym concurrent requests based on global
         * config in general sectioin or local section entry */
        ring_cfg = inst + inst_cy_sym_req;
        int requests = atoi(entries[ring_cfg].c_str());
        if (!requests)
            requests = cfg->gen->cy_sym_req;
        entries[ring_cfg] = boost::lexical_cast<std::string>(requests);
        /* Add number of asym concurrent requests based on global
         * config in general sectioin or local section entry */
        ring_cfg = inst + inst_cy_asym_req;
        requests = atoi(entries[ring_cfg].c_str());
        if (!requests)
            requests = cfg->gen->cy_asym_req;
        entries[ring_cfg] = boost::lexical_cast<std::string>(requests);
    }
}
