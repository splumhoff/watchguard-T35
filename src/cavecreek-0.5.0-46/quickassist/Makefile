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

# Ensure The ICP_ROOT environmental var is defined.
ifndef ICP_ROOT
$(error ICP_ROOT is undefined. Please set the path \
        "-> setenv ICP_ROOT <path to root of Source Code>")
endif

DATE=$(shell date '+%m_%d_%y')

ifeq ($(ADF_PLATFORM), ACCELDEVVF)
	DRIVER=icp_qa_al_vf.ko
else
	DRIVER=icp_qa_al.ko
endif

#Paths to Top-Level Makefiles for each team####

OSAL_PATH=$(ICP_ROOT)/quickassist/utilities/osal/
HAL_PATH=$(ICP_ROOT)/quickassist/utilities/downloader/
HAL_LIB_PATH=$(ICP_ROOT)/quickassist/utilities/downloader/
QAT_FW_PATH=$(ICP_ROOT)/quickassist/lookaside/firmware/
LAC_PATH=$(ICP_ROOT)/quickassist/lookaside/access_layer/
CONFIG_PATH=$(ICP_ROOT)/quickassist/config/
ICP_TOOLS_TARGET?=accelcomp
ADF_PATH = $(ICP_ROOT)/quickassist/adf/
ICP_BUILD_OUTPUT?=build_$(DATE)
ICP_TOP_ENV=$(ICP_BUILDSYSTEM_PATH)/build_files/env_files/
export ICP_ACCEL_INC=YES

LAC_LIB_DIR=$(LAC_PATH)/build/libs
ALL_TARGETS = clean lac_lib_dir libosal libosal_user hal adf adf_user lac lac_user qat-fw install_scripts

ifndef ICP_NUMA_MEM
ALL_TARGETS += adfvf lacvf
endif

all: $(ALL_TARGETS)
	@echo 'Build Done';

user: lac_lib_dir libosal_user adf_user lac_user

kernel: lac_lib_dir libosal hal adf lac qat-fw install_scripts

install_scripts:
	@echo ; echo 'Copying Install Scripts';
	@cp $(ICP_BUILDSYSTEM_PATH)/build_files/qat_service $(ICP_BUILD_OUTPUT)/;
	@chmod 750 $(ICP_BUILD_OUTPUT)/qat_service;
	@cp $(ICP_BUILDSYSTEM_PATH)/build_files/gige_watchdog_service $(ICP_BUILD_OUTPUT)/;
	@chmod 750 $(ICP_BUILD_OUTPUT)/gige_watchdog_service;
	@cp $(CONFIG_PATH)/dh89xxcc_qa_dev0.conf $(ICP_BUILD_OUTPUT)/;
	@cp $(CONFIG_PATH)/dh89xxcc_qa_dev1.conf $(ICP_BUILD_OUTPUT)/;
	@cp $(CONFIG_PATH)/dh89xxcc_qa_dev0_single_accel.conf $(ICP_BUILD_OUTPUT)/;
	@cp $(CONFIG_PATH)/sxxxx_qa_dev0.conf $(ICP_BUILD_OUTPUT)/;


#libosal needed by hal and adf
libosal: output_dir lac_lib_dir
	@echo ; echo 'Building lib OSAL';
	@cd $(OSAL_PATH)/src && export ONE_KO_RELEASE_PACKAGE=1 && $(MAKE) ICP_ENV_DIR=$(ICP_TOP_ENV) OS_LEVEL=linux ICP_OS_LEVEL=kernel_space lib_kernel
	@cd $(OSAL_PATH)/src  && mkdir -p build/linux_2.6/kernel_space/
	@cd $(OSAL_PATH)/src  && cp ./linux/kernel_space/build/linux_2.6/kernel_space/libosal.a ./build/linux_2.6/kernel_space/
	@cd $(OSAL_PATH)/src  && cp ./linux/kernel_space/build/linux_2.6/kernel_space/libosal.a $(LAC_LIB_DIR)/


