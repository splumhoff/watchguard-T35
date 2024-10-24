################################################################
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
################################################################
SAMPLE_PATH ?= $(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code
PERF_PATH ?= $(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/performance
FIPS_PATH?=$(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/fips

#include the makefile with all the default and common Make variable definitions
include $(ICP_BUILDSYSTEM_PATH)/build_files/common.mk
KERNEL_SOURCE_ROOT?=/usr/src/kernels/`uname -r`-`uname -m`
PERF_OUTPUT_NAME=cpa_sample_code


SAMPLE_BUILD_OUTPUT?=$(SAMPLE_PATH)/build

output_dir:
	test -d $(SAMPLE_BUILD_OUTPUT) || mkdir $(SAMPLE_BUILD_OUTPUT);
fips_code:
	@if test -d $(FIPS_PATH); then echo ; echo 'Building FIPS Code'; fi;
	@if test -d $(FIPS_PATH); \
        then cd $(FIPS_PATH) && \
        make OS_LEVEL=linux\
         TEAM_NAME=$(TEAM_NAME) \
         ICP_ROOT=$(ICP_ROOT) \
         ICP_OS=$(ICP_OS) \
         ICP_OS_LEVEL=$(ICP_OS_LEVEL) \
         KERNEL_SOURCE_ROOT=$(KERNEL_SOURCE_ROOT) \
         ICP_ENV_DIR=$(ICP_ENV_DIR); \
     fi;

fips_user_code:
	@if test -d $(FIPS_PATH); then echo ; echo 'Building FIPS Code'; fi;
	@if test -d $(FIPS_PATH); \
        then cd $(FIPS_PATH) && \
        make OS_LEVEL=linux\
         TEAM_NAME=$(TEAM_NAME) \
         ICP_ROOT=$(ICP_ROOT) \
         ICP_OS=$(ICP_OS) \
         ICP_OS_LEVEL=user_space \
         KERNEL_SOURCE_ROOT=$(KERNEL_SOURCE_ROOT) \
         ICP_ENV_DIR=$(ICP_ENV_DIR); \
     fi;

ifeq ($(LATENCY_CODE),1)
	EXTRA_CFLAGS += -DLATENCY_CODE
endif
ifeq ($(USE_ZLIB),1)
	EXTRA_CFLAGS += -DUSE_ZLIB
endif

performance_code: perf_kernel perf_qae
#	@echo ; echo 'Building Performance Code';
#	@cd $(PERF_PATH) && export ICP_ENV_DIR=$(ICP_ENV_DIR) && \
#        make \
#         TEAM_NAME=$(TEAM_NAME) \
#         ICP_ROOT=$(ICP_ROOT) \
#         ICP_OS=$(ICP_OS) \
#         ICP_OS_LEVEL=user_space \
#         KERNEL_SOURCE_ROOT=$(KERNEL_SOURCE_ROOT) \
#         ICP_ENV_DIR=$(ICP_ENV_DIR); \


perf_all: perf_user perf_kernel perf_qae test 
test:
	@echo ; echo "Build output location $(SAMPLE_BUILD_OUTPUT)";
perf_user: output_dir
	@cd $(PERF_PATH) && $(MAKE) clean ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=user_space lib_shared exe
	@cp $(PERF_PATH)/build/linux_2.6/user_space/$(PERF_OUTPUT_NAME)_s.so $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(PERF_PATH)/build/linux_2.6/user_space/$(PERF_OUTPUT_NAME) $(SAMPLE_BUILD_OUTPUT)/;

perf_kernel: output_dir
	@cd $(PERF_PATH) && $(MAKE)  ICP_OS_LEVEL=kernel_space 
	@cp $(PERF_PATH)/build/linux_2.6/$(ICP_OS_LEVEL)/$(PERF_OUTPUT_NAME).ko $(SAMPLE_BUILD_OUTPUT)/;
	
perf_qae: output_dir
	@cd $(PERF_PATH)/qae/linux/kernel_space && $(MAKE)  ICP_OS_LEVEL=kernel_space 
	@cp $(PERF_PATH)/qae/linux/kernel_space/build/linux_2.6/$(ICP_OS_LEVEL)/qaeMemDrv.ko $(SAMPLE_BUILD_OUTPUT)/;


func: output_dir
	@cd $(FUNC_PATH) && $(MAKE) all


all: fips_code perf_user perf_kernel perf_qae func
perf: perf_kernel perf_qae perf_user

clean:
	@cd $(FIPS_PATH) && \
        make \
         TEAM_NAME=$(TEAM_NAME) \
         ICP_OS=$(ICP_OS) \
         ICP_OS_LEVEL=$(ICP_OS_LEVEL) \
         ICP_ROOT=$(ICP_ROOT) \
         KERNEL_SOURCE_ROOT=$(KERNEL_SOURCE_ROOT) \
         ICP_ENV_DIR=$(ICP_ENV_DIR) \
        clean;
	@cd $(PERF_PATH) && \
        make \
         TEAM_NAME=$(TEAM_NAME) \
         ICP_OS=$(ICP_OS) \
         ICP_OS_LEVEL=$(ICP_OS_LEVEL) \
         ICP_ROOT=$(ICP_ROOT) \
         KERNEL_SOURCE_ROOT=$(KERNEL_SOURCE_ROOT) \
         ICP_ENV_DIR=$(ICP_ENV_DIR) \
        clean;

