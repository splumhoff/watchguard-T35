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
#include "dev_config.h"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sys/ioctl.h>

#include "adf_ctl.h"
#include "bundle.h"
#include "config_section.h"
#include "device.h"
#include "sections.h"
#include "utils.h"

extern int qat_file;
extern dev_config* cfg;

dev_config::dev_config(adf_dev_status_info* dev_info)
    : dev_info(dev_info)
    , config_file(path_to_config)
    , gen(NULL)
{
    int inst = dev_info->instance_id;
    config_file += dev_info->name;
    config_file += config_file_dev_name;
    config_file += boost::lexical_cast<std::string>(inst);
    config_file += config_extension;

    dev = std::unique_ptr<device>(new device(dev_info));
    read();
}

dev_config::~dev_config()
{
    /* Free all configuration */
    for (auto it = sections.begin(); it != sections.end(); ++it)
    {
        delete *it;
    }
    for (auto it = sections_derived.begin(); it != sections_derived.end(); ++it)
    {
        delete *it;
    }
    for (auto it = user_cfg_key_val.begin(); it != user_cfg_key_val.end(); ++it)
    {
        delete *it;
    }
    for (auto it = user_cfg_sections.begin(); it != user_cfg_sections.end();
         ++it)
    {
        delete *it;
    }
}

void dev_config::read()
{
    /* Read the whole config file */
    std::cout << "Processing " << config_file << std::endl;
    boost::property_tree::ini_parser::read_ini(config_file, config);
}

std::string dev_config::validate_key(const std::string& s) throw()
{
    std::string san_str = utils::sanitize_string(s);
    if (san_str.length() < ADF_CFG_MAX_KEY_LEN_IN_BYTES)
        return san_str;
    else
        throw std::runtime_error(san_str + ": entry too long");
}

std::string dev_config::validate_value(const std::string& s) throw()
{
    std::string san_str = utils::sanitize_string(s);
    if (san_str.length() < ADF_CFG_MAX_VAL_LEN_IN_BYTES)
        return san_str.c_str();
    else
        throw std::runtime_error(san_str + ": entry too long");
}

std::string dev_config::validate_section(const std::string& s) throw()
{
    std::string san_str = utils::sanitize_string(s);
    if (san_str.length() < ADF_CFG_MAX_SECTION_LEN_IN_BYTES)
        return san_str.c_str();
    else
        throw std::runtime_error(san_str + ": entry too long");
}

