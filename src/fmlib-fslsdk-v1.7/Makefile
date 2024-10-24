#==============================================================================
# Copyright 2010-2012 Freescale Semiconductor, Inc
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#       * Redistributions of source code must retain the above copyright
# 	notice, this list of conditions and the following disclaimer.
#       * Redistributions in binary form must reproduce the above copyright
# 	notice, this list of conditions and the following disclaimer in the
# 	documentation and/or other materials provided with the distribution.
#       * Neither the name of Freescale Semiconductor nor the
# 	names of its contributors may be used to endorse or promote products
# 	derived from this software without specific prior written permission.
#
#
# ALTERNATIVELY, this software may be distributed under the terms of the
# GNU General Public License ("GPL") as published by the Free Software
# Foundation, either version 2 of that License or (at your option) any
# later version.
#
# This software is provided by Freescale Semiconductor "as is" and any
# express or implied warranties, including, but not limited to, the implied
# warranties of merchantability and fitness for a particular purpose are
# disclaimed. In no event shall Freescale Semiconductor be liable for any
# direct, indirect, incidental, special, exemplary, or consequential damages
# (including, but not limited to, procurement of substitute goods or services;
# loss of use, data, or profits; or business interruption) however caused and
# on any theory of liability, whether in contract, strict liability, or tort
# (including negligence or otherwise) arising in any way out of the use of
# this software, even if advised of the possibility of such damage.
#==============================================================================

# WARNING!
# Although invoking the all, archive targets in this Makefile from a Yocto-like
# environment may succeed, you will end up with N different libraries that are
# actually being built using the same compiler flags, i.e. the CFLAGS from the
# environment. That is because Yocto builds the fm-lib for various platforms
# one at a time, by invoking only the corresponding libfm-<arch>.a, then
# install-<arch> targets for each platform that it's currently configured for.

# Set this as appropriate for local builds
CROSS_COMPILE?=powerpc-linux-gnu-

# You must set KERNEL_SRC or the <DESTDIR, PREFIX> pair
# before invoking this Makefile as standalone
KERNEL_SRC?=$(DESTDIR)$(PREFIX)

# These should be inherited from the environment with which Yocto invokes us,
# but in case not, then here we're making sure that we're actually using
# the cross-tools, and NOT the native ones:
ifeq ($(CC),cc)
CC=$(CROSS_COMPILE)gcc
endif

ifeq ($(AR),ar)
AR=$(CROSS_COMPILE)ar
endif

INSTALL?=install

LIB_DEST_DIR?=$(PREFIX)/lib

# Add any collateral files that need to be included in the archive, here:
FM_LIB_DOCFILES=COPYING INSTALL README

# Don't touch these!
FM_LIB_YOCTO_STAGING=$(KERNEL_SRC)/include/linux/fmd
FM_IOCTL_INC?=$(KERNEL_SRC)/include/uapi/linux/fmd
FM_LIB_INC?=./include/fmd
FM_LIB_SRCDIR=./src
FM_LIB_INCLUDE:=$(FM_IOCTL_INC) \
		$(FM_IOCTL_INC)/Peripherals \
		$(FM_IOCTL_INC)/integrations \
		$(FM_LIB_INC) \
		$(FM_LIB_INC)/Peripherals \
		$(FM_LIB_INC)/integrations \
		$(FM_LIB_YOCTO_STAGING) \
		$(FM_LIB_YOCTO_STAGING)/Peripherals \
		$(FM_LIB_YOCTO_STAGING)/integrations

# These flags need to be passed to the compiler in any circumstance:
EXTRA_CFLAGS=-DNCSW_LINUX -fPIC -shared -mlongcall

# CFLAGS to use during out-of-Yocto (i.e. "local") build:
LOCAL_CFLAGS=-O2 -g0 -Wall \
	     -fexpensive-optimizations -frename-registers \
	     -fomit-frame-pointer -maix-struct-return \
	     -D__STDC_LIMIT_MACROS $(EXTRA_CFLAGS)


%.a: %.o
	@(echo "(AR) $(@)")
	@($(AR) rcsv $@ $^)

libfm-ppce500mc.o: EXTRA_CFLAGS+=-DP4080
libfm-ppc32e5500.o libfm-ppc64e5500.o: EXTRA_CFLAGS+=-DP5020
libfm-ppc32e6500.o libfm-ppc64e6500.o: EXTRA_CFLAGS+=-DB4860
libfm-ppc32e5500-fmv3.o libfm-ppc64e5500-fmv3.o: EXTRA_CFLAGS+=-DB4860
libfm-ppce500v2.o: EXTRA_CFLAGS+=-DP1023

