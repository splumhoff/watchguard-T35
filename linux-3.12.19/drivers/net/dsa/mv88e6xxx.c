/*
 * net/dsa/mv88e6xxx.c - Marvell 88e6xxx switch chip support
 * Copyright (c) 2008 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <net/dsa.h>
#include "mv88e6xxx.h"

#ifdef	CONFIG_WG_PLATFORM // WG:JB Base register for SMI commands
#define	SMI_CMD	wg_dsa_smi_reg
#endif

/* If the switch's ADDR[4:0] strap pins are strapped to zero, it will
 * use all 32 SMI bus addresses on its SMI bus, and all switch registers
 * will be directly accessible on some {device address,register address}
 * pair.  If the ADDR[4:0] pins are not strapped to zero, the switch
 * will only respond to SMI transactions to that specific address, and
 * an indirect addressing mechanism needs to be used to access its
 * registers.
 */
static int mv88e6xxx_reg_wait_ready(struct mii_bus *bus, int sw_addr)
{
	int ret;
	int i;

#ifdef CONFIG_WG_PLATFORM_PATCHES
	// WG:FL in order to avoid getting the invalid value of register, 
	// we need to increase the loop times of waiting 
	// and check the double hit times.
	int lasthit = 0;
	int doublehit = 0;
	for (i = 0; i < 100; i++) {
		if (doublehit >= 8)
			return 0;

		ret = mdiobus_read(bus, sw_addr, SMI_CMD|0);
		if (ret < 0)
			return ret;

		if ((ret & 0x8000) == 0) {
			if (lasthit == 0) {
				lasthit = 1;
				doublehit = 0;
			} else {
				doublehit++;
			}
		} else {
			lasthit = 0;
		}
	}
#else
	for (i = 0; i < 16; i++) {
		ret = mdiobus_read(bus, sw_addr, 0);
		if (ret < 0)
			return ret;

		if ((ret & 0x8000) == 0)
			return 0;
	}
#endif

	return -ETIMEDOUT;
}

int __mv88e6xxx_reg_read(struct mii_bus *bus, int sw_addr, int addr, int reg)
{
	int ret;

#ifdef	CONFIG_WG_PLATFORM	 // WG:JB MV88E6176
	if (SMI_CMD) sw_addr = REG_GLOBAL2;
	if ((sw_addr == 0) || ((SMI_CMD != 0) && (addr >= REG_PORT(0))))
#else
	if (sw_addr == 0)
#endif
		return mdiobus_read(bus, addr, reg);

#ifdef	CONFIG_WG_PLATFORM // WG:JB Dual connected switch
	if (likely(wg_dsa_count == 1)) sw_addr &= ~1;
#endif

	/* Wait for the bus to become free. */
	ret = mv88e6xxx_reg_wait_ready(bus, sw_addr);
	if (ret < 0)
		return ret;

	/* Transmit the read command. */
#ifdef	CONFIG_WG_PLATFORM	 // WG:JB MV88E6176
	ret = mdiobus_write(bus, sw_addr, SMI_CMD|0, 0x9800 | (DSA_PHY_MAP(addr) << 5) | reg);
#else
	ret = mdiobus_write(bus, sw_addr, 0, 0x9800 | (addr << 5) | reg);
#endif
	if (ret < 0)
		return ret;

	/* Wait for the read command to complete. */
	ret = mv88e6xxx_reg_wait_ready(bus, sw_addr);
	if (ret < 0)
		return ret;

	/* Read the data. */
#ifdef	CONFIG_WG_PLATFORM	 // WG:JB MV88E6176
	ret = mdiobus_read(bus, sw_addr, SMI_CMD|1);
#else
	ret = mdiobus_read(bus, sw_addr, 1);
#endif
	if (ret < 0)
		return ret;

	return ret & 0xffff;
}

int mv88e6xxx_reg_read(struct dsa_switch *ds, int addr, int reg)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int ret;

	mutex_lock(&ps->smi_mutex);
	ret = __mv88e6xxx_reg_read(ds->master_mii_bus,
				   ds->pd->sw_addr, addr, reg);
	mutex_unlock(&ps->smi_mutex);

	return ret;
}

