/*
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

/*
 * p1021mds board configuration file
 */
#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_HAS_TPL

#ifdef CONFIG_NAND
#define CONFIG_NAND_U_BOOT
#define CONFIG_RAMBOOT_NAND

#endif

#ifdef CONFIG_NAND_U_BOOT
#define CONFIG_SYS_TEXT_BASE_SPL	0xfff00000
#ifdef CONFIG_HAS_TPL
#define CONFIG_SYS_TEXT_BASE_TPL	0xf8f81000
#endif
#define CONFIG_SYS_TEXT_BASE	0x01001000
#define CONFIG_SYS_LDSCRIPT $(TOPDIR)/$(CPUDIR)/u-boot-nand.lds

#ifdef CONFIG_NAND_SPL
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE_SPL /* start of monitor */
#elif CONFIG_IN_TPL
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE_TPL /* start of monitor */
#else
#define CONFIG_SYS_MONITOR_BASE CONFIG_SYS_TEXT_BASE    /* start of monitor */
#endif
#endif

/* High Level Configuration Options */
#define CONFIG_BOOKE			/* BOOKE */
#define CONFIG_E500			/* BOOKE e500 family */
#define CONFIG_MPC85xx			/* MPC8540/60/55/41/48/68/P1021 */
#define CONFIG_P1021			/* P1021 silicon support */
#define CONFIG_P1021MDS			/* P1021MDS board specific */

#define CONFIG_FSL_LAW			/* Use common FSL init code */
#define CONFIG_FSL_ELBC			/* Has Enhance localbus controller */

/* Replace a call to get_clock_freq (after it is implemented)*/
#define CONFIG_SYS_CLK_FREQ	66666666
#define CONFIG_DDR_CLK_FREQ	CONFIG_SYS_CLK_FREQ

/*
 * These can be toggled for performance analysis, otherwise use default.
 */
#define CONFIG_L2_CACHE				/* toggle L2 cache	*/
#define CONFIG_BTB				/* toggle branch predition */

#define CONFIG_HWCONFIG


/*
 * Only possible on E500 Version 2 or newer cores.
 */
#define CONFIG_ENABLE_36BIT_PHYS

#define CONFIG_SYS_MEMTEST_START	0x00000000	/* memtest works on */
#define CONFIG_SYS_MEMTEST_END		0x1fffffff

#define CONFIG_SYS_LBC_LBCR	0x00080000	/* Implement conversion of
						addresses in the LBC */

/*
 * Base addresses -- Note these are effective addresses where the
 * actual resources get mapped (not physical addresses)
 */
#define CONFIG_SYS_CCSRBAR		0xffe00000	/* relocated CCSRBAR */
#define CONFIG_SYS_CCSRBAR_PHYS	CONFIG_SYS_CCSRBAR
						/* physical addr of CCSRBAR */
#if defined(CONFIG_RAMBOOT_NAND) && !defined(CONFIG_NAND_SPL)
#define CONFIG_SYS_CCSRBAR_DEFAULT	CONFIG_SYS_CCSRBAR
#else
#define CONFIG_SYS_CCSRBAR_DEFAULT	0xff700000	/* CCSRBAR Default */
#endif
#define CONFIG_SYS_IMMR         CONFIG_SYS_CCSRBAR
						/* PQII uses CONFIG_SYS_IMMR */

/* DDR Setup */
#define CONFIG_FSL_DDR3
#define CONFIG_SPD_EEPROM               /* Use SPD EEPROM for DDR setup*/
#define CONFIG_DDR_SPD
#define CONFIG_SYS_DDR_TLB_START	11

#define CONFIG_MEM_INIT_VALUE	0xDeadBeef

#define CONFIG_SYS_DDR_SDRAM_BASE	0x00000000
					/* DDR is system memory*/
#define CONFIG_SYS_SDRAM_BASE		CONFIG_SYS_DDR_SDRAM_BASE

#define CONFIG_NUM_DDR_CONTROLLERS	1
#define CONFIG_DIMM_SLOTS_PER_CTLR	1
#define CONFIG_CHIP_SELECTS_PER_CTRL	2

/* I2C addresses of SPD EEPROMs */
#define SPD_EEPROM_ADDRESS	0x51	/* CTLR 0 DIMM 0 */

/*
 * Config the L2 Cache as L2 SRAM
 */
