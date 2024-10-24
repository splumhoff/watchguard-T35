#
# Makefile	Main Makefile for the net-tools Package
#
# MII-TOOL	A tool for controlling MII-type ethernet devices
#               (transfer modes, etc). Taken from the net-tools package.
#
# Author:       Enrico Weigelt, metux IT service <weigelt@metux.de>
#
# URLs:         git://git.metux.de/oss-qm-packages.git
#               Branches/Tags: METUX.mii-tool*
#
#################################################################################
#
# NET-TOOLS	A collection of programs that form the base set of the
#		NET-3 Networking Distribution for the LINUX operating
#		system.
#
# Version:	2001-02-13
#
# Author:	Bernd Eckenfels <net-tools@lina.inka.de>
#		Copyright 1995-1996 Bernd Eckenfels, Germany
#
# URLs:		ftp://ftp.inka.de/pub/comp/Linux/networking/NetTools/ 
#		ftp://ftp.linux.org.uk/pub/linux/Networking/PROGRAMS/NetTools/
#		http://www.inka.de/sites/lina/linux/NetTools/index_en.html
#
# Based on:	Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
#		Copyright 1988-1993 MicroWalt Corporation
#
# Modifications:
#		Extensively modified from 01/21/94 onwards by
#		Alan Cox <A.Cox@swansea.ac.uk>
#		Copyright 1993-1994 Swansea University Computer Society
#
# Be careful! 
# This Makefile doesn't describe complete dependencies for all include files.
# If you change include files you might need to do make clean. 
#
#	{1.20}	Bernd Eckenfels:	Even more modifications for the new 
#					package layout
#	{1.21}	Bernd Eckenfels:	Check if config.in is newer than 
#					config.status
#	{1.22}  Bernd Eckenfels:	Include ypdomainname and nisdomainame
#
#	1.3.50-BETA6 private Release
#				
#960125	{1.23}	Bernd Eckenfels:	Peter Tobias' rewrite for 
#					makefile-based installation
#	1.3.50-BETA6a private Release
#
#960201 {1.24}	Bernd Eckenfels:	net-features.h added
#
#960201 1.3.50-BETA6b private Release
#
#960203 1.3.50-BETA6c private Release
#
#960204 1.3.50-BETA6d private Release
#
#960204 {1.25}	Bernd Eckenfels:	DISTRIBUTION added
#
#960205 1.3.50-BETA6e private Release
#
#960206	{1.26}	Bernd Eckenfels:	afrt.o removed (cleaner solution)
#
#960215 1.3.50-BETA6f Release
#
#960216 {1.30}	Bernd Eckenfels:	net-lib support
#960322 {1.31}	Bernd Eckenfels:	moveable netlib, TOPDIR
#960424 {1.32}	Bernd Eckenfels:	included the URLs in the Comment
#
#960514 1.31-alpha release
#
#960518 {1.33}	Bernd Eckenfels:	-I/usr/src/linux/include comment added
#
#	This program is free software; you can redistribute it
#	and/or  modify it under  the terms of  the GNU General
#	Public  License as  published  by  the  Free  Software
#	Foundation;  either  version 2 of the License, or  (at
#	your option) any later version.
#

PROGS   := mii-tool

include config.mk

# Compiler and Linker Options
# You may need to uncomment and edit these if you are using libc5 and IPv6.
COPTS = -D_GNU_SOURCE -O2 -Wall -g
ifeq ($(origin LOPTS), undefined)
LOPTS = 
endif

# -------- end of user definitions --------

MAINTAINER = weigelt@metux.de
RELEASE	   = 1.60

.EXPORT_ALL_VARIABLES:

CFLAGS	= $(COPTS) -I. -idirafter ./include/

SUBDIRS	= man/

MDEFINES = COPTS='$(COPTS)' LOPTS='$(LOPTS)' TOPDIR='$(TOPDIR)'

%.o:		%.c net-features.h version.h $<
		$(CC) $(CFLAGS) -c $<

all:		subdirs $(PROGS)

version.h:
		echo "#define RELEASE \"$(RELEASE)\"" >> version.h

mii-tool.o::	version.h

install:	all installbin installdata

update: 	all installbin installdata

mostlyclean:
		rm -f *.o DEADJOE config.new *~ *.orig lib/*.o

clean: mostlyclean
		rm -f $(PROGS)
		rm -f version.h
		@for i in $(SUBDIRS); do (cd $$i && $(MAKE) clean) ; done

clobber: 	clean
		rm -f $(PROGS) config.make
		@for i in $(SUBDIRS); do (cd $$i && $(MAKE) clobber) ; done

subdirs:
		@for i in $(SUBDIRS); do $(MAKE) -C $$i $(MDEFINES) ; done

mii-tool:	mii-tool.o
		$(CC) $(LDFLAGS) -o mii-tool mii-tool.o

installbin:
	$(INSTALL) -m 0755 -d $(DESTDIR)$(SBINDIR)
	$(INSTALL) -m 0755 mii-tool $(DESTDIR)$(SBINDIR)
	$(STRIP) $(STRIP_ARGS) $(DESTDIR)$(SBINDIR)/mii-tool

installdata:
	$(MAKE) -C man install

# End of Makefile.