int __mv88e6xxx_reg_write(struct mii_bus *bus, int sw_addr, int addr,
			  int reg, u16 val)
{
	int ret;

#ifdef	CONFIG_WG_PLATFORM	 // WG:JB MV88E6176
	if (SMI_CMD) sw_addr = REG_GLOBAL2;
	if ((sw_addr == 0) || ((SMI_CMD != 0) && (addr >= REG_PORT(0))))
#else
	if (sw_addr == 0)
#endif
		return mdiobus_write(bus, addr, reg, val);

#ifdef	CONFIG_WG_PLATFORM // WG:JB Dual connected switch
	if (likely(wg_dsa_count == 1)) sw_addr &= ~1;
#endif

	/* Wait for the bus to become free. */
	ret = mv88e6xxx_reg_wait_ready(bus, sw_addr);
	if (ret < 0)
		return ret;

	/* Transmit the data to write. */
#ifdef	CONFIG_WG_PLATFORM	 // WG:JB MV88E6176
	ret = mdiobus_write(bus, sw_addr, SMI_CMD|1, val);
#else
	ret = mdiobus_write(bus, sw_addr, 1, val);
#endif
	if (ret < 0)
		return ret;

	/* Transmit the write command. */
#ifdef	CONFIG_WG_PLATFORM	 // WG:JB MV88E6176
	ret = mdiobus_write(bus, sw_addr, SMI_CMD|0, 0x9400 | (DSA_PHY_MAP(addr) << 5) | reg);
#else
	ret = mdiobus_write(bus, sw_addr, 0, 0x9400 | (addr << 5) | reg);
#endif
	if (ret < 0)
		return ret;

	/* Wait for the write command to complete. */
	ret = mv88e6xxx_reg_wait_ready(bus, sw_addr);
	if (ret < 0)
		return ret;

	return 0;
}

int mv88e6xxx_reg_write(struct dsa_switch *ds, int addr, int reg, u16 val)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int ret;

	mutex_lock(&ps->smi_mutex);
	ret = __mv88e6xxx_reg_write(ds->master_mii_bus,
				    ds->pd->sw_addr, addr, reg, val);
	mutex_unlock(&ps->smi_mutex);

	return ret;
}

EXPORT_SYMBOL(__mv88e6xxx_reg_read);  // WG:JB Export Marvell register read
EXPORT_SYMBOL(__mv88e6xxx_reg_write); // WG:JB Export Marvell register write
EXPORT_SYMBOL(mv88e6xxx_reg_read);    // WG:JB Export Marvell register read
EXPORT_SYMBOL(mv88e6xxx_reg_write);   // WG:JB Export Marvell register write

int mv88e6xxx_config_prio(struct dsa_switch *ds)
{
	/* Configure the IP ToS mapping registers. */
	REG_WRITE(REG_GLOBAL, 0x10, 0x0000);
	REG_WRITE(REG_GLOBAL, 0x11, 0x0000);
	REG_WRITE(REG_GLOBAL, 0x12, 0x5555);
	REG_WRITE(REG_GLOBAL, 0x13, 0x5555);
	REG_WRITE(REG_GLOBAL, 0x14, 0xaaaa);
	REG_WRITE(REG_GLOBAL, 0x15, 0xaaaa);
	REG_WRITE(REG_GLOBAL, 0x16, 0xffff);
	REG_WRITE(REG_GLOBAL, 0x17, 0xffff);

	/* Configure the IEEE 802.1p priority mapping register. */
	REG_WRITE(REG_GLOBAL, 0x18, 0xfa41);

	return 0;
}

int mv88e6xxx_set_addr_direct(struct dsa_switch *ds, u8 *addr)
{
	REG_WRITE(REG_GLOBAL, 0x01, (addr[0] << 8) | addr[1]);
	REG_WRITE(REG_GLOBAL, 0x02, (addr[2] << 8) | addr[3]);
	REG_WRITE(REG_GLOBAL, 0x03, (addr[4] << 8) | addr[5]);

	return 0;
}

