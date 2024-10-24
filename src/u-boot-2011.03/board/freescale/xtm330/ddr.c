/*
 * Copyright 2009 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/mmu.h>
#include <asm/immap_85xx.h>
#include <asm/processor.h>
#include <asm/fsl_ddr_sdram.h>
#include <asm/io.h>
#include <asm/fsl_law.h>

DECLARE_GLOBAL_DATA_PTR;

extern void fsl_ddr_set_memctl_regs(const fsl_ddr_cfg_regs_t *regs,
				   unsigned int ctrl_num);

#define DATARATE_667MHZ 666666666

#if 0 /* The Original ddr parameter */
#define CONFIG_SYS_DDR_CS0_BNDS		0x0000003F
#define CONFIG_SYS_DDR_CS0_CONFIG	0x80014302
#define CONFIG_SYS_DDR_CS0_CONFIG_2	0x00000000
#define CONFIG_SYS_DDR_INIT_ADDR	0x00000000
#define CONFIG_SYS_DDR_INIT_EXT_ADDR	0x00000000
#define CONFIG_SYS_DDR_MODE_CONTROL	0x00000000
#define CONFIG_SYS_DDR_ZQ_CONTROL	0x89080600
#define CONFIG_SYS_DDR_WRLVL_CONTROL	0x8655A608
#define CONFIG_SYS_DDR_SR_CNTR		0x00000000
#define CONFIG_SYS_DDR_RCW_1		0x00000000
#define CONFIG_SYS_DDR_RCW_2		0x00000000
#define CONFIG_SYS_DDR_CONTROL		0xc70c0000	/* Type = DDR3*/
#define CONFIG_SYS_DDR_CONTROL_2	0x04401010
#define CONFIG_SYS_DDR_TIMING_4		0x00220001
#define CONFIG_SYS_DDR_TIMING_5		0x03402400

#define CONFIG_SYS_DDR_TIMING_3_667	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_667	0x00330004
#define CONFIG_SYS_DDR_TIMING_1_667	0x6f6b4846
#define CONFIG_SYS_DDR_TIMING_2_667	0x0fa8c8cf
#define CONFIG_SYS_DDR_CLK_CTRL_667	0x03000000
#define CONFIG_SYS_DDR_MODE_1_667	0x40461520
#define CONFIG_SYS_DDR_MODE_2_667	0x8000c000
#define CONFIG_SYS_DDR_INTERVAL_667	0x0c300000
#else /* From customer ddr parameter  */
#define CONFIG_SYS_DDR_CS0_BNDS		0x0000003F
#define CONFIG_SYS_DDR_CS0_CONFIG	0x80014302
#define CONFIG_SYS_DDR_CS0_CONFIG_2	0x00000000
#define CONFIG_SYS_DDR_INIT_ADDR	0x00000000
#define CONFIG_SYS_DDR_INIT_EXT_ADDR	0x00000000
#define CONFIG_SYS_DDR_MODE_CONTROL	0x00000000
#define CONFIG_SYS_DDR_ZQ_CONTROL	0x89080600
#define CONFIG_SYS_DDR_WRLVL_CONTROL	0x8655f607
#define CONFIG_SYS_DDR_SR_CNTR		0x00000000
#define CONFIG_SYS_DDR_RCW_1		0x00000000
#define CONFIG_SYS_DDR_RCW_2		0x00000000
#define CONFIG_SYS_DDR_CONTROL		0xc70c0008	/* Type = DDR3*/
#define CONFIG_SYS_DDR_CONTROL_2	0x24401010
#define CONFIG_SYS_DDR_TIMING_4		0x00000001
#define CONFIG_SYS_DDR_TIMING_5		0x02401400

#define CONFIG_SYS_DDR_TIMING_3_667	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_667	0x00330104
#define CONFIG_SYS_DDR_TIMING_1_667	0x5c5be544
#define CONFIG_SYS_DDR_TIMING_2_667	0x0fa890ce
#define CONFIG_SYS_DDR_CLK_CTRL_667	0x02800000
#define CONFIG_SYS_DDR_MODE_1_667	0x00061220
#define CONFIG_SYS_DDR_MODE_2_667	0x00000000
#define CONFIG_SYS_DDR_INTERVAL_667	0x0a28028a
#endif

fsl_ddr_cfg_regs_t ddr_cfg_regs_667 = {
	.cs[0].bnds = CONFIG_SYS_DDR_CS0_BNDS,
	.cs[0].config = CONFIG_SYS_DDR_CS0_CONFIG,
	.cs[0].config_2 = CONFIG_SYS_DDR_CS0_CONFIG_2,
	.timing_cfg_3 = CONFIG_SYS_DDR_TIMING_3_667,
	.timing_cfg_0 = CONFIG_SYS_DDR_TIMING_0_667,
	.timing_cfg_1 = CONFIG_SYS_DDR_TIMING_1_667,
	.timing_cfg_2 = CONFIG_SYS_DDR_TIMING_2_667,
	.ddr_sdram_cfg = CONFIG_SYS_DDR_CONTROL,
	.ddr_sdram_cfg_2 = CONFIG_SYS_DDR_CONTROL_2,
	.ddr_sdram_mode = CONFIG_SYS_DDR_MODE_1_667,
	.ddr_sdram_mode_2 = CONFIG_SYS_DDR_MODE_2_667,
	.ddr_sdram_md_cntl = CONFIG_SYS_DDR_MODE_CONTROL,
	.ddr_sdram_interval = CONFIG_SYS_DDR_INTERVAL_667,
	.ddr_data_init = CONFIG_MEM_INIT_VALUE,
	.ddr_sdram_clk_cntl = CONFIG_SYS_DDR_CLK_CTRL_667,
	.ddr_init_addr = CONFIG_SYS_DDR_INIT_ADDR,
	.ddr_init_ext_addr = CONFIG_SYS_DDR_INIT_EXT_ADDR,
	.timing_cfg_4 = CONFIG_SYS_DDR_TIMING_4,
	.timing_cfg_5 = CONFIG_SYS_DDR_TIMING_5,
	.ddr_zq_cntl = CONFIG_SYS_DDR_ZQ_CONTROL,
	.ddr_wrlvl_cntl = CONFIG_SYS_DDR_WRLVL_CONTROL,
	.ddr_sr_cntr = CONFIG_SYS_DDR_SR_CNTR,
	.ddr_sdram_rcw_1 = CONFIG_SYS_DDR_RCW_1,
	.ddr_sdram_rcw_2 = CONFIG_SYS_DDR_RCW_2,
/*
 *  From customer ddr parameter
 */
	.ddr_cdr1 =  0x80000000
};

/*
 * Fixed sdram init -- doesn't use serial presence detect.
 */

phys_size_t fixed_sdram (void)
{
	sys_info_t sysinfo;
	char buf[32];
	fsl_ddr_cfg_regs_t ddr_cfg_regs;
	size_t ddr_size;

	get_sys_info(&sysinfo);
	printf("Configuring DDR for %s MT/s data rate\n",
				strmhz(buf, sysinfo.freqDDRBus));

	memcpy(&ddr_cfg_regs, &ddr_cfg_regs_667, sizeof(ddr_cfg_regs));

	ddr_size = CONFIG_SYS_SDRAM_SIZE * 1024 * 1024;

	fsl_ddr_set_memctl_regs(&ddr_cfg_regs, 0);

	set_ddr_laws(0, ddr_size, LAW_TRGT_IF_DDR_1);
	return ddr_size;
}
