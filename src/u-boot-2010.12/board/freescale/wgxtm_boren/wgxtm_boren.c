/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_serdes.h>
#include <asm/io.h>
#include <miiphy.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <tsec.h>
#include <vsc7385.h>
#include <netdev.h>
#include <rtc.h>
#include <i2c.h>
#include <hwconfig.h>

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_WGXTM_BOREN) /* CONFIG_WGXTM_BOREN */
#define PCIE_RST_SET		0x00010000	/* 15 */
#define RGMII_PHY_RST_SET   0x00400000  /* 13 */
#define DDR_RST_SET         0x00020000  /* 14 */
#define USB2_PORT_OUT_EN    0x01000000

#define USB_RST_CLR         0x00080000  /* 12 */
#define USB_HUB_RST         0x00200000  /* 12 */

#define REG_SRDSCR3  	    0xffee300c

#define GPIO_DIR            0x007f0000

//#define BOARD_PERI_RST_SET  (PCIE_RST_SET | RGMII_PHY_RST_SET )
//#define BOARD_PERI_RST_SET  (PCIE_RST_SET | RGMII_PHY_RST_SET | DDR_RST_SET)
#define BOARD_PERI_RST_SET  ( RGMII_PHY_RST_SET | DDR_RST_SET | USB_HUB_RST)

#else
#define VSC7385_RST_SET		0x00080000
#define SLIC_RST_SET		0x00040000
#define SGMII_PHY_RST_SET	0x00020000
#define PCIE_RST_SET		0x00010000
#define RGMII_PHY_RST_SET	0x02000000

#define USB_RST_CLR		0x04000000
#define USB2_PORT_OUT_EN        0x01000000

#define GPIO_DIR		0x060f0000

#define BOARD_PERI_RST_SET	VSC7385_RST_SET | SLIC_RST_SET | \
				SGMII_PHY_RST_SET | PCIE_RST_SET | \
				RGMII_PHY_RST_SET
#endif /* end of CONFIG_WGXTM_BOREN */

#define SYSCLK_MASK	0x00200000
#define BOARDREV_MASK	0x10100000
#define BOARDREV_C	0x00100000
#define BOARDREV_D	0x00000000

#define SYSCLK_66	66666666
#define SYSCLK_100	100000000

unsigned long get_board_sys_clk(ulong dummy)
{
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);
	u32 val_gpdat, sysclk_gpio;
#if defined(CONFIG_WGXTM_BOREN)
	return SYSCLK_66;
#else
	val_gpdat = in_be32(&pgpio->gpdat);
	sysclk_gpio = val_gpdat & SYSCLK_MASK;

	if(sysclk_gpio == 0)
		return SYSCLK_66;
	else
		return SYSCLK_100;
#endif /* end of CONFIG_WGXTM_BOREN */
	return 0;
}

#ifdef CONFIG_MMC
int board_early_init_f (void)
{
	volatile ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);

	setbits_be32(&gur->pmuxcr,
			(MPC85xx_PMUXCR_SDHC_CD |
			 MPC85xx_PMUXCR_SDHC_WP));
	return 0;
}
#endif

int checkboard (void)
{

	u32 val_gpdat, board_rev_gpio;
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);
	unsigned int reg_val;
#ifndef CONFIG_WGXTM_BOREN
	char board_rev = 0;
	struct cpu_type *cpu;
#endif
	val_gpdat = in_be32(&pgpio->gpdat);
	board_rev_gpio = val_gpdat & BOARDREV_MASK;
#if defined(CONFIG_WGXTM_BOREN)
	printf("Board: Boren \n");
#else
	if (board_rev_gpio == BOARDREV_C)
		board_rev = 'C';
	else if (board_rev_gpio == BOARDREV_D)
		board_rev = 'D';
	else
		panic ("Unexpected Board REV %x detected!!\n", board_rev_gpio);

	cpu = gd->cpu;
	printf ("Board: %sRDB Rev%c\n", cpu->name, board_rev);
#endif /* end of CONFIG_WGXTM_BOREN */
#ifdef CONFIG_PHYS_64BIT
	puts ("(36-bit addrmap) \n");
