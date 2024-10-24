#########################################################################
#  
# @par
# This file is provided under a dual BSD/GPLv2 license.  When using or 
#   redistributing this file, you may do so under either license.
# 
#   GPL LICENSE SUMMARY
# 
#   Copyright(c) 2007-2016 Intel Corporation. All rights reserved.
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
# 
#  version: QAT1.5.L.1.13.0-19
############################################################################

#QA API and SAL PATHS
ifndef ICP_ROOT
$(error ICP_ROOT is undefined. Please set the path to the ICP_ROOT)
endif

ICP_API_DIR?=$(ICP_ROOT)/quickassist/include/
ICP_LAC_DIR?=$(ICP_ROOT)/quickassist/lookaside/access_layer/
SAMPLE_PATH?=$(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/functional/
ICP_BUILD_OUTPUT?=$(ICP_ROOT)/build

#ifdef WITH_CMDRV
ifeq ($(WITH_CMDRV),1)
    ifeq ($(WITH_ICP_TARGET),1)
        CMN_ROOT?=$(ICP_ROOT)/quickassist/utilities/libqae_mem/
        CMN_MODULE_NAME?=libqae_mem
    else
        CMN_ROOT?=$(ICP_ROOT)/quickassist/utilities/libusdm_drv/
        CMN_MODULE_NAME?=libusdm_drv
    endif
endif
#endif
CMN_ROOT?=$(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/performance/qae/
CMN_MODULE_NAME?=qaeMemDrv

export CMN_ROOT
export CMN_MODULE_NAME

DO_CRYPTO?=1
ifeq ($(DO_CRYPTO),1)
        EXTRA_CFLAGS+=-DDO_CRYPTO
endif

#include files
INCLUDES += -I$(ICP_API_DIR) \
	-I$(ICP_API_DIR)lac \
	-I$(ICP_API_DIR)dc \
	-I$(ICP_LAC_DIR)include \
	-I$(SAMPLE_PATH)include

#default builds user
ICP_OS_LEVEL?=user_space
OS?=linux
ICP_OS?=linux_2.6
RM=rm -vf
RM-DIR=rm -rfv

ifeq ($(ICP_OS_LEVEL),user_space)
#############################################################
#
# Build user space executible
#
############################################################
ifeq ($(WITH_UPSTREAM),1)
    ifeq ($(WITH_ICP_TARGET),1)
        ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libicp_qa_al_s.so
    else
        ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libqat_s.so
    endif
else
        ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libicp_qa_al_s.so
endif

#ifdef WITH_CMDRV
ifeq ($(WITH_CMDRV),1)
	ADDITIONAL_OBJECTS += $(CMN_ROOT)/$(OS)/build/$(ICP_OS)/user_space/$(CMN_MODULE_NAME).a
endif
#endif

ADDITIONAL_OBJECTS += -L/usr/Lib -lpthread -lcrypto

ifeq ($(WITH_UPSTREAM),1)
        EXTRA_CFLAGS+=-DWITH_UPSTREAM
        ADDITIONAL_OBJECTS += -ludev
endif

USER_INCLUDES= $(INCLUDES)
USER_INCLUDES+= -I$(CMN_ROOT)/
ifeq ($(WITH_CMDRV),1)
	EXTRA_CFLAGS+=-DWITH_CMDRV
else
USER_SOURCE_FILES += $(CMN_ROOT)/$(OS)/user_space/qae_mem_utils.c
endif

default: clean
	gcc -Wall -O1 $(USER_INCLUDES)  -DUSER_SPACE $(EXTRA_CFLAGS) \
	$(USER_SOURCE_FILES) $(ADDITIONAL_OBJECTS) -o $(OUTPUT_NAME)

clean:
	$(RM) *.o $(OUTPUT_NAME)
else
#############################################################
#
# Build kernel space module
#
############################################################
EXTRA_CFLAGS+=$(INCLUDES)
KBUILD_EXTRA_SYMBOLS += $(SAMPLE_PATH)/../../Module.symvers
export $(KBUILD_EXTRA_SYMBOLS)

default: clean
	make -C $(KERNEL_SOURCE_ROOT) M=$(PWD) modules

clean:
	$(RM) *.mod.* *.ko *.o *.a
	$(RM) modules.order Module.symvers .*.*.*
	$(RM-DIR) .tmp_versions
endif
