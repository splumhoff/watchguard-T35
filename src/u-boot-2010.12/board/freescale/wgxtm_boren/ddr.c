/*
 * Copyright 2009, 2011 Freescale Semiconductor, Inc.
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

#ifdef CONFIG_WGXTM_BOREN /* DDR Settings for  */
#if defined(CONFIG_WGXTM_T50_BOREN)
/* DDR setup for T50: DDR size = 2048M */
/* P1020 w/ 2G DDR3 */
#define CONFIG_SYS_DDR_CS0_BNDS			0x0000007F
#define CONFIG_SYS_DDR_CS0_CONFIG		0x80014402
#define CONFIG_SYS_DDR_CS0_CONFIG_2		0x00000000
#define CONFIG_SYS_DDR_INIT_ADDR		0x00000000
#define CONFIG_SYS_DDR_INIT_EXT_ADDR	0x00000000
#define CONFIG_SYS_DDR_MODE_CONTROL		0x00000000 
#define CONFIG_SYS_DDR_ZQ_CONTROL		0x89080600
#define CONFIG_SYS_DDR_WRLVL_CONTROL	0x8655F607
#define CONFIG_SYS_DDR_SR_CNTR		0x00000000
#define CONFIG_SYS_DDR_RCW_1		0x00000000
#define CONFIG_SYS_DDR_RCW_2		0x00000000
#define CONFIG_SYS_DDR_CONTROL		0xC70C0008	/* Type = DDR3*/
#define CONFIG_SYS_DDR_CONTROL_2	0x24401010
#define CONFIG_SYS_DDR_TIMING_4		0x00000001
#define CONFIG_SYS_DDR_TIMING_5		0x02401400

#define CONFIG_SYS_DDR_TIMING_3_400	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_400	0x00330004
#define CONFIG_SYS_DDR_TIMING_1_400	0x6F6B4846
#define CONFIG_SYS_DDR_TIMING_2_400	0x0FA8C8CF
#define CONFIG_SYS_DDR_CLK_CTRL_400	0x03000000
#define CONFIG_SYS_DDR_MODE_1_400	0x40461520
#define CONFIG_SYS_DDR_MODE_2_400	0x8000C000
#define CONFIG_SYS_DDR_INTERVAL_400	0x0C300000

#define CONFIG_SYS_DDR_TIMING_3_533	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_533	0x00330004
#define CONFIG_SYS_DDR_TIMING_1_533	0x6F6B4846
#define CONFIG_SYS_DDR_TIMING_2_533	0x0FA8C8CF
#define CONFIG_SYS_DDR_CLK_CTRL_533	0x03000000
#define CONFIG_SYS_DDR_MODE_1_533	0x40461520
#define CONFIG_SYS_DDR_MODE_2_533	0x8000C000
#define CONFIG_SYS_DDR_INTERVAL_533	0x0C300000

#define CONFIG_SYS_DDR_TIMING_3_667	0x00070000
#define CONFIG_SYS_DDR_TIMING_0_667	0x00330104
#define CONFIG_SYS_DDR_TIMING_1_667	0x6f6b0644
#define CONFIG_SYS_DDR_TIMING_2_667	0x0FA8910c/* tFAW as 17 clks */
#define CONFIG_SYS_DDR_CLK_CTRL_667	0x02800000
#define CONFIG_SYS_DDR_MODE_1_667	0x00061220
#define CONFIG_SYS_DDR_MODE_2_667	0x00000000
#define CONFIG_SYS_DDR_INTERVAL_667	0x0A280000//0x0A28028A

