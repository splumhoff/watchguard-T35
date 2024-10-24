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
 */

#include <common.h>
#include <i2c.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/fsl_law.h>
#include <asm/fsl_ddr_sdram.h>
#include <asm/fsl_ddr_dimm_params.h>

void fsl_ddr_board_options(memctl_options_t *popts,
				dimm_params_t *pdimm,
				unsigned int ctrl_num)
{
	/*
	 * Factors to consider for clock adjust:
	 */
	popts->clk_adjust = 6;

	/*
	 * Factors to consider for CPO:
	 */
	popts->cpo_override = 0x1f;

	/*
	 * Factors to consider for write data delay:
	 */
	popts->write_data_delay = 2;

	/*
	 * Factors to consider for half-strength driver enable:
	 */
	popts->half_strength_driver_enable = 1;

	/*
	 * Rtt and Rtt_WR override
	 */
	popts->rtt_override = 1;
	popts->rtt_override_value = DDR3_RTT_40_OHM; /* 40 Ohm rtt */
	popts->rtt_wr_override_value = 2; /* Rtt_WR */

	/* Write leveling override */
	popts->wrlvl_en = 1;
	popts->wrlvl_override = 1;
	popts->wrlvl_sample = 0xa;
	popts->wrlvl_start = 0x8;
	/*
	 * P1021 supports max 32-bit DDR width
	 */
	popts->data_bus_width = 1;

	/*
	 * disable on-the-fly burst chop mode for 32 bit data bus
	 */
	popts->OTF_burst_chop_en = 0;

	/*
	 * Set fixed 8 beat burst for 32 bit data bus
	 */
	popts->burst_length = DDR_BL8;
}
