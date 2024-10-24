#
# Top-level Look-Aside Crypto makefile, to kick-off a build for all sub-dirs
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
###############################################################################

# Ensure The ICP_ENV_DIR environmental var is defined.
ifndef ICP_ENV_DIR
$(error ICP_ENV_DIR is undefined. Please set the path to your environment makefile \
        "-> setenv ICP_ENV_DIR <path>")
endif

#Add your project environment Makefile
include $(ICP_ENV_DIR)/environment.mk
#Lac include paths
include $(LAC_DIR)/common.mk
include $(ICP_ENV_DIR)/$(ICP_OS)_$(ICP_OS_LEVEL).mk
##directories
ICP_BUILD_OUTPUT_DIR=build/$(ICP_OS)/$(ICP_OS_LEVEL)
#the folder where the output will be created.
ICP_FINAL_OUTPUT_DIR=$(ICP_BUILD_OUTPUT_DIR)

#<ICP_EXPORT_OSAL_SYMBOLS_FOR_INTEG_TEST>
ifeq ($(ICP_OS_LEVEL), kernel_space)
EXTRA_CFLAGS += -DICP_EXPORT_OSAL_SYMBOLS_FOR_INTEG_TEST
endif

#</ICP_EXPORT_OSAL_SYMBOLS_FOR_INTEG_TEST>
OSAL_PATH=$(ICP_ROOT)/quickassist/utilities/osal/

ifdef ICP_DC_ONLY
EXTRA_CFLAGS += -DICP_DC_ONLY
endif

#############Define the output section###############
BASENAME=qat


ifeq ($(ADF_PLATFORM), ACCELDEVVF)
	EXTRA_CFLAGS += -DACCELDEVVF
	ifeq ($(ICP_OS_LEVEL), user_space)
		OUTPUT_NAME+=lib$(BASENAME)_vf
	endif
else #($(ADF_PLATFORM), ACCELDEVVF)
		OUTPUT_NAME+=lib$(BASENAME)
endif #($(ADF_PLATFORM), ACCELDEVVF)
MODULE_SOURCES=linux/icp_qa_module.c

LIB_STATIC=$(OUTPUT_NAME).a
LIB_SHARED=$(OUTPUT_NAME).so

# add the path and list of source libraries,
ADDITIONAL_KERNEL_LIBS= common/utils/$(ICP_BUILD_OUTPUT_DIR)/utils.a \
			common/ctrl/$(ICP_BUILD_OUTPUT_DIR)/init.a \
                        common/compression/$(ICP_BUILD_OUTPUT_DIR)/compression.a
ifndef ICP_DC_ONLY
ADDITIONAL_KERNEL_LIBS += common/crypto/sym/$(ICP_BUILD_OUTPUT_DIR)/sym.a \
			  common/crypto/sym/qat/$(ICP_BUILD_OUTPUT_DIR)/sym_qat.a \
			  common/crypto/sym/key/$(ICP_BUILD_OUTPUT_DIR)/sym_key.a \
			  common/crypto/asym/pke_common/$(ICP_BUILD_OUTPUT_DIR)/pke_common.a \
			  common/crypto/asym/diffie_hellman/$(ICP_BUILD_OUTPUT_DIR)/diffie_hellman.a \
			  common/crypto/asym/rsa/$(ICP_BUILD_OUTPUT_DIR)/rsa.a \
			  common/crypto/asym/dsa/$(ICP_BUILD_OUTPUT_DIR)/dsa.a \
		  	  common/crypto/asym/prime/$(ICP_BUILD_OUTPUT_DIR)/prime.a \
		  	  common/crypto/asym/large_number/$(ICP_BUILD_OUTPUT_DIR)/ln.a \
			  common/crypto/asym/ecc/$(ICP_BUILD_OUTPUT_DIR)/elliptic_curve.a
endif
ADDITIONAL_KERNEL_LIBS += common/qat_comms/$(ICP_BUILD_OUTPUT_DIR)/qat_comms.a

ifdef KPT
ADDITIONAL_KERNEL_LIBS += common/crypto/kpt/ksp/$(ICP_BUILD_OUTPUT_DIR)/kptksp.a
ADDITIONAL_KERNEL_LIBS += common/crypto/kpt/crypto/$(ICP_BUILD_OUTPUT_DIR)/kptcycomm.a
endif

ifdef ONE_KO_RELEASE_PACKAGE
EXTRA_CFLAGS += -DONE_KO_RELEASE_PACKAGE
ifeq ($(ICP_OS_LEVEL), kernel_space)
    ADDITIONAL_OBJECTS =  ../build/libs/libadf.a
    ADDITIONAL_OBJECTS += ../build/libs/libosal.a
ifneq ($(ADF_PLATFORM), ACCELDEVVF)
    ADDITIONAL_OBJECTS += ../build/libs/icp_ae_loader_kernel.a
endif
endif
endif

ifeq ($(ICP_OS_LEVEL), user_space)
ADDITIONAL_KERNEL_LIBS += user/user.a
endif

ADDITIONAL_LIBS+=$(ICP_BUILD_OUTPUT)/libusdm_drv_s.so

###########End of Define the output section#######################
SUBDIRS=$(patsubst %/$(ICP_BUILD_OUTPUT_DIR)/,%,$(dir $(ADDITIONAL_KERNEL_LIBS)))

