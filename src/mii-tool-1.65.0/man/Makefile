#
# man/Makefile	man/ Makefile for the net-tools Package
#
# NET-TOOLS	A collection of programs that form the base set of the
#		NET-3 Networking Distribution for the LINUX operating
#		system.
#
# Version:	Makefile 0.03 (2000-10-08)
#
# Authors:	Bernd Eckenfels <net-tools@lina.inka.de>
#               Arnaldo Carvalhoa de Melo <acme@conectiva.com.br>
#		Copyright 1995-1996 Bernd Eckebnfels, Germany
#                              1998 Arnaldo Carvalho de Melo, Brazil
#
#960322	{0.01}	Bernd Eckenfels:	creation to make main Makefile cleaner
#980701	{0.02}	Arnaldo C. Melo:	making the main Makefile *more* cleaner :)
#001008 {0.03}  Bernd Eckenfels:	RH patch for FHS (man page now in mandir)
#
#
#	This program is free software; you can redistribute it
#	and/or  modify it under  the terms of  the GNU General
#	Public  License as  published  by  the  Free  Software
#	Foundation;  either  version 2 of the License, or  (at
#	your option) any later version.
#

include ../config.mk

LANGS=en_US

all:

clean:
		rm -f DEADJOE *~ *.orig
		rm -f */DEADJOE */*~ */*.orig

clobber:	clean

install:
		LANGS=$(LANGS) ; \
		for LANG in $$LANGS; do \
			cd $$LANG; \
				if [ "$$LANG" = "en_US" ] ; then \
					MAN_DIR=$(DESTDIR)$(MANDIR); \
				else \
					MAN_DIR=$(DESTDIR)$(MANDIR)/$$LANG; \
				fi ; \
				$(INSTALL) -d -m 755 $$MAN_DIR/man8 ; \
				$(INSTALL) -m 644 mii-tool.8 $$MAN_DIR/man8 ; \
			cd ..; \
		done

# End of man/Makefile.
