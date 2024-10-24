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
#include "utils.h"

#include "adf_cfg_user.h"
#include "config_section.h"
#include <boost/lexical_cast.hpp>
#include <cctype>
#include <climits>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace utils
{

static size_t long_max_str_len = get_max_str_len<long>();

adf_cfg_val_type get_value_type(const std::string& val)
{
    /* Check for hex number */
    if (val.size() >= 3 && val.find_first_of("0x") == 0)
    {
        bool validHexNumber = true;
        for (unsigned i = 2; i < val.size(); ++i)
        {
            if (!isxdigit(val[i]))
            {
                validHexNumber = false;
                break;
            }
        }

        if (validHexNumber)
        {
            /* Only long values are supported for HEX values, so anything longer
             * than that has to be considered as a STR */
            if (val.size() - 2 > 2 * sizeof(long))
            {
                return ADF_STR;
            }

            return ADF_HEX;
        }
    }

    /* Check for decimal number */
    bool validDecNumber = true;
    for (unsigned i = 0; i < val.size(); ++i)
    {
        if (!isdigit(val[i]))
        {
            validDecNumber = false;
            break;
        }
    }
    if (validDecNumber)
    {
        /* Only long values are supported for DEC values, so anything longer
         * than that has to be considered as a STR */
        if (val.size() > long_max_str_len)
        {
            return ADF_STR;
        }

        return ADF_DEC;
    }

    /* If the format doesn't match an hex nor a decimal number,
     * it has to be a string */
    return ADF_STR;
}

int get_single_core_from_affinity(const affinity_mask_t& affinity_mask) throw()
{
    if (affinity_mask.all())
    {
        return ADF_CFG_AFFINITY_WHATEVER;
    }

    for (size_t i = 0; i < affinity_mask.size(); ++i)
    {
        if (affinity_mask.test(i))
        {
            return i;
        }
    }
    throw std::runtime_error("Affinity mask does not contain any bit set");
}

int get_byte_from_affinity(const affinity_mask_t& affinity_mask, int offset)
{
    int res = 0;
    for (int i = 0; i < 8; ++i)
    {
        int bit = affinity_mask.test(8 * offset + i) ? 1 : 0;
        res |= (bit << i);
    }
    return res;
}

std::string
get_hex_string_from_affinity_mask(const affinity_mask_t& affinity_mask)
{
    std::stringstream ss;
    for (ssize_t i = (affinity_mask.size() / 8 - 1); i >= 0; --i)
    {
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
           << get_byte_from_affinity(affinity_mask, i);
    }
    return ss.str();
}

void print_configuration(const adf_user_cfg_ctl_data& dev_data,
                         std::ostream& stream)
{
    adf_user_cfg_section* section = dev_data.config_section;
    while (section != NULL)
    {
        adf_user_cfg_key_val* user_cfg = section->params;
        stream << "[" << section->name << "]\n";

        while (user_cfg != NULL)
        {
            stream << user_cfg->key << " = ";
            char* val = user_cfg->val;
            switch (user_cfg->type)
            {
                case ADF_STR:
                    stream << val << "\n";
                    break;
                case ADF_DEC:
                    stream << boost::lexical_cast<std::string>(*((long*)val))
                           << "\n";
                    break;
                case ADF_HEX:
                    stream << std::hex << std::setfill('0') << std::setw(2)
                           << "\n";
                    break;
            }

            user_cfg = user_cfg->next;
        }

        section = section->next;
    }

    stream.flush();
}

std::string sanitize_string(const std::string& str)
{
    std::string san_str = str.substr(0, str.find_first_of(" #"));
    return san_str;
}

} // namespace utils
