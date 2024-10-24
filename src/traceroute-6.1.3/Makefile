
CC=$(TARGET_CC)

all:
	$(CC) $(TARGET_CFLAGS) -pipe -O3 -fomit-frame-pointer -lresolv -lm -o traceroute traceroute.c

clean:
	rm -f traceroute traceroute.o
