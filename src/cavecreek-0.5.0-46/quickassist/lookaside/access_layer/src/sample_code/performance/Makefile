#########################################################################
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
############################################################################


####################Common variables and definitions########################
# Ensure The ICP_ENV_DIR environmental var is defined.
ICP_ENV_DIR?=$(ICP_ROOT)/quickassist/build_system/build_files/env_files
ICP_BUILDSYSTEM_PATH?=$(ICP_ROOT)/quickassist/build_system

#Add your project environment Makefile
include $(ICP_ENV_DIR)/environment.mk
#Lac include paths
include $(LAC_DIR)/common.mk
ICP_OS?=linux
ICP_OS_LEVEL?=kernel_space
ICP_BUILD_OUTPUT?=$(ICP_ROOT)/build
INCLUDE_COMPRESSION?=1

ifeq ($(ICP_OS),linux_2.6)
OS=linux
else
OS=freebsd
endif

PERF_SAMPLE_SRC_ROOT ?= $(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/performance

#include the makefile with all the default and common Make variable definitions
include $(ICP_BUILDSYSTEM_PATH)/build_files/common.mk

#Add the name for the executable, Library or Module output definitions
OUTPUT_NAME=cpa_sample_code
# List of Source Files to be compiled (to be in a single line or on different lines separated by a "\" and tab.

SOURCES:= framework/$(OS)/$(ICP_OS_LEVEL)/cpa_sample_code_utils.c \
	framework/cpa_sample_code_framework.c \
	cpa_sample_code_main.c \
	crypto/cpa_sample_code_crypto_utils.c \
	qae/$(OS)/$(ICP_OS_LEVEL)/qae_mem_utils.c \
	crypto/cpa_sample_code_sym_perf.c \
    crypto/cpa_sample_code_sym_perf_dp.c \
	crypto/cpa_sample_code_rsa_perf.c \
	crypto/cpa_sample_code_dsa_perf.c \
	crypto/cpa_sample_code_ecdsa_perf.c \
	crypto/cpa_sample_code_dh_perf.c

ifeq ($(INCLUDE_COMPRESSION),1)
SOURCES+=  compression/cpa_sample_code_dc_utils.c \
	compression/cpa_sample_code_dc_dp.c \
	compression/cpa_sample_code_dc_2k.c \
	compression/cpa_sample_code_dc_stateful.c \
	compression/cpa_sample_code_zlib.c

EXTRA_CFLAGS += -DINCLUDE_COMPRESSION
endif


ifeq ($(LATENCY_CODE),1)
	EXTRA_CFLAGS += -DLATENCY_CODE=1
endif


ifeq ($(USE_ZLIB),1)
	EXTRA_CFLAGS += -DUSE_ZLIB
endif

ifeq ($(ICP_OS_LEVEL),kernel_space)
SOURCES += framework/$(OS)/$(ICP_OS_LEVEL)/cpa_sample_code_module.c
endif


ifeq ($(NEWDISPLAY),1)
    SOURCES += compression/cpa_sample_code_printdc_utils.c \
        crypto/cpa_sample_code_print_utils.c
    EXTRA_CFLAGS += -DNEWDISPLAY
endif

#common includes between all supported OSes
INCLUDES += -I$(PERF_SAMPLE_SRC_ROOT) \
	-I$(PERF_SAMPLE_SRC_ROOT)/framework/ \
	-I$(PERF_SAMPLE_SRC_ROOT)/framework/$(OS)/$(ICP_OS_LEVEL) \
	-I$(PERF_SAMPLE_SRC_ROOT)/crypto/ \
	-I$(PERF_SAMPLE_SRC_ROOT)/compression/ \
	-I$(PERF_SAMPLE_SRC_ROOT)/qae

ifeq ($(ICP_OS_LEVEL),user_space)
EXTRA_CFLAGS += -DUSER_SPACE -D_GNU_SOURCE
LD_LIBRARY_PATH?=$(ICP_BUILD_OUTPUT)
ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/icp_qa_al_s.so
ADDITIONAL_OBJECTS += -L/Lib -lpthread
ADDITIONAL_OBJECTS += -lcrypto -lz
else
EXTRA_CFLAGS += -DKERNEL_SPACE
KBUILD_EXTRA_SYMBOLS += $(PERF_SAMPLE_SRC_ROOT)/../../Module.symvers
export $KBUILD_EXTRA_SYMBOLS
endif

#include your $(ICP_OS)_$(ICP_OS_LEVEL).mk file
include $(ICP_ENV_DIR)/$(ICP_OS)_$(ICP_OS_LEVEL).mk

# On the line directly below list the outputs you wish to build for
ifeq ($(ICP_OS_LEVEL),user_space)
install: exe
lib: lib_shared
all: lib_shared exe
else
install: module
endif


###################Include rules makefiles########################
include $(ICP_BUILDSYSTEM_PATH)/build_files/rules.mk
###################End of Rules inclusion#########################