#define CONFIG_SYS_DDR_TIMING_3_800	0x00030000
#define CONFIG_SYS_DDR_TIMING_0_800	0x00330104
#define CONFIG_SYS_DDR_TIMING_1_800	0x6F6D8644
#define CONFIG_SYS_DDR_TIMING_2_800	0x0FA890CF
#define CONFIG_SYS_DDR_CLK_CTRL_800	0x02800000
#define CONFIG_SYS_DDR_MODE_1_800	0x00061420
#define CONFIG_SYS_DDR_MODE_2_800	0x00000000
#define CONFIG_SYS_DDR_INTERVAL_800	0x0C300000//0x0C30030C
#elif defined(CONFIG_WGXTM_T30_BOREN)	/* P1011, DDR setup for T30: DDR size = 1024M */
/* P1020 w/ 1G DDR3 */
#define CONFIG_SYS_DDR_CS0_BNDS			0x0000003F
#define CONFIG_SYS_DDR_CS0_CONFIG		0x80014302
#define CONFIG_SYS_DDR_CS0_CONFIG_2		0x00000000
#define CONFIG_SYS_DDR_INIT_ADDR		0x00000000
#define CONFIG_SYS_DDR_INIT_EXT_ADDR	0x00000000
#define CONFIG_SYS_DDR_MODE_CONTROL		0x00000000 
#define CONFIG_SYS_DDR_ZQ_CONTROL		0x89080600
#define CONFIG_SYS_DDR_WRLVL_CONTROL	0x8655F607
#define CONFIG_SYS_DDR_SR_CNTR		0x00000000
#define CONFIG_SYS_DDR_RCW_1		0x00000000
#define CONFIG_SYS_DDR_RCW_2		0x00000000
#define CONFIG_SYS_DDR_CONTROL		0xC70C0008	/* Type = DDR3*/
#define CONFIG_SYS_DDR_CONTROL_2	0x24401010
#define CONFIG_SYS_DDR_TIMING_4		0x00000001
#define CONFIG_SYS_DDR_TIMING_5		0x02401400

#define CONFIG_SYS_DDR_TIMING_3_400	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_400	0x00330004
#define CONFIG_SYS_DDR_TIMING_1_400	0x6F6B4846
#define CONFIG_SYS_DDR_TIMING_2_400	0x0FA8C8CF
#define CONFIG_SYS_DDR_CLK_CTRL_400	0x03000000
#define CONFIG_SYS_DDR_MODE_1_400	0x40461520
#define CONFIG_SYS_DDR_MODE_2_400	0x8000C000
#define CONFIG_SYS_DDR_INTERVAL_400	0x0C300000

#define CONFIG_SYS_DDR_TIMING_3_533	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_533	0x00330004
#define CONFIG_SYS_DDR_TIMING_1_533	0x6F6B4846
#define CONFIG_SYS_DDR_TIMING_2_533	0x0FA8C8CF
#define CONFIG_SYS_DDR_CLK_CTRL_533	0x03000000
#define CONFIG_SYS_DDR_MODE_1_533	0x40461520
#define CONFIG_SYS_DDR_MODE_2_533	0x8000C000
#define CONFIG_SYS_DDR_INTERVAL_533	0x0C300000

#define CONFIG_SYS_DDR_TIMING_3_667	0x00020000
#define CONFIG_SYS_DDR_TIMING_0_667	0x00330104
#define CONFIG_SYS_DDR_TIMING_1_667	0x5D5BD544
#define CONFIG_SYS_DDR_TIMING_2_667	0x0FA890D1/* tFAW as 17 clks */
#define CONFIG_SYS_DDR_CLK_CTRL_667	0x02800000
#define CONFIG_SYS_DDR_MODE_1_667	0x00061220
#define CONFIG_SYS_DDR_MODE_2_667	0x00000000
#define CONFIG_SYS_DDR_INTERVAL_667	0x0A280000//0x0A28028A