#define CONFIG_SYS_INIT_L2_ADDR         0xf8f80000
#define CONFIG_SYS_INIT_L2_ADDR_PHYS    CONFIG_SYS_INIT_L2_ADDR
#define CONFIG_SYS_L2_SIZE              (256 << 10)
#define CONFIG_SYS_INIT_L2_END  (CONFIG_SYS_INIT_L2_ADDR + CONFIG_SYS_L2_SIZE)

/*
 * Memory map
 *
 * 0x0000_0000 0x1fff_ffff	DDR3			512MB cacheable
 * 0xa000_0000 0xbfff_ffff	PCIE2 Mem		512MB non-cacheable
 * 0xc000_0000 0xdfff_ffff	PCIE1 Mem		512MB non-cacheable
 * 0xffc1_0000 0xffc1_ffff	PCIE2 IO range		64K non-cacheable
 * 0xffc2_0000 0xffc2_ffff	PCIE1 IO range		64K non-cacheable
 * 0xf800_0000 0xf800_7fff	BCSR on CS1		32KB non-cacheable
 * 0xf801_0000 0xf801_ffff	PMC1 on CS2		64KB non-cacheable
 * 0xf802_0000 0xf802_ffff	PMC0 on CS3		64KB non-cacheable
 * 0xfc00_0000 0xfdff_ffff	NAND on CS0		32MB non-cacheable
 * 0xffe0_0000 0xffef_ffff	CCSRBAR			1M
 */


/*
 * Local Bus Definitions
 */

#define CONFIG_SYS_BCSR_BASE		0xf8000000
#define CONFIG_SYS_BCSR_BASE_PHYS	CONFIG_SYS_BCSR_BASE

#define CONFIG_SYS_PIB_PMC1_BASE	0xf8010000
					/* start of PIB-QOC3(PMC1)  64K */
#define CONFIG_SYS_PIB_PMC1_BASE_PHYS	CONFIG_SYS_PIB_PMC1_BASE

#define CONFIG_SYS_PIB_PMC0_BASE	0xf8020000
					/* start of PIB-T1/E1(PMC0) 64K */
#define CONFIG_SYS_PIB_PMC0_BASE_PHYS	CONFIG_SYS_PIB_PMC0_BASE

/* chip select 1 - BCSR*/
#define CONFIG_SYS_BR1_PRELIM  (BR_PHYS_ADDR(CONFIG_SYS_BCSR_BASE_PHYS) \
				| BR_PS_8 | BR_V)
#define CONFIG_SYS_OR1_PRELIM  (OR_AM_32KB | OR_GPCM_CSNT | OR_GPCM_XACS \
				| OR_GPCM_SCY | OR_GPCM_TRLX | OR_GPCM_EHTR \
				| OR_GPCM_EAD)

/* chip select 2 - PIB(QOC3-PMC1)*/
#define CONFIG_SYS_BR2_PRELIM  (BR_PHYS_ADDR(CONFIG_SYS_PIB_PMC1_BASE_PHYS) \
				| BR_PS_8 | BR_V)
#define CONFIG_SYS_OR2_PRELIM  (OR_AM_64KB | OR_GPCM_CSNT | OR_GPCM_XACS \
				| OR_GPCM_SCY | OR_GPCM_TRLX | OR_GPCM_EHTR \
				| OR_GPCM_EAD)

/* chip select 3 - PIB(T1/E1-PMC0)*/
#define CONFIG_SYS_BR3_PRELIM  (BR_PHYS_ADDR(CONFIG_SYS_PIB_PMC0_BASE_PHYS) \
				| BR_PS_8 | BR_V)
#define CONFIG_SYS_OR3_PRELIM  (OR_AM_64KB | OR_GPCM_CSNT | OR_GPCM_XACS \
				| OR_GPCM_SCY | OR_GPCM_TRLX | OR_GPCM_EHTR \
				| OR_GPCM_EAD)

#define CONFIG_SYS_NO_FLASH

#if defined(CONFIG_RAMBOOT_NAND) || defined(CONFIG_RAMBOOT_SDCARD) \
		 || defined(CONFIG_RAMBOOT_SPIFLASH)
#define CONFIG_SYS_RAMBOOT
#else
#undef CONFIG_SYS_RAMBOOT
#endif

