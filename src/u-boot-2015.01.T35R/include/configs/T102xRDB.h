/*
 * Copyright 2014 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

/*
 * T1024/T1023 RDB board configuration file
 */

#ifndef __T1024RDB_H
#define __T1024RDB_H

#define CONFIG_T35
#define CONFIG_WG_BOOTMENU
/* High Level Configuration Options */
#define CONFIG_SYS_GENERIC_BOARD
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOOKE
#define CONFIG_E500			/* BOOKE e500 family */
#define CONFIG_E500MC			/* BOOKE e500mc family */
#define CONFIG_SYS_BOOK3E_HV		/* Category E.HV supported */
#define CONFIG_MP			/* support multiple processors */
#define CONFIG_PHYS_64BIT
#define CONFIG_ENABLE_36BIT_PHYS

#ifdef CONFIG_PHYS_64BIT
#define CONFIG_ADDR_MAP		1
#define CONFIG_SYS_NUM_ADDR_MAP	64	/* number of TLB1 entries */
#endif

#define CONFIG_SYS_FSL_CPC		/* Corenet Platform Cache */
#define CONFIG_SYS_NUM_CPC		CONFIG_NUM_DDR_CONTROLLERS
#define CONFIG_FSL_IFC			/* Enable IFC Support */

#define CONFIG_FSL_LAW			/* Use common FSL init code */
#define CONFIG_ENV_OVERWRITE

/* WG:Enable the 'boot from /boot' option on uboot menu */
#ifdef CONFIG_WG_BOOTMENU
 #define ADD_WGMENU_BOOTBOOT
#endif

/* support deep sleep */
#ifdef CONFIG_PPC_T1024
#define CONFIG_DEEP_SLEEP
#endif
#if defined(CONFIG_DEEP_SLEEP)
#define CONFIG_SILENT_CONSOLE
#define CONFIG_BOARD_EARLY_INIT_F
#endif

#ifdef CONFIG_RAMBOOT_PBL
#define CONFIG_SYS_FSL_PBL_PBI board/freescale/t102xrdb/t1024_pbi.cfg
#if defined(CONFIG_T1024RDB)
#define CONFIG_SYS_FSL_PBL_RCW board/freescale/t102xrdb/t1024_rcw.cfg
#elif defined(CONFIG_T1023RDB)
#define CONFIG_SYS_FSL_PBL_RCW board/freescale/t102xrdb/t1023_rcw.cfg
#endif
#define CONFIG_SPL_MPC8XXX_INIT_DDR_SUPPORT
#define CONFIG_SPL_ENV_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_FLUSH_IMAGE
#define CONFIG_SPL_TARGET		"u-boot-with-spl.bin"
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT
#define CONFIG_FSL_LAW			/* Use common FSL init code */
#define CONFIG_SYS_TEXT_BASE		0x30001000
#define CONFIG_SPL_TEXT_BASE		0xFFFD8000
#define CONFIG_SPL_PAD_TO		0x40000
#define CONFIG_SPL_MAX_SIZE		0x28000
#define RESET_VECTOR_OFFSET		0x27FFC
#define BOOT_PAGE_OFFSET		0x27000
#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_SKIP_RELOCATE
#define CONFIG_SPL_COMMON_INIT_DDR
#define CONFIG_SYS_CCSR_DO_NOT_RELOCATE
#define CONFIG_SYS_NO_FLASH
#endif

#ifdef CONFIG_NAND
#define CONFIG_SPL_NAND_SUPPORT
#define CONFIG_SYS_NAND_U_BOOT_SIZE	(768 << 10)
#define CONFIG_SYS_NAND_U_BOOT_DST	0x30000000
#define CONFIG_SYS_NAND_U_BOOT_START	0x30000000
#define CONFIG_SYS_NAND_U_BOOT_OFFS	(256 << 10)
#define CONFIG_SYS_LDSCRIPT	"arch/powerpc/cpu/mpc85xx/u-boot-nand.lds"
#define CONFIG_SPL_NAND_BOOT
#endif

#ifdef CONFIG_SPIFLASH
#define CONFIG_RESET_VECTOR_ADDRESS		0x30000FFC
#define CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_SPI_FLASH_SUPPORT
#define CONFIG_SPL_SPI_FLASH_MINIMAL
#define CONFIG_SYS_SPI_FLASH_U_BOOT_SIZE	(768 << 10)
#define CONFIG_SYS_SPI_FLASH_U_BOOT_DST		(0x30000000)
#define CONFIG_SYS_SPI_FLASH_U_BOOT_START	(0x30000000)
#define CONFIG_SYS_SPI_FLASH_U_BOOT_OFFS	(256 << 10)
#define CONFIG_SYS_LDSCRIPT		"arch/powerpc/cpu/mpc85xx/u-boot.lds"
#ifndef CONFIG_SPL_BUILD
#define CONFIG_SYS_MPC85XX_NO_RESETVEC
#endif
#define CONFIG_SPL_SPI_BOOT
#endif

#ifdef CONFIG_SDCARD
#define CONFIG_RESET_VECTOR_ADDRESS	0x30000FFC
#define CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SPL_MMC_MINIMAL
#define CONFIG_SYS_MMC_U_BOOT_SIZE	(768 << 10)
#define CONFIG_SYS_MMC_U_BOOT_DST	(0x30000000)
#define CONFIG_SYS_MMC_U_BOOT_START	(0x30000000)
#define CONFIG_SYS_MMC_U_BOOT_OFFS	(260 << 10)
#define CONFIG_SYS_LDSCRIPT		"arch/powerpc/cpu/mpc85xx/u-boot.lds"
#ifndef CONFIG_SPL_BUILD
#define CONFIG_SYS_MPC85XX_NO_RESETVEC
#endif
#define CONFIG_SPL_MMC_BOOT
#endif

#endif /* CONFIG_RAMBOOT_PBL */

#ifndef CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_TEXT_BASE	0xeff40000
#endif

#ifndef CONFIG_RESET_VECTOR_ADDRESS
#define CONFIG_RESET_VECTOR_ADDRESS	0xeffffffc
#endif

#ifndef CONFIG_SYS_NO_FLASH
#define CONFIG_FLASH_CFI_DRIVER
#define CONFIG_SYS_FLASH_CFI
#define CONFIG_SYS_FLASH_USE_BUFFER_WRITE
#endif

/* PCIe Boot - Master */
#define CONFIG_SRIO_PCIE_BOOT_MASTER
/*
 * for slave u-boot IMAGE instored in master memory space,
 * PHYS must be aligned based on the SIZE
 */
