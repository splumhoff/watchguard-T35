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
ifeq ($(ICP_ROOT),/DH89xxCC_Release)
SAMPLE_PATH=$(ICP_ROOT)/quickassist/lookaside/access_layer/src/sample_code/functional
DC_PATH=$(SAMPLE_PATH)/dc
SYM_PATH=$(SAMPLE_PATH)/sym
ASYM_PATH=$(SAMPLE_PATH)/asym
ICP_API_DIR=$(ICP_ROOT)/quickassist/include/
ICP_LAC_DIR = $(ICP_ROOT)/quickassist/lookaside/access_layer/
endif

ifndef SAMPLE_PATH
$(error SAMPLE_PATH is undefined. Please set the path to your environment makefile \
        "-> setenv SAMPLE_PATH <functional sample code path>")
endif

ifndef DC_PATH
$(error DC_PATH is undefined. Please set the path to your environment makefile \
        "-> setenv DC_PATH <SAMPLE_PATH/dc>")
endif

ifndef SYM_PATH
$(error SYM_PATH is undefined. Please set the path to your environment makefile \
        "-> setenv SYM_PATH <SAMPLE_PATH/sym>")
endif

ifndef ASYM_PATH
$(error ASYM_PATH is undefined. Please set the path to your environment makefile \
        "-> setenv ASYM_PATH <SAMPLE_PATH/asym>")
endif

ifndef ICP_API_DIR
$(error ICP_API_DIR is undefined. Please set the path to the ICP_API_DIR \
        "-> setenv ICP_API_DIR <ICP_ROOT>/quickassist/include/")
endif

ifndef  ICP_LAC_DIR
$(error ICP_LAC_DIR is undefined. Please set the path to the ICP_LAC_DIR \
        "-> setenv ICP_LAC_DIR <ICP_ROOT>/quickassist/lookaside/access_layer/")
endif

SAMPLE_BUILD_OUTPUT=$(SAMPLE_PATH)/build

output_dir:
	test -d $(SAMPLE_BUILD_OUTPUT) || mkdir $(SAMPLE_BUILD_OUTPUT);
	
dc: dc_dp_sample stateless_sample stateful_sample
	@echo ; echo "Build dc component";
dc_dp_sample: output_dir
	@cd $(DC_PATH)/dc_dp_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(DC_PATH)/dc_dp_sample/dc_dp_sample $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(DC_PATH)/dc_dp_sample/dc_dp_sample.ko $(SAMPLE_BUILD_OUTPUT)/;

stateless_sample: output_dir
	@cd $(DC_PATH)/stateless_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(DC_PATH)/stateless_sample/dc_stateless_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(DC_PATH)/stateless_sample/dc_stateless_sample $(SAMPLE_BUILD_OUTPUT)/;
	
stateful_sample: output_dir
	@cd $(DC_PATH)/stateful_sample && $(MAKE) clean ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=user_space
	@cp $(DC_PATH)/stateful_sample/dc_stateful_sample $(SAMPLE_BUILD_OUTPUT)/;

sym: algchaining_sample ccm_sample cipher_sample drbg_sample gcm_sample hash_file_sample hash_sample ipsec_sample nrbg_sample sym_dp_sample
	@echo ; echo "Build sym component";
algchaining_sample: output_dir
	@cd $(SYM_PATH)/alg_chaining_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/alg_chaining_sample/algchaining_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/alg_chaining_sample/algchaining_sample $(SAMPLE_BUILD_OUTPUT)/;
	
ccm_sample: output_dir
	@cd $(SYM_PATH)/ccm_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/ccm_sample/ccm_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/ccm_sample/ccm_sample $(SAMPLE_BUILD_OUTPUT)/;
	
cipher_sample: output_dir
	@cd $(SYM_PATH)/cipher_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/cipher_sample/cipher_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/cipher_sample/cipher_sample $(SAMPLE_BUILD_OUTPUT)/;
	
drbg_sample: output_dir
	@cd $(SYM_PATH)/drbg_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/drbg_sample/drbg_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/drbg_sample/drbg_sample $(SAMPLE_BUILD_OUTPUT)/;
	
gcm_sample: output_dir
	@cd $(SYM_PATH)/gcm_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/gcm_sample/gcm_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/gcm_sample/gcm_sample $(SAMPLE_BUILD_OUTPUT)/;
	
hash_file_sample: output_dir
	@cd $(SYM_PATH)/hash_file_sample && $(MAKE) clean  ICP_OS_LEVEL=user_space&& $(MAKE) ICP_OS_LEVEL=user_space
	@cp $(SYM_PATH)/hash_file_sample/hash_file_sample $(SAMPLE_BUILD_OUTPUT)/;
	
hash_sample: output_dir
	@cd $(SYM_PATH)/hash_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/hash_sample/hash_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/hash_sample/hash_sample $(SAMPLE_BUILD_OUTPUT)/;
	
ipsec_sample: output_dir
	@cd $(SYM_PATH)/ipsec_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/ipsec_sample/ipsec_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/ipsec_sample/ipsec_sample $(SAMPLE_BUILD_OUTPUT)/;
	
nrbg_sample: output_dir
	@cd $(SYM_PATH)/nrbg_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/nrbg_sample/nrbg_sample $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/nrbg_sample/nrbg_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	
#ssl_sample: output_dir
#	@cd $(SYM_PATH)/ssl_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
#	@cp $(SYM_PATH)/ssl_sample/ssl_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
#	@cp $(SYM_PATH)/ssl_sample/ssl_sample $(SAMPLE_BUILD_OUTPUT)/;
	
sym_dp_sample: output_dir
	@cd $(SYM_PATH)/symdp_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(SYM_PATH)/symdp_sample/sym_dp_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(SYM_PATH)/symdp_sample/sym_dp_sample $(SAMPLE_BUILD_OUTPUT)/;

asym: diffie_hellman_sample prime_sample
	@echo ; echo "Build asym component";
diffie_hellman_sample: output_dir
	@cd $(ASYM_PATH)/diffie_hellman_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(ASYM_PATH)/diffie_hellman_sample/dh_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(ASYM_PATH)/diffie_hellman_sample/dh_sample $(SAMPLE_BUILD_OUTPUT)/;

prime_sample: output_dir
	@cd $(ASYM_PATH)/prime_sample && $(MAKE) clean && $(MAKE) ICP_OS_LEVEL=user_space && $(MAKE) ICP_OS_LEVEL=kernel_space
	@cp $(ASYM_PATH)/prime_sample/prime_sample.ko $(SAMPLE_BUILD_OUTPUT)/;
	@cp $(ASYM_PATH)/prime_sample/prime_sample $(SAMPLE_BUILD_OUTPUT)/;
	
all: dc sym asym