int mv88e6xxx_set_addr_indirect(struct dsa_switch *ds, u8 *addr)
{
	int i;
	int ret;

	for (i = 0; i < 6; i++) {
		int j;

		/* Write the MAC address byte. */
		REG_WRITE(REG_GLOBAL2, 0x0d, 0x8000 | (i << 8) | addr[i]);

		/* Wait for the write to complete. */
		for (j = 0; j < 16; j++) {
			ret = REG_READ(REG_GLOBAL2, 0x0d);
			if ((ret & 0x8000) == 0)
				break;
		}
		if (j == 16)
			return -ETIMEDOUT;
	}

	return 0;
}

int mv88e6xxx_phy_read(struct dsa_switch *ds, int addr, int regnum)
{
	if (addr >= 0)
		return mv88e6xxx_reg_read(ds, addr, regnum);
	return 0xffff;
}

int mv88e6xxx_phy_write(struct dsa_switch *ds, int addr, int regnum, u16 val)
{
	if (addr >= 0)
		return mv88e6xxx_reg_write(ds, addr, regnum, val);
	return 0;
}

#ifdef CONFIG_NET_DSA_MV88E6XXX_NEED_PPU
static int mv88e6xxx_ppu_disable(struct dsa_switch *ds)
{
	int ret;
	unsigned long timeout;

	ret = REG_READ(REG_GLOBAL, 0x04);
	REG_WRITE(REG_GLOBAL, 0x04, ret & ~0x4000);

	timeout = jiffies + 1 * HZ;
	while (time_before(jiffies, timeout)) {
		ret = REG_READ(REG_GLOBAL, 0x00);
		usleep_range(1000, 2000);
		if ((ret & 0xc000) != 0xc000)
			return 0;
	}

	return -ETIMEDOUT;
}

static int mv88e6xxx_ppu_enable(struct dsa_switch *ds)
{
	int ret;
	unsigned long timeout;

	ret = REG_READ(REG_GLOBAL, 0x04);
	REG_WRITE(REG_GLOBAL, 0x04, ret | 0x4000);

	timeout = jiffies + 1 * HZ;
	while (time_before(jiffies, timeout)) {
		ret = REG_READ(REG_GLOBAL, 0x00);
		usleep_range(1000, 2000);
		if ((ret & 0xc000) == 0xc000)
			return 0;
	}

	return -ETIMEDOUT;
}

static void mv88e6xxx_ppu_reenable_work(struct work_struct *ugly)
{
	struct mv88e6xxx_priv_state *ps;

	ps = container_of(ugly, struct mv88e6xxx_priv_state, ppu_work);
	if (mutex_trylock(&ps->ppu_mutex)) {
		struct dsa_switch *ds = ((struct dsa_switch *)ps) - 1;

		if (mv88e6xxx_ppu_enable(ds) == 0)
			ps->ppu_disabled = 0;
		mutex_unlock(&ps->ppu_mutex);
	}
}

static void mv88e6xxx_ppu_reenable_timer(unsigned long _ps)
{
	struct mv88e6xxx_priv_state *ps = (void *)_ps;

	schedule_work(&ps->ppu_work);
}

static int mv88e6xxx_ppu_access_get(struct dsa_switch *ds)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int ret;

	mutex_lock(&ps->ppu_mutex);

	/* If the PHY polling unit is enabled, disable it so that
	 * we can access the PHY registers.  If it was already
	 * disabled, cancel the timer that is going to re-enable
	 * it.
	 */
	if (!ps->ppu_disabled) {
		ret = mv88e6xxx_ppu_disable(ds);
		if (ret < 0) {
			mutex_unlock(&ps->ppu_mutex);
			return ret;
		}
		ps->ppu_disabled = 1;
	} else {
		del_timer(&ps->ppu_timer);
		ret = 0;
	}

	return ret;
}

