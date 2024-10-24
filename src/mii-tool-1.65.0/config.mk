ifndef PREFIX
PREFIX := /usr
endif

ifndef DATADIR
DATADIR := $(PREFIX)/share
endif

ifndef MANDIR
MANDIR := $(DATADIR)/man
endif

ifndef MAN1DIR
MAN1DIR := $(MANDIR)/man1
endif

ifndef SBINDIR
SBINDIR := /sbin
endif

ifndef INSTALL
INSTALL := install
endif

ifndef STRIP
STRIP := strip
endif

ifndef STRIP_ARGS
STRIP_ARGS := --strip-unneeded
endif

ifndef CC
CC := gcc
endif
