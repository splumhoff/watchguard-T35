#
# Copyright (C) 2010 Freescale Semiconductor, Inc.
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

IN_TPL := y
PAD_TO := 0xf8f9c000

include $(TOPDIR)/config.mk

LDSCRIPT= $(TOPDIR)/$(CPUDIR)/u-boot-tpl.lds
LDFLAGS := -T $(LDSCRIPT) -Ttext $(CONFIG_SYS_TEXT_BASE_TPL) $(PLATFORM_LDFLAGS)
AFLAGS	+= -DCONFIG_IN_TPL
CFLAGS	+= -DCONFIG_IN_TPL

SOBJS	= start.o ticks.o ppcstring.o
COBJS	= cache.o cpu_init_early.o cpu_init_nand.o fsl_law.o law.o speed.o \
	  tpl_boot.o tlb.o tlb_table.o ddr-gen3.o time.o ddr.o cpu.o fsl_lbc.o \
	  string.o hwconfig.o time_lib.o ddr_spd.o ctype.o div64.o crc32.o\
	  console.o cmd_nvedit.o env_common.o env_nand.o vsprintf.o \
	  display_options.o hashtable.o dlmalloc.o stdio.o ns16550.o serial.o \
	  errno.o command.o serial_driver.o qsort.o

ifdef CONFIG_RAMBOOT_NAND
COBJS += nand_boot_fsl_elbc.o
endif

LIBS = $(OBJTREE)/arch/powerpc/cpu/mpc8xxx/ddr/libddr.o
LIBS += $(OBJTREE)/drivers/i2c/libi2c.o

SRCS	:= $(addprefix $(obj),$(SOBJS:.o=.S) $(COBJS:.o=.c))
OBJS	:= $(addprefix $(obj),$(SOBJS) $(COBJS))
__OBJS	:= $(SOBJS) $(COBJS)
__LIBS	:= $(addprefix $(obj), $(LIBS))
LNDIR	:= $(OBJTREE)/tpl/board/$(BOARDDIR)

tplobj	:= $(OBJTREE)/tpl/

ALL	= $(tplobj)u-boot-tpl $(tplobj)u-boot-tpl.bin

all:	$(obj).depend $(ALL)

$(tplobj)u-boot-tpl.bin: $(tplobj)u-boot-tpl
	$(OBJCOPY) ${OBJCFLAGS} --pad-to=$(PAD_TO) -O binary $< $@

$(tplobj)u-boot-tpl:	$(OBJS) $(LIBS)
	cd $(LNDIR) && $(LD) $(LDFLAGS) $(__OBJS) $(__LIBS) \
		$(PLATFORM_LIBS) \
		-Map $(tplobj)u-boot-tpl.map \
		-o $(tplobj)u-boot-tpl

# create symbolic links for common files

$(obj)cache.c:
	@rm -f $(obj)cache.c
	ln -sf $(SRCTREE)/arch/powerpc/lib/cache.c $(obj)cache.c

$(obj)cpu_init_early.c:
	@rm -f $(obj)cpu_init_early.c
	ln -sf $(SRCTREE)/$(CPUDIR)/cpu_init_early.c $(obj)cpu_init_early.c

$(obj)fsl_lbc.c:
	@rm -f $(obj)fsl_lbc.c
	ln -sf $(SRCTREE)/arch/powerpc/cpu/mpc8xxx/fsl_lbc.c $(obj)fsl_lbc.c

$(obj)cpu.c:
	@rm -f $(obj)cpu.c
	ln -sf $(SRCTREE)/$(CPUDIR)/cpu.c $(obj)cpu.c

$(obj)cpu_init_nand.c:
	@rm -f $(obj)cpu_init_nand.c
	ln -sf $(SRCTREE)/$(CPUDIR)/cpu_init_nand.c $(obj)cpu_init_nand.c

$(obj)fsl_law.c:
	@rm -f $(obj)fsl_law.c
	ln -sf $(SRCTREE)/drivers/misc/fsl_law.c $(obj)fsl_law.c

$(obj)law.c:
	@rm -f $(obj)law.c
	ln -sf $(SRCTREE)/board/$(BOARDDIR)/law.c $(obj)law.c

$(obj)nand_boot_fsl_elbc.c:
	@rm -f $(obj)nand_boot_fsl_elbc.c
	ln -sf $(SRCTREE)/nand_spl/nand_boot_fsl_elbc.c \
	       $(obj)nand_boot_fsl_elbc.c

$(obj)fixed_ivor.S:
	@rm -f $(obj)fixed_ivor.S
	ln -sf $(SRCTREE)/$(CPUDIR)/fixed_ivor.S $(obj)fixed_ivor.S

$(obj)start.S: $(obj)fixed_ivor.S
	@rm -f $(obj)start.S
	ln -sf $(SRCTREE)/$(CPUDIR)/start.S $(obj)start.S

$(obj)speed.c:
	@rm -f $(obj)speed.c
	ln -sf $(SRCTREE)/$(CPUDIR)/speed.c $(obj)speed.c

$(obj)interrupts.c:
	@rm -f $(obj)interrupts.c
	ln -sf $(SRCTREE)/arch/powerpc/lib/interrupts.c $(obj)interrupts.c

$(obj)ticks.S:
	@rm -f $(obj)ticks.S
	ln -sf $(SRCTREE)/arch/powerpc/lib/ticks.S $(obj)ticks.S