#define CONFIG_SRIO_PCIE_BOOT_IMAGE_MEM_BUS1 0xfff00000ull
#define CONFIG_SRIO_PCIE_BOOT_IMAGE_SIZE     0x100000 /* 1M */
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SRIO_PCIE_BOOT_IMAGE_MEM_PHYS 0xfef200000ull
#define CONFIG_SRIO_PCIE_BOOT_IMAGE_MEM_BUS2 0x3fff00000ull
#else
#define CONFIG_SRIO_PCIE_BOOT_IMAGE_MEM_PHYS 0xef200000
#define CONFIG_SRIO_PCIE_BOOT_IMAGE_MEM_BUS2 0xfff00000
#endif
/*
 * for slave UCODE and ENV instored in master memory space,
 * PHYS must be aligned based on the SIZE
 */
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SRIO_PCIE_BOOT_UCODE_ENV_MEM_PHYS 0xfef100000ull
#define CONFIG_SRIO_PCIE_BOOT_UCODE_ENV_MEM_BUS	 0x3ffe00000ull
#else
#define CONFIG_SRIO_PCIE_BOOT_UCODE_ENV_MEM_PHYS 0xef100000
#define CONFIG_SRIO_PCIE_BOOT_UCODE_ENV_MEM_BUS  0xffe00000
#endif
#define CONFIG_SRIO_PCIE_BOOT_UCODE_ENV_SIZE	0x40000 /* 256K */
/* slave core release by master*/
#define CONFIG_SRIO_PCIE_BOOT_BRR_OFFSET	0xe00e4
#define CONFIG_SRIO_PCIE_BOOT_RELEASE_MASK	0x00000001 /* release core 0 */

/* PCIe Boot - Slave */
#ifdef CONFIG_SRIO_PCIE_BOOT_SLAVE
#define CONFIG_SYS_SRIO_PCIE_BOOT_UCODE_ENV_ADDR 0xFFE00000
#define CONFIG_SYS_SRIO_PCIE_BOOT_UCODE_ENV_ADDR_PHYS \
		(0x300000000ull | CONFIG_SYS_SRIO_PCIE_BOOT_UCODE_ENV_ADDR)
/* Set 1M boot space for PCIe boot */
#define CONFIG_SYS_SRIO_PCIE_BOOT_SLAVE_ADDR (CONFIG_SYS_TEXT_BASE & 0xfff00000)
#define CONFIG_SYS_SRIO_PCIE_BOOT_SLAVE_ADDR_PHYS	\
		(0x300000000ull | CONFIG_SYS_SRIO_PCIE_BOOT_SLAVE_ADDR)
#define CONFIG_RESET_VECTOR_ADDRESS 0xfffffffc
#define CONFIG_SYS_NO_FLASH
#endif

#if defined(CONFIG_SPIFLASH)
#define CONFIG_SYS_EXTRA_ENV_RELOC
#define CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_SPI_BUS		0
#define CONFIG_ENV_SPI_CS		0
#define CONFIG_ENV_SPI_MAX_HZ		10000000
#define CONFIG_ENV_SPI_MODE		0
#define CONFIG_ENV_SIZE			0x2000		/* 8KB */
#define CONFIG_ENV_OFFSET		0x100000	/* 1MB */
#if defined(CONFIG_T1024RDB)
#define CONFIG_ENV_SECT_SIZE		0x10000
#elif defined(CONFIG_T1023RDB)
#define CONFIG_ENV_SECT_SIZE		0x40000
#endif
#elif defined(CONFIG_SDCARD)
#define CONFIG_SYS_EXTRA_ENV_RELOC
#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_ENV_SIZE			0x2000
#define CONFIG_ENV_OFFSET		(512 * 0x800)
#elif defined(CONFIG_NAND)
#define CONFIG_SYS_EXTRA_ENV_RELOC
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_ENV_SIZE			0x2000
#if defined(CONFIG_T1024RDB)
#define CONFIG_ENV_OFFSET		(2 * CONFIG_SYS_NAND_BLOCK_SIZE)
#elif defined(CONFIG_T1023RDB)
#define CONFIG_ENV_OFFSET		(10 * CONFIG_SYS_NAND_BLOCK_SIZE)
#endif
#elif defined(CONFIG_SRIO_PCIE_BOOT_SLAVE)
#define CONFIG_ENV_IS_IN_REMOTE
#define CONFIG_ENV_ADDR		0xffe20000
#define CONFIG_ENV_SIZE		0x2000
#elif defined(CONFIG_ENV_IS_NOWHERE)
#define CONFIG_ENV_SIZE		0x2000
#else
#define CONFIG_ENV_IS_IN_FLASH
#define CONFIG_ENV_ADDR		(CONFIG_SYS_MONITOR_BASE - CONFIG_ENV_SECT_SIZE)
#define CONFIG_ENV_SIZE		0x2000
#define CONFIG_ENV_SECT_SIZE	0x20000 /* 128K (one sector) */
#endif


#ifndef __ASSEMBLY__
unsigned long get_board_sys_clk(void);
unsigned long get_board_ddr_clk(void);
#endif

#define CONFIG_SYS_CLK_FREQ	100000000
#define CONFIG_DDR_CLK_FREQ	100000000

/*
 * These can be toggled for performance analysis, otherwise use default.
 */
#define CONFIG_SYS_CACHE_STASHING
#define CONFIG_BACKSIDE_L2_CACHE
#define CONFIG_SYS_INIT_L2CSR0		L2CSR0_L2E
#define CONFIG_BTB			/* toggle branch predition */
#define CONFIG_DDR_ECC
#ifdef CONFIG_DDR_ECC
#define CONFIG_ECC_INIT_VIA_DDRCONTROLLER
#define CONFIG_MEM_INIT_VALUE		0xdeadbeef
#endif

#define CONFIG_CMD_MEMTEST
#define CONFIG_SYS_MEMTEST_START	0x00200000 /* memtest works on */
#define CONFIG_SYS_MEMTEST_END		0x00400000
#define CONFIG_SYS_ALT_MEMTEST
#define CONFIG_PANIC_HANG	/* do not reset board on panic */

/*
 *  Config the L3 Cache as L3 SRAM
 */
#define CONFIG_SYS_INIT_L3_ADDR		0xFFFC0000
#define CONFIG_SYS_L3_SIZE		(256 << 10)
#define CONFIG_SPL_GD_ADDR		(CONFIG_SYS_INIT_L3_ADDR + 32 * 1024)
#ifdef CONFIG_RAMBOOT_PBL
#define CONFIG_ENV_ADDR			(CONFIG_SPL_GD_ADDR + 4 * 1024)
#endif
#define CONFIG_SPL_RELOC_MALLOC_ADDR	(CONFIG_SPL_GD_ADDR + 12 * 1024)
#define CONFIG_SPL_RELOC_MALLOC_SIZE	(30 << 10)
#define CONFIG_SPL_RELOC_STACK		(CONFIG_SPL_GD_ADDR + 64 * 1024)
#define CONFIG_SPL_RELOC_STACK_SIZE	(22 << 10)

#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_DCSRBAR		0xf0000000
#define CONFIG_SYS_DCSRBAR_PHYS		0xf00000000ull
#endif

/* EEPROM */
#define CONFIG_SYS_I2C_EEPROM_NXID
#define CONFIG_SYS_EEPROM_BUS_NUM	0
#define CONFIG_SYS_I2C_EEPROM_ADDR	0x51
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	2
#define CONFIG_SYS_EEPROM_PAGE_WRITE_BITS 3
#define CONFIG_SYS_EEPROM_PAGE_WRITE_DELAY_MS 5

/*
 * DDR Setup
 */