libfm-ppc32e5500-fmv3.o: CFLAGS?=-m32 -mhard-float -mcpu=e5500  $(LOCAL_CFLAGS)
libfm-ppc64e5500-fmv3.o: CFLAGS?=-m64 -mhard-float -mcpu=e5500  $(LOCAL_CFLAGS)
libfm-ppce500mc.o:  CFLAGS?=-m32 -mhard-float -mcpu=e500mc $(LOCAL_CFLAGS)
libfm-ppc32e5500.o: CFLAGS?=-m32 -mhard-float -mcpu=e5500  $(LOCAL_CFLAGS)
libfm-ppc64e5500.o: CFLAGS?=-m64 -mhard-float -mcpu=e5500  $(LOCAL_CFLAGS)
libfm-ppce500v2.o:  CFLAGS?=-m32 -msoft-float -mcpu=8548   $(LOCAL_CFLAGS)
libfm-ppc32e6500.o: CFLAGS?=-m32 -mhard-float -mcpu=e6500  $(LOCAL_CFLAGS)
libfm-ppc64e6500.o: CFLAGS?=-m64 -mhard-float -mcpu=e6500  $(LOCAL_CFLAGS)
libfm-ppce500v2.o:  CFLAGS?=-m32 -msoft-float -mcpu=8548   $(LOCAL_CFLAGS)

CFLAGS+=$(EXTRA_CFLAGS) -isystem $(KERNEL_SRC)/include


all: libfm-ppc32e5500.a libfm-ppc64e5500.a libfm-ppce500mc.a \
		libfm-ppce500v2.a libfm-ppc64e6500.a libfm-ppc32e6500.a \
		libfm-ppc32e5500-fmv3.a libfm-ppc64e5500-fmv3.a

libfm-ppc32e5500.o libfm-ppc64e5500.o libfm-ppce500mc.o libfm-ppce500v2.o \
		libfm-ppc32e6500.o libfm-ppc64e6500.o \
		libfm-ppc32e5500-fmv3.o libfm-ppc64e5500-fmv3.o: \
		$(FM_LIB_SRCDIR)/fm_lib.c $(wildcard $(addsuffix /*.h,$(FM_LIB_INCLUDE)))
	@(echo "(CC)  $@")
	@(echo "$(CC) $(CFLAGS) $(addprefix -I,$(FM_LIB_INCLUDE)) -c -o $@ $<" > .$@.cmd)
	@($(CC) $(CFLAGS) $(addprefix -I,$(FM_LIB_INCLUDE)) -c -o $@ $<)

archive: all .version
	@(echo "Creating tarball for fmlib...")
	@(tar -czf fm-lib-bi.tar.gz . \
		--transform "s,\\.\\(.*\\),fm-lib\\1," \
		--exclude=Makefile --exclude=.version --exclude-vcs --exclude=*.tar.gz)

install-%: %.a
	@(echo "Installing...")
	@($(INSTALL) -d $(DESTDIR)$(LIB_DEST_DIR))
	@($(INSTALL) $< $(DESTDIR)$(LIB_DEST_DIR))
	@(ln -s $< $(DESTDIR)$(LIB_DEST_DIR)/libfm.a)
	@(cp -r -p ./include $(DESTDIR)$(PREFIX))
	@($(INSTALL) -d $(DESTDIR)$(PREFIX)/share/doc/fm-lib)
	@($(INSTALL) $(FM_LIB_DOCFILES) $(DESTDIR)$(PREFIX)/share/doc/fm-lib)

targets help:
	@(echo)
	@(echo "make all: build libraries for all platforms (local build)")
	@(echo)
	@(echo "make archive: build tarball with fm libraries")
	@(echo)
	@(echo "make libfm-<arch>.a (e.g. \"make libfm-ppce500mc.a\"):")
	@(echo "	build library for specific platform <arch>")
	@(echo)
	@(echo "The available make libfm-<arch>.a targets are:")
	@(echo "	libfm-ppce500mc.a		(P2, P3, P4)")
	@(echo "	libfm-ppc32e5500.a		(P5 - 32b)")
	@(echo "	libfm-ppc64e5500.a		(P5 - 64b)")
	@(echo "	libfm-ppc32e6500.a		(B4/T4 - 32b)")
	@(echo "	libfm-ppc64e6500.a		(B4/T4 - 64b)")
	@(echo "	libfm-ppc32e5500-fmv3.a		(t1040 - 32b)")
	@(echo "	libfm-ppc64e5500-fmv3.a		(t1040 - 64b)")
	@(echo "	libfm-ppce500v2.a		(P1023)")
	@(echo)
	@(echo "make install-libfm-<arch> (e.g. \"make install-libfm-ppce500mc\"):")
	@(echo "	install the library and headers to the location specified by DESTDIR, PREFIX")
	@(echo)
	@(echo "Please also mind the fact that the environment for invoking 'make' with this Makefile")
	@(echo "needs also contain certain required variables, most notably KERNEL_SRC!")
	@(echo)

clean:
	@(echo "Cleaning...")
	@(rm -rf *.a *.o *.d .*.o.cmd)

.PHONY: targets help clean