#ifdef CONFIG_NAND_SPL
#define CONFIG_SYS_NAND_BASE		0xFFF00000
#else
#define CONFIG_SYS_NAND_BASE		0xFC000000
#endif
#define CONFIG_SYS_NAND_BASE_PHYS	CONFIG_SYS_NAND_BASE
#define CONFIG_SYS_NAND_BASE_LIST	{ CONFIG_SYS_NAND_BASE, }
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define NAND_MAX_CHIPS			1
#define CONFIG_MTD_NAND_VERIFY_WRITE
#define CONFIG_CMD_NAND
#define CONFIG_NAND_FSL_ELBC
#define CONFIG_SYS_NAND_BLOCK_SIZE	(16 * 1024)

/* NAND boot: 4K NAND loader config */
#ifdef CONFIG_NAND_SPL
#define CONFIG_SYS_NAND_SPL_SIZE	0x1000
#define CONFIG_SYS_NAND_U_BOOT_SIZE	(112 << 10)
#define CONFIG_SYS_NAND_U_BOOT_DST	CONFIG_SYS_INIT_L2_ADDR
#define CONFIG_SYS_NAND_U_BOOT_START	CONFIG_SYS_INIT_L2_ADDR
#define CONFIG_SYS_NAND_U_BOOT_OFFS	(16 << 10)
#define CONFIG_SYS_NAND_U_BOOT_RELOC    (CONFIG_SYS_INIT_L2_END - 0x2000)
#define CONFIG_SYS_NAND_U_BOOT_RELOC_SP ((CONFIG_SYS_INIT_L2_END - 1) & ~0xF)
#endif
#ifdef CONFIG_IN_TPL
/* tpl boot: 112K  tpl uboot config*/
#define CONFIG_SYS_NAND_U_BOOT_SIZE	(512 << 10)
#define CONFIG_SYS_NAND_U_BOOT_DST	(0x01000000)
#define CONFIG_SYS_NAND_U_BOOT_START	(0x01000000)
#define CONFIG_SYS_NAND_U_BOOT_OFFS	(128 << 10)
#endif

/* NAND FLASH CONFIG */
#define CONFIG_NAND_BR_PRELIM	(CONFIG_SYS_NAND_BASE_PHYS \
				| (2<<BR_DECC_SHIFT) /* Use HW ECC */ \
				| BR_PS_8	     /* Port Size = 8 bit */ \
				| BR_MS_FCM	     /* MSEL = FCM */ \
				| BR_V)		     /* valid */
#define CONFIG_NAND_OR_PRELIM	(0xFFF80000	     /* length 32K */ \
				| OR_FCM_CSCT \
				| OR_FCM_CST \
				| OR_FCM_CHT \
				| OR_FCM_SCY_1 \
				| OR_FCM_TRLX \
				| OR_FCM_EHTR)
/* chip select 0 - NAND */
#define CONFIG_SYS_BR0_PRELIM	CONFIG_NAND_BR_PRELIM /* NAND Base Address */
#define CONFIG_SYS_OR0_PRELIM	CONFIG_NAND_OR_PRELIM /* NAND Options */

#define CONFIG_SYS_INIT_RAM_LOCK	1
#define CONFIG_SYS_INIT_RAM_ADDR	0xffd00000 /* Initial RAM address */
#define CONFIG_SYS_INIT_RAM_SIZE	0x00004000 /* End of used area in RAM */