#define CONFIG_VERY_BIG_RAM
#define CONFIG_SYS_DDR_SDRAM_BASE	0x00000000
#define CONFIG_SYS_SDRAM_BASE		CONFIG_SYS_DDR_SDRAM_BASE
#define CONFIG_DIMM_SLOTS_PER_CTLR	1
#define CONFIG_CHIP_SELECTS_PER_CTRL	(4 * CONFIG_DIMM_SLOTS_PER_CTLR)
#define CONFIG_FSL_DDR_INTERACTIVE
#if defined(CONFIG_T1024RDB)
#define CONFIG_DDR_SPD
#define CONFIG_SYS_FSL_DDR3
#define CONFIG_SYS_SPD_BUS_NUM	0
#define SPD_EEPROM_ADDRESS	0x50
#define CONFIG_SYS_SDRAM_SIZE	4096	/* for fixed parameter use */
#elif defined(CONFIG_T1023RDB)
#define CONFIG_SYS_FSL_DDR4
#define CONFIG_SYS_DDR_RAW_TIMING
#define CONFIG_SYS_SDRAM_SIZE   2048
#endif

/*
 * IFC Definitions
 */
#define CONFIG_SYS_FLASH_BASE	0xe8000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_FLASH_BASE_PHYS	(0xf00000000ull | CONFIG_SYS_FLASH_BASE)
#else
#define CONFIG_SYS_FLASH_BASE_PHYS	CONFIG_SYS_FLASH_BASE
#endif

#define CONFIG_SYS_NOR0_CSPR_EXT	(0xf)
#define CONFIG_SYS_NOR0_CSPR	(CSPR_PHYS_ADDR(CONFIG_SYS_FLASH_BASE_PHYS) | \
				CSPR_PORT_SIZE_16 | \
				CSPR_MSEL_NOR | \
				CSPR_V)
#define CONFIG_SYS_NOR_AMASK	IFC_AMASK(128*1024*1024)

/* NOR Flash Timing Params */
#if defined(CONFIG_T1024RDB)
#define CONFIG_SYS_NOR_CSOR	CSOR_NAND_TRHZ_80
#elif defined(CONFIG_T1023RDB)
#define CONFIG_SYS_NOR_CSOR    (CSOR_NOR_ADM_SHIFT(0) | \
				CSOR_NAND_TRHZ_80 | CSOR_NOR_ADM_SHFT_MODE_EN)
#endif
#define CONFIG_SYS_NOR_FTIM0	(FTIM0_NOR_TACSE(0x4) | \
				FTIM0_NOR_TEADC(0x5) | \
				FTIM0_NOR_TEAHC(0x5))
#define CONFIG_SYS_NOR_FTIM1	(FTIM1_NOR_TACO(0x35) | \
				FTIM1_NOR_TRAD_NOR(0x1A) |\
				FTIM1_NOR_TSEQRAD_NOR(0x13))
#define CONFIG_SYS_NOR_FTIM2	(FTIM2_NOR_TCS(0x4) | \
				FTIM2_NOR_TCH(0x4) | \
				FTIM2_NOR_TWPH(0x0E) | \
				FTIM2_NOR_TWP(0x1c))
#define CONFIG_SYS_NOR_FTIM3	0x0

#define CONFIG_SYS_FLASH_QUIET_TEST
#define CONFIG_FLASH_SHOW_PROGRESS	45 /* count down from 45/5: 9..1 */

#define CONFIG_SYS_MAX_FLASH_BANKS	1	/* number of banks */
#define CONFIG_SYS_MAX_FLASH_SECT	1024	/* sectors per device */
#define CONFIG_SYS_FLASH_ERASE_TOUT	60000	/* Flash Erase Timeout (ms) */
#define CONFIG_SYS_FLASH_WRITE_TOUT	500	/* Flash Write Timeout (ms) */

#define CONFIG_SYS_FLASH_EMPTY_INFO
#define CONFIG_SYS_FLASH_BANKS_LIST	{CONFIG_SYS_FLASH_BASE_PHYS}

#ifdef CONFIG_T1024RDB
/* CPLD on IFC */
#define CONFIG_SYS_CPLD_BASE		0xffdf0000
#define CONFIG_SYS_CPLD_BASE_PHYS	(0xf00000000ull | CONFIG_SYS_CPLD_BASE)
#define CONFIG_SYS_CSPR2_EXT		(0xf)
#define CONFIG_SYS_CSPR2		(CSPR_PHYS_ADDR(CONFIG_SYS_CPLD_BASE) \
						| CSPR_PORT_SIZE_8 \
						| CSPR_MSEL_GPCM \
						| CSPR_V)
#define CONFIG_SYS_AMASK2		IFC_AMASK(64*1024)
#define CONFIG_SYS_CSOR2		0x0

/* CPLD Timing parameters for IFC CS2 */
#define CONFIG_SYS_CS2_FTIM0		(FTIM0_GPCM_TACSE(0x0e) | \
						FTIM0_GPCM_TEADC(0x0e) | \
						FTIM0_GPCM_TEAHC(0x0e))
#define CONFIG_SYS_CS2_FTIM1		(FTIM1_GPCM_TACO(0x0e) | \
						FTIM1_GPCM_TRAD(0x1f))
#define CONFIG_SYS_CS2_FTIM2		(FTIM2_GPCM_TCS(0x0e) | \
						FTIM2_GPCM_TCH(0x8) | \
						FTIM2_GPCM_TWP(0x1f))
#define CONFIG_SYS_CS2_FTIM3		0x0
#endif

/* NAND Flash on IFC */
#define CONFIG_NAND_FSL_IFC
#define CONFIG_SYS_NAND_BASE		0xff800000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_NAND_BASE_PHYS	(0xf00000000ull | CONFIG_SYS_NAND_BASE)
#else
#define CONFIG_SYS_NAND_BASE_PHYS	CONFIG_SYS_NAND_BASE
#endif
#define CONFIG_SYS_NAND_CSPR_EXT	(0xf)
#define CONFIG_SYS_NAND_CSPR	(CSPR_PHYS_ADDR(CONFIG_SYS_NAND_BASE_PHYS) \
				| CSPR_PORT_SIZE_8 /* Port Size = 8 bit */ \
				| CSPR_MSEL_NAND	/* MSEL = NAND */ \
				| CSPR_V)
#define CONFIG_SYS_NAND_AMASK	IFC_AMASK(64*1024)

#if defined(CONFIG_T1024RDB)
#define CONFIG_SYS_NAND_CSOR	(CSOR_NAND_ECC_ENC_EN	/* ECC on encode */ \
				| CSOR_NAND_ECC_DEC_EN  /* ECC on decode */ \
				| CSOR_NAND_ECC_MODE_4  /* 4-bit ECC */ \
				| CSOR_NAND_RAL_3	/* RAL = 3Byes */ \
				| CSOR_NAND_PGS_4K	/* Page Size = 4K */ \
				| CSOR_NAND_SPRZ_224	/* Spare size = 224 */ \
				| CSOR_NAND_PB(64))	/*Pages Per Block = 64*/