#endif

    /* fix PCIE detect error with writing SRDSCR3*/
    out_be32((volatile unsigned *)REG_SRDSCR3,0x00001313);


    setbits_be32(&pgpio->gpdir, GPIO_DIR);

/*
 * Bringing the following peripherals out of reset via GPIOs
 * 0 = reset and 1 = out of reset
 * GPIO12 - Reset to Ethernet Switch
 * GPIO13 - Reset to SLIC/SLAC devices
 * GPIO14 - Reset to SGMII_PHY_N
 * GPIO15 - Reset to PCIe slots
 * GPIO6  - Reset to RGMII PHY
 * GPIO5  - Reset to USB3300 devices 1 = reset and 0 = out of reset
 */
#if defined(CONFIG_WGXTM_BOREN) && !defined(CONFIG_SDCARD)
	//clrsetbits_be32(&pgpio->gpdat, BOARD_PERI_RST_SET,USB_RST_CLR);
	//udelay(100000); 
	//clrsetbits_be32(&pgpio->gpdat, USB_RST_CLR, BOARD_PERI_RST_SET); 
	//udelay(100000); 
	clrsetbits_be32(&pgpio->gpdat, BOARD_PERI_RST_SET,USB_RST_CLR);
	udelay(100000); 
	clrsetbits_be32(&pgpio->gpdat, USB_RST_CLR, BOARD_PERI_RST_SET); 
    /* switch ddr pcie reset */
    //printf("Line is %d,function\n",__LINE__,__func__);
#if 1
	reg_val = pgpio->gpdat;
	reg_val |= BOARD_PERI_RST_SET;  
	pgpio->gpdat = reg_val; 
	udelay(100000); 
	
	reg_val &= ~BOARD_PERI_RST_SET;  
	pgpio->gpdat = reg_val; 
	udelay(100000); 
	
	reg_val |= BOARD_PERI_RST_SET;  
	pgpio->gpdat = reg_val; 
	udelay(100000);
	
	reg_val &= ~BOARD_PERI_RST_SET;  
	pgpio->gpdat = reg_val; 
	udelay(100000); 
	
	reg_val |= BOARD_PERI_RST_SET;  
	pgpio->gpdat = reg_val; 
	udelay(100000);
#endif
	/* usb reset */
	reg_val = pgpio->gpdat;
	reg_val &= ~USB_RST_CLR;  
	pgpio->gpdat = reg_val; 
	udelay(100000); 
	
	reg_val |= USB_RST_CLR;  
	pgpio->gpdat = reg_val; 
	udelay(100000); 
	
	reg_val &= ~USB_RST_CLR;  
	pgpio->gpdat = reg_val; 
	udelay(100000);
#else
//	clrsetbits_be32(&pgpio->gpdat, USB_RST_CLR, BOARD_PERI_RST_SET);
#endif /* end of CONFIG_WGXTM_BOREN */
	return 0;
}

int misc_init_r(void)
{
#if 0
#if defined(CONFIG_SDCARD) || defined(CONFIG_SPIFLASH)
	ccsr_gur_t *gur = (void *)CONFIG_SYS_MPC85xx_GUTS_ADDR;
	ccsr_gpio_t *gpio = (void *)CONFIG_SYS_MPC85xx_GPIO_ADDR;

	setbits_be32(&gpio->gpdir, USB2_PORT_OUT_EN);
	setbits_be32(&gpio->gpdat, USB2_PORT_OUT_EN);
	setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_ELBC_OFF_USB2_ON);
#endif
#endif
	return 0;
}

