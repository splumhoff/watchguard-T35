/*
 * (C) Copyright 2009
 * Marvell Semiconductor <www.marvell.com>
 * Prafulla Wadaskar <prafulla@marvell.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _MV88E61XX_H
#define _MV88E61XX_H

#include <miiphy.h>

#define MV88E61XX_CPU_PORT		0x5
#ifdef CONFIG_BOREN_ETHNET_SETUP
#ifndef MV88E61XX_MAX_PORTS_NUM
 #define MV88E61XX_MAX_PORTS_NUM	0x7		/* 88E6171R has 7 ports */
#endif
#define SC_MII_DEV_ADDR			0x10		/* Set by HW */
#define MV88E61XX_PHY_TIMEOUT		10
#define MV88E61XX_PORT_PHYCTRL_REG		0x1
#define MV88E61XX_PORT_PHYCTRL_REG_RXDELAY_BIT	15
#define MV88E61XX_PORT_PHYCTRL_REG_TXDELAY_BIT	14
#define SC_GPIO_SW_RST			13
#define SC_eTSEC1_PORT			6
#define SC_eTSEC3_PORT			5
#else
#ifndef MV88E61XX_MAX_PORTS_NUM
 #define MV88E61XX_MAX_PORTS_NUM	0x6
#endif
#define MV88E61XX_PHY_TIMEOUT		100000
#endif

/* port dev-addr (= port + 0x10) */
#define MV88E61XX_PRT_OFST		0x10
/* port registers */
#define MV88E61XX_PCS_CTRL_REG		0x1
#define MV88E61XX_PRT_CTRL_REG		0x4
#define MV88E61XX_PRT_VMAP_REG		0x6
#define MV88E61XX_PRT_VID_REG		0x7
#define MV88E61XX_RGMII_TIMECTRL_REG	0x1A

/* global registers dev-addr */
#define MV88E61XX_GLBREG_DEVADR	0x1B
/* global registers */
#define MV88E61XX_SGSR			0x00
#define MV88E61XX_SGCR			0x04

/* global 2 registers dev-addr */
#define MV88E61XX_GLB2REG_DEVADR	0x1C
/* global 2 registers */
#define MV88E61XX_PHY_CMD		0x18
#define MV88E61XX_PHY_DATA		0x19
/* global 2 phy commands */
#define MV88E61XX_PHY_WRITE_CMD		0x9400
#define MV88E61XX_PHY_READ_CMD		0x9800

#define MV88E61XX_BUSY_OFST		15
#define MV88E61XX_MODE_OFST		12
#define MV88E61XX_OP_OFST		10
#define MV88E61XX_ADDR_OFST		5

#ifdef CONFIG_MV88E61XX_MULTICHIP_ADRMODE
static int mv88e61xx_busychk_multic(char *name, u32 devaddr);
#ifdef CONFIG_BOREN_ETHNET_SETUP
void mv88e61xx_switch_write(char *name, u32 phy_adr,
	u32 reg_ofs, u16 data);
void mv88e61xx_switch_read(char *name, u32 phy_adr,
	u32 reg_ofs, u16 *data);
#else
static void mv88e61xx_switch_write(char *name, u32 phy_adr,
	u32 reg_ofs, u16 data);
static void mv88e61xx_switch_read(char *name, u32 phy_adr,
	u32 reg_ofs, u16 *data);
#endif
#define wr_switch_reg mv88e61xx_switch_write
#define rd_switch_reg mv88e61xx_switch_read
#else
/* switch appears a s simple PHY and can thus use miiphy */
#define wr_switch_reg miiphy_write
#define rd_switch_reg miiphy_read
#endif /* CONFIG_MV88E61XX_MULTICHIP_ADRMODE */

#endif /* _MV88E61XX_H */
