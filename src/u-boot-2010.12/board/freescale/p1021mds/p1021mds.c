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
#include <hwconfig.h>
#include <pci.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_pci.h>
#include <asm/io.h>
#include <asm/mp.h>
#include <i2c.h>
#include <ioports.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <fsl_esdhc.h>
#include <tsec.h>
#include <netdev.h>

#ifdef CONFIG_QE
const qe_iop_conf_t qe_iop_conf_tab[] = {
	/* QE_MUX_MDC */
	{1,  19, 1, 0, 1}, /* QE_MUX_MDC	*/
	/* QE_MUX_MDIO */
	{1,  20, 3, 0, 1}, /* QE_MUX_MDIO	*/

	/* UCC_1_MII */
	{0, 23, 2, 0, 2}, /* CLK12 */
	{0, 24, 2, 0, 1}, /* CLK9 */
	{0,  7, 1, 0, 2}, /* ENET1_TXD0_SER1_TXD0      */
	{0,  9, 1, 0, 2}, /* ENET1_TXD1_SER1_TXD1      */
	{0, 11, 1, 0, 2}, /* ENET1_TXD2_SER1_TXD2      */
	{0, 12, 1, 0, 2}, /* ENET1_TXD3_SER1_TXD3      */
	{0,  6, 2, 0, 2}, /* ENET1_RXD0_SER1_RXD0      */
	{0, 10, 2, 0, 2}, /* ENET1_RXD1_SER1_RXD1      */
	{0, 14, 2, 0, 2}, /* ENET1_RXD2_SER1_RXD2      */
	{0, 15, 2, 0, 2}, /* ENET1_RXD3_SER1_RXD3      */
	{0,  5, 1, 0, 2}, /* ENET1_TX_EN_SER1_RTS_B    */
	{0, 13, 1, 0, 2}, /* ENET1_TX_ER               */
	{0,  4, 2, 0, 2}, /* ENET1_RX_DV_SER1_CTS_B    */
	{0,  8, 2, 0, 2}, /* ENET1_RX_ER_SER1_CD_B    */
	{0, 17, 2, 0, 2}, /* ENET1_CRS    */
	{0, 16, 2, 0, 2}, /* ENET1_COL    */

	/* UCC_5_RMII */
	{1, 11, 2, 0, 1}, /* CLK13 */
	{1, 7,  1, 0, 2}, /* ENET5_TXD0_SER5_TXD0      */
	{1, 10, 1, 0, 2}, /* ENET5_TXD1_SER5_TXD1      */
	{1, 6, 2, 0, 2}, /* ENET5_RXD0_SER5_RXD0      */
	{1, 9, 2, 0, 2}, /* ENET5_RXD1_SER5_RXD1      */
	{1, 5, 1, 0, 2}, /* ENET5_TX_EN_SER5_RTS_B    */
	{1, 4, 2, 0, 2}, /* ENET5_RX_DV_SER5_CTS_B    */
	{1, 8, 2, 0, 2}, /* ENET5_RX_ER_SER5_CD_B    */

	{0,  0, 0, 0, QE_IOP_TAB_END} /* END of table */
};
#endif

int board_early_init_f(void)
{

	fsl_lbc_t *lbc = LBC_BASE_ADDR;

#ifdef CONFIG_MMC
	ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);

	setbits_be32(&gur->pmuxcr,
		(MPC85xx_PMUXCR_SDHC_CD | MPC85xx_PMUXCR_SDHC_WP));
#endif

	/* Set ABSWP to implement conversion of addresses in the LBC */
	setbits_be32(&lbc->lbcr, CONFIG_SYS_LBC_LBCR);

	return 0;
}

int checkboard(void)
{
	printf("Board: P1021 MDS\n");

	return 0;
}

#ifdef CONFIG_PCI
void pci_init_board(void)
{
	fsl_pcie_init_board(0);
}
#endif

#ifdef CONFIG_TSEC_ENET
int board_eth_init(bd_t *bis)
{
	struct tsec_info_struct tsec_info[3];
	ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);
	int num = 0;

#ifdef CONFIG_TSEC1
	SET_STD_TSEC_INFO(tsec_info[num], 1);
	num++;
#endif

#ifdef CONFIG_TSEC2
	SET_STD_TSEC_INFO(tsec_info[num], 2);
	num++;
#endif

#ifdef CONFIG_TSEC3
	SET_STD_TSEC_INFO(tsec_info[num], 3);
	if (!(in_be32(&gur->pordevsr) & MPC85xx_PORDEVSR_SGMII3_DIS))
		tsec_info[num].flags |= TSEC_SGMII;
	num++;
#endif

	if (!num) {
		printf("No TSECs initialized\n");
		return 0;
	}

	tsec_eth_init(bis, tsec_info, num);

#if defined(CONFIG_UEC_ETH)
	/*  QE0 and QE3 need to be exposed for UCC1 and UCC5 Eth mode */
	setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_QE0);
	setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_QE3);

	uec_standard_init(bis);
#endif

	return pci_eth_init(bis);
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
void ft_board_setup(void *blob, bd_t *bd)
{
	phys_addr_t base;
	phys_size_t size;

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();

	fdt_fixup_memory(blob, base, size);

	FT_FSL_PCI_SETUP;

#ifdef CONFIG_QE
	do_fixup_by_compat(blob, "fsl,qe", "status", "okay",
			sizeof("okay"), 0);
#endif
}
#endif
