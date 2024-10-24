#
# Copyright (C) 2009 Freescale Semiconductor, Inc.
#
# Author: Chen Gong
# 	  Mingkai Hu <Mingkai.hu@freescale.com>
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

OBJTARGET = boot_format

DEBUG := 0

# This is the prefix that will be used for almost all directories below.
PREFIX := /usr/local

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc

# LDFLAGS = -v -fno-builtin -static  --cref,-Map,$@.map

# Set flags to add preprocessor or compiler flags
CFLAGS := -m32
LDFLAGS := -m32

# Determine the default compiler flags
# Set CFLAGS above to add your own flags to all.
ALL_CFLAGS := -Wall -I.
ALL_LDFLAGS :=

ifeq ($(DEBUG),1)
ALL_CFLAGS += -DDEBUG;
ALL_CFLAGS += -O -g
else
ALL_CFLAGS += -O2
endif

ALL_CFLAGS += $(CFLAGS)
ALL_LDFLAGS += $(LDFLAGS)

OBJS = boot_format.o

.PHONY: all install clean $(OBJTARGET)

ALL = $(OBJTARGET)

all: $(ALL)

$(OBJTARGET): $(OBJS)
	$(LD) $(ALL_LDFLAGS) $(OBJS) -o $(OBJTARGET)

install:
	install -d $(PREFIX)/boot_format/
	install boot_format $(PREFIX)/boot_format/
	install -m 644 config_*.dat $(PREFIX)/boot_format/

clean:
	@rm -f *.dasm *.obj *.bin *.src *.o xref.txt $(OBJTARGET)

%.o: %.c
	$(CC) $(ALL_CFLAGS) -c $< -o $@