#define CONFIG_SYS_DDR_TIMING_3_800	0x00030000
#define CONFIG_SYS_DDR_TIMING_0_800	0x00330104
#define CONFIG_SYS_DDR_TIMING_1_800	0x6F6D8644
#define CONFIG_SYS_DDR_TIMING_2_800	0x0FA890CF
#define CONFIG_SYS_DDR_CLK_CTRL_800	0x02800000
#define CONFIG_SYS_DDR_MODE_1_800	0x00061420
#define CONFIG_SYS_DDR_MODE_2_800	0x00000000
#define CONFIG_SYS_DDR_INTERVAL_800	0x0C300000//0x0C30030C
#else //(_SC_DRAM_SIZE_ == 512)
#define CONFIG_SYS_DDR_CS0_BNDS         0x0000001F//ok
#define CONFIG_SYS_DDR_CS0_CONFIG       0x80014202//0x80414202//0x80014202//ok
#define CONFIG_SYS_DDR_CS0_CONFIG_2     0x00000000
#define CONFIG_SYS_DDR_INIT_ADDR        0x00000000
#define CONFIG_SYS_DDR_INIT_EXT_ADDR    0x00000000
#define CONFIG_SYS_DDR_MODE_CONTROL     0x00000000
#define CONFIG_SYS_DDR_ZQ_CONTROL       0x89080600
#define CONFIG_SYS_DDR_WRLVL_CONTROL    0x8655f604//0x8675f608//0x8655F608
#define CONFIG_SYS_DDR_SR_CNTR      0x00000000
#define CONFIG_SYS_DDR_RCW_1        0x00000000
#define CONFIG_SYS_DDR_RCW_2        0x00000000
#define CONFIG_SYS_DDR_CONTROL      0xc70c0008//0x470c0000//0xC7140008  /* Type = DDR3*/
#define CONFIG_SYS_DDR_CONTROL_2    0x24401010
#define CONFIG_SYS_DDR_TIMING_4     0x00000001
#define CONFIG_SYS_DDR_TIMING_5     0x01401400

#define CONFIG_SYS_DDR_TIMING_3_400 0x00020000
#define CONFIG_SYS_DDR_TIMING_0_400 0x00330004
#define CONFIG_SYS_DDR_TIMING_1_400 0x6F6B4846
#define CONFIG_SYS_DDR_TIMING_2_400 0x0FA8C8CF
#define CONFIG_SYS_DDR_CLK_CTRL_400 0x03000000
#define CONFIG_SYS_DDR_MODE_1_400   0x40461520
#define CONFIG_SYS_DDR_MODE_2_400   0x8000C000
#define CONFIG_SYS_DDR_INTERVAL_400 0x0C300000

#define CONFIG_SYS_DDR_TIMING_3_533 0x00030000
#define CONFIG_SYS_DDR_TIMING_0_533 0x00330004
#define CONFIG_SYS_DDR_TIMING_1_533 0x6F6B4846
#define CONFIG_SYS_DDR_TIMING_2_533 0x0FA8C8CF
#define CONFIG_SYS_DDR_CLK_CTRL_533 0x03000000
#define CONFIG_SYS_DDR_MODE_1_533   0x40461520
#define CONFIG_SYS_DDR_MODE_2_533   0x8000C000
#define CONFIG_SYS_DDR_INTERVAL_533 0x0C300000

#define CONFIG_SYS_DDR_TIMING_3_667 0x00020000//0x00010000
#define CONFIG_SYS_DDR_TIMING_0_667 0x40210104//0x00220104//0x00110104
#define CONFIG_SYS_DDR_TIMING_1_667 0x5c59e544//0x5d59e544//0x5C59D544
#define CONFIG_SYS_DDR_TIMING_2_667 0x0fa8910b//0x0fa89114//0x0fa8910f//0x0fa888cd//0x0FA8910A
#define CONFIG_SYS_DDR_CLK_CTRL_667 0x03000000//0x02800000
#define CONFIG_SYS_DDR_MODE_1_667   0x00461210//0x00041210//0x40461210//0x40061210//0x00441210//0x00061210
#define CONFIG_SYS_DDR_MODE_2_667   0x00000000
#define CONFIG_SYS_DDR_INTERVAL_667 0x0a28028a//0x0a280100//0x0A28028A//0x0A28028A