$(obj)bootm.c:
	@rm -f $(obj)bootm.c
	ln -sf $(SRCTREE)/arch/powerpc/lib/bootm.c $(obj)bootm.c

$(obj)tlb.c:
	@rm -f $(obj)tlb.c
	ln -sf $(SRCTREE)/$(CPUDIR)/tlb.c $(obj)tlb.c

$(obj)tlb_table.c:
	@rm -f $(obj)tlb_table.c
	ln -sf $(SRCTREE)/board/$(BOARDDIR)/tlb.c $(obj)tlb_table.c

$(obj)ddr.c:
	@rm -f $(obj)ddr.c
	ln -sf $(SRCTREE)/board/$(BOARDDIR)/ddr.c $(obj)ddr.c

$(obj)time.c:
	@rm -f $(obj)time.o
	ln -sf $(SRCTREE)/arch/powerpc/lib/time.c $(obj)time.c

$(obj)ddr-gen3.c:
	@rm -f $(obj)ddr-gen3.c
	ln -sf $(SRCTREE)/$(CPUDIR)/ddr-gen3.c $(obj)ddr-gen3.c

$(obj)ppcstring.S:
	@rm -f $(obj)ppcstring.S
	ln -sf $(SRCTREE)/arch/powerpc/lib/ppcstring.S $(obj)ppcstring.S

$(obj)ns16550.c:
	@rm -f $(obj)ns16550.c
	ln -sf $(SRCTREE)/drivers/serial/ns16550.c $(obj)ns16550.c

$(obj)serial_driver.c:
	@rm -f $(obj)serial_driver.c
	ln -sf $(SRCTREE)/drivers/serial/serial.c $(obj)serial_driver.c

$(obj)time_lib.c:
	@rm -f $(obj)time_lib.o
	ln -sf $(SRCTREE)/lib/time.c $(obj)time_lib.c

$(obj)ddr_spd.c:
	@rm -f $(obj)ddr_spd.c
	ln -sf $(SRCTREE)/common/ddr_spd.c $(obj)ddr_spd.c

$(obj)ctype.c:
	@rm -f $(obj)ctype.c
	ln -sf $(SRCTREE)/lib/ctype.c $(obj)ctype.c

$(obj)div64.c:
	@rm -f $(obj)div64.c
	ln -sf $(SRCTREE)/lib/div64.c $(obj)div64.c

$(obj)crc32.c:
	@rm -f $(obj)crc32.c
	ln -sf $(SRCTREE)/lib/crc32.c $(obj)crc32.c

$(obj)env_common.c:
	@rm -f $(obj)env_common.c
	ln -sf $(SRCTREE)/common/env_common.c $(obj)env_common.c

$(obj)env_nand.c:
	@rm -f $(obj)env_nand.c
	ln -sf $(SRCTREE)/common/env_nand.c $(obj)env_nand.c

$(obj)cmd_nvedit.c:
	@rm -f $(obj)cmd_nvedit.c
	ln -sf $(SRCTREE)/common/cmd_nvedit.c $(obj)cmd_nvedit.c

$(obj)console.c:
	@rm -f $(obj)console.c
	ln -sf $(SRCTREE)/common/console.c $(obj)console.c

$(obj)dlmalloc.c:
	@rm -f $(obj)dlmalloc.c
	ln -sf $(SRCTREE)/common/dlmalloc.c $(obj)dlmalloc.c

$(obj)hwconfig.c:
	@rm -f $(obj)hwconfig.c
	ln -sf $(SRCTREE)/common/hwconfig.c $(obj)hwconfig.c

$(obj)stdio.c:
	@rm -f $(obj)stdio.c
	ln -sf $(SRCTREE)/common/stdio.c $(obj)stdio.c

$(obj)string.c:
	@rm -f $(obj)string.c
	ln -sf $(SRCTREE)/lib/string.c $(obj)string.c

$(obj)vsprintf.c:
	@rm -f $(obj)vsprintf.c
	ln -sf $(SRCTREE)/lib/vsprintf.c $(obj)vsprintf.c

$(obj)display_options.c:
	@rm -f $(obj)display_options.c
	ln -sf $(SRCTREE)/lib/display_options.c $(obj)display_options.c

$(obj)hashtable.c:
	@rm -f $(obj)hashtable.c
	ln -sf $(SRCTREE)/lib/hashtable.c $(obj)hashtable.c

$(obj)serial.c:
	@rm -f $(obj)serial.c
	ln -sf $(SRCTREE)/common/serial.c $(obj)serial.c

$(obj)command.c:
	@rm -f $(obj)command.c
	ln -sf $(SRCTREE)/common/command.c $(obj)command.c

$(obj)errno.c:
	@rm -f $(obj)errno.c
	ln -sf $(SRCTREE)/lib/errno.c $(obj)errno.c

$(obj)qsort.c:
	@rm -f $(obj)qsort.c
	ln -sf $(SRCTREE)/lib/qsort.c $(obj)qsort.c

ifneq ($(OBJTREE), $(SRCTREE))
$(obj)tpl_boot.c:
	@rm -f $(obj)tpl_boot.c
	ln -s $(SRCTREE)/tpl/freescale/tpl_boot.c $(obj)tpl_boot.c
endif

#########################################################################

$(obj)%.o:	$(obj)%.S
	$(CC) $(AFLAGS) -c -o $@ $<

$(obj)%.o:	$(obj)%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
