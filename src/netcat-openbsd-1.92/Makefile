#	$OpenBSD: Makefile,v 1.6 2001/09/02 18:45:41 jakob Exp $

PROG=	netcat
SRCS=	netcat.c atomicio.c socks.c \
        openbsd-compat/base64.c openbsd-compat/readpassphrase.c

#CFLAGS += `pkg-config --cflags glib-2.0`
INC = -Iopenbsd-compat 
#LIBS = `pkg-config --libs glib-2.0`
OBJS = $(SRCS:.c=.o)

all: netcat
netcat: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o netcat -lglib-2.0

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -f $(OBJS) netcat
