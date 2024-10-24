/*
 * QorIQ 10G MDIO Controller
 *
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * Authors: Andy Fleming <afleming@freescale.com>
 *          Timur Tabi <timur@freescale.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/of_platform.h>
#include <linux/of_mdio.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#if 1 
struct mii_bus *sw_mdio_bus;
#endif


/* Number of microseconds to wait for a register to respond */
#define TIMEOUT	1000

struct tgec_mdio_controller {
	__be32	reserved[12];
	__be32	mdio_stat;	/* MDIO configuration and status */
	__be32	mdio_ctl;	/* MDIO control */
	__be32	mdio_data;	/* MDIO data */
	__be32	mdio_addr;	/* MDIO address */
} __packed;

/* Taken from memac_mdio.c */
#define MDIO_STAT_ENC (1 << 6)
#define MDIO_STAT_HOLD_15_CLK (7 << 2)

#define MDIO_STAT_CLKDIV(x)	(((x>>1) & 0xff) << 8)
#define MDIO_STAT_BSY		(1 << 0)
#define MDIO_STAT_RD_ER		(1 << 1)
#define MDIO_CTL_DEV_ADDR(x) 	(x & 0x1f)
#define MDIO_CTL_PORT_ADDR(x)	((x & 0x1f) << 5)
#define MDIO_CTL_PRE_DIS	(1 << 10)
#define MDIO_CTL_SCAN_EN	(1 << 11)
#define MDIO_CTL_POST_INC	(1 << 14)
#define MDIO_CTL_READ		(1 << 15)

#define MDIO_DATA(x)		(x & 0xffff)
#define MDIO_DATA_BSY		(1 << 31)

/*
 * Wait untill the MDIO bus is free
 */
static int xgmac_wait_until_free(struct device *dev,
				 struct tgec_mdio_controller __iomem *regs)
{
	uint32_t status;