#define CONFIG_SYS_NAND_BLOCK_SIZE	(512 * 1024)
#elif defined(CONFIG_T1023RDB)
#define CONFIG_SYS_NAND_CSOR	(CSOR_NAND_ECC_ENC_EN   /* ECC on encode */ \
				| CSOR_NAND_ECC_DEC_EN  /* ECC on decode */ \
				| CSOR_NAND_ECC_MODE_4	/* 4-bit ECC */ \
				| CSOR_NAND_RAL_3	/* RAL 3Bytes */ \
				| CSOR_NAND_PGS_2K	/* Page Size = 2K */ \
				| CSOR_NAND_SPRZ_128	/* Spare size = 128 */ \
				| CSOR_NAND_PB(64))	/*Pages Per Block = 64*/
#define CONFIG_SYS_NAND_BLOCK_SIZE	(128 * 1024)
#endif

#define CONFIG_SYS_NAND_ONFI_DETECTION
/* ONFI NAND Flash mode0 Timing Params */
#define CONFIG_SYS_NAND_FTIM0		(FTIM0_NAND_TCCST(0x07) | \
					FTIM0_NAND_TWP(0x18)   | \
					FTIM0_NAND_TWCHT(0x07) | \
					FTIM0_NAND_TWH(0x0a))
#define CONFIG_SYS_NAND_FTIM1		(FTIM1_NAND_TADLE(0x32) | \
					FTIM1_NAND_TWBE(0x39)  | \
					FTIM1_NAND_TRR(0x0e)   | \
					FTIM1_NAND_TRP(0x18))
#define CONFIG_SYS_NAND_FTIM2		(FTIM2_NAND_TRAD(0x0f) | \
					FTIM2_NAND_TREH(0x0a) | \
					FTIM2_NAND_TWHRE(0x1e))
#define CONFIG_SYS_NAND_FTIM3		0x0

#define CONFIG_SYS_NAND_DDR_LAW		11
#define CONFIG_SYS_NAND_BASE_LIST	{ CONFIG_SYS_NAND_BASE }
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_MTD_NAND_VERIFY_WRITE

#if defined(CONFIG_NAND)
#define CONFIG_SYS_CSPR0_EXT		CONFIG_SYS_NAND_CSPR_EXT
#define CONFIG_SYS_CSPR0		CONFIG_SYS_NAND_CSPR
#define CONFIG_SYS_AMASK0		CONFIG_SYS_NAND_AMASK
#define CONFIG_SYS_CSOR0		CONFIG_SYS_NAND_CSOR
#define CONFIG_SYS_CS0_FTIM0		CONFIG_SYS_NAND_FTIM0
#define CONFIG_SYS_CS0_FTIM1		CONFIG_SYS_NAND_FTIM1
#define CONFIG_SYS_CS0_FTIM2		CONFIG_SYS_NAND_FTIM2
#define CONFIG_SYS_CS0_FTIM3		CONFIG_SYS_NAND_FTIM3
#define CONFIG_SYS_CSPR1_EXT		CONFIG_SYS_NOR0_CSPR_EXT
#define CONFIG_SYS_CSPR1		CONFIG_SYS_NOR0_CSPR
#define CONFIG_SYS_AMASK1		CONFIG_SYS_NOR_AMASK
#define CONFIG_SYS_CSOR1		CONFIG_SYS_NOR_CSOR
#define CONFIG_SYS_CS1_FTIM0		CONFIG_SYS_NOR_FTIM0
#define CONFIG_SYS_CS1_FTIM1		CONFIG_SYS_NOR_FTIM1
#define CONFIG_SYS_CS1_FTIM2		CONFIG_SYS_NOR_FTIM2
#define CONFIG_SYS_CS1_FTIM3		CONFIG_SYS_NOR_FTIM3
#else
#define CONFIG_SYS_CSPR0_EXT		CONFIG_SYS_NOR0_CSPR_EXT
#define CONFIG_SYS_CSPR0		CONFIG_SYS_NOR0_CSPR
#define CONFIG_SYS_AMASK0		CONFIG_SYS_NOR_AMASK
#define CONFIG_SYS_CSOR0		CONFIG_SYS_NOR_CSOR
#define CONFIG_SYS_CS0_FTIM0		CONFIG_SYS_NOR_FTIM0
#define CONFIG_SYS_CS0_FTIM1		CONFIG_SYS_NOR_FTIM1
#define CONFIG_SYS_CS0_FTIM2		CONFIG_SYS_NOR_FTIM2
#define CONFIG_SYS_CS0_FTIM3		CONFIG_SYS_NOR_FTIM3
#define CONFIG_SYS_CSPR1_EXT		CONFIG_SYS_NAND_CSPR_EXT
#define CONFIG_SYS_CSPR1		CONFIG_SYS_NAND_CSPR
#define CONFIG_SYS_AMASK1		CONFIG_SYS_NAND_AMASK
#define CONFIG_SYS_CSOR1		CONFIG_SYS_NAND_CSOR
#define CONFIG_SYS_CS1_FTIM0		CONFIG_SYS_NAND_FTIM0
#define CONFIG_SYS_CS1_FTIM1		CONFIG_SYS_NAND_FTIM1
#define CONFIG_SYS_CS1_FTIM2		CONFIG_SYS_NAND_FTIM2
#define CONFIG_SYS_CS1_FTIM3		CONFIG_SYS_NAND_FTIM3
#endif

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_MONITOR_BASE		CONFIG_SPL_TEXT_BASE
#else
#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE
#endif

#if defined(CONFIG_RAMBOOT_PBL)
#define CONFIG_SYS_RAMBOOT
#endif

#define CONFIG_BOARD_EARLY_INIT_R
#define CONFIG_MISC_INIT_R

#define CONFIG_HWCONFIG

/* define to use L1 as initial stack */
#define CONFIG_L1_INIT_RAM
#define CONFIG_SYS_INIT_RAM_LOCK
#define CONFIG_SYS_INIT_RAM_ADDR	0xfdd00000	/* Initial L1 address */
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_INIT_RAM_ADDR_PHYS_HIGH	0xf
#define CONFIG_SYS_INIT_RAM_ADDR_PHYS_LOW	0xfe03c000
/* The assembler doesn't like typecast */
#define CONFIG_SYS_INIT_RAM_ADDR_PHYS \
	((CONFIG_SYS_INIT_RAM_ADDR_PHYS_HIGH * 1ull << 32) | \
	  CONFIG_SYS_INIT_RAM_ADDR_PHYS_LOW)
#else
#define CONFIG_SYS_INIT_RAM_ADDR_PHYS	0xfe03c000 /* Initial L1 address */
#define CONFIG_SYS_INIT_RAM_ADDR_PHYS_HIGH 0
#define CONFIG_SYS_INIT_RAM_ADDR_PHYS_LOW CONFIG_SYS_INIT_RAM_ADDR_PHYS
#endif
#define CONFIG_SYS_INIT_RAM_SIZE		0x00004000

#define CONFIG_SYS_GBL_DATA_OFFSET	(CONFIG_SYS_INIT_RAM_SIZE - \
					GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_GBL_DATA_OFFSET

#define CONFIG_SYS_MONITOR_LEN		(768 * 1024)
#define CONFIG_SYS_MALLOC_LEN		(10 * 1024 * 1024)

/* Serial Port */
#define CONFIG_CONS_INDEX	1
#define CONFIG_SYS_NS16550
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	1
#define CONFIG_SYS_NS16550_CLK		(get_bus_freq(0)/2)

