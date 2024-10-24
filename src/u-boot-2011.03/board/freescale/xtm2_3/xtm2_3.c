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
#include <netdev.h>
#include <rtc.h>
#include <miiphy.h>
#include <version.h>

DECLARE_GLOBAL_DATA_PTR;

#define PCIE_RST_SET		0x00010000	/* GPIO 15 */
#define RGMII_PHY_RST_SET	0x00040000	/* GPIO 13 */
#define DDR_RESET		0x00020000	/* GPIO 14 */
#define USB_RST_CLR		0x00080000	/* GPIO 12 */
#define USB2_PORT_OUT_EN	0x01000000
#define GPIO_DIR		0x060f0000
#define BOARD_PERI_RST_SET	(PCIE_RST_SET | RGMII_PHY_RST_SET | DDR_RESET)

#define SYSCLK_MASK	0x00200000
#define BOARDREV_MASK	0x10100000
#define BOARDREV_B	0x10100000
#define BOARDREV_C	0x00100000
#define BOARDREV_D	0x00000000

#define SYSCLK_66	66666666
#define SYSCLK_50	50000000
#define SYSCLK_100	100000000

unsigned long get_board_sys_clk(ulong dummy)
{
	return SYSCLK_66;
}

int board_early_init_f (void)
{
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	setbits_be32(&pgpio->gpdir, GPIO_DIR);
	clrsetbits_be32(&pgpio->gpdat, USB_RST_CLR, BOARD_PERI_RST_SET);
	return 0;
}

int checkboard (void)
{
	char board_rev = 0; 
	struct cpu_type *cpu;
	board_rev = 'A';
	cpu = gd->cpu;
	printf ("Board: %sRDB\n", cpu->name);
#ifdef CONFIG_WG_COMMON
	printf ("%s\n",BOARD_VERSION);
#endif
	return 0;
}

int board_early_init_r(void)
{
	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	const u8 flash_esel = find_tlb_idx((void *)flashbase, 1);

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	/* invalidate existing TLB entry for flash */
	disable_tlb(flash_esel);

	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,
			MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,
			0, flash_esel, BOOKE_PAGESZ_16M, 1);
	//	rtc_reset();
	return 0;
}


#ifdef CONFIG_TSEC_ENET
int board_eth_init(bd_t *bis)
{
	struct tsec_info_struct tsec_info[4];
	int num = 0;
	unsigned short value;

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
	tsec_eth_init(bis, tsec_info, num);
	for(num=0; num<1; num++) {	
		/* Get PHY ID of switch to make sure work well */
		miiphy_write("eTSEC1", 0x10+num, 0, 0x9a03);
		miiphy_read("eTSEC1",  0x10+num, 1, &value);
		if ( (value&0xffff) != 0x1712 ) {
			printf("Error: Switch setting incorrect: SW1%d value: %x!\n", num, value);
		}
		printf("Init SW1%d switch to forwarding mode", num);
		//Reset and Disable INTx 
		DLAY SMIRW(1, 0x10+num, 4, 0xa000, 0, 0, 0x1b, 0);

		//set rgmii delay     
		DLAY SMIRW(1, 0x10+num, 1, 0xc03e, 0, 0, 0x15, 0);	//P15
		
		//set LED function
		DLAY SMIRW(1, 0x10+num, 0x16, 0x9038, 0, 0,0x10, 0);	//P10
		DLAY SMIRW(1, 0x10+num, 0x16, 0x9038, 0, 0,0x11, 0);	//P11   
		DLAY SMIRW(1, 0x10+num, 0x16, 0x9038, 0, 0,0x12, 0);	//P12
		DLAY SMIRW(1, 0x10+num, 0x16, 0x9038, 0, 0,0x13, 0);	//P13   
		DLAY SMIRW(1, 0x10+num, 0x16, 0x9038, 0, 0,0x14, 0);	//P14  
		printf(".");

		//set port to forwarding mode
		DLAY SMIRW(1, 0x10+num, 4, 0x007f, 0, 0, 0x10, 0);
		DLAY SMIRW(1, 0x10+num, 4 ,0x007f, 0, 0, 0x11, 0);
		DLAY SMIRW(1, 0x10+num, 4 ,0x007f, 0, 0, 0x12, 0);
		DLAY SMIRW(1, 0x10+num, 4 ,0x007f, 0, 0, 0x13, 0);
		DLAY SMIRW(1, 0x10+num, 4 ,0x007f, 0, 0, 0x14, 0);
		DLAY SMIRW(1, 0x10+num, 4 ,0x007f, 0, 0, 0x15, 0);
		DLAY SMIRW(1, 0x10+num, 4 ,0x007f, 0, 0, 0x16, 0);
		printf(".");

		//set pvlan table
		DLAY SMIRW(1, 0x10+num, 6, 0x0020, 0, 0, 0x10, 0);
		DLAY SMIRW(1, 0x10+num, 6 ,0x0020, 0, 0, 0x11, 0);
		DLAY SMIRW(1, 0x10+num, 6 ,0x0020, 0, 0, 0x12, 0);
		DLAY SMIRW(1, 0x10+num, 6 ,0x0020, 0, 0, 0x13, 0);
		DLAY SMIRW(1, 0x10+num, 6 ,0x0020, 0, 0, 0x14, 0);
		DLAY SMIRW(1, 0x10+num, 6 ,0x001f, 0, 0, 0x15, 0);
		DLAY SMIRW(1, 0x10+num, 6 ,0x0000, 0, 0, 0x16, 0);
		printf(".");

		//reset PHY
		DLAY SMIRW(1, 0x10+num, 0 ,0x9140, 0, 0, 0x0, 0);
		DLAY SMIRW(1, 0x10+num, 0 ,0x9140, 0, 0, 0x1, 0);
		DLAY SMIRW(1, 0x10+num, 0 ,0x9140, 0, 0, 0x2, 0);
		DLAY SMIRW(1, 0x10+num, 0 ,0x9140, 0, 0, 0x3, 0);
		DLAY SMIRW(1, 0x10+num, 0 ,0x9140, 0, 0, 0x4, 0);
		printf(". Done\n");
	}
	return pci_eth_init(bis);
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
extern void ft_pci_board_setup(void *blob);

void ft_board_setup(void *blob, bd_t *bd)
{
	phys_addr_t base;
	phys_size_t size;

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();

#if defined(CONFIG_PCI)
	ft_pci_board_setup(blob);
#endif /* #if defined(CONFIG_PCI) */

	fdt_fixup_memory(blob, (u64)base, (u64)size);
}
#endif

#ifdef CONFIG_MP
extern void cpu_mp_lmb_reserve(struct lmb *lmb);

void board_lmb_reserve(struct lmb *lmb)
{
	cpu_mp_lmb_reserve(lmb);
}
#endif