#define CONFIG_SYS_DDR_TIMING_3_800 0x00030000
#define CONFIG_SYS_DDR_TIMING_0_800 0x00330104
#define CONFIG_SYS_DDR_TIMING_1_800 0x6F6D8644
#define CONFIG_SYS_DDR_TIMING_2_800 0x0FA890CF
#define CONFIG_SYS_DDR_CLK_CTRL_800 0x02800000
#define CONFIG_SYS_DDR_MODE_1_800   0x00061420
#define CONFIG_SYS_DDR_MODE_2_800   0x00000000
#define CONFIG_SYS_DDR_INTERVAL_800 0x0C300000//0x0C30030C
#endif
#endif /* END_OF_AP331IB */

fsl_ddr_cfg_regs_t ddr_cfg_regs_400 = {
	.cs[0].bnds = CONFIG_SYS_DDR_CS0_BNDS,
	.cs[0].config = CONFIG_SYS_DDR_CS0_CONFIG,
	.cs[0].config_2 = CONFIG_SYS_DDR_CS0_CONFIG_2,
	.timing_cfg_3 = CONFIG_SYS_DDR_TIMING_3_400,
	.timing_cfg_0 = CONFIG_SYS_DDR_TIMING_0_400,
	.timing_cfg_1 = CONFIG_SYS_DDR_TIMING_1_400,
	.timing_cfg_2 = CONFIG_SYS_DDR_TIMING_2_400,
	.ddr_sdram_cfg = CONFIG_SYS_DDR_CONTROL,
	.ddr_sdram_cfg_2 = CONFIG_SYS_DDR_CONTROL_2,
	.ddr_sdram_mode = CONFIG_SYS_DDR_MODE_1_400,
	.ddr_sdram_mode_2 = CONFIG_SYS_DDR_MODE_2_400,
	.ddr_sdram_md_cntl = CONFIG_SYS_DDR_MODE_CONTROL,
	.ddr_sdram_interval = CONFIG_SYS_DDR_INTERVAL_400,
	.ddr_data_init = CONFIG_MEM_INIT_VALUE,
	.ddr_sdram_clk_cntl = CONFIG_SYS_DDR_CLK_CTRL_400,
	.ddr_init_addr = CONFIG_SYS_DDR_INIT_ADDR,
	.ddr_init_ext_addr = CONFIG_SYS_DDR_INIT_EXT_ADDR,
	.timing_cfg_4 = CONFIG_SYS_DDR_TIMING_4,
	.timing_cfg_5 = CONFIG_SYS_DDR_TIMING_5,
	.ddr_zq_cntl = CONFIG_SYS_DDR_ZQ_CONTROL,
	.ddr_wrlvl_cntl = CONFIG_SYS_DDR_WRLVL_CONTROL,
	.ddr_sr_cntr = CONFIG_SYS_DDR_SR_CNTR,
	.ddr_sdram_rcw_1 = CONFIG_SYS_DDR_RCW_1,
	.ddr_sdram_rcw_2 = CONFIG_SYS_DDR_RCW_2
};