#define CONFIG_SYS_BAUDRATE_TABLE	\
	{300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}

#define CONFIG_SYS_NS16550_COM1	(CONFIG_SYS_CCSRBAR+0x11C500)
#define CONFIG_SYS_NS16550_COM2	(CONFIG_SYS_CCSRBAR+0x11C600)
#define CONFIG_SYS_NS16550_COM3	(CONFIG_SYS_CCSRBAR+0x11D500)
#define CONFIG_SYS_NS16550_COM4	(CONFIG_SYS_CCSRBAR+0x11D600)
#define CONFIG_SYS_CONSOLE_IS_IN_ENV	/* determine from environment */

/* Use the HUSH parser */
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2 "> "

/* Video */
#undef CONFIG_FSL_DIU_FB	/* RDB doesn't support DIU */
#ifdef CONFIG_FSL_DIU_FB
#define CONFIG_SYS_DIU_ADDR	(CONFIG_SYS_CCSRBAR + 0x180000)
#define CONFIG_VIDEO
#define CONFIG_CMD_BMP
#define CONFIG_CFB_CONSOLE
#define CONFIG_VIDEO_SW_CURSOR
#define CONFIG_VGA_AS_SINGLE_DEVICE
#define CONFIG_VIDEO_LOGO
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_CFI_FLASH_USE_WEAK_ACCESSORS
/*
 * With CONFIG_CFI_FLASH_USE_WEAK_ACCESSORS, flash I/O is really slow, so
 * disable empty flash sector detection, which is I/O-intensive.
 */
#undef CONFIG_SYS_FLASH_EMPTY_INFO
#endif

/* pass open firmware flat tree */
#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP
#define CONFIG_OF_STDOUT_VIA_ALIAS

/* new uImage format support */
#define CONFIG_FIT
#define CONFIG_FIT_VERBOSE	/* enable fit_format_{error,warning}() */

/* I2C */
#define CONFIG_SYS_I2C
#define CONFIG_SYS_I2C_FSL		/* Use FSL common I2C driver */
#define CONFIG_SYS_FSL_I2C_SPEED	50000	/* I2C speed in Hz */
#define CONFIG_SYS_FSL_I2C_SLAVE	0x7F
#define CONFIG_SYS_FSL_I2C2_SPEED	50000	/* I2C speed in Hz */
#define CONFIG_SYS_FSL_I2C2_SLAVE	0x7F
#define CONFIG_SYS_FSL_I2C_OFFSET	0x118000
#define CONFIG_SYS_FSL_I2C2_OFFSET	0x118100

#define I2C_PCA6408_BUS_NUM		1
#define I2C_PCA6408_ADDR		0x20

/* I2C bus multiplexer */
#define I2C_MUX_CH_DEFAULT	0x8

/*
 * RTC configuration
 */
#define RTC
#define CONFIG_RTC_DS1337	1
#define CONFIG_SYS_I2C_RTC_ADDR	0x68

/*
 * eSPI - Enhanced SPI
 */
#define CONFIG_FSL_ESPI
#define CONFIG_SPI_FLASH
#if defined(CONFIG_T1024RDB)
#define CONFIG_SPI_FLASH_STMICRO
#elif defined(CONFIG_T1023RDB)
#define CONFIG_SPI_FLASH_SPANSION
#endif
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_BAR
#define CONFIG_SF_DEFAULT_SPEED	10000000
#define CONFIG_SF_DEFAULT_MODE	0
#define CONFIG_SPI_FLASH_WINBOND

/*
 * SATA
 */

#define CONFIG_FSL_SATA_V2
#ifdef CONFIG_FSL_SATA_V2
#define CONFIG_LIBATA
#define CONFIG_FSL_SATA
#define CONFIG_SYS_SATA_MAX_DEVICE	1
#define CONFIG_SATA1
#define CONFIG_SYS_SATA1			CONFIG_SYS_MPC85xx_SATA1_ADDR
#define CONFIG_SYS_SATA1_FLAGS		FLAGS_DMA
#define CONFIG_LBA48
#define CONFIG_SATA
#define CONFIG_CMD_SATA
#endif

/*
 * General PCIe
 * Memory space is mapped 1-1, but I/O space must start from 0.
 */
#define CONFIG_PCI		/* Enable PCI/PCIE */
#define CONFIG_PCIE1		/* PCIE controler 1 */
#define CONFIG_PCIE2		/* PCIE controler 2 */
#define CONFIG_PCIE3		/* PCIE controler 3 */
#ifdef CONFIG_PPC_T1040
#define CONFIG_PCIE4		/* PCIE controler 4 */
#endif
#define CONFIG_FSL_PCI_INIT	/* Use common FSL init code */
#define CONFIG_SYS_PCI_64BIT	/* enable 64-bit PCI resources */
#define CONFIG_PCI_INDIRECT_BRIDGE

#ifdef CONFIG_PCI
/* controller 1, direct to uli, tgtid 3, Base address 20000 */
#ifdef CONFIG_PCIE1
#define	CONFIG_SYS_PCIE1_MEM_VIRT	0x80000000
#ifdef CONFIG_PHYS_64BIT
#define	CONFIG_SYS_PCIE1_MEM_BUS	0xe0000000
#define	CONFIG_SYS_PCIE1_MEM_PHYS	0xc00000000ull
#else
#define CONFIG_SYS_PCIE1_MEM_BUS	0x80000000
#define CONFIG_SYS_PCIE1_MEM_PHYS	0x80000000
#endif
#define CONFIG_SYS_PCIE1_MEM_SIZE	0x10000000	/* 256M */
#define CONFIG_SYS_PCIE1_IO_VIRT	0xf8000000
#define CONFIG_SYS_PCIE1_IO_BUS		0x00000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE1_IO_PHYS	0xff8000000ull
#else
#define CONFIG_SYS_PCIE1_IO_PHYS	0xf8000000
#endif
#define CONFIG_SYS_PCIE1_IO_SIZE	0x00010000	/* 64k */
#endif

/* controller 2, Slot 2, tgtid 2, Base address 201000 */
#ifdef CONFIG_PCIE2
#define CONFIG_SYS_PCIE2_MEM_VIRT	0x90000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE2_MEM_BUS	0xe0000000
#define CONFIG_SYS_PCIE2_MEM_PHYS	0xc10000000ull
#else
#define CONFIG_SYS_PCIE2_MEM_BUS	0x90000000
#define CONFIG_SYS_PCIE2_MEM_PHYS	0x90000000
#endif
#define CONFIG_SYS_PCIE2_MEM_SIZE	0x10000000	/* 256M */
#define CONFIG_SYS_PCIE2_IO_VIRT	0xf8010000
#define CONFIG_SYS_PCIE2_IO_BUS		0x00000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE2_IO_PHYS	0xff8010000ull
#else
#define CONFIG_SYS_PCIE2_IO_PHYS	0xf8010000
#endif
#define CONFIG_SYS_PCIE2_IO_SIZE	0x00010000	/* 64k */
#endif

