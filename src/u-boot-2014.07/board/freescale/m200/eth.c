/*
 * Copyright 2014 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <netdev.h>
#include <asm/immap_85xx.h>
#include <fm_eth.h>
#include <fsl_mdio.h>
#include <malloc.h>
#include <asm/fsl_dtsec.h>
#include <miiphy.h>

#include "../common/fman.h"

int board_eth_init(bd_t *bis)
{
#ifdef CONFIG_FMAN_ENET
	struct memac_mdio_info memac_mdio_info;
	unsigned int i;
	int phy_addr = 0;
	unsigned short value;
#endif
	printf("Initializing Fman\n");

	memac_mdio_info.regs =
		(struct memac_mdio_controller *)CONFIG_SYS_FM1_DTSEC_MDIO_ADDR;
	memac_mdio_info.name = DEFAULT_FM_MDIO_NAME;

	/* Register the real 1G MDIO bus */
	fm_memac_mdio_init(bis, &memac_mdio_info);

	/*
	 * Program on board RGMII, SGMII PHY addresses.
	 */
	for (i = FM1_DTSEC1; i < FM1_DTSEC1 + CONFIG_SYS_NUM_FM1_DTSEC; i++) {
		int idx = i - FM1_DTSEC1;

		switch (fm_info_get_enet_if(i)) {
		case PHY_INTERFACE_MODE_SGMII:
			if ( i == FM1_DTSEC1 )
			fm_info_set_phy_address(FM1_DTSEC1, 1);
			else if ( i == FM1_DTSEC2 )
			fm_info_set_phy_address(FM1_DTSEC2, 2);
			else if ( i == FM1_DTSEC3 )
			fm_info_set_phy_address(FM1_DTSEC3, 0);
			break;
		case PHY_INTERFACE_MODE_RGMII:
			if (FM1_DTSEC4 == i)
				phy_addr = CONFIG_SYS_RGMII1_PHY_ADDR;
			if (FM1_DTSEC5 == i)
				phy_addr = CONFIG_SYS_RGMII2_PHY_ADDR;
			fm_info_set_phy_address(i, phy_addr);
			break;
		case PHY_INTERFACE_MODE_QSGMII:
			fm_info_set_phy_address(i, 0);
			break;
		case PHY_INTERFACE_MODE_NONE:
			fm_info_set_phy_address(i, 0);
			break;
		default:
			printf("Fman1: DTSEC%u set to unknown interface %i\n",
			       idx + 1, fm_info_get_enet_if(i));
			fm_info_set_phy_address(i, 0);
			break;
		}
		fm_info_set_mdio(i,
				 miiphy_get_dev_by_name(DEFAULT_FM_MDIO_NAME));
	}

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
		printf("Init switch to forwarding mode... ");
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
		DLAY
		DLAY
		DLAY
		printf("Done\n");
	}

	return pci_eth_init(bis);
}
