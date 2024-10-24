###############################################################################
#
# File which sets up the desired environmental variabled for ADF
#
# @par
# This file is provided under a dual BSD/GPLv2 license.  When using or 
#   redistributing this file, you may do so under either license.
# 
#   GPL LICENSE SUMMARY
# 
#   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
# 
#   This program is free software; you can redistribute it and/or modify 
#   it under the terms of version 2 of the GNU General Public License as
#   published by the Free Software Foundation.
# 
#   This program is distributed in the hope that it will be useful, but 
#   WITHOUT ANY WARRANTY; without even the implied warranty of 
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#   General Public License for more details.
# 
#   You should have received a copy of the GNU General Public License 
#   along with this program; if not, write to the Free Software 
#   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#   The full GNU General Public License is included in this distribution 
#   in the file called LICENSE.GPL.
# 
#   Contact Information:
#   Intel Corporation
# 
#   BSD LICENSE 
# 
#   Copyright(c) 2007-2012 Intel Corporation. All rights reserved.
#   All rights reserved.
# 
#   Redistribution and use in source and binary forms, with or without 
#   modification, are permitted provided that the following conditions 
#   are met:
# 
#     * Redistributions of source code must retain the above copyright 
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright 
#       notice, this list of conditions and the following disclaimer in 
#       the documentation and/or other materials provided with the 
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its 
#       contributors may be used to endorse or promote products derived 
#       from this software without specific prior written permission.
# 
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# 
#  version: SXXXX.L.0.5.0-46
#
###############################################################################

#-----------
# ADF Environment.mk
# Sets up overall environment settings to be pulled into the make system
#
#-----------
# API Directories
ICP_ROOT?=/vobs
ICP_BUILDSYSTEM_PATH?=$(ICP_ROOT)/icp_dev/build_system


VOB_PREFIX?=$(shell perl -e 'my @count=@ARGV[0]=~tr/\//\//;for(@i=1; @i[0]<=@count[0]; @i[0]++){print "../";}' $(ICP_ENV_DIR))../

API_DIR=$(ICP_ROOT)/Acceleration/include
ADF_DIR=$(ICP_ROOT)/Acceleration/drivers/icp_adf
HAL_LIB_DIR=$(ICP_ROOT)/Acceleration/library/icp_services/RuntimeTargetLibrary
HAL_OS_DIR=$(ICP_ROOT)/Acceleration/library/icp_services/RuntimeTargetLibrary/include/os/linux
HAL_DIR=$(ICP_ROOT)/Acceleration/library/icp_services/RuntimeTargetLibrary
OSAL_DIR=$(ICP_ROOT)/Acceleration/library/icp_utils/OSAL

ADF_TRANSPORT_DIR=$(ADF_DIR)/transport
ADF_COMMS_DIR=$(ADF_DIR)
ADF_PLATFORM_DIR=$(ADF_DIR)/platform
ADF_DRIVERS_DIR=$(ADF_DIR)/drivers
ADF_ACCEL_MGR_DIR=$(ADF_DIR)/accel_mgr
ADF_ACCELENGINE_DIR=$(ADF_DIR)/accelengine
ADF_CTL_DIR=$(ADF_DIR)/user/config_ctl
ADF_PROCESS_PROXY_DIR=$(ADF_DIR)/user/user_proxy
ICP_ADF_DIR=$(ADF_DIR)

#-----------
# OSAL directories
OSAL_SRCDIR := $(OSAL_DIR)
INCLUDES += $(OSAL_INCLUDES)
#Enable osal spinlocks
EXTRA_CFLAGS+= -DENABLE_SPINLOCK

#-----------
# Linux Kernel
KERNEL_SOURCE_ROOT?=/lib/modules/`uname -r`/build

#-----------
# Build System Settings
OS_LEVEL?=kernel_space
OS?=linux_2.6
ICP_OS_LEVEL?=kernel_space
ICP_OS?=linux_2.6
ICP_CORE?=ia
EXTRA_WARNINGS?=n
INTEL_DEV?=YES
KW_PROJECT_NAME?=ADF
DOXYFILE=$(PWD)/adf/Doxyfile
DOXYGEN_OUTPUT_DIR=/home/`whoami`/public_html/tolapai/security/doc

# Enable checking for NULL params
EXTRA_CFLAGS+=-DICP_PARAM_CHECK