/* controller 3, Slot 1, tgtid 1, Base address 202000 */
#ifdef CONFIG_PCIE3
#define CONFIG_SYS_PCIE3_MEM_VIRT	0xa0000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE3_MEM_BUS	0xe0000000
#define CONFIG_SYS_PCIE3_MEM_PHYS	0xc20000000ull
#else
#define CONFIG_SYS_PCIE3_MEM_BUS	0xa0000000
#define CONFIG_SYS_PCIE3_MEM_PHYS	0xa0000000
#endif
#define CONFIG_SYS_PCIE3_MEM_SIZE	0x10000000	/* 256M */
#define CONFIG_SYS_PCIE3_IO_VIRT	0xf8020000
#define CONFIG_SYS_PCIE3_IO_BUS		0x00000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE3_IO_PHYS	0xff8020000ull
#else
#define CONFIG_SYS_PCIE3_IO_PHYS	0xf8020000
#endif
#define CONFIG_SYS_PCIE3_IO_SIZE	0x00010000	/* 64k */
#endif

/* controller 4, Base address 203000, to be removed */
#ifdef CONFIG_PCIE4
#define CONFIG_SYS_PCIE4_MEM_VIRT       0xb0000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE4_MEM_BUS	0xe0000000
#define CONFIG_SYS_PCIE4_MEM_PHYS       0xc30000000ull
#else
#define CONFIG_SYS_PCIE4_MEM_BUS	0xb0000000
#define CONFIG_SYS_PCIE4_MEM_PHYS	0xb0000000
#endif
#define CONFIG_SYS_PCIE4_MEM_SIZE       0x10000000      /* 256M */
#define CONFIG_SYS_PCIE4_IO_VIRT	0xf8030000
#define CONFIG_SYS_PCIE4_IO_BUS		0x00000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_PCIE4_IO_PHYS	0xff8030000ull
#else
#define CONFIG_SYS_PCIE4_IO_PHYS	0xf8030000
#endif
#define CONFIG_SYS_PCIE4_IO_SIZE	0x00010000      /* 64k */
#endif

#define CONFIG_PCI_PNP			/* do pci plug-and-play */
#define CONFIG_E1000
#define CONFIG_PCI_SCAN_SHOW		/* show pci devices on startup */
#define CONFIG_DOS_PARTITION
#endif	/* CONFIG_PCI */

/*
 * USB
 */
#define CONFIG_HAS_FSL_DR_USB

#ifdef CONFIG_HAS_FSL_DR_USB
#define CONFIG_USB_EHCI
#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_USB_EHCI_FSL
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET
#define CONFIG_CMD_EXT2
#endif

/*
 * SDHC
 */
#ifdef CONFIG_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR	CONFIG_SYS_MPC85xx_ESDHC_ADDR
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION
#endif

/* Qman/Bman */
#ifndef CONFIG_NOBQFMAN
#define CONFIG_SYS_DPAA_QBMAN		/* Support Q/Bman */
#define CONFIG_SYS_BMAN_NUM_PORTALS	10
#define CONFIG_SYS_BMAN_MEM_BASE	0xf4000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_BMAN_MEM_PHYS	0xff4000000ull
#else
#define CONFIG_SYS_BMAN_MEM_PHYS	CONFIG_SYS_BMAN_MEM_BASE
#endif
#define CONFIG_SYS_BMAN_MEM_SIZE	0x02000000
#define CONFIG_SYS_BMAN_SP_CENA_SIZE    0x4000
#define CONFIG_SYS_BMAN_SP_CINH_SIZE    0x1000
#define CONFIG_SYS_BMAN_CENA_BASE       CONFIG_SYS_BMAN_MEM_BASE
#define CONFIG_SYS_BMAN_CENA_SIZE       (CONFIG_SYS_BMAN_MEM_SIZE >> 1)
#define CONFIG_SYS_BMAN_CINH_BASE       (CONFIG_SYS_BMAN_MEM_BASE + \
					CONFIG_SYS_BMAN_CENA_SIZE)
#define CONFIG_SYS_BMAN_CINH_SIZE       (CONFIG_SYS_BMAN_MEM_SIZE >> 1)
#define CONFIG_SYS_BMAN_SWP_ISDR_REG	0xE08
#define CONFIG_SYS_QMAN_NUM_PORTALS	10
#define CONFIG_SYS_QMAN_MEM_BASE	0xf6000000
#ifdef CONFIG_PHYS_64BIT
#define CONFIG_SYS_QMAN_MEM_PHYS	0xff6000000ull
#else
#define CONFIG_SYS_QMAN_MEM_PHYS	CONFIG_SYS_QMAN_MEM_BASE
#endif
#define CONFIG_SYS_QMAN_MEM_SIZE	0x02000000
#define CONFIG_SYS_QMAN_SP_CENA_SIZE    0x4000
#define CONFIG_SYS_QMAN_SP_CINH_SIZE    0x1000
#define CONFIG_SYS_QMAN_CENA_BASE       CONFIG_SYS_QMAN_MEM_BASE
#define CONFIG_SYS_QMAN_CENA_SIZE       (CONFIG_SYS_QMAN_MEM_SIZE >> 1)
#define CONFIG_SYS_QMAN_CINH_BASE       (CONFIG_SYS_QMAN_MEM_BASE + \
					CONFIG_SYS_QMAN_CENA_SIZE)
#define CONFIG_SYS_QMAN_CINH_SIZE       (CONFIG_SYS_QMAN_MEM_SIZE >> 1)
#define CONFIG_SYS_QMAN_SWP_ISDR_REG	0xE08

#define CONFIG_SYS_DPAA_FMAN

#ifdef CONFIG_T1024RDB
#define CONFIG_QE
#define CONFIG_U_QE
#endif
/* Default address of microcode for the Linux FMan driver */
#if defined(CONFIG_SPIFLASH)
/*
 * env is stored at 0x100000, sector size is 0x10000, ucode is stored after
 * env, so we got 0x110000.
 */
#define CONFIG_SYS_QE_FW_IN_SPIFLASH
#define CONFIG_SYS_FMAN_FW_ADDR	0x110000
#define CONFIG_SYS_QE_FW_ADDR	0x130000
#elif defined(CONFIG_SDCARD)
/*
 * PBL SD boot image should stored at 0x1000(8 blocks), the size of the image is
 * about 1MB (2048 blocks), Env is stored after the image, and the env size is
 * 0x2000 (16 blocks), 8 + 2048 + 16 = 2072, enlarge it to 2080(0x820).
 */
#define CONFIG_SYS_QE_FMAN_FW_IN_MMC
#define CONFIG_SYS_FMAN_FW_ADDR		(512 * 0x820)
#define CONFIG_SYS_QE_FW_ADDR		(512 * 0x920)
#elif defined(CONFIG_NAND)
#define CONFIG_SYS_QE_FMAN_FW_IN_NAND
#if defined(CONFIG_T1024RDB)
#define CONFIG_SYS_FMAN_FW_ADDR		(3 * CONFIG_SYS_NAND_BLOCK_SIZE)
#define CONFIG_SYS_QE_FW_ADDR		(4 * CONFIG_SYS_NAND_BLOCK_SIZE)
#elif defined(CONFIG_T1023RDB)
#define CONFIG_SYS_FMAN_FW_ADDR		(11 * CONFIG_SYS_NAND_BLOCK_SIZE)
#define CONFIG_SYS_QE_FW_ADDR		(12 * CONFIG_SYS_NAND_BLOCK_SIZE)
#endif
#elif defined(CONFIG_SRIO_PCIE_BOOT_SLAVE)
/*
 * Slave has no ucode locally, it can fetch this from remote. When implementing
 * in two corenet boards, slave's ucode could be stored in master's memory
 * space, the address can be mapped from slave TLB->slave LAW->
 * slave SRIO or PCIE outbound window->master inbound window->
 * master LAW->the ucode address in master's memory space.
 */
