/* Copyright 2013 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <ns16550.h>
#include <nand.h>
#include <i2c.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <spi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

phys_size_t get_effective_memsize(void)
{
	return CONFIG_SYS_L3_SIZE;
}

unsigned long get_board_sys_clk(void)
{
	return CONFIG_SYS_CLK_FREQ;
}

unsigned long get_board_ddr_clk(void)
{
	return CONFIG_DDR_CLK_FREQ;
}

void board_init_f(ulong bootflag)
{
	u32 plat_ratio, sys_clk, ccb_clk;
	ccsr_gur_t *gur = (void *)CONFIG_SYS_MPC85xx_GUTS_ADDR;

	/* Memcpy existing GD at CONFIG_SPL_GD_ADDR */
	memcpy((void *)CONFIG_SPL_GD_ADDR, (void *)gd, sizeof(gd_t));

	/* Update GD pointer */
	gd = (gd_t *)(CONFIG_SPL_GD_ADDR);

	console_init_f();

	/* initialize selected port with appropriate baud rate */
	sys_clk = get_board_sys_clk();
	plat_ratio = (in_be32(&gur->rcwsr[0]) >> 25) & 0x1f;
	ccb_clk = sys_clk * plat_ratio / 2;

	NS16550_init((NS16550_t)CONFIG_SYS_NS16550_COM1,
		     ccb_clk / 16 / CONFIG_BAUDRATE);

#if defined(CONFIG_SPL_MMC_BOOT)
	puts("\nSD boot...\n");
#elif defined(CONFIG_SPL_SPI_BOOT)
	puts("\nSPI boot...\n");
#elif defined(CONFIG_SPL_NAND_BOOT)
	puts("\nNAND boot...\n");
#endif

	relocate_code(CONFIG_SPL_RELOC_STACK, (gd_t *)CONFIG_SPL_GD_ADDR, 0x0);
}

void board_init_r(gd_t *gd, ulong dest_addr)
{
	bd_t *bd;

	bd = (bd_t *)(gd + sizeof(gd_t));
	memset(bd, 0, sizeof(bd_t));
	gd->bd = bd;
	bd->bi_memstart = CONFIG_SYS_INIT_L3_ADDR;
	bd->bi_memsize = CONFIG_SYS_L3_SIZE;

	probecpu();
	get_clocks();
	mem_malloc_init(CONFIG_SPL_RELOC_MALLOC_ADDR,
			CONFIG_SPL_RELOC_MALLOC_SIZE);

#ifndef CONFIG_SPL_NAND_BOOT
	env_init();
#endif

#ifdef CONFIG_SPL_MMC_BOOT
	mmc_initialize(bd);
#endif

	/* relocate environment function pointers etc. */
#ifdef CONFIG_SPL_NAND_BOOT
	nand_spl_load_image(CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE,
			    (uchar *)CONFIG_ENV_ADDR);
	gd->env_addr  = (ulong)(CONFIG_ENV_ADDR);
	gd->env_valid = 1;
#else
	env_relocate();
#endif

	i2c_init_all();

	gd->ram_size = initdram(0);

#ifdef CONFIG_SPL_MMC_BOOT
	mmc_boot();
#elif defined(CONFIG_SPL_SPI_BOOT)
	spi_boot();
#elif defined(CONFIG_SPL_NAND_BOOT)
	nand_boot();
#endif
}