#define CONFIG_SYS_GBL_DATA_SIZE	128	/* num bytes initial data */
#define CONFIG_SYS_GBL_DATA_OFFSET	\
			(CONFIG_SYS_INIT_RAM_SIZE - CONFIG_SYS_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_GBL_DATA_OFFSET

#define CONFIG_SYS_MONITOR_LEN	(512 * 1024)	/* Reserve 512 kB for Mon */
#define CONFIG_SYS_MALLOC_LEN	(1024 * 1024)	/* Reserved for malloc */

/* Serial Port */
#define CONFIG_CONS_INDEX		1
#define CONFIG_SERIAL_MULTI
#undef	CONFIG_SERIAL_SOFTWARE_FIFO
#define CONFIG_SYS_NS16550
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE    1
#define CONFIG_SYS_NS16550_CLK		get_bus_freq(0)
#define CONFIG_SYS_CONSOLE_IS_IN_ENV   /* determine from environment */

#define CONFIG_SYS_BAUDRATE_TABLE  \
	{300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}

#define CONFIG_SYS_NS16550_COM1        (CONFIG_SYS_CCSRBAR+0x4500)
#define CONFIG_SYS_NS16550_COM2        (CONFIG_SYS_CCSRBAR+0x4600)
#ifdef CONFIG_NAND_SPL
#define CONFIG_NS16550_MIN_FUNCTIONS
#endif

#define CONFIG_BAUDRATE	115200

/* Use the HUSH parser*/
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2 "> "

/*
 * I2C
 */
#define CONFIG_FSL_I2C		/* Use FSL common I2C driver */
#define CONFIG_HARD_I2C		/* I2C with hardware support*/
#undef	CONFIG_SOFT_I2C		/* I2C bit-banged */
#define CONFIG_I2C_MULTI_BUS
#define CONFIG_SYS_I2C_SPEED	400000	/* I2C speed and slave address */
#define CONFIG_SYS_I2C_SLAVE	0x7F
#define CONFIG_SYS_I2C_NOPROBES	{{0, 0x69}}	/* Don't probe these addrs */
#define CONFIG_SYS_I2C_OFFSET	0x3000
#define CONFIG_SYS_I2C2_OFFSET	0x3100

/*
 * Environment
 */
#if defined(CONFIG_NAND)
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_OFFSET	(576 * 1024)
#define CONFIG_ENV_SIZE		CONFIG_SYS_NAND_BLOCK_SIZE
#endif
#define CONFIG_ENV_ADDR		(CONFIG_SYS_INIT_L2_ADDR + (128 << 10))

#define CONFIG_SYS_HZ	1000		/* decrementer freq: 1ms ticks */

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_LONGHELP		/* undef to save memory	*/
#define CONFIG_CMDLINE_EDITING		/* Command-line editing */
#define CONFIG_SYS_LOAD_ADDR	0x2000000	/* default load address */
#define CONFIG_SYS_PROMPT	"=> "		/* Monitor Command Prompt */
#if defined(CONFIG_CMD_KGDB)
#define CONFIG_SYS_CBSIZE	1024		/* Console I/O Buffer Size */
#else
#define CONFIG_SYS_CBSIZE	512		/* Console I/O Buffer Size */
#endif
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT)+16)
						/* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16		/* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE
						/* Boot Argument Buffer Size */

/*********************************/
#ifndef CONFIG_IN_TPL

#define CONFIG_MP			/* Multiprocessor support */

#define CONFIG_PCI			/* Enable PCI/PCIE */
#define CONFIG_PCIE1			/* PCIE controller */
#define CONFIG_PCIE2			/* PCIE controller */
#define CONFIG_FSL_PCI_INIT		/* use common fsl pci init code */
#define CONFIG_FSL_PCIE_RESET		/* need PCIe reset errata */
#define CONFIG_SYS_PCI_64BIT		/* enable 64-bit PCI resources */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_TSEC_ENET		/* tsec ethernet support */

/* pass open firmware flat tree */
#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_OF_STDOUT_VIA_ALIAS

#define CONFIG_SYS_64BIT_VSPRINTF
#define CONFIG_SYS_64BIT_STRTOUL

/* new uImage format support */
#define CONFIG_FIT
#define CONFIG_FIT_VERBOSE	 /* enable fit_format_{error,warning}() */

/* TSEC support */
#if defined(CONFIG_TSEC_ENET)

#ifndef CONFIG_NET_MULTI
#define CONFIG_NET_MULTI
#endif

#define CONFIG_MII		/* MII PHY management */
#define CONFIG_MII_DEFAULT_TSEC	1	/* Allow unregistered phys */
#define CONFIG_TSEC1
#define CONFIG_TSEC1_NAME	"eTSEC1"
#define CONFIG_TSEC2
#define CONFIG_TSEC2_NAME	"eTSEC2"
#define CONFIG_TSEC3
#define CONFIG_TSEC3_NAME	"eTSEC3"

#define TSEC1_PHY_ADDR		0
#define TSEC1_FLAGS		(TSEC_GIGABIT | TSEC_REDUCED)
#define TSEC1_PHYIDX		0

#define TSEC2_PHY_ADDR		4
#define TSEC2_FLAGS		(TSEC_GIGABIT | TSEC_SGMII)
#define TSEC2_PHYIDX		0

