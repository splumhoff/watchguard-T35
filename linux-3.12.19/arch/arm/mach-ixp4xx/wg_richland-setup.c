/*
 * arch/arm/mach-ixp4xx/richland-setup.c
 *
 * IXP435 Richland (XTM3) board-setup
 * Code based on ixdp425-setup.c
 *
 * Copyright (C) 2003-2005 MontaVista Software, Inc.
 * Copyright (C) 2010 WatchGuard, Inc.
 *
 * Author: Deepak Saxena <dsaxena@plexity.net>
 * Author: Bryan Hundven <bryan.hundven@watchguard.com>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand-gpio.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>

/* Board Specific GPIO PINs */
#define RICHLAND_GPIO_NAND_CS		(10)
#define RICHLAND_GPIO_NAND_ALE		(9)
#define RICHLAND_GPIO_NAND_CLE		(8)
#define RICHLAND_GPIO_NAND_RB		(7)
#define RICHLAND_GPIO_SDA		5
#define RICHLAND_GPIO_SCL		4

/* NAND Partition Sizes */
#define ONEMEG 1024*1024
#define P1OFS 0*ONEMEG
#define P1SIZE 4*ONEMEG
#define P2OFS P1OFS + P1SIZE
#define P2SIZE 124*ONEMEG
#define P3OFS P2OFS + P2SIZE
#define P3SIZE 100*ONEMEG
#define P4OFS P3OFS + P3SIZE
#define P4SIZE 4*ONEMEG
#define P5OFS P4OFS + P4SIZE
#define P5SIZE 24*ONEMEG

