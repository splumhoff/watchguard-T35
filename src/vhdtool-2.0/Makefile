WG_STATIC = -static
LDFLAGS = $(WG_LDFLAGS) $(WG_STATIC) -luuid
CFLAGS = $(WG_CFLAGS) -g -Wall

all: vhdtool fix2dynvhd

shared: WG_STATIC := 
shared: vhdtool-shared fix2dynvhd-shared

VhdTool.o: VhdTool.c wg_vhdtool.h

vhdtool vhdtool-shared: VhdTool.o
	$(CC) $^ $(LDFLAGS) -o $@

fix2dynvhd fix2dynvhd-shared: fix2dynvhd.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

install:
	cp vhdtool* fix2dynvhd fix2dynvhd-shared $(DESTDIR)/bin

clean:
	rm -f vhdtool vhdtool-shared
	rm -f fix2dynvhd fix2dynvhd-shared
	rm -f *.o
	rm -rf test

test:   vhdtool fix2dynvhd
	rm -rf test
	mkdir test
	dd if=/dev/zero of=test/cf.vhd bs=1024k count=128
	set -x; \
	./vhdtool convert test/cf.vhd; \
	OLDSIZE="$$(($$(stat -c%s test/cf.vhd)-512))"; \
	dd if=test/cf.vhd of=test/cf.footer bs=1 skip=$$OLDSIZE >& /dev/null
	./fix2dynvhd -v test/cf.vhd test/cf-dyn.vhd
	@OLDSIZE="$$(($$(stat -c%s test/cf-dyn.vhd)-512))"; \
	dd if=test/cf-dyn.vhd of=test/cf-dyn.footer1 bs=1 count=512 >& /dev/null; \
	dd if=test/cf-dyn.vhd of=test/cf-dyn.footer2 bs=1 skip=$$OLDSIZE >& /dev/null; \
	dd if=test/cf-dyn.vhd of=test/cf-dyn.header bs=1 skip=512 count=1024 >& /dev/null; \
	dd if=test/cf-dyn.vhd of=test/cf-dyn.nofooter bs=1 count=$$OLDSIZE >& /dev/null; \
	dd if=test/cf-dyn.nofooter of=test/cf-dyn.barefile bs=1 skip=1536 >& /dev/null
	@echo -n "fixed footer: "
	@if ! ./footercheck.py ref/cf.footer test/cf.footer; then touch test/FAIL; fi
	@echo -n "dynamic footer 1: "
	@if ! ./footercheck.py ref/cf-dyn.footer test/cf-dyn.footer1; then touch test/FAIL; fi
	@echo -n "dynamic footer 2: "
	@if ! ./footercheck.py ref/cf-dyn.footer test/cf-dyn.footer2; then touch test/FAIL; fi
	@echo -n "dynamic header check: "
	@if diff ref/cf-dyn.header test/cf-dyn.header >& /dev/null; then echo "PASS"; \
	else echo "Fail"; touch test/FAIL; fi
	@echo -n "dynamic data check: "
	@if diff ref/cf-dyn.barefile test/cf-dyn.barefile >& /dev/null; then echo "PASS"; \
	else echo "Fail"; touch test/FAIL; fi
	@if [ -f test/FAIL ]; then echo FAIL; false; else echo PASS; fi

.PHONY: all clean install test shared
