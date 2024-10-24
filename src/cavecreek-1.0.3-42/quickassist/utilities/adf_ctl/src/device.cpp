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
#include "device.h"

#include "bundle.h"
#include <boost/lexical_cast.hpp>
#include <stdexcept>

int device::max_kernel_bundle_nr = -1;

device::device(adf_dev_status_info* dev_info)
    : name(dev_info->name)
    , dev_id((int)dev_info->accel_id)
{
    /* Initialize max_kernel_bundle_nr for each device */
    max_kernel_bundle_nr = -1;
    for (size_t i = 0; i < dev_info->banks_per_accel; i++)
    {
        bundle* b = new bundle(i, rings_per_bank);
        bundles.push_back(b);
    }
}

device::~device()
{
    for (auto bundle_itr = bundles.begin(); bundle_itr != bundles.end();
         bundle_itr++)
    {
        delete *bundle_itr;
    }
}

unsigned int device::get_id() throw() { return dev_id; }

void device::get_ring_pairs(service_instance& inst,
                            const std::string& process_name)
{
    service_instance* free_inst;

    /* If this is a user bundle not using interrupts, try any bundle not
     * using interrupts */
    if (kernel_sec != process_name && config_resp_poll == inst.polling_mode)
    {
        for (auto bundle_itr = bundles.begin(); bundle_itr != bundles.end();
             bundle_itr++)
        {
            auto bundle = *bundle_itr;
            free_inst = bundle->get_free_instance(inst, process_name);
            if (free_inst == NULL)
            {
                continue;
            }

            bundle->get_ring_pairs(inst, process_name, free_inst);
            return;
        }

        /* If this is an interrupt instance: KERNEL inst or EPOLL user inst*/
    }
    else
    {
        bundle_type free_bundle_type;
        if (kernel_sec == process_name)
        {
            free_bundle_type = bundle_type::KERNEL;
        }
        else
        {
            free_bundle_type = bundle_type::USER;
        }

        bundle* first_free_bundle = NULL;

        /* Try to use a previously allocated interrupt bundle with this affinity
         */
        for (auto bundle_itr = bundles.begin(); bundle_itr != bundles.end();
             bundle_itr++)
        {
            auto bundle = *bundle_itr;
            if ((free_bundle_type == bundle->type)
                && (inst.affinity_mask == bundle->affinity_mask))
            {
                free_inst = bundle->get_free_instance(inst, process_name);
                if (free_inst == NULL)
                {
                    continue;
                }

                bundle->get_ring_pairs(inst, process_name, free_inst);
                return;
            }
            else if (first_free_bundle == NULL && bundle->is_free())
            {
                first_free_bundle = bundle;
            }
        }

        /* No previous interrupt bundle found with this affinity. Allocate a new
         * one
         */
        if (first_free_bundle != NULL)
        {
            free_inst
                = first_free_bundle->get_free_instance(inst, process_name);
            first_free_bundle->get_ring_pairs(inst, process_name, free_inst);

            if (bundle_type::KERNEL == free_bundle_type)
            {
                max_kernel_bundle_nr = first_free_bundle->number;
            }
            return;
        }
    }

    throw std::runtime_error("Don't have enough rings for instance " + inst.name
                             + " in process "
                             + process_name);
}