	/* Wait till the bus is free */
	status = spin_event_timeout(
		!((in_be32(&regs->mdio_stat)) & MDIO_STAT_BSY), TIMEOUT, 0);
	if (!status) {
		dev_err(dev, "timeout waiting for bus to be free\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Wait till the MDIO read or write operation is complete
 */
static int xgmac_wait_until_done(struct device *dev,
				 struct tgec_mdio_controller __iomem *regs)
{
	uint32_t status;

	/* Wait till the MDIO write is complete */
	status = spin_event_timeout(
		!((in_be32(&regs->mdio_data)) & MDIO_DATA_BSY), TIMEOUT, 0);
	if (!status) {
		dev_err(dev, "timeout waiting for operation to complete\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Write value to the PHY for this device to the register at regnum,waiting
 * until the write is done before it returns.  All PHY configuration has to be
 * done through the TSEC1 MIIM regs.
 */
static int xgmac_mdio_write(struct mii_bus *bus, int phy_id, int regnum, u16 value)
{
	struct tgec_mdio_controller __iomem *regs = bus->priv;
	uint16_t dev_addr;
	u32 mdio_ctl, mdio_stat;
	int ret;

	mdio_stat = in_be32(&regs->mdio_stat);
	if (regnum & MII_ADDR_C45) {
		/* Clause 45 (ie 10G) */
		dev_addr = (regnum >> 16) & 0x1f;
		mdio_stat |= MDIO_STAT_ENC;
	} else {
		/* Clause 22 (ie 1G) */
		dev_addr = regnum & 0x1f;
		mdio_stat &= ~MDIO_STAT_ENC;
	}

	out_be32(&regs->mdio_stat, mdio_stat);

	ret = xgmac_wait_until_free(&bus->dev, regs);
	if (ret)
		return ret;

	/* Set the port and dev addr */
	mdio_ctl = MDIO_CTL_PORT_ADDR(phy_id) | MDIO_CTL_DEV_ADDR(dev_addr);
	out_be32(&regs->mdio_ctl, mdio_ctl);

	/* Set the register address */
	if (regnum & MII_ADDR_C45) {
		out_be32(&regs->mdio_addr, regnum & 0xffff);

		ret = xgmac_wait_until_free(&bus->dev, regs);
		if (ret)
			return ret;
	}

	/* Write the value to the register */
	out_be32(&regs->mdio_data, MDIO_DATA(value));

	ret = xgmac_wait_until_done(&bus->dev, regs);
	if (ret)
		return ret;

	return 0;
}

/*
 * Reads from register regnum in the PHY for device dev, returning the value.
 * Clears miimcom first.  All PHY configuration has to be done through the
 * TSEC1 MIIM regs.
 */
static int xgmac_mdio_read(struct mii_bus *bus, int phy_id, int regnum)
{
	struct tgec_mdio_controller __iomem *regs = bus->priv;
	uint16_t dev_addr;
	uint32_t mdio_stat;
	uint32_t mdio_ctl;
	uint16_t value;
	int ret;

	mdio_stat = in_be32(&regs->mdio_stat);
	if (regnum & MII_ADDR_C45) {
		dev_addr = (regnum >> 16) & 0x1f;
		mdio_stat |= MDIO_STAT_ENC;
	} else {
		dev_addr = regnum & 0x1f;
		mdio_stat = ~MDIO_STAT_ENC;
	}

	out_be32(&regs->mdio_stat, mdio_stat);

	ret = xgmac_wait_until_free(&bus->dev, regs);
	if (ret)
		return ret;

	/* Set the Port and Device Addrs */
	mdio_ctl = MDIO_CTL_PORT_ADDR(phy_id) | MDIO_CTL_DEV_ADDR(dev_addr);
	out_be32(&regs->mdio_ctl, mdio_ctl);

	/* Set the register address */
	if (regnum & MII_ADDR_C45) {
		out_be32(&regs->mdio_addr, regnum & 0xffff);

		ret = xgmac_wait_until_free(&bus->dev, regs);
		if (ret)
			return ret;
	}

	/* Initiate the read */
	out_be32(&regs->mdio_ctl, mdio_ctl | MDIO_CTL_READ);

	ret = xgmac_wait_until_done(&bus->dev, regs);
	if (ret)
		return ret;

	/* Return all Fs if nothing was there */
	if (in_be32(&regs->mdio_stat) & MDIO_STAT_RD_ER) {
		dev_err(&bus->dev, "MDIO read error\n");
		return 0xffff;
	}

	value = in_be32(&regs->mdio_data) & 0xffff;
	dev_dbg(&bus->dev, "read %04x\n", value);

	return value;
}





/* Reset the MIIM registers, and wait for the bus to free */
static int xgmac_mdio_reset(struct mii_bus *bus)
{
	struct tgec_mdio_controller __iomem *regs = bus->priv;
	int ret;

	mutex_lock(&bus->mdio_lock);

	/* Setup the MII Mgmt clock speed */
	clrbits32(&regs->mdio_stat, MDIO_STAT_ENC);

	ret = xgmac_wait_until_free(&bus->dev, regs);

	mutex_unlock(&bus->mdio_lock);

	return ret;
}


#if 1

#define SMIBusy               0x8000
#define SMIBusy_Active        0x8000
#define SMIBusy_R_VALID       0x8000
#define SMIMode_22    (1 << 12)
#define SMIMode_45    (0 << 12)
#define SMIOp_R       (2<<10)
#define SMIOp_W       (1<<10)
#define SMIDevAddr(n) (((n)&0x1f) <<5)
#define SMIRegAddr(n) ((n)&0x1f)

#define SMI_CMD               0x0
#define SMI_DATA      0x1
#define Timeout_smi   0x1000

#define SMI_Global_1  0x1b
#define SMI_Global_1_GC 0x04

int cvm_oct_88e6171_read(struct mii_bus *new_bus, int phy_id, int phy_addr, int phy_reg, uint16_t *offset)
{
        int i;
        uint16_t data = 0;
        uint16_t regData = 0;

        for( i=0 ; i < Timeout_smi ; i++ )
        {
                data =new_bus->read(new_bus, phy_id, SMI_CMD);
                if (!(data & SMIBusy))
                        break;
        }

        if(i == Timeout_smi)
        {
                printk("Time Out\n");
                return -1;
        }

        regData = ( SMIDevAddr(phy_addr) | SMIRegAddr(phy_reg) | SMIOp_R | SMIMode_22 | SMIBusy_Active ) ;

        new_bus->write(new_bus, phy_id, SMI_CMD, regData);

        for( i=0 ; i < Timeout_smi ; i++ )
        {
                data =new_bus->read(new_bus, phy_id, SMI_CMD);
                if (!(data & SMIBusy_R_VALID))
                        break;
        }

        if(i == Timeout_smi)
        {
                printk("Time Out\n");
                return -1;
        }
        *offset =new_bus->read(new_bus, phy_id, SMI_DATA);
        return 0;
}


int cvm_oct_88e6171_write(struct mii_bus *new_bus, int phy_id, int phy_addr, int phy_reg, uint16_t regdata)
{
        int i;
        uint16_t regData = 0;
        uint16_t data = 0;

        for( i=0 ; i < Timeout_smi ; i++ )
        {
                data =new_bus->read(new_bus, phy_id, SMI_CMD);
                if (!(data & SMIBusy))
                        break;
        }

        if(i == Timeout_smi)
                return -1;
        regData = ( SMIDevAddr(phy_addr) | SMIRegAddr(phy_reg) | SMIOp_W | SMIMode_22 | SMIBusy_Active ) ;

        new_bus->write(new_bus, phy_id, SMI_DATA, regdata);
        new_bus->write(new_bus, phy_id, SMI_CMD, regData);

        for( i=0 ; i < Timeout_smi ; i++ )
        {
                data =new_bus->read(new_bus, phy_id, SMI_CMD);
                if (!(data & SMIBusy_R_VALID))
                        break;
        }

        if(i == Timeout_smi)
                return -1;
        return 0;
}

int cvm_oct_88e6171_port_status(int phy_id, int port , uint16_t reg ,uint16_t *port_status)
{
        struct mii_bus *dev;
        int err = 0;
        uint16_t regData = 0;
        uint16_t regTmp = 0;
        uint16_t regStatus = 0;
        dev = sw_mdio_bus;
        err = cvm_oct_88e6171_read ( dev, phy_id, SMI_Global_1 , SMI_Global_1_GC , &regData );
        if( err < 0)
                return -1;
        regTmp = regData & 0xbfff;
        cvm_oct_88e6171_write ( dev, phy_id, SMI_Global_1 , SMI_Global_1_GC , regTmp );
        udelay(100);
        err = cvm_oct_88e6171_read ( dev, phy_id, port , reg , &regStatus );
        udelay(100);
        cvm_oct_88e6171_write ( dev, phy_id, SMI_Global_1 , SMI_Global_1_GC , regData );
        *port_status = regStatus ;
        return err ;
}


int cvm_oct_88e6171_port_write(int phy_id, int port , uint16_t reg ,uint16_t regdata)
{
        struct mii_bus *dev;
        int err;
        uint16_t regData = 0;
        uint16_t regTmp = 0;
        dev = sw_mdio_bus;
        err = cvm_oct_88e6171_read ( dev, phy_id, SMI_Global_1 , SMI_Global_1_GC , &regData );
        if( err < 0)
                return -1;
        regTmp = regData & 0xbfff;
        cvm_oct_88e6171_write ( dev, phy_id, SMI_Global_1 , SMI_Global_1_GC , regTmp );
        udelay(100);
        cvm_oct_88e6171_write ( dev, phy_id, port , reg , regdata );
        udelay(100);
        cvm_oct_88e6171_write ( dev, phy_id, SMI_Global_1 , SMI_Global_1_GC , regData );
        return err ;
}

struct _PHY_6171 {
	unsigned char phy_id;
        unsigned short port;
        unsigned short reg;
        unsigned short regdata;
};
struct _SW_6171 {
	unsigned char phy_id;
        uint16_t devaddr;
        uint16_t regaddr;
        uint16_t regdata;
};

int cvm_oct_switch_ioctl( struct file *filp,unsigned int cmd, unsigned long arg)
{
        struct mii_bus *dev;
        struct _SW_6171 *sw_6171;
        struct _PHY_6171 *phy_6171;
        int status;
        dev = sw_mdio_bus;

        switch(cmd){
        case 0:
                phy_6171 = (struct _PHY_6171 *)arg;
                status = cvm_oct_88e6171_port_status(phy_6171->phy_id, phy_6171->port, phy_6171->reg ,&phy_6171->regdata);
                if(status < 0)
                {
                        printk("Get Phy data failed \n");
                        return -EFAULT;
                }
        break;
        case 1:
                phy_6171 = (struct _PHY_6171 *)arg;
                status = cvm_oct_88e6171_port_write(phy_6171->phy_id, phy_6171->port, phy_6171->reg ,phy_6171->regdata);
                if(status < 0)
                {
                        printk("Get Phy data failed \n");
                        return -EFAULT;
                }
        break;
        case 2:
                sw_6171 = (struct _SW_6171 *)arg;
                status = cvm_oct_88e6171_read ( dev, sw_6171->phy_id, sw_6171->devaddr , sw_6171->regaddr , &sw_6171->regdata );
                if(status < 0)
                {
                        printk("Get SW data failed \n");
                        return -EFAULT;
                }
        break;
        case 3:
                sw_6171 = (struct _SW_6171 *)arg;
                status = cvm_oct_88e6171_write ( dev, sw_6171->phy_id, sw_6171->devaddr , sw_6171->regaddr , sw_6171->regdata );
                if(status < 0)
                {
                        printk("Set SW data failed \n");
                        return -EFAULT;
                }
        break;
        }
        return 0;
}
#if 1
static int cvm_oct_stats_show(struct seq_file *m, void *v)
{
        return 0;
}

#endif
static int cvm_oct_stats_open(struct inode *inode, struct file *file)
{
      return single_open(file, cvm_oct_stats_show, NULL);
}

static struct file_operations cvm_oct_stats_operations = {
        .owner =            THIS_MODULE,
        .unlocked_ioctl         = cvm_oct_switch_ioctl,
        .open           = cvm_oct_stats_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

#endif



static int xgmac_mdio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mii_bus *bus;
	struct resource res;
	int ret;
#if 1
        u64 addr = 0;
	int err=0;
#endif


	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		dev_err(&pdev->dev, "could not obtain address\n");
		return ret;
	}

	bus = mdiobus_alloc_size(PHY_MAX_ADDR * sizeof(int));
	if (!bus)
		return -ENOMEM;

	bus->name = "Freescale XGMAC MDIO Bus";
	bus->read = xgmac_mdio_read;
	bus->write = xgmac_mdio_write;
	bus->reset = xgmac_mdio_reset;
	bus->irq = bus->priv;
	bus->parent = &pdev->dev;
	snprintf(bus->id, MII_BUS_ID_SIZE, "%llx", (unsigned long long)res.start);

	/* Set the PHY base address */
	bus->priv = of_iomap(np, 0);
	if (!bus->priv) {
		ret = -ENOMEM;
		goto err_ioremap;
	}
#if 1
        /* Set the PHY base address */
        addr = (unsigned long long)res.start;
#endif

	ret = of_mdiobus_register(bus, np);
	if (ret) {
		dev_err(&pdev->dev, "cannot register MDIO bus\n");
		goto err_registration;
	}

	platform_set_drvdata(pdev, bus);

#if 1
        //printk("addr =%llx aaa %s  \n",addr,bus->id);
        //if (addr == 0xffe24000) {
        if (addr == 0xffe4fc000) {
                sw_mdio_bus = bus;
                err = register_chrdev(248, "88e6171_drv", &cvm_oct_stats_operations);
        }
#endif


	return 0;

err_registration:
	iounmap(bus->priv);

err_ioremap:
	mdiobus_free(bus);

	return ret;
}

static int xgmac_mdio_remove(struct platform_device *pdev)
{
	struct mii_bus *bus = platform_get_drvdata(pdev);

	mdiobus_unregister(bus);
	iounmap(bus->priv);
	mdiobus_free(bus);
	unregister_chrdev(248, "88e6171_drv");

	return 0;
}

static struct of_device_id xgmac_mdio_match[] = {
	{
		.compatible = "fsl,fman-xmdio",
	},
	{
		.compatible = "fsl,fman-memac-mdio",
	},
	{},
};
MODULE_DEVICE_TABLE(of, xgmac_mdio_match);

static struct platform_driver xgmac_mdio_driver = {
	.driver = {
		.name = "fsl-fman_xmdio",
		.of_match_table = xgmac_mdio_match,
	},
	.probe = xgmac_mdio_probe,
	.remove = xgmac_mdio_remove,
};

module_platform_driver(xgmac_mdio_driver);

MODULE_DESCRIPTION("Freescale QorIQ 10G MDIO Controller");
MODULE_LICENSE("GPL v2");