#userspace osal library
libosal_user: output_dir lac_lib_dir
	@echo ; echo 'Building OSAL for user space';
	@cd $(OSAL_PATH)/src && $(MAKE) ICP_ENV_DIR=$(ICP_TOP_ENV) OS_LEVEL=linux ICP_OS_LEVEL=user_space lib;\
	echo ; echo 'Copying OSAL library';
	cp $(OSAL_PATH)/src/build/linux_2.6/user_space/libosal.a $(ICP_BUILD_OUTPUT)/;

hal: output_dir libosal
	@echo ; echo 'Building Hal Libs';
	@cd $(HAL_PATH)/Target_CoreLibs/makDefs && $(MAKE) -f coreLibs_kernel.mak ICP_TOOLS_TARGET=$(ICP_TOOLS_TARGET);
	@echo ; echo 'Copying HAL Binary to $(LAC_LIB_DIR)';
	@cp $(HAL_LIB_PATH)/lib_linux_le/$(ICP_TOOLS_TARGET)/icp_ae_loader_kernel.a $(LAC_LIB_DIR)/

adf: output_dir lac_lib_dir libosal hal
	@echo ; echo 'Building ADF';
	@cd $(ADF_PATH) && export ADF_PLATFORM=ACCELDEV && export ICP_ENV_DIR=$(ICP_TOP_ENV) export ONE_KO_RELEASE_PACKAGE=1 && ICP_OS_LEVEL=kernel_space && $(MAKE);
	cp $(ADF_PATH)/build/linux_2.6/libadf.a $(LAC_LIB_DIR)/;

adfvf: output_dir lac_lib_dir libosal
	@echo ; echo 'Building ADFVF';
	@cd $(ADF_PATH) && export ADF_PLATFORM=ACCELDEVVF && export ICP_ENV_DIR=$(ICP_TOP_ENV) export ONE_KO_RELEASE_PACKAGE=1 && ICP_OS_LEVEL=kernel_space && $(MAKE);
	cp $(ADF_PATH)/build/linux_2.6/libadf.a $(LAC_LIB_DIR)/;

adf_user: output_dir lac_lib_dir libosal_user
	@echo ; echo 'Building user ADF';
	@cd $(ADF_PATH) && export ADF_PLATFORM=ACCELDEV && export ICP_ENV_DIR=$(ICP_TOP_ENV) && export ONE_KO_RELEASE_PACKAGE=1 && ICP_OS_LEVEL=user_space && $(MAKE) adf_user;
	cp $(ADF_PATH)/build/linux_2.6/libadf_proxy.a $(ICP_BUILD_OUTPUT)/;
	cp $(ADF_PATH)/build/linux_2.6/adf_ctl $(ICP_BUILD_OUTPUT)/;
	cp $(ADF_PATH)/build/linux_2.6/icp_gige_watchdog $(ICP_BUILD_OUTPUT)/;

lac: output_dir libosal hal adf
	@echo ; echo 'Building LAC';
	@cd $(LAC_PATH) && export ICP_ENV_DIR=$(ICP_TOP_ENV) && export ONE_KO_RELEASE_PACKAGE=1 && cd src && $(MAKE) ICP_OS_LEVEL=kernel_space OS_LEVEL=linux;
	@cp $(LAC_PATH)/src/build/linux_2.6/kernel_space/$(DRIVER) $(ICP_BUILD_OUTPUT)/;

lacvf: output_dir libosal adfvf
	@echo ; echo 'Building LACVF';
	@cd $(LAC_PATH) && export ADF_PLATFORM=ACCELDEVVF && export ICP_ENV_DIR=$(ICP_TOP_ENV) && export ONE_KO_RELEASE_PACKAGE=1 && cd src && $(MAKE) ICP_OS_LEVEL=kernel_space OS_LEVEL=linux;
	@cp $(LAC_PATH)/src/build/linux_2.6/kernel_space/icp_qa_al_vf.ko $(ICP_BUILD_OUTPUT)/;