fsl_ddr_cfg_regs_t ddr_cfg_regs_533 = {
	.cs[0].bnds = CONFIG_SYS_DDR_CS0_BNDS,
	.cs[0].config = CONFIG_SYS_DDR_CS0_CONFIG,
	.cs[0].config_2 = CONFIG_SYS_DDR_CS0_CONFIG_2,
	.timing_cfg_3 = CONFIG_SYS_DDR_TIMING_3_533,
	.timing_cfg_0 = CONFIG_SYS_DDR_TIMING_0_533,
	.timing_cfg_1 = CONFIG_SYS_DDR_TIMING_1_533,
	.timing_cfg_2 = CONFIG_SYS_DDR_TIMING_2_533,
	.ddr_sdram_cfg = CONFIG_SYS_DDR_CONTROL,
	.ddr_sdram_cfg_2 = CONFIG_SYS_DDR_CONTROL_2,
	.ddr_sdram_mode = CONFIG_SYS_DDR_MODE_1_533,
	.ddr_sdram_mode_2 = CONFIG_SYS_DDR_MODE_2_533,
	.ddr_sdram_md_cntl = CONFIG_SYS_DDR_MODE_CONTROL,
	.ddr_sdram_interval = CONFIG_SYS_DDR_INTERVAL_533,
	.ddr_data_init = CONFIG_MEM_INIT_VALUE,
	.ddr_sdram_clk_cntl = CONFIG_SYS_DDR_CLK_CTRL_533,
	.ddr_init_addr = CONFIG_SYS_DDR_INIT_ADDR,
	.ddr_init_ext_addr = CONFIG_SYS_DDR_INIT_EXT_ADDR,
	.timing_cfg_4 = CONFIG_SYS_DDR_TIMING_4,
	.timing_cfg_5 = CONFIG_SYS_DDR_TIMING_5,
	.ddr_zq_cntl = CONFIG_SYS_DDR_ZQ_CONTROL,
	.ddr_wrlvl_cntl = CONFIG_SYS_DDR_WRLVL_CONTROL,
	.ddr_sr_cntr = CONFIG_SYS_DDR_SR_CNTR,
	.ddr_sdram_rcw_1 = CONFIG_SYS_DDR_RCW_1,
	.ddr_sdram_rcw_2 = CONFIG_SYS_DDR_RCW_2
};

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
	.ddr_sdram_rcw_2 = CONFIG_SYS_DDR_RCW_2
};

fsl_ddr_cfg_regs_t ddr_cfg_regs_800 = {
	.cs[0].bnds = CONFIG_SYS_DDR_CS0_BNDS,
	.cs[0].config = CONFIG_SYS_DDR_CS0_CONFIG,
	.cs[0].config_2 = CONFIG_SYS_DDR_CS0_CONFIG_2,
	.timing_cfg_3 = CONFIG_SYS_DDR_TIMING_3_800,
	.timing_cfg_0 = CONFIG_SYS_DDR_TIMING_0_800,
	.timing_cfg_1 = CONFIG_SYS_DDR_TIMING_1_800,
	.timing_cfg_2 = CONFIG_SYS_DDR_TIMING_2_800,
	.ddr_sdram_cfg = CONFIG_SYS_DDR_CONTROL,
	.ddr_sdram_cfg_2 = CONFIG_SYS_DDR_CONTROL_2,
	.ddr_sdram_mode = CONFIG_SYS_DDR_MODE_1_800,
	.ddr_sdram_mode_2 = CONFIG_SYS_DDR_MODE_2_800,
	.ddr_sdram_md_cntl = CONFIG_SYS_DDR_MODE_CONTROL,
	.ddr_sdram_interval = CONFIG_SYS_DDR_INTERVAL_800,
	.ddr_data_init = CONFIG_MEM_INIT_VALUE,
	.ddr_sdram_clk_cntl = CONFIG_SYS_DDR_CLK_CTRL_800,
	.ddr_init_addr = CONFIG_SYS_DDR_INIT_ADDR,
	.ddr_init_ext_addr = CONFIG_SYS_DDR_INIT_EXT_ADDR,
	.timing_cfg_4 = CONFIG_SYS_DDR_TIMING_4,
	.timing_cfg_5 = CONFIG_SYS_DDR_TIMING_5,
	.ddr_zq_cntl = CONFIG_SYS_DDR_ZQ_CONTROL,
	.ddr_wrlvl_cntl = CONFIG_SYS_DDR_WRLVL_CONTROL,
	.ddr_sr_cntr = CONFIG_SYS_DDR_SR_CNTR,
	.ddr_sdram_rcw_1 = CONFIG_SYS_DDR_RCW_1,
	.ddr_sdram_rcw_2 = CONFIG_SYS_DDR_RCW_2
};