LAC-kernel_module:lib_kernel
	@echo 'Creating kernel module'; \
	echo '*** NOTE: Filtering of link-warnings enabled (ixOsal, QatComms) ***';\
	$(MAKE) -C $(KERNEL_SOURCE_ROOT)/ M=$(PWD) 2>&1 | grep \
        -i -v -e "^\*\*\* Warning: \"ixOsal.*\" \[$(PWD)/lac.ko\] undefined\!" \
        -i -v -e "^\*\*\* Warning: \"QatComms.*\" \[$(PWD)/lac.ko\] undefined\!" \
		-i -v -e "^\*\*\* Warning: \"icp_Qatal.*\" \[$(PWD)/lac.ko\] undefined\!"; \
	echo "Copying outputs to $(ICP_BUILD_OUTPUT_DIR)";\
	test -d $(ICP_BUILD_OUTPUT_DIR) || mkdir -p $(ICP_BUILD_OUTPUT_DIR);\
	test -f lib.a  &&  mv lib.a $(ICP_BUILD_OUTPUT_DIR)/$(LIB_STATIC);\
	test -f $(OUTPUT_NAME).ko  &&  mv -f $(OUTPUT_NAME).ko $(ICP_BUILD_OUTPUT_DIR);\
	test -f $(OUTPUT_NAME).o  &&  mv -f *.o $(ICP_BUILD_OUTPUT_DIR);\
	$(RM) -rf *.mod.* .*.cmd;

obj-m := $(OUTPUT_NAME).o
$(OUTPUT_NAME)-objs := $(ADDITIONAL_KERNEL_LIBS) $(ADDITIONAL_OBJECTS) \
		$(patsubst %.c,%.o, $(MODULE_SOURCES) $(ADDITIONAL_OBJECTS))
lib-m := $(ADDITIONAL_KERNEL_LIBS) $(ADDITIONAL_OBJECTS)

lib_kernel::$(SUBDIRS)
lib::$(SUBDIRS)
lib::$(LIB_STATIC)

$(SUBDIRS)::
	echo 'Entering Directory $@' && cd $@ && $(MAKE)
.PHONY: lib

ifeq ($(ICP_OS_LEVEL),kernel_space)
lib_static: $(LIB_STATIC)
dirs:
	@echo 'Creating output directory' ;\
	test -d $(ICP_FINAL_OUTPUT_DIR) || mkdir -p $(ICP_FINAL_OUTPUT_DIR);

$(LIB_STATIC): lib_kernel dirs
	@echo 'Creating static library ${LIB_STATIC}'; \
	$(MAKE) -C $(KERNEL_SOURCE_ROOT)/ M=$(PWD) obj-m=""; \
	echo 'Copying outputs';\
	test -f lib.a  &&  mv lib.a $(ICP_FINAL_OUTPUT_DIR)/$(LIB_STATIC);\
	$(RM) -f *.o *.mod.* .*.cmd;
else
lib_static: lib
	@for  dir in $(SUBDIRS); do \
	(cp $$dir/build/$(ICP_OS)/$(ICP_OS_LEVEL)/*.o $(ICP_BUILD_OUTPUT_DIR) ; ) \
	done ;\
	cd $(ICP_FINAL_OUTPUT_DIR);\
	echo $(ARCHIVER) $(LIB_STATIC_FLAGS) r $(LIB_STATIC) $(ADDITIONAL_OBJECTS) *.o;\
	$(ARCHIVER) $(LIB_STATIC_FLAGS) r $(LIB_STATIC) $(ADDITIONAL_OBJECTS) *.o;

lib_shared: lib
	@for  dir in $(SUBDIRS); do \
	(cp $$dir/build/$(ICP_OS)/$(ICP_OS_LEVEL)/*.o $(ICP_BUILD_OUTPUT_DIR) ; ) \
	done ;\
	cd $(ICP_FINAL_OUTPUT_DIR);\
#	cmd="$(LINKER) $(LIB_SHARED_FLAGS) -o $(LIB_SHARED) -lc $(ADDITIONAL_OBJECTS) $(ADDITIONAL_LIBS) *.o -lpthread -ludev \
	cmd="$(LINKER) $(LIB_SHARED_FLAGS) -o $(LIB_SHARED) -lc $(ADDITIONAL_OBJECTS) $(ADDITIONAL_LIBS) *.o -lpthread \
		-Bstatic -L$(ADF_DIR)/src/build/$(ICP_OS)/$(ICP_OS_LEVEL) -ladf -L$(OSAL_DIR)/src/build/$(ICP_OS)/$(ICP_OS_LEVEL)/ -losal -Bdynamic"; \
	echo "$$cmd"; \
	$$cmd

endif

ifneq ($(ICP_OS_LEVEL),kernel_space)
include $(ICP_BUILDSYSTEM_PATH)/build_files/rules.mk
endif

include $(ICP_BUILDSYSTEM_PATH)/build_files/Core/$(ICP_CORE).mk
include $(ICP_BUILDSYSTEM_PATH)/build_files/OS/$(ICP_OS).mk

.DEFAULT: all

all:
	$(LOOP)

lac_clean:
	@echo 'Removing derived objects...'; \
	$(RM) -rf *.o *.a *.mod.* *.ko .*.cmd; \
	$(RM) -rf .tmp_versions; \
	$(RM) -rf $(ICP_BUILD_OUTPUT_DIR);
	@for dir in  $(SUBDIRS); do \
		(echo ; echo $$dir :; cd $$dir; \
	($(MAKE) clean) || return 1) \
	done
