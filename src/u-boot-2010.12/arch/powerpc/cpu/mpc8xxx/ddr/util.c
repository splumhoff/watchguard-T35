/*
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 as published by the Free Software Foundation.
 */

#include <common.h>
#include <asm/fsl_law.h>

#include "ddr.h"

/*
 * Round mclk_ps to nearest 10 ps in memory controller code.
 *
 * If an imprecise data rate is too high due to rounding error
 * propagation, compute a suitably rounded mclk_ps to compute
 * a working memory controller configuration.
 */
unsigned int get_memory_clk_period_ps(void)
{
	unsigned int mclk_ps;

	mclk_ps = 2000000000000ULL / get_ddr_freq(0);
	/* round to nearest 10 ps */
	return 10 * ((mclk_ps + 5) / 10);
}

/* Convert picoseconds into DRAM clock cycles (rounding up if needed). */
unsigned int picos_to_mclk(unsigned int picos)
{
	const unsigned long long ULL_2e12 = 2000000000000ULL;
	const unsigned long long ULL_8Fs = 0xFFFFFFFFULL;
	unsigned long long clks;
	unsigned long long clks_temp;

	if (!picos)
		return 0;

	clks = get_ddr_freq(0) * (unsigned long long) picos;
	clks_temp = clks;
	clks = clks / ULL_2e12;
	if (clks_temp % ULL_2e12) {
		clks++;
	}

	if (clks > ULL_8Fs) {
		clks = ULL_8Fs;
	}

	return (unsigned int) clks;
}

unsigned int mclk_to_picos(unsigned int mclk)
{
	return get_memory_clk_period_ps() * mclk;
}

void
__fsl_ddr_set_lawbar(const common_timing_params_t *memctl_common_params,
			   unsigned int memctl_interleaved,
			   unsigned int ctrl_num)
{
	unsigned long long base = memctl_common_params->base_address;
	unsigned long long size = memctl_common_params->total_mem;

	/*
	 * If no DIMMs on this controller, do not proceed any further.
	 */
	if (!memctl_common_params->ndimms_present) {
		return;
	}

#if !defined(CONFIG_PHYS_64BIT)
	if (base >= CONFIG_MAX_MEM_MAPPED)
		return;
	if ((base + size) >= CONFIG_MAX_MEM_MAPPED)
		size = CONFIG_MAX_MEM_MAPPED - base;
#endif

	if (ctrl_num == 0) {
		/*
		 * Set up LAW for DDR controller 1 space.
		 */
		unsigned int lawbar1_target_id = memctl_interleaved
			? LAW_TRGT_IF_DDR_INTRLV : LAW_TRGT_IF_DDR_1;

		if (set_ddr_laws(base, size, lawbar1_target_id) < 0) {
			printf("%s: ERROR (ctrl #0, intrlv=%d)\n", __func__,
				memctl_interleaved);
			return ;
		}
	} else if (ctrl_num == 1) {
		if (set_ddr_laws(base, size, LAW_TRGT_IF_DDR_2) < 0) {
			printf("%s: ERROR (ctrl #1)\n", __func__);
			return ;
		}
	} else {
		printf("%s: unexpected DDR controller number (%u)\n", __func__,
			ctrl_num);
	}
}

__attribute__((weak, alias("__fsl_ddr_set_lawbar"))) void
fsl_ddr_set_lawbar(const common_timing_params_t *memctl_common_params,
			 unsigned int memctl_interleaved,
			 unsigned int ctrl_num);

void board_add_ram_info(int use_default)
{
#if defined(CONFIG_MPC85xx)
	volatile ccsr_ddr_t *ddr = (void *)(CONFIG_SYS_MPC85xx_DDR_ADDR);
#elif defined(CONFIG_MPC86xx)
	volatile ccsr_ddr_t *ddr = (void *)(CONFIG_SYS_MPC86xx_DDR_ADDR);
#endif
#if (CONFIG_NUM_DDR_CONTROLLERS > 1)
	uint32_t cs0_config = in_be32(&ddr->cs0_config);
#endif
	uint32_t sdram_cfg = in_be32(&ddr->sdram_cfg);
	int cas_lat;

	puts(" (DDR");
	switch ((sdram_cfg & SDRAM_CFG_SDRAM_TYPE_MASK) >>
		SDRAM_CFG_SDRAM_TYPE_SHIFT) {
	case SDRAM_TYPE_DDR1:
		puts("1");
		break;
	case SDRAM_TYPE_DDR2:
		puts("2");
		break;
	case SDRAM_TYPE_DDR3:
		puts("3");
		break;
	default:
		puts("?");
		break;
	}

	if (sdram_cfg & SDRAM_CFG_32_BE)
		puts(", 32-bit");
	else if (sdram_cfg & SDRAM_CFG_16_BE)
		puts(", 16-bit");
	else
		puts(", 64-bit");

	/* Calculate CAS latency based on timing cfg values */
	cas_lat = ((in_be32(&ddr->timing_cfg_1) >> 16) & 0xf) + 1;
	if ((in_be32(&ddr->timing_cfg_3) >> 12) & 1)
		cas_lat += (8 << 1);
	printf(", CL=%d", cas_lat >> 1);
	if (cas_lat & 0x1)
		puts(".5");

	if (sdram_cfg & SDRAM_CFG_ECC_EN)
		puts(", ECC on)");
	else
		puts(", ECC off)");

#if (CONFIG_NUM_DDR_CONTROLLERS > 1)
	if (cs0_config & 0x20000000) {
		puts("\n");
		puts("       DDR Controller Interleaving Mode: ");

		switch ((cs0_config >> 24) & 0xf) {
		case FSL_DDR_CACHE_LINE_INTERLEAVING:
			puts("cache line");
			break;
		case FSL_DDR_PAGE_INTERLEAVING:
			puts("page");
			break;
		case FSL_DDR_BANK_INTERLEAVING:
			puts("bank");
			break;
		case FSL_DDR_SUPERBANK_INTERLEAVING:
			puts("super-bank");
			break;
		default:
			puts("invalid");
			break;
		}
	}
#endif

	if ((sdram_cfg >> 8) & 0x7f) {
		puts("\n");
		puts("       DDR Chip-Select Interleaving Mode: ");
		switch(sdram_cfg >> 8 & 0x7f) {
		case FSL_DDR_CS0_CS1_CS2_CS3:
			puts("CS0+CS1+CS2+CS3");
			break;
		case FSL_DDR_CS0_CS1:
			puts("CS0+CS1");
			break;
		case FSL_DDR_CS2_CS3:
			puts("CS2+CS3");
			break;
		case FSL_DDR_CS0_CS1_AND_CS2_CS3:
			puts("CS0+CS1 and CS2+CS3");
			break;
		default:
			puts("invalid");
			break;
		}
	}
}