void dev_config::configure_dev()
{
    config_section* s;
    std::string sec_name;
    /* Loop through all sections from the given configuration file
     * and create section instances that will be used later to
     * porcess the configuration */
    BOOST_FOREACH (boost::property_tree::ptree::value_type& section,
                   config.get_child(""))
    {
        sec_name = validate_section(section.first);
        if (sec_name == general_sec)
        {
            /* Check if we already have generel section */
            if (gen)
            {
                /* Note: uniqueness of sections and keys
                 * within a section is guaranteed by boost::ptree.
                 * This check will prevent multiple calls to
                 * configure_dev() on one dev_config instance. */
                throw std::runtime_error(
                    "You can call configure_dev() only once");
            }
            gen = new section_general(sec_name.c_str());
            s = dynamic_cast<config_section*>(gen);
        }
        else if (sec_name == kernel_sec)
        {
            s = new section_kernel(sec_name);
        }
        else
        {
            s = new config_section(sec_name);
        }
        sections.push_back(s);

        /* Loop through all entries in a section and store
         * it in out new created config_section or derived type */
        BOOST_FOREACH (boost::property_tree::ptree::value_type& value,
                       config.get_child(section.first))
        {
            *s += std::make_pair(validate_key(value.first.data()),
                                 validate_value(value.second.data()));
        }
    }

    /* Create accel sections based on device type */
    for (size_t i = 0; i < dev_info->num_logical_accel; i++)
    {
        s = new section_accelerator(accelerator_sec);
        sections.push_back(s);
    }

    /* Now process general section first as the other
     * section may depend on it. */
    try
    {
        gen->process_section();
    }
    catch (std::exception& e)
    {
        std::cerr << "Failed to process GENERAL section " << std::endl;
        throw;
    }
    /* Now process all the other sections */
    for (auto sec_itr = sections.begin(); sec_itr != sections.end(); ++sec_itr)
    {
        auto section = *sec_itr;
        try
        {
            if (section->get_name() != general_sec)
                section->process_section(dev_info->accel_id);
            else
                continue;
        }
        catch (std::exception& e)
        {
            std::cerr << "Failed to process section " << section->get_name()
                      << std::endl;
            throw;
        }
    }
    /* Add an entry marking the first user space bundle */
    *gen += std::make_pair(
        first_user_bundle,
        boost::lexical_cast<std::string>(device::max_kernel_bundle_nr + 1));

    /* Join the real section and derived sections into one vector */
    sections.insert(
        sections.end(), sections_derived.begin(), sections_derived.end());
    sections_derived.erase(sections_derived.begin(), sections_derived.end());
    /* After everything is processed make sure that all sections names are
     * unique
     */
    std::vector<config_section*>::iterator itr = sections.begin();
    for (; itr != sections.end(); ++itr)
    {
        std::vector<config_section*>::iterator itr2
            = std::unique(itr, sections.end(), config_section::has_same_name);
        if (itr2 != sections.end())
        {
            /* We have a duplicate */
            std::string dup_name = "Duplicate section name ";
            dup_name += (*itr2)->get_name();
            throw std::runtime_error(dup_name);
        }
    }

    /* Finally load the configuration to the driver
     * and start the device */
    adf_user_cfg_ctl_data dev_data;
    dev_data.device_id = cfg->dev->dev_id;
    dev_data.config_section = NULL;
    adf_user_cfg_section* section;
    adf_user_cfg_section* section_prev = NULL;
    adf_user_cfg_key_val* key_val;
    adf_user_cfg_key_val* key_val_prev = NULL;

    for (auto sec_itr = sections.begin(); sec_itr != sections.end(); ++sec_itr)
    {
        auto this_section = *sec_itr;
        section = new adf_user_cfg_section;
        /* This is stored for convenience to be deleted later
         * in destructor, same as key_values below */
        user_cfg_sections.push_back(section);
        memset(section, 0, sizeof(adf_user_cfg_section));
        if (dev_data.config_section == NULL)
        {
            dev_data.config_section = section;
        }
        if (section_prev)
            section_prev->next = section;
        section_prev = section;
        strncpy(section->name,
                this_section->get_name().c_str(),
                ADF_CFG_MAX_SECTION_LEN_IN_BYTES);
        key_val_prev = NULL;
        for (auto val_itr = this_section->get_entries().begin();
             val_itr != this_section->get_entries().end();
             ++val_itr)
        {
            auto value = *val_itr;
            key_val = new adf_user_cfg_key_val;
            user_cfg_key_val.push_back(key_val);
            memset(key_val, 0, sizeof(adf_user_cfg_key_val));
            if (section->params == NULL)
            {
                section->params = key_val;
            }
            if (key_val_prev)
                key_val_prev->next = key_val;
            key_val_prev = key_val;
            strncpy(key_val->key,
                    value.first.c_str(),
                    ADF_CFG_MAX_KEY_LEN_IN_BYTES);
            key_val->type = utils::get_value_type(value.second);
            long val = 0;
            std::stringstream ss;
            switch (key_val->type)
            {
                case ADF_STR:
                    strncpy(key_val->val,
                            value.second.c_str(),
                            ADF_CFG_MAX_VAL_LEN_IN_BYTES);
                    break;
                case ADF_DEC:
                    val = boost::lexical_cast<long>(value.second.c_str());
                    memcpy(key_val->val, &val, sizeof(long));
                    break;
                case ADF_HEX:
                    ss << std::hex << value.second;
                    ss >> val;
                    memcpy(key_val->val, &val, sizeof(long));
                    break;
            }
        }
    }

    if (ioctl(qat_file, IOCTL_CONFIG_SYS_RESOURCE_PARAMETERS, &dev_data))
    {
        std::cerr << "Ioctl failed" << std::endl;
        throw std::runtime_error("Failed to load config data to device");
    }
    if (ioctl(qat_file, IOCTL_START_ACCEL_DEV, &dev_data))
    {
        std::cerr << "Ioctl failed" << std::endl;
        throw std::runtime_error("Failed to start device");
    }

    /* finally set IRQ affinity for the device */
    std::fstream irqs;
    irqs.open("/proc/interrupts");
    if (!irqs.is_open())
    {
        std::cerr << "QAT Error: could not open /proc/interrupts file"
                  << std::endl;
        return;
    }
    for (auto bundle_itr = dev->bundles.begin();
         bundle_itr != dev->bundles.end();
         ++bundle_itr)
    {
        auto bundle = *bundle_itr;
        if (bundle->is_free() || !bundle->is_interrupt_mode())
            continue;

        char buffer[128];
        snprintf(buffer,
                 sizeof(buffer),
                 adf_uio_name,
                 dev->name.c_str(),
                 dev->dev_id,
                 bundle->number);
        std::string irq(buffer);
        irq += "_";

        char line[1024];
        while (irqs.good())
        {
            irqs.getline(line, 1024);
            std::string line_str(line);
            line_str += "_";

            if (line_str.find(irq) != std::string::npos)
            {
                int irq_nr = atoi(line);
                std::ofstream irq_file;
                std::string irq_file_name = "/proc/irq/";
                irq_file_name += boost::lexical_cast<std::string>(irq_nr);
                irq_file_name += "/smp_affinity";
                irq_file.open(irq_file_name);
                if (!irq_file.is_open())
                {
                    std::cerr << "QAT Error: could not open " << irq_file_name
                              << " file" << std::endl;
                    irqs.seekg(0, std::ios::beg);
                    break;
                }

                std::string affinity_mask_hex
                    = utils::get_hex_string_from_affinity_mask(
                        bundle->affinity_mask);
                irq_file << affinity_mask_hex;
                irq_file.close();
                if (!irq_file)
                {
                    throw std::runtime_error(
                        "Error setting affinity for bundle "
                        + boost::lexical_cast<std::string>(bundle->number)
                        + " in "
                        + irq_file_name
                        + ": 0x"
                        + affinity_mask_hex);
                }
                irqs.seekg(0, std::ios::beg);
                break;
            }
        }
    }
    irqs.close();
}