#ifdef CONFIG_TSEC3_IN_SGMII	/* Need to set SW8.6 to 0 */
#define TSEC3_PHY_ADDR		6
#define TSEC3_FLAGS		(TSEC_GIGABIT | TSEC_SGMII)
#else
#define TSEC3_PHY_ADDR		1
#define TSEC3_FLAGS		(TSEC_GIGABIT | TSEC_REDUCED)
#endif
#define TSEC3_PHYIDX		0

#define CONFIG_ETHPRIME		"eTSEC1"

#define CONFIG_PHY_GIGE		/* Include GbE speed/duplex detection */
#endif /* CONFIG_TSEC_ENET */

#define CONFIG_QE

#ifdef CONFIG_QE
/* QE microcode/firmware address */
#define CONFIG_SYS_QE_FW_IN_NAND 0x1f00000
#define CONFIG_SYS_QE_FW_ADDR   0x10000000
#define CONFIG_SYS_QE_FW_LENGTH 0x10000

/*
 * QE UEC ethernet configuration
 */
#define CONFIG_MIIM_ADDRESS	(CONFIG_SYS_CCSRBAR + 0x82120)

#define CONFIG_UEC_ETH
#define CONFIG_PHY_MODE_NEED_CHANGE

#define CONFIG_UEC_ETH1		/* GETH1 */
#define CONFIG_HAS_ETH0

#ifdef CONFIG_UEC_ETH1
#define CONFIG_SYS_UEC1_UCC_NUM		0       	/* UCC1 */
#define CONFIG_SYS_UEC1_RX_CLK		QE_CLK12	/* CLK12 for MII */
#define CONFIG_SYS_UEC1_TX_CLK		QE_CLK9		/* CLK9 for MII */
#define CONFIG_SYS_UEC1_ETH_TYPE	FAST_ETH
#define CONFIG_SYS_UEC1_PHY_ADDR	0x0     	/* 0x0 for MII */
#define CONFIG_SYS_UEC1_INTERFACE_TYPE	MII
#define CONFIG_SYS_UEC1_INTERFACE_SPEED	100
#endif /* CONFIG_UEC_ETH1 */

#define CONFIG_UEC_ETH5		/* GETH5 */
#define CONFIG_HAS_ETH1

#ifdef CONFIG_UEC_ETH5
#define CONFIG_SYS_UEC5_UCC_NUM		4       	/* UCC5 */
#define CONFIG_SYS_UEC5_RX_CLK		QE_CLK_NONE
#define CONFIG_SYS_UEC5_TX_CLK		QE_CLK13	/* CLK 13 for RMII */
#define CONFIG_SYS_UEC5_ETH_TYPE	FAST_ETH
#define CONFIG_SYS_UEC5_PHY_ADDR	0x3		/* 0x3 for RMII */
#define CONFIG_SYS_UEC5_INTERFACE_TYPE	RMII
#define CONFIG_SYS_UEC5_INTERFACE_SPEED	100
#endif /* CONFIG_UEC_ETH2 */

#endif /* CONFIG_QE */

/*
 * I2C2 EEPROM
 */
#define CONFIG_ID_EEPROM
#define CONFIG_SYS_I2C_EEPROM_NXID
#define CONFIG_SYS_I2C_EEPROM_ADDR      0x52
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1
#define CONFIG_SYS_EEPROM_BUS_NUM       1

#define PLPPAR1_I2C_BIT_MASK		0x0000000F
#define PLPPAR1_I2C2_VAL		0x00000000
#define PLPPAR1_ESDHC_VAL		0x0000000A
#define PLPDIR1_I2C_BIT_MASK		0x0000000F
#define PLPDIR1_I2C2_VAL		0x0000000F
#define PLPDIR1_ESDHC_VAL		0x00000006

/*
 * General PCI
 * Memory Addresses are mapped 1-1. I/O is mapped from 0
 */
#define CONFIG_SYS_PCIE2_MEM_VIRT	0xa0000000
#define CONFIG_SYS_PCIE2_MEM_BUS	0xa0000000
#define CONFIG_SYS_PCIE2_MEM_PHYS	0xa0000000
#define CONFIG_SYS_PCIE2_MEM_SIZE	0x20000000	/* 512M */
#define CONFIG_SYS_PCIE2_IO_VIRT	0xffc10000
#define CONFIG_SYS_PCIE2_IO_BUS		0x00000000
#define CONFIG_SYS_PCIE2_IO_PHYS	0xffc10000
#define CONFIG_SYS_PCIE2_IO_SIZE	0x00010000	/* 64K */

