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
ICP_BUILDSYSTEM_PATH?=$(ICP_ROOT)/quickassist/build_system

#Add your project environment Makefile
include $(ICP_ENV_DIR)/environment.mk
#Lac include paths
include $(LAC_DIR)/common.mk
ICP_OS?=linux
ICP_OS_LEVEL?=kernel_space
ICP_BUILD_OUTPUT?=$(ICP_ROOT)/build
INCLUDE_COMPRESSION=1

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

ifeq ($(ICP_OS),linux_2.6)
OS=linux
else
OS=freebsd
endif

PERF_SAMPLE_SRC_ROOT ?= $(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/performance
SAMPLE_BUILD_OUTPUT?=$(PERF_SAMPLE_SRC_ROOT)/../build
#include the makefile with all the default and common Make variable definitions
include $(ICP_BUILDSYSTEM_PATH)/build_files/common.mk

#Add the name for the executable, Library or Module output definitions
OUTPUT_NAME=cpa_sample_code


# List of Source Files to be compiled (to be in a single line or on different lines separated by a "\" and tab.

DO_CRYPTO?=1

ifeq ($(DO_CRYPTO),1)
EXTRA_CFLAGS += -DDO_CRYPTO
endif

ifeq ($(SC_BNP_ENABLED),1)
        EXTRA_CFLAGS += -DSC_BNP_ENABLED
endif

ifeq ($(SC_CNV_ENABLED),1)
        EXTRA_CFLAGS += -DSC_CNV_ENABLED
endif

ifeq ($(SC_WITH_QAT18),1)
    EXTRA_CFLAGS += -DSC_WITH_QAT18
    EXTRA_CFLAGS += -DSC_WITH_QAT17
endif

SOURCES:= framework/$(OS)/$(ICP_OS_LEVEL)/cpa_sample_code_utils.c \
	framework/cpa_sample_code_framework.c \
	cpa_sample_code_main.c 

ifeq ($(DO_CRYPTO),1)
SOURCES+= crypto/cpa_sample_code_crypto_utils.c \
	crypto/cpa_sample_code_sym_perf.c \
	crypto/cpa_sample_code_sym_perf_dp.c \
	crypto/cpa_sample_code_rsa_perf.c \
	crypto/cpa_sample_code_dsa_perf.c \
	crypto/cpa_sample_code_ecdsa_perf.c \
	crypto/cpa_sample_code_dh_perf.c

ifneq ($(WITH_UPSTREAM),1)
SOURCES+= crypto/cpa_sample_code_nrbg_perf.c
endif

endif

ifeq ($(INCLUDE_COMPRESSION),1)
SOURCES+=  compression/cpa_sample_code_dc_utils.c \
	compression/cpa_sample_code_dc_dp.c \
	compression/cpa_sample_code_dc_2k.c \
	compression/cpa_sample_code_dc_stateful.c \
	compression/cpa_sample_code_zlib.c 
ifeq ($(SC_BNP_ENABLED),1)
SOURCES+=  compression/batch_and_pack/cpa_sample_code_dc_bnp.c
endif

EXTRA_CFLAGS += -DINCLUDE_COMPRESSION
endif


ifeq ($(SC_ENABLE_DYNAMIC_COMPRESSION),1)
EXTRA_CFLAGS += -DSC_ENABLE_DYNAMIC_COMPRESSION
endif

ifeq ($(SC_EPOLL_DISABLED),1)
EXTRA_CFLAGS += -DSC_EPOLL_DISABLED
endif

	
ifeq ($(LATENCY_CODE),1)
	EXTRA_CFLAGS += -DLATENCY_CODE=1
endif

ifeq ($(STV_BUFFER_SIZE_DEBUG),1)
	EXTRA_CFLAGS += -DSTV_BUFFER_SIZE_DEBUG=1
endif

ifeq ($(STV_ADD_TEST_IDS),1)
	EXTRA_CFLAGS += -DSTV_ADD_TEST_IDS=1
endif

ifeq ($(STV_DIRECTION_CONTROL),1)
	EXTRA_CFLAGS += -DSTV_DIRECTION_CONTROL=1
endif


ifeq ($(USE_ZLIB),1)
	EXTRA_CFLAGS += -DUSE_ZLIB
endif

ifeq ($(ICP_OS_LEVEL),kernel_space)
SOURCES += framework/$(OS)/$(ICP_OS_LEVEL)/cpa_sample_code_module.c 
endif

ifeq ($(DO_CRYPTO),1)
SOURCES += crypto/cpa_sample_code_ike_rsa_perf.c 
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
        -I$(PERF_SAMPLE_SRC_ROOT)/compression/batch_and_pack/ \
	-I$(CMN_ROOT)/

ifeq ($(WITH_CMDRV),1)
	EXTRA_CFLAGS+=-DWITH_CMDRV
else
        SOURCES += qae/$(OS)/$(ICP_OS_LEVEL)/qae_mem_utils.c
endif

	
ifeq ($(ICP_OS_LEVEL),user_space)
EXTRA_CFLAGS += -DUSER_SPACE -D_GNU_SOURCE
LD_LIBRARY_PATH?=$(ICP_BUILD_OUTPUT)

ifeq ($(WITH_CPA_MUX),1)
	EXTRA_CFLAGS+=-DWITH_CPA_MUX
	ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libqat_mux_s.so
else
ifeq ($(WITH_UPSTREAM),1)
    ifeq ($(WITH_ICP_TARGET),1)
        ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libicp_qa_al_s.so
    else
        ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libqat_s.so
    endif
else
	ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libicp_qa_al_s.so
endif
endif

#ifdef WITH_CMDRV
ifeq ($(WITH_CMDRV),1)
	ifeq ($(WITH_ICP_TARGET),1)
	ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libqae_mem.a
	else
	ADDITIONAL_OBJECTS += $(ICP_BUILD_OUTPUT)/libusdm_drv.a
	endif
endif
#endif

ADDITIONAL_OBJECTS += -L/Lib -lpthread
ADDITIONAL_OBJECTS += -lcrypto -lrt
ifeq ($(WITH_UPSTREAM),1)
	EXTRA_CFLAGS+=-DWITH_UPSTREAM
	ADDITIONAL_OBJECTS += -ludev
endif
ifeq ($(USE_ZLIB),1)
ADDITIONAL_OBJECTS += -lcrypto -lz
endif
MACHINE?=$(shell uname -m)
ifeq ($(MACHINE), x86_64)
    ifeq ($(ARCH), i386)
            EXTRA_CFLAGS+=-DSAMPLE_KERNEL64_USER32
    endif
    ifeq ($(ARCH), i686)
             EXTRA_CFLAGS+=-DSAMPLE_KERNEL64_USER32
             EXTRA_CFLAGS+=-march=i686
    endif
endif

else
EXTRA_CFLAGS += -DKERNEL_SPACE

#ifdef WITH_CMDRV
ifeq ($(WITH_CMDRV),1)
	EXTRA_LDFLAGS += $(CMN_ROOT)/$(OS)/build/$(ICP_OS)/kernel_space/qae_mem_utils.o
endif
#endif

KBUILD_EXTRA_SYMBOLS += $(PERF_SAMPLE_SRC_ROOT)/../../Module.symvers
export $KBUILD_EXTRA_SYMBOLS
endif
EXTRA_CFLAGS+=-DSAL_IOMMU_CODE

#include your $(ICP_OS)_$(ICP_OS_LEVEL).mk file
include $(ICP_ENV_DIR)/$(ICP_OS)_$(ICP_OS_LEVEL).mk

RM=rm -f

# On the line directly below list the outputs you wish to build for
ifeq ($(ICP_OS_LEVEL),user_space)
install: exe
lib: lib_shared
all: lib_shared exe
else
install: module
endif
.PHONY : clean
clean: perf_clean
perf_clean:
	@echo ; echo "Cleaning performance generated files. $(PERF_SAMPLE_SRC_ROOT) "; 
	$(RM) $(PERF_SAMPLE_SRC_ROOT)/build/$(ICP_OS)/user_space/*;
	$(RM) $(PERF_SAMPLE_SRC_ROOT)/crypto/.*.o.cmd ;
	$(RM) $(PERF_SAMPLE_SRC_ROOT)/compression/.*.o.cmd ;
	$(RM) $(PERF_SAMPLE_SRC_ROOT)/framework/.*.o.cmd ;
	$(RM) $(PERF_SAMPLE_SRC_ROOT)/framework/$(OS)/kernel_space/.*.o.cmd ;


###################Include rules makefiles########################
include $(ICP_BUILDSYSTEM_PATH)/build_files/rules.mk
###################End of Rules inclusion#########################