#define CONFIG_SYS_QE_FMAN_FW_IN_REMOTE
#define CONFIG_SYS_FMAN_FW_ADDR		0xFFE00000
#else
#define CONFIG_SYS_QE_FMAN_FW_IN_NOR
#define CONFIG_SYS_FMAN_FW_ADDR		0xEFF00000
#define CONFIG_SYS_QE_FW_ADDR		0xEFE00000
#endif
#define CONFIG_SYS_QE_FMAN_FW_LENGTH	0x10000
#define CONFIG_SYS_FDT_PAD		(0x3000 + CONFIG_SYS_QE_FMAN_FW_LENGTH)
#endif /* CONFIG_NOBQFMAN */

#ifdef CONFIG_SYS_DPAA_FMAN
#define CONFIG_FMAN_ENET
#define CONFIG_PHYLIB_10G
#define CONFIG_PHY_REALTEK
#define CONFIG_PHY_AQUANTIA
#if defined(CONFIG_T1024RDB)
#define MARVELL_SWITCH_PORT5_ADDR      0x15
#define MARVELL_SWITCH_PORT6_ADDR      0x16
#define RGMII_PHY1_ADDR		0x2
#define RGMII_PHY2_ADDR		0x6
#define SGMII_AQR_PHY_ADDR	0x2
#define FM1_10GEC1_PHY_ADDR	0x1
#elif defined(CONFIG_T1023RDB)
#define RGMII_PHY1_ADDR		0x1
#define SGMII_RTK_PHY_ADDR	0x3
#define SGMII_AQR_PHY_ADDR	0x2
#endif
#endif

#ifdef CONFIG_FMAN_ENET
#define CONFIG_MII		/* MII PHY management */
#define CONFIG_ETHPRIME		"FM1@DTSEC4"
#define CONFIG_PHY_GIGE		/* Include GbE speed/duplex detection */
#endif

/*
 * Dynamic MTD Partition support with mtdparts
 */
#ifndef CONFIG_SYS_NO_FLASH
#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS
#define CONFIG_CMD_MTDPARTS
#define CONFIG_FLASH_CFI_MTD
#define MTDIDS_DEFAULT "nor0=fe8000000.nor,nand0=fff800000.flash," \
			"spi0=spife110000.1"
#define MTDPARTS_DEFAULT "mtdparts=fe8000000.nor:1m(uboot),5m(kernel)," \
			"128k(dtb),96m(fs),-(user);fff800000.flash:1m(uboot)," \
			"5m(kernel),128k(dtb),96m(fs),-(user);spife110000.0:" \
			"1m(uboot),5m(kernel),128k(dtb),-(user)"
#endif

/*
 * Environment
 */
#define CONFIG_LOADS_ECHO		/* echo on for serial download */
#define CONFIG_SYS_LOADS_BAUD_CHANGE	/* allow baudrate change */

/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_DATE
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_EEPROM
#define CONFIG_CMD_ELF
#define CONFIG_CMD_ERRATA
#define CONFIG_CMD_GREPENV
#define CONFIG_CMD_IRQ
#define CONFIG_CMD_I2C
#define CONFIG_CMD_MII
#define CONFIG_CMD_PING
#define CONFIG_CMD_ECHO
#define CONFIG_CMD_REGINFO
#define CONFIG_CMD_SETEXPR
#define CONFIG_CMD_BDI

#ifdef CONFIG_PCI
#define CONFIG_CMD_PCI
#define CONFIG_CMD_NET
#endif

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_LONGHELP			/* undef to save memory	*/
#define CONFIG_CMDLINE_EDITING			/* Command-line editing */
#define CONFIG_AUTO_COMPLETE			/* add autocompletion support */
#define CONFIG_SYS_LOAD_ADDR	0x2000000	/* default load address */
#define CONFIG_SYS_PROMPT	"=> "		/* Monitor Command Prompt */
#ifdef CONFIG_CMD_KGDB
#define CONFIG_SYS_CBSIZE	1024		/* Console I/O Buffer Size */
#else
#define CONFIG_SYS_CBSIZE	256		/* Console I/O Buffer Size */
#endif
#define CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE+sizeof(CONFIG_SYS_PROMPT)+16)
#define CONFIG_SYS_MAXARGS	16		/* max number of command args */
#define CONFIG_SYS_BARGSIZE  CONFIG_SYS_CBSIZE  /* Boot Argument Buffer Size */

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 64 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ	(64 << 20)	/* Initial map for Linux*/
#define CONFIG_SYS_BOOTM_LEN	(64 << 20)	/* Increase max gunzip size */

#ifdef CONFIG_CMD_KGDB
#define CONFIG_KGDB_BAUDRATE	230400	/* speed to run kgdb serial port */
#endif

/*
 * Environment Configuration
 */
#define CONFIG_ROOTPATH		"/opt/nfsroot"
#define CONFIG_BOOTFILE		"uImage"
#define CONFIG_UBOOTPATH	u-boot.bin /* U-Boot image on TFTP server */
#define CONFIG_LOADADDR		1000000 /* default location for tftp, bootm */
#define CONFIG_BOOTDELAY	10	/* -1 disables auto-boot */
#define CONFIG_BAUDRATE		115200
#define __USB_PHY_TYPE		utmi

#ifdef CONFIG_PPC_T1024
#define CONFIG_BOARDNAME t1024rdb
#define BANK_INTLV auto
#else
#define CONFIG_BOARDNAME t1023rdb
#define BANK_INTLV  null
#endif

#if 0   /*SENAO*/
#define	CONFIG_EXTRA_ENV_SETTINGS				\
	"hwconfig=fsl_ddr:ctlr_intlv=cacheline,"		\
	"bank_intlv=" __stringify(BANK_INTLV) "\0"		\
	"usb1:dr_mode=host,phy_type=" __stringify(__USB_PHY_TYPE) "\0"  \
	"ramdiskfile=" __stringify(CONFIG_BOARDNAME) "/ramdisk.uboot\0" \
	"fdtfile=" __stringify(CONFIG_BOARDNAME) "/"		\
	__stringify(CONFIG_BOARDNAME) ".dtb\0"			\
	"uboot=" __stringify(CONFIG_UBOOTPATH) "\0"		\
	"ubootaddr=" __stringify(CONFIG_SYS_TEXT_BASE) "\0"	\
	"bootargs=root=/dev/ram rw console=ttyS0,115200\0" \
	"netdev=eth0\0"						\
	"tftpflash=tftpboot $loadaddr $uboot && "		\
	"protect off $ubootaddr +$filesize && "			\
	"erase $ubootaddr +$filesize && "			\
	"cp.b $loadaddr $ubootaddr $filesize && "		\
	"protect on $ubootaddr +$filesize && "			\
	"cmp.b $loadaddr $ubootaddr $filesize\0"		\
	"consoledev=ttyS0\0"					\
	"ramdiskaddr=2000000\0"					\
	"fdtaddr=c00000\0"					\
	"bdev=sda3\0"

