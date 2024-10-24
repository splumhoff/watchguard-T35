/*
 * Marvell PHY drivers
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * author Andy Fleming
 */
#include <config.h>
#include <common.h>
#include <phy.h>

static int sw_startup(struct phy_device *phydev)
{
	phydev->link = 1;
	phydev->duplex = DUPLEX_FULL;
	phydev->speed = SPEED_1000;
	return 0;

}

/* Marvell 88E1111S */
static int sw_config(struct phy_device *phydev)
{

	return 0;
}

static struct phy_driver SW_driver = {
	.name = "Marvell 88E6171",
	.uid = 0x12345678,
	.mask = 0xffffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &sw_config,
	.startup = &sw_startup,
	.shutdown = &genphy_shutdown,
};


int phy_marvell_sw_init(void)
{
	phy_register(&SW_driver);

	return 0;
}
