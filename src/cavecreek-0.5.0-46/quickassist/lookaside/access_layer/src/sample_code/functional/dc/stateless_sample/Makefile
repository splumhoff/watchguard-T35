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

#QA API and SAL PATHS
ifeq ($(ICP_ROOT),/DH89xxCC_Release)
ICP_API_DIR=$(ICP_ROOT)/quickassist/include/
ICP_LAC_DIR = $(ICP_ROOT)/quickassist/lookaside/access_layer/
else 
	ifndef ICP_API_DIR
$(error ICP_API_DIR is undefined. Please set the path to the ICP_API_DIR \
        "-> setenv ICP_API_DIR <ICP_ROOT>/quickassist/include/")
endif

ifndef  ICP_LAC_DIR
$(error ICP_LAC_DIR is undefined. Please set the path to the ICP_LAC_DIR \
        "-> setenv ICP_LAC_DIR <ICP_ROOT>/quickassist/lookaside/access_layer/")
endif
endif

#Add the name for the executable, Library or Module output definitions
OUTPUT_NAME=dc_stateless_sample


#include files
INCLUDES += -I$(ICP_API_DIR) \
	-I$(ICP_API_DIR)lac \
	-I$(ICP_API_DIR)dc \
	-I$(ICP_LAC_DIR)include \
	-I$(ICP_LAC_DIR)src/sample_code/functional/include/

#default builds user
ICP_OS_LEVEL?=user_space

ifeq ($(ICP_OS_LEVEL),user_space)
#############################################################
#
# Build user space executible
#
############################################################
ADDITIONAL_OBJECTS += $(ICP_LAC_DIR)src/build/linux_2.6/user_space/icp_qa_al_s.so
ADDITIONAL_OBJECTS += -L/usr/Lib -lpthread -lcrypto -lz

USER_SOURCE_FILES = ../../common/cpa_sample_utils.c cpa_dc_stateless_sample.c
USER_SOURCE_FILES += ../../../performance/qae/linux/user_space/qae_mem_utils.c
USER_SOURCE_FILES += cpa_dc_sample_user.c

USER_INCLUDES= $(INCLUDES)
USER_INCLUDES+= -I$(ICP_LAC_DIR)src/sample_code/performance/qae/

default: clean
	gcc -Wall -O3 $(USER_INCLUDES)  -DUSER_SPACE \
	$(USER_SOURCE_FILES) $(ADDITIONAL_OBJECTS) -o $(OUTPUT_NAME)

clean:
	rm -f *.o $(OUTPUT_NAME)

else
#############################################################
#
# Build kernel space module
#
############################################################
EXTRA_CFLAGS+=$(INCLUDES)
obj-m := $(OUTPUT_NAME).o
$(OUTPUT_NAME)-objs := ../../common/cpa_sample_utils.o \
	cpa_dc_stateless_sample.o cpa_dc_sample_linux_kernel_module.o

default: clean
	make -C $(KERNEL_SOURCE_ROOT) M=$(PWD) modules

clean:
	/bin/rm -rf *.mod.c *.ko *.o *.a
endif

