#
# Copyright 2014 Freescale Semiconductor, Inc.
#
# SPDX-License-Identifier:      GPL-2.0+
#

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-$(CONFIG_M300) += m300.o
obj-$(CONFIG_M300) += eth.o
obj-$(CONFIG_PCI)      += pci.o
endif

obj-y   += ddr.o
obj-y   += law.o
obj-y   += tlb.o
