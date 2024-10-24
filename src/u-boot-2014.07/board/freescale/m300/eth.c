/*
 * Copyright 2014 Freescale Semiconductor, Inc.
 *
 * Shengzhou Liu <Shengzhou.Liu@freescale.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <netdev.h>
#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_law.h>
#include <asm/fsl_serdes.h>
#include <asm/fsl_portals.h>
#include <asm/fsl_liodn.h>
#include <malloc.h>
#include <fm_eth.h>
#include <fsl_mdio.h>
#include <miiphy.h>
#include <phy.h>
#include <asm/fsl_dtsec.h>
#include <asm/fsl_serdes.h>

int board_eth_init(bd_t *bis)
{
#if defined(CONFIG_FMAN_ENET)
	struct memac_mdio_info dtsec_mdio_info;

	unsigned short value;


	dtsec_mdio_info.regs =
		(struct memac_mdio_controller *)CONFIG_SYS_FM1_DTSEC_MDIO_ADDR;

	dtsec_mdio_info.name = DEFAULT_FM_MDIO_NAME;

	/* Register the 1G MDIO bus */
	fm_memac_mdio_init(bis, &dtsec_mdio_info);


	/* Set the two on-board RGMII PHY address */
	fm_info_set_phy_address(FM1_DTSEC3, RGMII_PHY1_ADDR);
	fm_info_set_phy_address(FM1_DTSEC4, RGMII_PHY2_ADDR);
	fm_info_set_phy_address(FM1_DTSEC10, 0);
	fm_info_set_phy_address(FM1_DTSEC1, 1);
	fm_info_set_phy_address(FM1_DTSEC2, 2);

	fm_info_set_mdio(FM1_DTSEC10, miiphy_get_dev_by_name(DEFAULT_FM_MDIO_NAME));
	fm_info_set_mdio(FM1_DTSEC1, miiphy_get_dev_by_name(DEFAULT_FM_MDIO_NAME));
	fm_info_set_mdio(FM1_DTSEC2, miiphy_get_dev_by_name(DEFAULT_FM_MDIO_NAME));
	fm_info_set_mdio(FM1_DTSEC3, miiphy_get_dev_by_name(DEFAULT_FM_MDIO_NAME));
	fm_info_set_mdio(FM1_DTSEC4, miiphy_get_dev_by_name(DEFAULT_FM_MDIO_NAME));
	fm_disable_port(FM1_DTSEC6);

	cpu_eth_init(bis);

	const char      *devname;
	devname = miiphy_get_current_dev();
	miiphy_write(devname, 0x10, 0, 0x9a03);
	miiphy_read(devname,  0x10, 1, &value);
	if ( (value&0xffff) != 0x1712 ) {
		printf("Error!!!! switch setting incorrect, check H/W\n");
	}
	else
	{
		printf("Init switch to forwarding mode... \n");
		/* Reset and Disable INTx  */
		DLAY SMIRW(1, 0x10, 4, 0xe000, 0, 0, 0x1b, 0);

		/* set rgmii delay  */
		DLAY SMIRW(1, 0x10, 1, 0xc03e, 0, 0, 0x15, 0);  /* P15 */
		DLAY SMIRW(1, 0x10, 1, 0xc03e, 0, 0, 0x16, 0);  /* P16 */
		/* set LED function */
		DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x10, 0);/* P0 */
		DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x11, 0);/* P1 */
		DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x12, 0);/* P1 */
		DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x13, 0);/* P3 */
		DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x14, 0);/* P4 */
		/* set port to forwarding mode */
		DLAY SMIRW(1, 0x10, 4, 0x007f, 0, 0, 0x10, 0);
		DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x11, 0);
		DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x12, 0);
		DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x13, 0);
		DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x14, 0);
		DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x15, 0);
		DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x16, 0);
		/* reset PHY */
		DLAY
		DLAY
		DLAY
	}

#endif /* CONFIG_FMAN_ENET */

	return pci_eth_init(bis);
}

void fdt_fixup_board_enet(void *fdt)
{
	return;
}