int board_early_init_r(void)
{
	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	const u8 flash_esel = find_tlb_idx((void *)flashbase, 1);
	volatile ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);
	unsigned int orig_bus = i2c_get_bus_num();
	u8 i2c_data;

	i2c_set_bus_num(1);
	if (i2c_read(CONFIG_SYS_I2C_PCA9557_ADDR, 0,
		1, &i2c_data, sizeof(i2c_data)) == 0) {
		if (i2c_data & 0x2)
			puts("NOR Flash Bank : Secondary\n");
		else
			puts("NOR Flash Bank : Primary\n");

		if (i2c_data & 0x1) {
			setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_SD_DATA);
			puts("SD/MMC : 8-bit Mode\n");
			puts("eSPI : Disabled\n");
		} else {
			puts("SD/MMC : 4-bit Mode\n");
			puts("eSPI : Enabled\n");
		}
	} else {
		puts("Failed reading I2C Chip 0x18 on bus 1\n");
	}
    //gpio_drv_dir_output(11);
    //gpio_drv_set_output(11,0);
	i2c_set_bus_num(orig_bus);

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	printf("flash_esel is %d\n", flash_esel);
	/* invalidate existing TLB entry for flash */
	disable_tlb(flash_esel);

#if defined(CONFIG_WGXTM_BOREN)
	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,
			MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,
			0, flash_esel, BOOKE_PAGESZ_64M, 1);
#else
	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,
			MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,
			0, flash_esel, BOOKE_PAGESZ_16M, 1);
#endif
	rtc_reset();
	return 0;
}


