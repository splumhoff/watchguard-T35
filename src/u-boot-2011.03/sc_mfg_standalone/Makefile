include $(TOPDIR)/config.mk

ELF-$(ARCH)  :=
ELF-$(BOARD) :=
ELF-$(CPU)   :=
ELF-y        := sc_do_mfg


ELF := $(strip $(ELF-y) $(ELF-$(ARCH)) $(ELF-$(BOARD)) $(ELF-$(CPU)))

SREC = $(addsuffix .srec,$(ELF))
BIN  = $(addsuffix .bin,$(ELF))

COBJS	:= $(ELF:=.o)

LIB	= $(obj)libstubs.a

LIBAOBJS-$(ARCH)     :=
LIBAOBJS-$(CPU)      :=
LIBAOBJS-powerpc     += $(ARCH)_longjmp.o $(ARCH)_setjmp.o
#LIBAOBJS-mpc8xx      += test_burst_lib.o
LIBAOBJS := $(LIBAOBJS-$(ARCH)) $(LIBAOBJS-$(CPU))

LIBCOBJS = stubs.o
###############################
# Sercomm C OBJs
###############################
# LED
LIBCOBJS += led_test.o
# NAND
LIBCOBJS += nand_test.o
# NOR
LIBCOBJS += nor_test.o
# DRAM
LIBCOBJS += ram_test_port.o ram_test_error.o ram_test_main.o ram_test_random.o ram_test_algo.o
# RTC
LIBCOBJS += rtc_test.o
# MAIN
LIBCOBJS += sc_mfg_results.o



LIBOBJS	= $(addprefix $(obj),$(LIBAOBJS) $(LIBCOBJS))

SRCS	:= $(COBJS:.o=.c) $(LIBCOBJS:.o=.c) $(LIBAOBJS:.o=.S)
OBJS	:= $(addprefix $(obj),$(COBJS))
ELF	:= $(addprefix $(obj),$(ELF))
BIN	:= $(addprefix $(obj),$(BIN))
SREC	:= $(addprefix $(obj),$(SREC))

gcclibdir := $(shell dirname `$(CC) -print-libgcc-file-name`)

CPPFLAGS += -I..

CFLAGS += -I.

all:	$(obj).depend $(OBJS) $(LIB) $(obj)sc_do_mfg.srec $(obj)sc_do_mfg.bin $(obj)sc_do_mfg

#########################################################################
$(LIB):	$(obj).depend $(LIBOBJS)
		$(AR) $(ARFLAGS) $@ $(LIBOBJS)

$(ELF):
$(obj)%:	$(obj)%.o $(LIB)
		$(LD) -g -Ttext $(STANDALONE_LOAD_ADDR) \
			-o $@ -e $(SYM_PREFIX)$(notdir $(<:.o=)) $< $(LIB) \
			-L$(gcclibdir) -lgcc

$(SREC):
$(obj)%.srec:	$(obj)%
		$(OBJCOPY) -O srec $< $@ 2>/dev/null

$(BIN):
$(obj)%.bin:	$(obj)%
		$(OBJCOPY) -O binary $< $@ 2>/dev/null

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
