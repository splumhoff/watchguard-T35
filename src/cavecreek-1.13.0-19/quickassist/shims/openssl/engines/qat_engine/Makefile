#
# Makefile to build the qat_engine outside of the openssl source tree
#
# Requires:
#     ICP_ROOT to point to the top of the ICP source tree
#
#     ICP_BUILD_OUTPUT to point to the directory where the userspace
#     icp libraries live.
#
#     SYSTEM_INCLUDE needs to have the path to the openssl headers
#     if they are not in the default include path.
#
INCLUDES = -Iqat_mem -I$(SYSTEM_INCLUDE)
CFLAGS = $(INCLUDES) -g -fPIC -Wall -O2 

# Include path for Intel QAT API
INCLUDES += -I${ICP_ROOT}/quickassist/include
INCLUDES += -I${ICP_ROOT}/quickassist/include/lac
INCLUDES += -I$(ICP_ROOT)/quickassist/lookaside/access_layer/include

LIBICP = -Wl,$(ICP_BUILD_OUTPUT)/icp_qa_al_s.so

LIBQAT_SRC = e_qat.c qae_mem_utils.c 

LIBQAT_OBJ = $(LIBQAT_SRC:%.c=%.o)

all: libqat.so

libqat.so: $(LIBQAT_OBJ) Makefile
	$(CC) -shared -o libqat.so $(LIBQAT_OBJ) $(LIBICP) -lrt 

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff *.so *.sl *.dll

