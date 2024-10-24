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
##########################################################################
####################################################
# Top-Level Makefiles for the IA code.
####################################################
#Ensure the ICP_BUILDSYSTEM_PATH is set#################
ifndef ICP_BUILDSYSTEM_PATH
$(error ICP_BUILDSYSTEM_PATH is undefined. Please set the path to the top of the build structure \
	"-> setenv ICP_BUILDSYSTEM_PATH <path>")
endif

ICP_OS_LEVEL?=user_space
ICP_OS?=linux_2.6
ICP_CORE?=ia
LAC_DIR?=$(ICP_ROOT)/me_acceleration_layer/access_layer/look_aside_acceleration/
KW_BIN_DIR?=/opt/klocwork8/bin/

#include the common.mk which contains the variables e.g. compiler etc.
include $(ICP_BUILDSYSTEM_PATH)/build_files/common.mk
include $(ICP_BUILDSYSTEM_PATH)/build_files/Core/ia.mk
include $(ICP_BUILDSYSTEM_PATH)/build_files/tools/protexIP.mk
include $(LAC_DIR)/common.mk

#standard rules for making the sub-directories
clean depend install distclean :
	$(LOOP)

#list all the sub-directories that you wish "make" to execute in for the standard rules of "all clean depend install distclean".
SUBDIRS= look_aside_acceleration/src/common look_aside_acceleration/unit_test/
DATE=$(shell date +%d"_"%m"_"%y)
KWORK_DIR?=$(PWD)/tables_dir/LAC_component/
KW_PROJECT_NAME?=$(TEAM_NAME)
DOXYFILE=$(PWD)/look_aside_acceleration/Doxyfile
DOXYGEN_OUTPUT_DIR=/home/`whoami`/public_html/tolapai/security/doc

#############add you component here and the path to its source makefiles if the component name is not the same as the component directory###################
######example#########
########ifeq ($(COMP),<NAME OF THE COMPONENT>)
########COMP_PATH=<PATH TO THE COMPONENT MAKEFILE>
########endif

ifdef COMP

########add components here#############

COMP_PATH= $(shell find . -name "$(COMP)" -type d | grep "src")
UNIT_COMP_PATH= $(shell find . -name "$(COMP)" -type d | grep "unit_test" | grep -v "stubs" )

##########do not edit below this point#############################
ifndef COMP_PATH
$(error COMP_PATH is not defined, the component "$(COMP)" is not in the list)
endif #end of COMP_PATH
endif #end of COMP

ifdef FEATURE
#need to append src on to end of feature
FEATURE_PATH=$(shell find . -name "$(FEATURE)" -type d -maxdepth 2 | grep -v "release")/src
endif #end of FEATURE

###Rule to build everything####################
COMP_LIST=alg_chain sym hash random cipher hash cpm diffie_hellman dsa free_cache ctrl keygen crypto_common mod_exp pke_common ecc random rsa session sym sym_common 
RULES= lib_kernel lib_user unit_test coverage

all:
	@for rule in $(RULES); do \
	for comp in $(COMP_LIST); do\
        (echo ; echo $$rule ;  \
                $(MAKE) $$rule COMP=$$comp || false) \
        done \
	done;
	$(MAKE) lac_kernel_module;
	$(MAKE) LAC-nrbg_sample
	$(MAKE) lac_doc;
######################

###########generic rules for calling sub-directories to build themselves#############
feature_lib_user:
	@echo ;echo 'Building component library $(FEATURE)';  \
	cd $(FEATURE_PATH) && $(MAKE) lib target_lib=$(FEATURE) || (cd $(FEATURE_PATH) && $(MAKE))

feature_lib_kernel:
	@echo ;echo 'Building component library $(FEATURE) for kernel space';  \
    cd $(FEATURE_PATH) && $(MAKE) lib target_lib=$(FEATURE) ICP_OS_LEVEL=kernel_space || (cd $(FEATURE_PATH) && $(MAKE) ICP_OS_LEVEL=kernel_space)

lib_user:
	@echo ;echo 'Building component module $(COMP)';  \
	cd $(COMP_PATH) && $(MAKE)

lib_kernel:
	@echo ;echo 'Building component module $(COMP) for kernel space';  \
    cd $(COMP_PATH) && $(MAKE) ICP_OS_LEVEL=kernel_space

unit_test::
	@echo ;echo 'Building unit test $(COMP)';  \
	cd $(UNIT_COMP_PATH) && $(MAKE) check ICP_OS_LEVEL=user_space OS_LEVEL=user_space