#define CONFIG_LINUX					\
	"setenv bootargs root=/dev/ram rw "		\
	"console=$consoledev,$baudrate $othbootargs;"	\
	"setenv ramdiskaddr 0x02000000;"		\
	"setenv fdtaddr 0x00c00000;"			\
	"setenv loadaddr 0x1000000;"			\
	"bootm $loadaddr $ramdiskaddr $fdtaddr"
#else
#define	CONFIG_EXTRA_ENV_SETTINGS				\
	"hwconfig=fsl_ddr:ctlr_intlv=cacheline,"		\
	"bank_intlv=" __stringify(BANK_INTLV) "\0"		\
	"usb1:dr_mode=host,phy_type=" __stringify(__USB_PHY_TYPE) "\0"  \
	"ramdiskfile=" __stringify(CONFIG_BOARDNAME) "/ramdisk.uboot\0" \
	"fdtfile=" __stringify(CONFIG_BOARDNAME) "/"		\
	__stringify(CONFIG_BOARDNAME) ".dtb\0"			\
	"uboot=" __stringify(CONFIG_UBOOTPATH) "\0"		\
	"ubootaddr=" __stringify(CONFIG_SYS_TEXT_BASE) "\0"	\
	"bootargs=root=/dev/ram rw console=ttyS0,115200\0" \
	"netdev=eth0\0"						\
	"tftpflash=tftpboot $loadaddr $uboot && "		\
	"protect off $ubootaddr +$filesize && "			\
	"erase $ubootaddr +$filesize && "			\
	"cp.b $loadaddr $ubootaddr $filesize && "		\
	"protect on $ubootaddr +$filesize && "			\
	"cmp.b $loadaddr $ubootaddr $filesize\0"		\
	"consoledev=ttyS0\0"					\
	"ramdiskaddr=2000000\0"					\
	"fdtaddr=2000000\0"					\
	"bdev=sda3\0"	\
	"ethaddr=00:0B:6B:01:01:01\0"	\
	"eth1addr=00:0B:6B:01:01:02\0"	\
	"eth2addr=00:0B:6B:01:01:03\0"	\
	"eth3addr=00:0B:6B:01:01:04\0"	\
	"bootdelay=3\0"	\
	"ethact=FM1@DTSEC4\0"	\
	"ethprime=FM1@DTSEC4\0"	\
	"loadaddr=1000000\0"	\
	"fsaddr=4000000\0"	\
	"boot1=setenv bootargs root=/dev/sda2 rw "	\
	"console=ttyS0,115200 "					\
    "rootdelay=15;"							\
    "sata init;"	\
	"ext2load sata 0:2 $loadaddr /boot/uImage-t1024rdb-64b.bin;"	\
	"ext2load sata 0:2 $fdtaddr /boot/uImage-t1024rdb-64b.dtb;"			\
	"bootm $loadaddr - $fdtaddr\0"  \
	"boot2=setenv bootargs root=/dev/ram rw  "	\
	"console=ttyS0,115200 "					\
    "ramdisk_size=1024000;" 				\
    "sata init;"	\
	"ext2load sata 0:2 $loadaddr /boot/uImage-t1024rdb-32b.bin;"	\
	"ext2load sata 0:2 $fdtaddr /boot/uImage-t1024rdb-32b.dtb;"		\
	"ext2load sata 0:2 $fsaddr /boot/uImage-t1024rdb-32b.ext2.gz.u-boot;"		\
	"bootm $loadaddr $fsaddr $fdtaddr\0"  \
	"fdtfile=t35.dtb\0"                                    \
	"WGKernelfile=uImage_t35\0"                                \
	"SysARoot=sda3\0"	\
	"SysBRoot=sda2\0"	\
	"wgBootBoot=setenv bootargs root=/dev/$SysARoot rw rootdelay=2 " \
	"console=$consoledev,$baudrate $othbootargs; "	\
	"sata init; " \
	"ext2load sata 0:1 $fdtaddr $fdtfile; " \
	"ext2load sata 0:1 $loadaddr $WGKernelfile; " \
	"bootm $loadaddr - $fdtaddr;\0"             \
	"wgBootSysA=setenv bootargs root=/dev/$SysARoot rw rootdelay=2 " \
	"console=$consoledev,$baudrate $othbootargs; "	\
	"sata init; " \
	"ext2load sata 0:3 $fdtaddr $fdtfile; " \
	"ext2load sata 0:3 $loadaddr $WGKernelfile; " \
	"bootm $loadaddr - $fdtaddr;\0"             \
	"wgBootSysB=setenv bootargs root=/dev/$SysBRoot rw rootdelay=2 " \
	"console=$consoledev,$baudrate $othbootargs; "	\
	"sata init; " \
	"ext2load sata 0:2 $fdtaddr $fdtfile; " \
	"ext2load sata 0:2 $loadaddr $WGKernelfile; " \
	"bootm $loadaddr - $fdtaddr;\0"             \
	"wgBootRecovery=setenv bootargs wgmode=safe root=/dev/$SysARoot rw " \
	"console=$consoledev,$baudrate $othbootargs; "	\
	"sata init; " \
	"ext2load sata 0:3 $fdtaddr $fdtfile; " \
	"ext2load sata 0:3 $loadaddr $WGKernelfile; " \
	"bootm $loadaddr - $fdtaddr;\0"  \
	"nuke_env=sf probe 0;sf erase 100000 10000;\0"  \
	"setup_network=mii write 0x15 0x4 0x7f;" \
	"ipaddr=10.0.1.1;" \
	"serveraddr=10.0.1.13;" \
	"netmask=255.255.255.0;" \
	"mii write 0x11 0x4 0x7f; mii write 0x1c 0x18 0x9420;" \
        "setenv ipaddr '10.0.1.1'; setenv serverip '10.0.1.13';" \
        "setenv netmask '255.255.255.0';\0" \
	"flash_bootloader=tftp 0x1000000 u-boot_T1024RDB_SPIFLASH.bin;" \
	"sf probe 0; sf erase 0 0x100000; sf write 0x1000000 0 $filesize;" \
	"sf read 300000 0 fff0; md 3000e0; cmp.b 1000000 300000 fff0;\0"

#define CONFIG_LINUX					\
	"run boot1"
#endif

#define CONFIG_NFSBOOTCOMMAND			\
	"setenv bootargs root=/dev/nfs rw "	\
	"nfsroot=$serverip:$rootpath "		\
	"ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:off " \
	"console=$consoledev,$baudrate $othbootargs;"	\
	"tftp $loadaddr $bootfile;"		\
	"tftp $fdtaddr $fdtfile;"		\
	"bootm $loadaddr - $fdtaddr"

#define CONFIG_BOOTCOMMAND		"run wgBootSysA"

#ifdef CONFIG_SECURE_BOOT
#include <asm/fsl_secure_boot.h>
#endif

#endif	/* __T1024RDB_H */
