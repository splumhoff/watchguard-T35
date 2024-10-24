/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <netdev.h>
#include <linux/compiler.h>
#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/cache.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_law.h>
#include <asm/fsl_serdes.h>
#include <asm/fsl_portals.h>
#include <asm/fsl_liodn.h>
#include <fm_eth.h>
#include <asm/mpc85xx_gpio.h>
#include <version.h>


#include "m200.h"

/* 
 * 	GPO 27 reset
 * 	GPO 28 
 * 	GPO 29 
 * 	RED 	28=L 29=H
 * 	GREEN	28=H 29=L
 *
 */
#define GPIO_RESET  0x10
#define GPIO_RED  0x8    
#define GPIO_GREEN  0x4
#define GPIO_DIR 0x0000001c

DECLARE_GLOBAL_DATA_PTR;

int checkboard(void)
{
	struct cpu_type *cpu = gd->arch.cpu;
        volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	printf("Board: %sRDB, ", cpu->name);
	printf ("%s\n",BOARD_VERSION);
#ifndef CONFIG_RAMBOOT_PBL
        setbits_be32(&pgpio->gpdir, GPIO_DIR);
        clrsetbits_be32(&pgpio->gpdat, GPIO_GREEN, GPIO_RED);
	gpio_set_value(4, 1);
	udelay(5);
	gpio_set_value(4, 0);
	udelay(5);
	gpio_set_value(4, 1);
#endif


	return 0;
}


int board_early_init_r(void)
{
	const u8 temp[7] = { 0x49, 0x4E, 0x52, 0x56, 0x59, 0x5c, 0x5f}; 
	const u8 fan[7]  = { 0x0a, 0x12, 0x1b, 0x24, 0x2d, 0x36, 0x3f};
	int i = 0;

#ifdef CONFIG_SYS_FLASH_BASE
	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	const u8 flash_esel = find_tlb_idx((void *)flashbase, 1);

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	/* invalidate existing TLB entry for flash */
	disable_tlb(flash_esel);

	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,
		MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,
		0, flash_esel, BOOKE_PAGESZ_256M, 1);
#endif
	set_liodns();
#ifdef CONFIG_SYS_DPAA_QBMAN
	setup_portals();
#endif
/*W83793D Smart fan enbale*/
	/*  W83793D smartfan default setting */
	i2c_reg_write(0x2d, 0, 0x82);
	/* Enable Temp 1 smartfan function */
	i2c_reg_write(0x2d, 0x1, 0x4);
	for ( i = 0 ; i < 7 ; i++)
	{
		/* Set temp level */
		i2c_reg_write(0x2d, 0x30 + i, temp[i]);
		/* Set fan level */
		i2c_reg_write(0x2d, 0x38 + i, fan[i]);
	}
	i2c_reg_write(0x2d, 0, 0x80);
	/* Set Uptime */
	i2c_reg_write(0x2d, 0xc3, 0x1);
	/* Set Downtime */
	i2c_reg_write(0x2d, 0xc4, 0x20);
        /* Set Critical Temperature */
        i2c_reg_write(0x2d, 0xc5, 0x5f);
/*W83793D Smart fan enbale*/
	return 0;
}

int misc_init_r(void)
{
	return 0;
}

void ft_board_setup(void *blob, bd_t *bd)
{
	phys_addr_t base;
	phys_size_t size;

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();

	fdt_fixup_memory(blob, (u64)base, (u64)size);

#ifdef CONFIG_PCI
	pci_of_setup(blob, bd);
#endif

	fdt_fixup_liodn(blob);

#ifdef CONFIG_HAS_FSL_DR_USB
	fdt_fixup_dr_usb(blob, bd);
#endif

#ifdef CONFIG_SYS_DPAA_FMAN
	fdt_fixup_fman_ethernet(blob);
#endif
}
#ifdef CONFIG_DEEP_SLEEP
void board_mem_sleep_setup(void)
{
	/* does not provide HW signals for power management */
/*	CPLD_WRITE(misc_ctl_status, (CPLD_READ(misc_ctl_status) & ~0x40)); */
	/* Disable MCKE isolation */
	udelay(1);
}
#endif
