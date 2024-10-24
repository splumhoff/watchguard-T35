CFLAGS= -O -Wall
CC=gcc
INSTALL=install

LIBS=libmii.o

all: mii-diag

mii-diag:
	$(CC) $(CFLAGS) -Wstrict-prototypes -O mii-diag.c -DLIBMII \
				libmii.c -o mii-diag

install-mii-diag:
	mkdir -p $(DESTDIR)/sbin
	mkdir -p $(DESTDIR)/man/man8
	$(INSTALL) mii-diag $(DESTDIR)/sbin/
	$(INSTALL) mii-diag.8 $(DESTDIR)/man/man8/

install: all install-mii-diag

clean:
	rm -f *.o mii-diag
