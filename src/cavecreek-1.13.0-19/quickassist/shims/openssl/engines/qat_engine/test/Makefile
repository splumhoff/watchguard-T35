CFLAGS+=-g -I${OPENSSL_ROOT}/include \
-I${OPENSSL_ROOT}/crypto \
-I${OPENSSL_ROOT} \
-I$(ICP_ROOT)/quickassist/lookaside/access_layer/include \
-I$(ICP_ROOT)/quickassist/include \
-I$(OPENSSL_ROOT)/engines/qat_engine


CC=gcc
#COVERAGE=-fprofile-arcs -ftest-coverage -O3
#COVERAGE=-finstrument-functions -ggdb3 -rdynamic
COVERAGE=-ggdb3 -pg
# COVERAGE=-O3
CFLAGS+=$(COVERAGE) -Wall
SRCS=\
main.c \
tests.c \
tests_md5.c \
tests_sha1.c \
tests_sha256.c \
tests_sha512.c \
tests_des3.c \
tests_des.c \
tests_aes256.c \
tests_aes192.c \
tests_aes128.c \
tests_rc4.c \
tests_rsa.c \
tests_dsa.c \
tests_hmac.c \
tests_dh.c \
tests_aes_cbc_hmac_sha1.c

OBJS = $(SRCS:%.c=%.o)
#COVERAGE_OBJS = $(SRCS:%.c=%.gcno) $(SRCS:%.c=%.gcda)
COVERAGE_OBJS =

LIBQAT=$(ICP_ROOT)/build/icp_qa_al.a -L$(ICP_ROOT)/build -ladf_proxy -losal -lcrypto

testapp: $(OBJS) tests.h ${OPENSSL_ROOT}/libcrypto.a 
	$(CC) -o testapp $(OBJS) $(COVERAGE) -L${OPENSSL_ROOT}/ \
	-lcrypto -lssl -ldl -lrt \
	-lpthread \
	$(LIBQAT)
#	fiber.o vxthread.o \

fiber.o: fiber.c
	$(CC) -c $(CFLAGS) fiber.c

vxthread.o: vxthread.c
	$(CC)  -c $(CFLAGS) vxthread.c

timeswap: timeswap.o
	gcc -o timeswap timeswap.o -lpthread

clean: $(OBJS)
	rm $(OBJS) $(COVERAGE_OBJS) testapp