static void mv88e6xxx_ppu_access_put(struct dsa_switch *ds)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);

	/* Schedule a timer to re-enable the PHY polling unit. */
	mod_timer(&ps->ppu_timer, jiffies + msecs_to_jiffies(10));
	mutex_unlock(&ps->ppu_mutex);
}

void mv88e6xxx_ppu_state_init(struct dsa_switch *ds)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);

	mutex_init(&ps->ppu_mutex);
	INIT_WORK(&ps->ppu_work, mv88e6xxx_ppu_reenable_work);
	init_timer(&ps->ppu_timer);
	ps->ppu_timer.data = (unsigned long)ps;
	ps->ppu_timer.function = mv88e6xxx_ppu_reenable_timer;
}

int mv88e6xxx_phy_read_ppu(struct dsa_switch *ds, int addr, int regnum)
{
	int ret;

	ret = mv88e6xxx_ppu_access_get(ds);
	if (ret >= 0) {
		ret = mv88e6xxx_reg_read(ds, addr, regnum);
		mv88e6xxx_ppu_access_put(ds);
	}

	return ret;
}

int mv88e6xxx_phy_write_ppu(struct dsa_switch *ds, int addr,
			    int regnum, u16 val)
{
	int ret;

	ret = mv88e6xxx_ppu_access_get(ds);
	if (ret >= 0) {
		ret = mv88e6xxx_reg_write(ds, addr, regnum, val);
		mv88e6xxx_ppu_access_put(ds);
	}

	return ret;
}
#endif