lac_user: output_dir libosal_user adf_user
	@echo ; echo 'Building LAC in user space';
	@cd $(LAC_PATH) && export ICP_ENV_DIR=$(ICP_TOP_ENV) && export ONE_KO_RELEASE_PACKAGE=1 && cd src && $(MAKE) lib_static ICP_OS_LEVEL=user_space;
	cp $(LAC_PATH)/src/build/linux_2.6/user_space/icp_qa_al.a $(ICP_BUILD_OUTPUT)/;
	@cd $(LAC_PATH) && export ICP_ENV_DIR=$(ICP_TOP_ENV) && export ONE_KO_RELEASE_PACKAGE=1 && cd src && $(MAKE) lib_shared ICP_OS_LEVEL=user_space;
	cp $(LAC_PATH)/src/build/linux_2.6/user_space/icp_qa_al_s.so $(ICP_BUILD_OUTPUT)/;

qat-fw: output_dir
	@echo ; echo 'Copying DH89XXCC QAT-FW Binary';
	@cp -f $(QAT_FW_PATH)/icp_qat_ae.mof $(ICP_BUILD_OUTPUT)/mof_firmware.bin;
	@echo ; echo 'Copying DH89XXCC MMP Binary';
	@cp -f $(QAT_FW_PATH)/icp_qat_pke.mof $(ICP_BUILD_OUTPUT)/mmp_firmware.bin;
	@echo ; echo 'Copying SXXXX QAT-FW Binary';
	@cp -f $(QAT_FW_PATH)/icp_qat_nae.mof $(ICP_BUILD_OUTPUT)/mof_firmware_sxxxx.bin;
	@echo ; echo 'Copying SXXXX MMP Binary';
	@cp -f $(QAT_FW_PATH)/icp_qat_pke.mof $(ICP_BUILD_OUTPUT)/mmp_firmware_sxxxx.bin;

clean:
	@echo ; echo 'Cleaning OSAL';
	@cd $(OSAL_PATH)/src && $(MAKE) ICP_ENV_DIR=$(ICP_TOP_ENV) ICP_OS_LEVEL=kernel_space OS_LEVEL=linux osal_clean;
	@cd $(OSAL_PATH)/src && $(MAKE) ICP_ENV_DIR=$(ICP_TOP_ENV) ICP_OS_LEVEL=user_space OS_LEVEL=linux osal_clean;
	@echo ; echo 'Cleaning LAC';
	@cd $(LAC_PATH) && export ICP_ENV_DIR=$(ICP_TOP_ENV) && cd src && $(MAKE) clean ICP_OS_LEVEL=kernel_space;
	@cd $(LAC_PATH) && export ICP_ENV_DIR=$(ICP_TOP_ENV) && cd src && $(MAKE) clean ICP_OS_LEVEL=user_space;
	@echo ; echo 'Cleaning Hal Libs';
	@cd $(HAL_PATH)/Target_CoreLibs/makDefs/ && export EXTERNAL_INTERFACE=$(ICP_ROOT)/external_interfaces && $(MAKE) SYS=LINUX -f coreLibs_kernel.mak  clean;
	@echo ; echo 'Cleaning ADF';
	@cd $(ADF_PATH) && export ADF_PLATFORM=ACCELDEV && export ICP_ENV_DIR=$(ICP_TOP_ENV) && export ADF_DIR=$(ADF_PATH) && export OSAL_DIR=$(OSAL_PATH) && export HAL_LIB_DIR=$(HAL_LIB_PATH) && $(MAKE) clean
	@echo ; echo 'Cleaning Binaries in Output Dir';
	@rm -rf $(ICP_BUILD_OUTPUT);
	@rm -rf $(LAC_LIB_DIR);
	@echo 'Cleaning done';


output_dir:
	test -d $(ICP_BUILD_OUTPUT) || mkdir -p $(ICP_BUILD_OUTPUT);

lac_lib_dir:
	test -d $(LAC_LIB_DIR) || mkdir -p $(LAC_LIB_DIR);

