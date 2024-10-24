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
#include "bundle.h"

#include "global.h"
#include <exception>
#include <iostream>
#include <stdexcept>

#include "adf_ctl.h"

bundle::bundle(int nr, int rings_per_bundle)
    : sections(nr_proc_per_bundle)
    , number(nr)
    , type(bundle_type::FREE)
    , polling_mode(-1)
{
    /* Set default affinity to all cores */
    affinity_mask.set();

    sections.clear();
    service_instance* inst;
    /* Static dh895 ring configuration */
    /* Make all instances user for now */
    for (int i = 0; i < 2; i++)
    {
        inst = new crypto_instance();
        inst->tx_rings.push_back(i);
        inst->tx_rings.push_back(i + 2);
        inst->rx_rings.push_back(i + 8);
        inst->rx_rings.push_back(i + 10);
        instances.push_back(inst);
    }
    for (int i = 0; i < 2; i++)
    {
        inst = new compression_instance();
        inst->tx_rings.push_back(i + 6);
        inst->rx_rings.push_back(i + 14);
        instances.push_back(inst);
    }
}

bundle::~bundle()
{
    for (auto inst_itr = instances.begin(); inst_itr != instances.end();
         ++inst_itr)
        delete *inst_itr;
}

bool bundle::is_interrupt_mode()
{
    return bundle_type::KERNEL == this->type
        || config_resp_epoll == this->polling_mode;
}

bool bundle::can_be_shared(const std::string& process_name, int polling_mode)
{
    if (is_free())
        return true;

    /* The bundle cannot be shared if it's already being used for a different
     * polling mode */
    if (this->polling_mode != polling_mode)
        return false;

    /* Non-interrupt bundles can be shared with different processes,
     * while interrupt bundles can only be shared with the same process */
    return (config_resp_poll == this->polling_mode
            || process_name == sections.at(0));
}

bool bundle::is_free() { return bundle_type::FREE == this->type; }

service_instance* bundle::get_free_instance(const service_instance& inst,
                                            const std::string& process_name)
{
    service_instance* ret_instance = NULL;

    /* Check whether we can share this bundle */
    if (can_be_shared(process_name, inst.polling_mode))
    {
        for (auto it = instances.begin(); it != instances.end(); ++it)
        {
            if (inst.stype == (*it)->stype)
            {
                ret_instance = *it;
                instances.erase(it);
                break;
            }
        }
    }
    return ret_instance;
}

int bundle::get_ring_pairs(service_instance& inst,
                           const std::string& process_name,
                           const service_instance* bundle_inst)
{
    /* Return error if this bundle is an interrupt bundle and the instance
     * needs a bundle not using interrupts */
    if (config_resp_poll == inst.polling_mode && is_interrupt_mode())
    {
        throw std::runtime_error(
            "Trying to get ring pairs for a non-interrupt bundle "
            "from an interrupt bundle");
    }

    /* Ensure the ring allocated is the same type (cy/dc) as the instance
     * requested */
    if (inst.stype != bundle_inst->stype)
    {
        throw std::runtime_error(
            "Got an instance of different type (cy/dc) than the "
            "one requested");
    }

    if (kernel_sec != process_name
        && (config_resp_epoll != inst.polling_mode
            && config_resp_poll != inst.polling_mode))
    {
        std::cerr
            << "User instance " << inst.name
            << " needs to be configured "
               "with IsPolled 1 or 2 for poll and epoll mode, respectively"
            << std::endl;
        throw std::runtime_error("Invalid configuration");
    }

    /* Assign this bundle to the section(s)
     * one bundle can be shared by upto 4 userspace processes */
    this->sections.push_back(process_name);
    if (is_free())
    {
        this->polling_mode = inst.polling_mode;
        this->type = (kernel_sec == process_name) ? bundle_type::KERNEL
                                                  : bundle_type::USER;
        /* Only set the affinity to the bundle in case of interrupt instance
         * For a non-interrupt instance, we don't care about the affinity at all
         */
        if (this->is_interrupt_mode())
        {
            this->affinity_mask = inst.affinity_mask;
        }
    }

    for (size_t i = 0; i < bundle_inst->how_many; i++)
    {
        inst.tx_rings.push_back(bundle_inst->tx_rings.at(i));
        inst.rx_rings.push_back(bundle_inst->rx_rings.at(i));
    }

    inst.bundle = this->number;
    delete bundle_inst;

    return 0;
}

const char* bundle::get_instance_type_str(service_instance& inst)
{
    switch (inst.stype)
    {
        case service_type::CRYPTO:
            return "Cy";
            break;
        case service_type::COMP:
            return "Dc";
            break;
    }
    return "";
}