void mv88e6xxx_poll_link(struct dsa_switch *ds)
{
	int i;
#ifdef	CONFIG_WG_PLATFORM
	static int iff_up_map = 0;

	mutex_lock(&wg_dsa_mutex);

	if (wg_dsa_sgmii_poll) {
		int sw = ds->pd->sw_addr & 1;
		static int delay[2] = {3, 3};
		struct net_device* dev = ds->dst->master_netdev;

		// WG:JB Poll SGMII links if sw device up and at least one eth device is up
		if (likely(--delay[sw] <  0))
		if (likely((iff_up_map != 0) && (dev->flags & IFF_UP))) {
			if (unlikely(wg_dsa_debug & 64))
			printk(KERN_EMERG "%s: sw1%d Poll\n", __FUNCTION__, sw);

			if (wg_dsa_sgmii_poll(sw) <= 0)
				delay[sw] = 0;
			else {
				delay[sw] = 5;
				ds->drv->setup(ds);
			}
		}
	}
#endif

	for (i = 0; i < DSA_MAX_PORTS; i++) {
		struct net_device *dev;
		int uninitialized_var(port_status);
		int link;
		int speed;
		int duplex;
		int fc;

		dev = ds->ports[i];
		if (dev == NULL)
			continue;

		link = 0;
		if (dev->flags & IFF_UP) {
			port_status = mv88e6xxx_reg_read(ds, REG_PORT(i), 0x00);
			if (port_status < 0)
				continue;

#ifdef	CONFIG_WG_ARCH_X86_64
			iff_up_map |= (1 << i);

			if (wg_dsa_type >= 61900)
			{
				static unsigned long last_port_reset[DSA_MAX_PORTS] = {0};

				// Glitch if no PHY found so check PHY itself for status
				while ((port_status & 0x100F) != 0x100F) {
					int phy_status = mv88e6xxx_reg_read(ds, i, 0x11);

					port_status = 0x300F;

					if (!(phy_status & 0x0400)) break; // Link+Speed
					port_status |= 0x0800;
					port_status |= ((phy_status >> 6) & 0x0300);

					if (!(phy_status & 0x2000)) break; // Duplex
					port_status |= 0x0400;

					if (!(phy_status & 0x0300)) break; // Pause
					port_status |= 0x8000;
				}

				if (unlikely(last_port_reset[i] == 0))
					last_port_reset[i] = jiffies;

				if (  likely((port_status & 0x380F) == 0x180F))
					last_port_reset[i] = jiffies;
				else
				if (unlikely((jiffies - last_port_reset[i]) > (30*HZ))) {

					last_port_reset[i] = jiffies;

					if (000&000) if (!(port_status & 0x0800))
					{
						mv88e6xxx_reg_write(ds, REG_PORT(i), 4, 0x003C);
						mv88e6xxx_reg_write(ds,		 i,  0, 0x9140);
						mv88e6xxx_reg_write(ds, REG_PORT(i), 4, 0x003F);

						if (unlikely(wg_dsa_debug & 32))
						printk(KERN_EMERG "%s: Port %2d Reset\n",
						       __FUNCTION__, i);
					}

					if (unlikely(wg_dsa_debug & 32))
					printk(KERN_EMERG "%s: Port %2d Status %4X\n",
					       __FUNCTION__, i, port_status);
				}
			}
#endif
#ifdef	CONFIG_WG_PLATFORM_PATCHES
#ifdef	CONFIG_WG_ARCH_FREESCALE
			// WG:JB this should be in chelan but it seems to make
			// make it unstable???
			if (wg_cpu_model != WG_CPU_P2020)
#endif
			{
			if (i >= 0 && i < DSA_PHY) {
			// WG:JB For 88E6176 The Port Status Register Bit[3:0]
			// is Config Mode, b'1111 means Internal PHY (port0 to
			//  port4 only) it should always be b'1111
				if (wg_dsa_type >= 61760) {
					if ((port_status & 0xF) != 0xF)
						continue;
				}
#ifdef	CONFIG_WG_ARCH_FREESCALE
			// WG:FL ignore the invalid data read from hardware.
			// see Table37 in 88E6171R_88E6171_Datasheet_P2-Rev0-03
			// the Port Status Register Bit[2:0] is Config Mode,
			// b'100  means Internal PHY (port0 to port4 only)
			// it should always be b'100
				else
				if (wg_dsa_type <  61760) {
					if ((port_status & 0x7) != 0x4)
						continue;
				}
#endif
			}

			// in order to de-bounce the change of port status, 
			// we always ignore the changes in the zigzag point!
			// record the last port status for each port
			{
				static int last_port_status[DSA_MAX_PORTS] = {0, };

				// debounce: ignore the change in the zigzag point.
				if (last_port_status[i]) {
					if (last_port_status[i] != port_status) {
						last_port_status[i] = port_status;
						continue;
					}
				} else {
					last_port_status[i] = port_status;
				}
			}
			}
#endif
			link = !!(port_status & 0x0800);
		}

		if (!link) {
			if (netif_carrier_ok(dev)) {
				netdev_info(dev, "link down\n");
				netif_carrier_off(dev);
			}
			continue;
		}

		switch (port_status & 0x0300) {
		case 0x0000:
			speed = 10;
			break;
		case 0x0100:
			speed = 100;
			break;
		case 0x0200:
			speed = 1000;
			break;
		default:
			speed = -1;
			break;
		}
		duplex = (port_status & 0x0400) ? 1 : 0;
		fc = (port_status & 0x8000) ? 1 : 0;

		if (!netif_carrier_ok(dev)) {
			netdev_info(dev,
				    "link up, %d Mb/s, %s duplex, flow control %sabled\n",
				    speed,
				    duplex ? "full" : "half",
				    fc ? "en" : "dis");
			netif_carrier_on(dev);
		}
	}

#ifdef	CONFIG_WG_PLATFORM
	mutex_unlock(&wg_dsa_mutex);
#endif
}

static int mv88e6xxx_stats_wait(struct dsa_switch *ds)
{
	int ret;
	int i;

	for (i = 0; i < 10; i++) {
		ret = REG_READ(REG_GLOBAL, 0x1d);
		if ((ret & 0x8000) == 0)
			return 0;
	}

	return -ETIMEDOUT;
}

