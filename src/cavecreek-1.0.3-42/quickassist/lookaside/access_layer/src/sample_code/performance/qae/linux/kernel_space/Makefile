#########################################################################
#  
# @par
#   BSD LICENSE
# 
#   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
#  version: QAT1.7.Upstream.L.1.0.3-42
############################################################################


####################Common variables and definitions########################
# Ensure The ICP_ENV_DIR environmental var is defined.
ICP_ENV_DIR?=$(ICP_ROOT)/quickassist/build_system/build_files/env_files

#Add your project environment Makefile
include $(ICP_ENV_DIR)/environment.mk
ICP_OS?=linux
ICP_OS_LEVEL?=kernel_space


ifeq ($(ICP_OS),linux_2.6)
OS=linux
else
OS=freebsd
endif

PERF_SAMPLE_SRC_ROOT ?= $(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/performance

#include the makefile with all the default and common Make variable definitions
include $(ICP_BUILDSYSTEM_PATH)/build_files/common.mk


#Add the name for the executable, Library or Module output definitions
OUTPUT_NAME=qaeMemDrv


# List of Source Files to be compiled (to be in a single line or on different lines separated by a "\" and tab.

SOURCES:= qae_mem_drv.c
	

ifeq ($(ICP_OS_LEVEL),kernel_space)
#SOURCES += framework/$(OS)/$(ICP_OS_LEVEL)/cpa_sample_code_module.c 
endif



	
#common includes between all supported OSes
INCLUDES= -I$(PERF_SAMPLE_SRC_ROOT) \
	-I$(PERF_SAMPLE_SRC_ROOT)/framework/ \
	-I$(PERF_SAMPLE_SRC_ROOT)/framework/$(OS)/$(ICP_OS_LEVEL) \
	-I$(PERF_SAMPLE_SRC_ROOT)/crypto/ \
	-I$(PERF_SAMPLE_SRC_ROOT)/compression/ \
	-I$(PERF_SAMPLE_SRC_ROOT)/qae/ \
        -I$(LAC_DIR)/include

	
ifeq ($(ICP_OS_LEVEL),user_space)
EXTRA_CFLAGS += -DUSER_SPACE -D_GNU_SOURCE
LD_LIBRARY_PATH?=/usr/lib
ADDITIONAL_OBJECTS += -L$(ICP_BUILD_OUTPUT)/icp_qa_al.a
ADDITIONAL_OBJECTS += -L$(ICP_BUILD_OUTPUT)/libadf_proxy.a
ADDITIONAL_OBJECTS += -L$(ICP_BUILD_OUTPUT)/libosal.a
ADDITIONAL_OBJECTS += -L/Lib -lpthread
else
EXTRA_CFLAGS += -DKERNEL_SPACE 
endif
EXTRA_CFLAGS+=-DSAL_IOMMU_CODE

KBUILD_EXTRA_SYMBOLS += $(PERF_SAMPLE_SRC_ROOT)/../../Module.symvers
export $(KBUILD_EXTRA_SYMBOLS)

#include your $(ICP_OS)_$(ICP_OS_LEVEL).mk file
include $(ICP_ENV_DIR)/$(ICP_OS)_$(ICP_OS_LEVEL).mk


# On the line directly below list the outputs you wish to build for
ifeq ($(ICP_OS_LEVEL),user_space)
install: exe
else
install: module
endif

clean: qae_clean
qae_clean:
	rm -rf $(PERF_SAMPLE_SRC_ROOT)/qae/linux/$(ICP_OS_LEVEL)/build/linux_2.6/$(ICP_OS_LEVEL)/
###################Include rules makefiles########################
include $(ICP_BUILDSYSTEM_PATH)/build_files/rules.mk
###################End of Rules inclusion#########################

