/*
 * drivers/net/phy/marvell.c
 *
 * Driver for Marvell PHYs
 *
 * Author: Andy Fleming
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * Copyright (c) 2013 Michael Stapelberg <michael@stapelberg.de>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/marvell_phy.h>
#include <linux/of.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

MODULE_DESCRIPTION("Marvell PHY SW driver");
MODULE_AUTHOR("Andy Fleming");
MODULE_LICENSE("GPL");



static int marvell_switch_init(void)
{
	return 0;
}

int marvell_switch_read_status(struct phy_device *phydev)
{
	phydev->link = 1;
	phydev->autoneg = AUTONEG_ENABLE ;
	phydev->speed = SPEED_1000;
	phydev->duplex = DUPLEX_FULL; 
	phydev->pause = 0;
	phydev->asym_pause = 0;
	return 0;
}
static int marvell_switch_config_aneg(struct phy_device *phydev)
{
	return 0;
}

static struct phy_driver marvell_sw_drivers = {
	.phy_id = 0x12345678,
	.phy_id_mask = 0xffffffff,
	.name = "Marvell SW",
	.features = 0,
	.config_init = &marvell_switch_init,
	.config_aneg = &marvell_switch_config_aneg,
	.read_status = &marvell_switch_read_status,
	.driver = { .owner = THIS_MODULE },
};


static int __init marvell_sw_init(void)
{
	int ret;
	ret = phy_driver_register(&marvell_sw_drivers);
	if (ret) {
		phy_driver_unregister(&marvell_sw_drivers);
		return ret;
	}
	return 0;
}
static void __exit marvell_sw_exit(void)
{
	phy_driver_unregister(&marvell_sw_drivers);
}

module_init(marvell_sw_init);
module_exit(marvell_sw_exit);

static struct mdio_device_id __maybe_unused marvell_sw_tbl[] = {
	{ 0x12345678, 0xffffffff},
	{ }
}

MODULE_DEVICE_TABLE(mdio, marvell_sw_tbl);