/* NOR Resources */
static struct resource richland_nor_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct mtd_partition richland_nor_parts[] = {
	{
		.name   = "Redboot",
		.offset = 0x00000000,
		/*
		 * The Redboot 2.04 binary that we use for Richland is bigger
		 * than 0x50000 bytes.  It's actually just less than 0x80000
		 * bytes, so we're shuffling the allotment between this
		 * partition and cfg0 and cfg1.  As of 2009-08-18, cfg0 uses
		 * ~37kB of the 192kB allotted to it, and cfg1 appears unused.
		 *
		 * We're shrinking cfg1, but leaving it in place to avoid
		 * having to shuffle the device nodes in WG Linux, and just in
		 * case a useful use case for it actually comes up.
		 *
		 * Note that all the partition sizes must be multiples of the
		 * erase block, which is 0x10000.
		 */
		.size   = 0x00080000
	}, {
		.name   = "cfg0",
		.offset = MTDPART_OFS_APPEND,
		.size   = 0x00020000
	}, {
		.name   = "cfg1",
		.offset = MTDPART_OFS_APPEND,
		.size   = 0x00010000
	}, {
		.name   = "mfg",
		.offset = MTDPART_OFS_APPEND,
		.size   = 0x00010000
	}, {
		.name   = "bootOpt",
		.offset = MTDPART_OFS_APPEND,
		.size   = 0x00010000
	}, {
		.name   = "RedbootConfig",
		.offset = 0x000e0000,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct flash_platform_data richland_nor_data[] = {
	{
		.map_name	= "cfi_probe",
		.width		= 2,
		.parts		= richland_nor_parts,
		.nr_parts	= ARRAY_SIZE(richland_nor_parts),
	},
};

static struct platform_device richland_nor = {
	.name		= "IXP4XX-Flash",
	.id		= 0,
	.dev		= {
		.platform_data = richland_nor_data,
	},
	.resource	= &richland_nor_resource,
	.num_resources	= 1,
};

/* GPIO NAND Resources */
static struct resource richland_nand_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct mtd_partition richland_nand_parts[] = {
	{
		.name	= "SysA Kernel",
		.offset	= P1OFS,
		.size	= P1SIZE
	}, {
		.name	= "SysA Code",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= P2SIZE
	}, {
		.name	= "SysA Data",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= P3SIZE
	}, {
		.name	= "SysB Kernel",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= P4SIZE
	}, {
		.name	= "SysB Code",
		.offset	= MTDPART_OFS_NXTBLK,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct gpio_nand_platdata richland_nand_data[] = {
	{
		.gpio_nce = RICHLAND_GPIO_NAND_CS,
		.gpio_cle = RICHLAND_GPIO_NAND_CLE,
		.gpio_ale = RICHLAND_GPIO_NAND_ALE,
		.gpio_rdy = RICHLAND_GPIO_NAND_RB,
		.gpio_nwp = -1,
		.parts = richland_nand_parts,
		.num_parts = ARRAY_SIZE(richland_nand_parts),
		.chip_delay = 40, /* Samsung new revision of NAND chips used in Richland need 40 micro src delay */
	},
};

static struct platform_device richland_nand = {
	.name		= "gpio-nand",
	.id		= -1,
	.dev		= {
		.platform_data = richland_nand_data,
	},
	.resource	= &richland_nand_resource,
	.num_resources	= 1,
};

/* I2C GPIO Resources */
static struct i2c_gpio_platform_data richland_i2c_gpio_data = {
	.sda_pin	= RICHLAND_GPIO_SDA,
	.scl_pin	= RICHLAND_GPIO_SCL,
	.udelay		= 10, 	/* RTL8366S(R)-GR_DataSheet_1 0: PAGE 81 t1 timing 
				   is 4ms, but linux i2c stack divides this by 2.
				   so doubling it. ?? */
	.timeout	= HZ,	/* some smbus/i2c devices need more time to respond.
				   raise the timeout (wait longer than normal(HZ/10) or
				   until it actually gets high) */
};

static struct platform_device richland_i2c_gpio = {
	.name		= "i2c-gpio",
	.id		= 0,
	.dev		= {
		.platform_data	= &richland_i2c_gpio_data,
	},
};

/* s-35390a Seiko RTC */
static struct i2c_board_info __initdata richland_i2c_rtc = {
	I2C_BOARD_INFO("s35390a", 0x30),
};

#define	IRQ_IXP4XX_USB_HOST0 (IRQ_IXP4XX_USB_HOST+0)
#define	IRQ_IXP4XX_USB_HOST1 (IRQ_IXP4XX_USB_HOST+1)
// WG:JB Note: HOST1 overlaps with IRQ_IXP4XX_USB_I2C is this OK?

/* USB Resources */
static struct resource richland_usb0_resources[] = {
	{
		.start	= 0xCD000000,
		.end	= 0xCD000300,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_IXP4XX_USB_HOST0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource richland_usb1_resources[] = {
	{
		.start	= 0xCE000000,
		.end	= 0xCE000300,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_IXP4XX_USB_HOST1,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 ehci_dma_mask = ~(u32)0;

static struct platform_device richland_usb0_device =  {
	.name		= "ixp4xx-ehci",
	.id		= 0,
	.resource	= richland_usb0_resources,
	.num_resources	= ARRAY_SIZE(richland_usb0_resources),
	.dev		= {
		.dma_mask		= &ehci_dma_mask,
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct platform_device richland_usb1_device = {
	.name		= "ixp4xx-ehci",
	.id		= 1,
	.resource	= richland_usb1_resources,
	.num_resources	= ARRAY_SIZE(richland_usb1_resources),
	.dev		= {
		.dma_mask		= &ehci_dma_mask,
		.coherent_dma_mask	= 0xffffffff,
	},
};

/* Serial UART Resources */
static struct resource richland_uart_resources[] = {
	{
		.start		= IXP4XX_UART1_BASE_PHYS,
		.end		= IXP4XX_UART1_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM
	},
};

static struct plat_serial8250_port richland_uart_data[] = {
	{
		.mapbase	= IXP4XX_UART1_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART1_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART1,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{ },
};

static struct platform_device richland_uart = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev.platform_data	= richland_uart_data,
	.num_resources		= ARRAY_SIZE(richland_uart_data),
	.resource		= richland_uart_resources
};

/* Built-in 10/100 Ethernet MAC interfaces */
static struct eth_plat_info richland_plat_eth[] = {
	{
		.phy		= 0,
		.rxq		= 3,
		.txreadyq	= 20,
	}, {
		.phy		= 1,
		.rxq		= 4,
		.txreadyq	= 21,
	}
};

static struct platform_device richland_eth[] = {
	{
		.name			= "ixp4xx_eth",
		.id			= IXP4XX_ETH_NPEB,
		.dev.platform_data	= richland_plat_eth,
	}, {
		.name			= "ixp4xx_eth",
		.id			= IXP4XX_ETH_NPEC,
		.dev.platform_data	= richland_plat_eth + 1,
	}
};

static struct platform_device *richland_devices[] __initdata = {
	&richland_i2c_gpio,
	&richland_nor,
	&richland_nand,
	&richland_uart,
	&richland_eth[0],
	&richland_eth[1],
	&richland_usb0_device,
	&richland_usb1_device,
};

static void __init richland_init(void)
{
	ixp4xx_sys_init();

	// NOR Flash on CS0
	richland_nor_resource.start	= IXP4XX_EXP_BUS_BASE(0);
	richland_nor_resource.end	= IXP4XX_EXP_BUS_END(0);

	// NAND Flash on CS1
	richland_nand_resource.start	= IXP4XX_EXP_BUS_BASE(1);
	richland_nand_resource.end	= IXP4XX_EXP_BUS_END(1);

	// Init serial uart
	richland_uart.num_resources = 1;
	richland_uart_data[1].flags = 0;

	// Register Seiko S-35390a RTC
	platform_add_devices(richland_devices, ARRAY_SIZE(richland_devices));
	i2c_register_board_info(0, &richland_i2c_rtc, 1);
}

#ifdef CONFIG_WG_ARCH_IXP4XX
MACHINE_START(KIXRP435, "WatchGuard XTM2 Platform (Richland)")
	/* Maintainer: WatchGuard, Inc. */
	.phys_io	= IXP4XX_PERIPHERAL_BASE_PHYS,
	.io_pg_offst	= ((IXP4XX_PERIPHERAL_BASE_VIRT) >> 18) & 0xfffc,
	.map_io		= ixp4xx_map_io,
	.init_irq	= ixp4xx_init_irq,
	.timer		= &ixp4xx_timer,
	.boot_params	= 0x0100,
	.init_machine	= richland_init,
MACHINE_END
#endif