static int mv88e6xxx_stats_snapshot(struct dsa_switch *ds, int port)
{
	int ret;

	/* Snapshot the hardware statistics counters for this port. */
#ifdef	CONFIG_WG_PLATFORM // WG:XD FBX-10004 Stats register bits vary
	if (unlikely(wg_dsa_type >= 61900))
	REG_WRITE(REG_GLOBAL, 0x1d, 0xd000 | ((DSA_PHY_MAP(port) + 1) << 5));
	else
	if (unlikely(wg_dsa_type >= 61760))
	REG_WRITE(REG_GLOBAL, 0x1d, 0xdc00 | ((DSA_PHY_MAP(port) + 1) << 5));
	else
#endif
	REG_WRITE(REG_GLOBAL, 0x1d, 0xdc00 | port);

	/* Wait for the snapshotting to complete. */
	ret = mv88e6xxx_stats_wait(ds);
	if (ret < 0)
		return ret;

	return 0;
}

static void mv88e6xxx_stats_read(struct dsa_switch *ds, int stat, u32 *val)
{
	u32 _val;
	int ret;

	*val = 0;

#ifdef	CONFIG_WG_PLATFORM // WG:JB Stats register bits vary
	if (unlikely(wg_dsa_type > 61760))
	ret = mv88e6xxx_reg_write(ds, REG_GLOBAL, 0x1d, 0xc000 | stat);
	else
#endif
	ret = mv88e6xxx_reg_write(ds, REG_GLOBAL, 0x1d, 0xcc00 | stat);
	if (ret < 0)
		return;

	ret = mv88e6xxx_stats_wait(ds);
	if (ret < 0)
		return;

	ret = mv88e6xxx_reg_read(ds, REG_GLOBAL, 0x1e);
	if (ret < 0)
		return;

	_val = ret << 16;

	ret = mv88e6xxx_reg_read(ds, REG_GLOBAL, 0x1f);
	if (ret < 0)
		return;

	*val = _val | ret;
}

void mv88e6xxx_get_strings(struct dsa_switch *ds,
			   int nr_stats, struct mv88e6xxx_hw_stat *stats,
			   int port, uint8_t *data)
{
	int i;

	for (i = 0; i < nr_stats; i++) {
		memcpy(data + i * ETH_GSTRING_LEN,
		       stats[i].string, ETH_GSTRING_LEN);
	}
}

void mv88e6xxx_get_ethtool_stats(struct dsa_switch *ds,
				 int nr_stats, struct mv88e6xxx_hw_stat *stats,
				 int port, uint64_t *data)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int ret;
	int i;

	mutex_lock(&ps->stats_mutex);

	ret = mv88e6xxx_stats_snapshot(ds, port);
	if (ret < 0) {
		mutex_unlock(&ps->stats_mutex);
		return;
	}

	/* Read each of the counters. */
	for (i = 0; i < nr_stats; i++) {
		struct mv88e6xxx_hw_stat *s = stats + i;
		u32 low;
		u32 high;

		mv88e6xxx_stats_read(ds, s->reg, &low);
		if (s->sizeof_stat == 8)
			mv88e6xxx_stats_read(ds, s->reg + 1, &high);
		else
			high = 0;

		data[i] = (((u64)high) << 32) | low;
	}

	mutex_unlock(&ps->stats_mutex);
}

static int __init mv88e6xxx_init(void)
{
#if IS_ENABLED(CONFIG_NET_DSA_MV88E6131)
	register_switch_driver(&mv88e6131_switch_driver);
#endif
#if IS_ENABLED(CONFIG_NET_DSA_MV88E6123_61_65)
	register_switch_driver(&mv88e6123_61_65_switch_driver);
#endif
	return 0;
}
module_init(mv88e6xxx_init);

static void __exit mv88e6xxx_cleanup(void)
{
#if IS_ENABLED(CONFIG_NET_DSA_MV88E6123_61_65)
	unregister_switch_driver(&mv88e6123_61_65_switch_driver);
#endif
#if IS_ENABLED(CONFIG_NET_DSA_MV88E6131)
	unregister_switch_driver(&mv88e6131_switch_driver);
#endif
}
module_exit(mv88e6xxx_cleanup);

MODULE_AUTHOR("Lennert Buytenhek <buytenh@wantstofly.org>");
MODULE_DESCRIPTION("Driver for Marvell 88E6XXX ethernet switch chips");
MODULE_LICENSE("GPL");