/*
 * Fixed sdram init -- doesn't use serial presence detect.
 */

phys_size_t fixed_sdram (void)
{
	char buf[32];
	fsl_ddr_cfg_regs_t ddr_cfg_regs;
	size_t ddr_size;
	struct cpu_type *cpu;
	ulong ddr_freq, ddr_freq_mhz;

	cpu = gd->cpu;
#if defined(CONFIG_WGXTM_T30_BOREN) || defined(CONFIG_WGXTM_T50_BOREN)
	/* fix DDR3 size for AP331IB */
	ddr_size = (CONFIG_SYS_SDRAM_SIZE * 1024 * 1024);
#else
	/* P1020 and it's derivatives support max 32bit DDR width */
	if (cpu->soc_ver == SVR_P1020 || cpu->soc_ver == SVR_P1020_E ||
		cpu->soc_ver == SVR_P1011 || cpu->soc_ver == SVR_P1011_E) {
		ddr_size = (CONFIG_SYS_SDRAM_SIZE * 1024 * 1024 / 2);
	} else {
		ddr_size = CONFIG_SYS_SDRAM_SIZE * 1024 * 1024;
	}
#endif /* END_OF_AP331IB */
#if defined(CONFIG_SYS_RAMBOOT)
	return ddr_size;
#endif
	ddr_freq = get_ddr_freq(0);
	ddr_freq_mhz = ddr_freq / 1000000;

	printf("Configuring DDR for %s MT/s data rate\n",
				strmhz(buf, ddr_freq));

	if(ddr_freq_mhz <= 400)
		memcpy(&ddr_cfg_regs, &ddr_cfg_regs_400, sizeof(ddr_cfg_regs));
	else if(ddr_freq_mhz <= 533)
		memcpy(&ddr_cfg_regs, &ddr_cfg_regs_533, sizeof(ddr_cfg_regs));
	else if(ddr_freq_mhz <= 667)
	{
		memcpy(&ddr_cfg_regs, &ddr_cfg_regs_667, sizeof(ddr_cfg_regs));
	}
	else if(ddr_freq_mhz <= 800)
		memcpy(&ddr_cfg_regs, &ddr_cfg_regs_800, sizeof(ddr_cfg_regs));
	else
		panic("Unsupported DDR data rate %s MT/s data rate\n",
					strmhz(buf, ddr_freq));

#if defined(CONFIG_WGXTM_T30_BOREN) || defined(CONFIG_WGXTM_T50_BOREN)
	/* disable DRAM Bus 32bit width support, we applied 16bit in AP331IB */
	/* P1020 and it's derivatives support max 32bit DDR width */
	if(cpu->soc_ver == SVR_P1020 || cpu->soc_ver == SVR_P1020_E ||
		cpu->soc_ver == SVR_P1011 || cpu->soc_ver == SVR_P1011_E) {
		ddr_cfg_regs.ddr_sdram_cfg |= SDRAM_CFG_32_BE;
		//ddr_cfg_regs.cs[0].bnds = 0x0000001F;
	}
#endif /* END_OF_AP331IB */
#if defined(CONFIG_WGXTM_T30_BOREN) || defined(CONFIG_WGXTM_T50_BOREN)
	ddr_cfg_regs.ddr_cdr1 = 0x80080000;//DDR_CDR1_DHC_EN;
	ddr_cfg_regs.ddr_cdr2 = 0x00000001;//DDR_CDR1_DHC_EN;
    printf("Set HW AUTO ODT\n");
    //out_be32(&ddr->timing_cfg_3, regs->timing_cfg_3);
	//ddr_cfg_regs.ddr_cdr1 |= DDR_CDR1_DHC_EN;
#endif
	fsl_ddr_set_memctl_regs(&ddr_cfg_regs, 0);
	set_ddr_laws(0, ddr_size, LAW_TRGT_IF_DDR_1);
	return ddr_size;
}