#define CONFIG_SYS_PCIE1_MEM_VIRT	0xc0000000
#define CONFIG_SYS_PCIE1_MEM_BUS	0xc0000000
#define CONFIG_SYS_PCIE1_MEM_PHYS	0xc0000000
#define CONFIG_SYS_PCIE1_MEM_SIZE	0x20000000	/* 512M */
#define CONFIG_SYS_PCIE1_IO_VIRT	0xffc20000
#define CONFIG_SYS_PCIE1_IO_BUS		0x00000000
#define CONFIG_SYS_PCIE1_IO_PHYS	0xffc20000
#define CONFIG_SYS_PCIE1_IO_SIZE	0x00010000	/* 64K */

#if defined(CONFIG_PCI)
#define CONFIG_PCI_PNP                 /* do pci plug-and-play */
#endif

#define CONFIG_E1000			/* Enable E1000 PCI Ethernet Card */

#define CONFIG_LOADS_ECHO		/* echo on for serial download */
#define CONFIG_SYS_LOADS_BAUD_CHANGE	/* allow baudrate change */


/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME


/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_PING
#define CONFIG_CMD_I2C
#define CONFIG_CMD_MII
#define CONFIG_CMD_ELF
#define CONFIG_CMD_IRQ
#define CONFIG_CMD_SETEXPR

#if defined(CONFIG_PCI)
    #define CONFIG_CMD_PCI
#endif


#undef CONFIG_WATCHDOG			/* watchdog disabled */

#define CONFIG_BOARD_EARLY_INIT_F	/* Call board_pre_init */

#define CONFIG_MMC
#ifdef CONFIG_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	CONFIG_SYS_MPC85xx_ESDHC_ADDR
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION
#endif

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 16 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ	(16 << 20)
					/* Initial Memory map for Linux*/

/*
 * Internal Definitions
 *
 * Boot Flags
 */
#define BOOTFLAG_COLD	0x01		/* Normal Power-On: Boot from FLASH */
#define BOOTFLAG_WARM	0x02		/* Software reboot */

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	230400	/* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	2	/* which serial port to use */
#endif

/*
 * Environment Configuration
 */
#define CONFIG_HOSTNAME	p1021mds
#define CONFIG_ROOTPATH	/nfsroot
#define CONFIG_BOOTFILE	uImage

#define CONFIG_LOADADDR	1000000   /*default location for tftp and bootm*/

#define CONFIG_BOOTDELAY 10       /* -1 disables auto-boot */
#undef  CONFIG_BOOTARGS           /* the boot command will set bootargs*/

#define	CONFIG_EXTRA_ENV_SETTINGS					\
	"netdev=eth0\0"							\
	"consoledev=ttyS0\0"						\
	"ramdiskaddr=2000000\0"						\
	"ramdiskfile=rootfs.ext2.gz.uboot\0"				\
	"fdtaddr=c00000\0"						\
	"fdtfile=p1021mds.dtb\0"					\
	"nfsargs=setenv bootargs root=/dev/nfs rw "			\
	"nfsroot=$serverip:$rootpath "					\
	"ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:off " \
	"console=$consoledev,$baudrate $othbootargs\0"			\
	"ramargs=setenv bootargs root=/dev/ram rw "			\
	"console=$consoledev,$baudrate $othbootargs\0"			\
	"hwconfig=fsl_ddr:ctrl_intlv=null\0"				\

#define CONFIG_NFSBOOTCOMMAND						\
	"run nfsargs;"							\
	"tftp $loadaddr $bootfile;"					\
	"tftp $fdtaddr $fdtfile;"					\
	"bootm $loadaddr - $fdtaddr"

#define CONFIG_RAMBOOTCOMMAND						\
	"run ramargs;"							\
	"tftp $ramdiskaddr $ramdiskfile;"				\
	"tftp $loadaddr $bootfile;"					\
	"bootm $loadaddr $ramdiskaddr"

#define CONFIG_BOOTCOMMAND  CONFIG_NFSBOOTCOMMAND

#endif /* !CONFIG_IN_TPL */
#endif	/* __CONFIG_H */