#ifdef CONFIG_TSEC_ENET
int board_eth_init(bd_t *bis)
{
	struct tsec_info_struct tsec_info[4];
	int num = 0;
	char *tmp;
	unsigned int vscfw_addr;

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
	if (is_serdes_configured(SGMII_TSEC3)) {
		puts("eTSEC3 is in sgmii mode.\n");
		tsec_info[num].flags |= TSEC_SGMII;
	}
	num++;
#endif
	if (!num) {
		printf("No TSECs initialized\n");
		return 0;
	}
#ifdef CONFIG_VSC7385_ENET
/* If a VSC7385 microcode image is present, then upload it. */
	if ((tmp = getenv ("vscfw_addr")) != NULL) {
		vscfw_addr = simple_strtoul (tmp, NULL, 16);
		printf("uploading VSC7385 microcode from %x\n", vscfw_addr);
		if (vsc7385_upload_firmware((void *) vscfw_addr,
					CONFIG_VSC7385_IMAGE_SIZE))
			puts("Failure uploading VSC7385 microcode.\n");
	} else
		puts("No address specified for VSC7385 microcode.\n");
#endif

	tsec_eth_init(bis, tsec_info, num);

	return pci_eth_init(bis);
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
extern void ft_pci_board_setup(void *blob);

void ft_board_setup(void *blob, bd_t *bd)
{
	const char *soc_usb_compat = "fsl-usb2-dr";
	int err, usb1_off, usb2_off;
	phys_addr_t base;
	phys_size_t size;

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();

#if defined(CONFIG_PCI)
	ft_pci_board_setup(blob);
#endif /* #if defined(CONFIG_PCI) */

	fdt_fixup_memory(blob, (u64)base, (u64)size);

	fdt_fixup_dr_usb(blob, bd);

#if defined(CONFIG_SDCARD) || defined(CONFIG_SPIFLASH)
	/* Delete eLBC node as it is muxed with USB2 controller */
	if (hwconfig("usb2")) {
		const char *soc_elbc_compat = "fsl,p1020-elbc";
		int off = fdt_node_offset_by_compatible(blob, -1,
			soc_elbc_compat);
		if (off < 0) {
			printf("WARNING: could not find compatible node"
				" %s: %s.\n", soc_elbc_compat,
				fdt_strerror(off));
				return;
		}
		err = fdt_del_node(blob, off);
		if (err < 0) {
			printf("WARNING: could not remove %s: %s.\n",
				soc_elbc_compat, fdt_strerror(err));
		}
		return;
	}
#endif
	/* Delete USB2 node as it is muxed with eLBC */
	usb1_off = fdt_node_offset_by_compatible(blob, -1,
		soc_usb_compat);
	if (usb1_off < 0) {
		printf("WARNING: could not find compatible node"
			" %s: %s.\n", soc_usb_compat,
			fdt_strerror(usb1_off));
		return;
	}
	usb2_off = fdt_node_offset_by_compatible(blob, usb1_off,
			soc_usb_compat);
	if (usb2_off < 0) {
		printf("WARNING: could not find compatible node"
			" %s: %s.\n", soc_usb_compat,
			fdt_strerror(usb2_off));
		return;
	}
	err = fdt_del_node(blob, usb2_off);
	if (err < 0)
		printf("WARNING: could not remove %s: %s.\n",
			soc_usb_compat, fdt_strerror(err));
}

/*read phy data*/
int boren_phy_read(int phy_id,int page,int reg)
{
    unsigned short	data;
    const char	*devname;

    devname = miiphy_get_current_dev();
    data = 0xffff;
    
    /* When page is not 0, must write page to 0x16 resigned */
    if(page != 0)
        miiphy_write(devname,phy_id,0x16,page);
    
    if (miiphy_read (devname, phy_id, reg, &data) != 0) {
        printf(
                "Error reading from the PHY id=%02x reg=%02x\n",
                phy_id, reg);
    }

    if(page != 0)
        miiphy_write(devname,phy_id,0x16,0);

    return data;
} 

/*write data to phy*/
int boren_phy_write(int phy_id,int page,int reg,int data)
{
    const char	*devname;
    devname = miiphy_get_current_dev();
    /* When page is not 0, must write page to 0x16 resigned */
    if(page != 0)
        miiphy_write(devname,phy_id,0x16,page);
    
    if (miiphy_write (devname, phy_id, reg, data) != 0) 
    {
        printf("Error writing to the PHY id=%02x reg=%02x\n",
                phy_id, reg);
    }

    if(page != 0)
        miiphy_write(devname,phy_id,0x16,0);

    return 0;
}

void boren_mv_phy_88e1116_init(void)
{
	udelay(1000000);
	boren_phy_write(0,0x3,0x10,0x1771);
	boren_phy_write(0,0x3,0x11,0x4415);
	boren_phy_write(1,0x3,0x10,0x1771);
	boren_phy_write(1,0x3,0x11,0x4415);
}

#define RGMII_PHY_RST_SET_BIT	13	
/* Configure and enable mv88e61xx Switch */
void reset_phy(void)
{
	/* H/W reset switch chip */
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);
	unsigned int gpio_val, gpio_dir;

	printf("MJ:<%s> restting \n", __func__);
	/* current gpio data & dir */
	gpio_dir = pgpio->gpdir;
	/* set output */
	gpio_dir |= (1 << (31 - RGMII_PHY_RST_SET_BIT));
	pgpio->gpdir = gpio_dir;
	udelay(20000);
	
	gpio_val = pgpio->gpdat;
	gpio_val &= ~(1 << (31 - RGMII_PHY_RST_SET_BIT));
	pgpio->gpdat = gpio_val;
	udelay(20000);
	/* */
	gpio_val = pgpio->gpdat;
	gpio_val |= (1 << (31 - RGMII_PHY_RST_SET_BIT));
	pgpio->gpdat = gpio_val;
	udelay(20000);
	
	gpio_val = pgpio->gpdat;
	gpio_val &= ~(1 << (31 - RGMII_PHY_RST_SET_BIT));
	pgpio->gpdat = gpio_val;
	udelay(20000);

	gpio_val = pgpio->gpdat;
	gpio_val |= (1 << (31 - RGMII_PHY_RST_SET_BIT));
	pgpio->gpdat = gpio_val;
	udelay(20000);

	/* configure and initialize switch */
	struct mv88e61xx_config swcfg = {
		.name = "eTSEC1",
		.vlancfg = MV88E61XX_VLANCFG_ROUTER,
		.rgmii_delay = MV88E61XX_RGMII_DELAY_EN,
		.led_init = MV88E61XX_LED_INIT_EN,
		.mdip = MV88E61XX_MDIP_REVERSE,
		.portstate = MV88E61XX_PORTSTT_FORWARDING,
		.cpuport = (1 << 6),
		.ports_enabled = 0x7f
	};

	printf("<%s> Invoked, calling mv88e61xx_switch_initialize\n", __func__);
	mv88e61xx_switch_initialize(&swcfg);
	/* wait 1s for switch stable(link up) */

	/* configure the mv switch and the phy setup */
	boren_mv_phy_88e1116_init();
}

#endif