coverage:
	@echo ;echo 'Building Code Coverage $(COMP)';  \
	cd $(UNIT_COMP_PATH) && $(MAKE) coverage OUTPUT_DIR=$(HOME)/public_html/tolapai/security/coverage/$(TEAM_NAME)/$(DATE)/$(COMP)

be_coverage_user:
	@echo ;echo 'Building Bullseye User Space Code Coverage $(COMP)';  \
	cd $(UNIT_COMP_PATH) && $(MAKE) be_coverage_user ICP_OS_LEVEL=user_space OS_LEVEL=user_space

run_coding_standards:
	@echo ;echo 'Running Coding Standards for $(COMP)';  \
	cd $(COMP_PATH) && $(MAKE) run_coding_standards

rtcoverage:
	@echo ;echo 'Building Code Coverage $(COMP) using Rational Test RealTime';  \
	cd $(UNIT_COMP_PATH) && $(MAKE) rtcoverage OUTPUT_DIR=$(HOME)/public_html/tolapai/security/rtcoverage/$(TEAM_NAME)/$(DATE)/$(COMP)

klocwork:
	test -d $(KWORK_DIR) || mkdir -p $(KWORK_DIR);\
    $(KW_BIN_DIR)/kwadmin build-project $(KW_PROJECT_NAME) $(KWORK_DIR)  --copy-sources true --using-build-spec /nfs/sie/disks/kw_sie_project_root/buildspecs/$(KW_PROJECT_NAME).out;\
    rm -rf  $(KWORK_DIR);

doc:
	@echo ;echo 'Building component module $(COMP)';  \
	cd $(COMP_PATH) && ( cat $(DOXYFILE) ; echo "$(DOXYGEN_OUTPUT_DIR)/$(COMP)" ) | doxygen -

lac_doc:
	@echo ;echo 'Building LAC Documentation';  \
	cd $(LAC_DIR)/src && ( cat $(DOXYFILE) ; echo "OUTPUT_DIRECTORY=$(DOXYGEN_OUTPUT_DIR)" ) | /nfs/sie/local/software/tools/doxygen/doxygen  -

LAC-lac_kernel_module:
	@echo ;echo 'Building LAC Kernel Module';  \
	 cd $(LAC_DIR)/src && $(MAKE) target_lib=lac ICP_OS_LEVEL=kernel_space

LAC-perf_kernel_module:
	@echo ;echo 'Building performance code';  \
	cd $(LAC_DIR)/src/sample_code/performance/ && $(MAKE) ICP_OS_LEVEL=kernel_space

LAC-sample_kernel_module:
	@echo ;echo 'Buildings Sample code':
	cd $(LAC_DIR)/src/sample_code/functional/ && \
	cd sym/alg_chaining_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module && cd - && \
	cd sym/cipher_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module && cd - && \
	cd sym/hash_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module && cd - && \
	cd sym/nrbg_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module && cd - && \
	cd asym/diffie_hellman_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module && cd - && \
	cd asym/prime_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module

LAC-nrbg_sample:
	@echo ;echo 'Building nrbg user-space Sample code':
	cd $(LAC_DIR)/src/sample_code/functional/ && \
    cd sym/nrbg_sample && $(MAKE) -f Makefile.user ICP_OS_LEVEL=user_space && cp build/$(ICP_OS)/user_space/nrbg_sample $(ICP_BUILD_OUTPUT) && cd - && \
    cd sym/nrbg_sample && $(MAKE) ICP_OS_LEVEL=kernel_space module && cp build/$(ICP_OS)/kernel_space/nrbg_sample.ko $(ICP_BUILD_OUTPUT) && cd -


lac_production: LAC-lac_kernel_module LAC-perf_kernel_module LAC-sample_kernel_module LAC-nrbg_sample

LAC-perf_coding_standards:
	@echo ;echo 'Running Coding Standards for performance code';  \
	cd $(LAC_DIR)/src/sample_code/performance/ && $(MAKE) run_coding_standards

help:
	@echo '#############################################'
	@echo 'All the available Rules'
	@echo '#############################################'
	@$(MAKE) --print-data-base --question | \
	awk '/^[^.%][-A-Za-z0-9_]*:/ \
	{ print substr($$1, 1, length($$1)-1) }' \
	| sort | \
	pr --omit-pagination --width=80 --column=4
	@echo '#############################################'
	@cat $(ICP_BUILDSYSTEM_PATH)/build_files/help.txt
