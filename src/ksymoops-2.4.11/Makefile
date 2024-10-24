# Makefile for ksymoops

# Defaults for vmlinux, ksyms, objects, lsmod, System.map.  Externalised so
# distributions can tweak to suit their own file system layout.

# To default to not reading a source, set to any empty string.
# To default to reading a source, supply a quoted and escaped string.

# If the string contains *r (*m, *n, *s) then it is replaced at run time by
# the current value of `uname -r` (-m, -n, -s).  '*' was chosen as something
# that rarely appears in filenames and does not cause problems like '%' or '$'.

# RedHat users might want defaults like these
# DEF_MAP := \"/boot/System.map-*r\"

DEF_VMLINUX :=				# default no vmlinux
DEF_KSYMS := \"/proc/ksyms\"		# default current ksyms
DEF_LSMOD := \"/proc/modules\"		# default current lsmod
DEF_OBJECTS := \"/lib/modules/*r/\"	# default current modules
DEF_MAP := \"/usr/src/linux/System.map\"	# default current map

# Cross compile program prefix for installing cross ksymoops, and
# running binutils (objdump & nm), normally empty for system binutils

# If you are building for a cross compile, these sample values might be useful.
# DEF_TARGET := \"elf64-bigmips\"
# DEF_ARCH := \"mips:8000\"
# CROSS := mips64-linux-
# BFD_PREFIX := /usr/mips64-linux

# Prefix for finding BFD library, normally empty for system bfd
BFD_PREFIX :=
DEF_TARGET :=				# default bfd target, empty for system
DEF_ARCH :=				# default bfd arch, empty for system
CROSS :=				# prefix for installed program, nm, objdump

# Prefix for install libraries
INSTALL_PREFIX := /usr
# Install program
INSTALL := /usr/bin/install

# Assignment followed by a comment includes the spaces between the value and the
# comment as part of the value, so strip those extra spaces.
override DEF_VMLINUX := $(strip $(DEF_VMLINUX))
override DEF_KSYMS := $(strip $(DEF_KSYMS))
override DEF_LSMOD := $(strip $(DEF_LSMOD))
override DEF_OBJECTS := $(strip $(DEF_OBJECTS))
override DEF_MAP := $(strip $(DEF_MAP))
override DEF_TARGET := $(strip $(DEF_TARGET))
override DEF_ARCH := $(strip $(DEF_ARCH))
override INSTALL_PREFIX := $(strip $(INSTALL_PREFIX))
override CROSS := $(strip $(CROSS))
override BFD_PREFIX := $(strip $(BFD_PREFIX))

DEFS := Makefile ksymoops.h

PROGS := ksymoops

CFLAGS  += $(MYCFLAGS)
LDFLAGS += $(MYLDFLAGS)

CC := gcc
CFLAGS += -Dlinux \
	 -Wall \
	 -Wno-conversion \
	 -Waggregate-return \
	 -Wstrict-prototypes \
	 -Wmissing-prototypes \
	 -DINSTALL_PREFIX="\"$(INSTALL_PREFIX)\"" \
	 -DCROSS="\"$(CROSS)\"" \
	 $(DEBUG)

ifneq ($(DEF_VMLINUX),)
	CFLAGS += -DDEF_VMLINUX=$(DEF_VMLINUX)
endif
ifneq ($(DEF_KSYMS),)
	CFLAGS += -DDEF_KSYMS=$(DEF_KSYMS)
endif
ifneq ($(DEF_LSMOD),)
	CFLAGS += -DDEF_LSMOD=$(DEF_LSMOD)
endif
ifneq ($(DEF_OBJECTS),)
	CFLAGS += -DDEF_OBJECTS=$(DEF_OBJECTS)
endif
ifneq ($(DEF_MAP),)
	CFLAGS += -DDEF_MAP=$(DEF_MAP)
endif
ifneq ($(DEF_TARGET),)
	CFLAGS += -DDEF_TARGET=$(DEF_TARGET)
endif
ifneq ($(DEF_ARCH),)
	CFLAGS += -DDEF_ARCH=$(DEF_ARCH)
endif
ifneq ($(BFD_PREFIX),)
	CFLAGS += -I$(BFD_PREFIX)/include
	LDFLAGS += -L$(BFD_PREFIX)/lib
endif
ifeq ($(INSTALL_MANDIR),)
	INSTALL_MANDIR := $(INSTALL_PREFIX)/man
endif

STATIC := -Wl,-Bstatic
DYNAMIC := -Wl,-Bdynamic

OBJECTS := io.o ksyms.o ksymoops.o map.o misc.o object.o oops.o re.o symbol.o

all: $(PROGS)

:	$(OBJECTS)

$(OBJECTS): $(DEFS)

$(PROGS): %: %.o $(DEFS) $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) $(STATIC) -lbfd -liberty $(DYNAMIC) -o $@
	-@size $@

clean:
	rm -f core *.o $(PROGS)

install: all
	$(INSTALL) -d $(INSTALL_PREFIX)/bin
	$(INSTALL) ksymoops $(INSTALL_PREFIX)/bin/$(CROSS)ksymoops
	$(INSTALL) -d $(INSTALL_MANDIR)/man8
	$(INSTALL) ksymoops.8 $(INSTALL_MANDIR)/man8/$(CROSS)ksymoops.8
