/*
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */
#include <common.h>
#include <mpc85xx.h>
#include <asm/io.h>
#include <ns16550.h>
#include <nand.h>
#include <asm/mmu.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_ddr_sdram.h>
#include <asm/fsl_law.h>

DECLARE_GLOBAL_DATA_PTR;

void board_init_f(ulong bootflag)
{
	uint plat_ratio, bus_clk, sys_clk = 0;
	ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);

	sys_clk = CONFIG_SYS_CLK_FREQ;

	plat_ratio = in_be32(&gur->porpllsr) & MPC85xx_PORPLLSR_PLAT_RATIO;
	plat_ratio >>= 1;
	bus_clk = plat_ratio * sys_clk;
	get_clocks();

	NS16550_init((NS16550_t)CONFIG_SYS_NS16550_COM1,
			bus_clk / 16 / CONFIG_BAUDRATE);

	/* load environment */
#ifdef CONFIG_NAND_U_BOOT
	nand_load(CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE,
				(uchar *)CONFIG_ENV_ADDR);
#endif

	gd->env_addr  = (ulong)(CONFIG_ENV_ADDR);
	gd->env_valid = 1;

	/* board specific DDR initialization */
	gd->ram_size = initdram(0);
	puts("DRAM:");
	print_size(gd->ram_size, "");

	puts("\nTertiary program loader running in sram... ");

	/*
	 * Load final image to DDR and let it run from there.
	 */
#ifdef CONFIG_NAND_U_BOOT
	nand_boot();
#endif
}

void board_init_r(gd_t *gd, ulong dest_addr)
{
}
