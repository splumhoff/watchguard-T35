#ifndef _SC_MFG_CMD_H_
#define _SC_MFG_CMD_H_
#include <asm/immap_85xx.h>
#include <common.h>
#include <nand.h>
#include <exports.h>
#include "ram_test.h"

/*************************************************************/
/* FLASH MAP for current design */
/* ------------------------------------
 * Size  : 0x00200000 | 0x00100000
 * Base  : 0xEFE00000 | 0xEFF00000
 * -----------------------------------------------------------------------------
 * UBOOT : 0xEFF80000 - 0xEFFFFFFF : 0x80000 # Uboot Image
 * ENV   : 0xEFF70000 - 0xEFF7FFFF : 0x10000 # Uboot ENV
 * TEST  : 0xEFF60000 - 0xEFF6FFFF : 0x10000 # WG UBOOT SW TEST Result
 * EMPTY : 0xEFF50000 - 0xEFF5FFFF : 0x10000 # WG MFG DATA
 * STAND : 0xEFF20000 - 0xEFF4FFFF : 0x30000 # WG CFG1
 * DTB   : 0xEFF00000 - 0xEFF1FFFF : 0x20000 # WG CFG0 (NEW bootopt partition)
 * -----------------------------------------------------------------------------
 */
#define FL_SIZE_UBOOT 		0x80000
#define FL_SIZE_ENV		0x10000
#define FL_SIZE_TEST_RESULT	0x10000
#define FL_SIZE_MFG_DATA	0x10000
#define FL_SIZE_CFG1		0x30000
#define FL_SIZE_CFG0		0x20000

/*
 * Global definition for SC MFG TEST
 */
#define NAND_FLASH_TEST_ITEM	0x22
#define NOR_FLASH_TEST_ITEM	0x23
#define PCI_TEST_ITEM		0x24
#define RTC_TEST_ITEM		0x25
#define LED_TEST_ITEM		0x26
#define DRAM_TEST_ITEM		0x27

/*
 * TEST RESULT Definition
 */
/*#define TEST_RESULT_OFFSET	(CONFIG_SYS_FLASH_SIZE - FL_SIZE_UBOOT - FL_SIZE_ENV - FL_SIZE_TEST_RESULT)*/	/* before ENV sector */
#define TEST_RESULT_BUF_LEN	(0x100)

#define DRAM_TEST_ERR_CHIP  	(0x70)
#define DRAM_TEST_ERR_ADDR	(0x80)
#define DRAM_TEST_ERR_TYPE	(0x90)

#define TEST_ITEM_NAND_OFFSET	(0x0000)
#define TEST_ITEM_NOR_OFFSET	(0x0100)
#define TEST_ITEM_RTC_OFFSET	(0x0200)
#define TEST_ITEM_LED_OFFSET	(0x0300)
#define TEST_ITEM_RAM_OFFSET	(0x0400)
#define TEST_ITEM_PCI_OFFSET	(0x0500)

/*
 * TEST PASS/FAIL NUMBER
 */
#define NOR_FLASH_PASS		(88)
#define NOR_FLASH_FAIL		(77)

#define NAND_FLASH_PASS		(88)
#define NAND_FLASH_FAIL		(77)

#define LED_TEST_PASS		(88)
#define LED_TEST_FAIL		(77)

#define RTC_TEST_PASS		(88)
#define RTC_TEST_FAIL		(77)

#define PCI_TEST_PASS		(88)
#define PCI_TEST_FAIL		(77)

#define DRAM_TEST_OK 		(99)
#define DRAM_TEST_FAIL_UNKNOWN 	(13)

/*
 * PASS/FAIL NUMBER OFFSET in test_result string
 */
#define TEST_RESULT_OFF		(99)

/*
 * STRINGs for PASS/FAIL
 */
#define TEST_DRAM_PASS_STR	" DRAM Test\n PASS \n "
#define TEST_DRAM_FAIL_STR	" DRAM Test\n FAIL \n "

#define TEST_NOR_FAIL_STR	" NOR Flash Test \n FAIL \n"
#define TEST_NOR_PASS_STR	" NOR Flash Test \n PASS \n"

#define TEST_LED_PASS_STR	" LED Test\n LED and Reset Button Test Pass\n"
#define TEST_LED_FAIL_SRT	" LED Test\n LED or Reset Button Test Fail\n"

#define TEST_RTC_PASS_STR	" RTC Test\nRTC Test Pass\nPass\n"
#define TEST_RTC_FAIL_STR	" RTC Test\nRTC Test Fail\nFail\n"

#define TEST_NAND_FAIL_STR	" NAND Flash Test \n FAIL \n"
#define TEST_NAND_PASS_STR	" NAND Flash Test \n PASS \n"
/*
 * TEST OPEARTION RETURN
 */
#define SC_MFG_TEST_OP_PASS	(0)
#define SC_MFG_TEST_OP_FAIL	(1)
/*
 * Save/Read Test Result to/from Flash
 */
int Set_Test_Result(char test_result[100], int test_item);
void Read_Test_Result(void);
/*
 * test result
 */
extern char test_result[];
extern int set_result_state;

/*--------------------------------------------------------------------
 * For DRAM Test
 *-------------------------------------------------------------------*/

/* Test items and pattern structure */
extern struct tseq_t tseq[];
/* help tp find test type */
extern int find_ram_test_item(int pat);
/* Main function for DRAM test */
int Ram_Test(void);

/*--------------------------------------------------------------------
 * For PCI Info List
 *-------------------------------------------------------------------*/
extern void pciinfo(int BusNum, int ShortPCIListing);

/*--------------------------------------------------------------------
 * For Nor Flash Check
 *-------------------------------------------------------------------*/
/*
 * Pattern used for test NOR flash
 */
#define NOR_FLASH_TEST_DATA	0x5A
/*
 * Nor Flash Block Size
 */
#define BLOCK_SIZE		(0x10000)	/* MX29LV800CT */
/*
 * Flash Base Address
 */
#define FLASH_ADDR_BASE (CONFIG_SYS_FLASH_BASE)
/*
 * Start Offset to be tested
 */
#define NOR_TEST_START_OFFSET \
	(CONFIG_SYS_FLASH_SIZE - FL_SIZE_UBOOT - FL_SIZE_ENV - FL_SIZE_TEST_RESULT - FL_SIZE_MFG_DATA)
/*
 * Number of blocks to be test
 */
#define BUFFER_NUM		3

int Fill_NOR_Flash(int write_data);
int Check_NOR_Flash(int read_data);

/*--------------------------------------------------------------------
 * For Nand Flash Check
 *-------------------------------------------------------------------*/
/*
 * Pattern used for test NAND flash
 */
#define NAND_FLASH_TEST_DATA	0x55

/* CONFIG_XTM3 && ! CONFIG_XTM3_1M means cut1 or cut2 newport */
#if defined(CONFIG_XTM2_1M) || (defined(CONFIG_XTM3) && !defined(CONFIG_XTM3_1M)) || defined(CONFIG_XTM2)
/* Samsung K9F2G08U0B */
# define NAND_TOTAL_BLK		(2048)
# define NAND_BLK_SIZE		(0x20000)
#elif defined(CONFIG_XTM3_1M)
/* Samsung K9F4G08U0D */
# define NAND_TOTAL_BLK		(4096)
# define NAND_BLK_SIZE		(0x20000)
#endif
/* allow 1% of bad block or reject the unit */
#define MAX_BADBLK_ALLOW	(NAND_TOTAL_BLK/100)

#define NAND_TEST_START_BLK	0
#define NAND_TEST_END_BLK	(NAND_TOTAL_BLK - 1)

#define TEST_QUIET		1
#define TEST_DEBUG		0

int Nand_Flash_Test(int test_mode);
int Nand_Flash_Check(unsigned short blk_num, int test_mode);
/*--------------------------------------------------------------------
 * For LED/BUTTON TEST definition
 *-------------------------------------------------------------------*/
/*
 * Headr Files for GPIO registers
 */

/*
 * LED/BTN definition
 */
#define LED_ATTENTION		(0x02)
#define LED_SW_STATUS		(0x03)
#define LED_SW_MODE		(0x04)
#define LED_WAP_BG		(0x05)
#define LED_WAP_N		(0x06)
#define LED_FAILOVER		(0x07)

#define BTN_RESET		(0x01)

/* LED is Low_Active */
#define LED_OFF			(1)
#define LED_ON			(0)

#define LED_TEST_CYCLE		(5)
#define LED_TEST_TIMEOUT	(20)

int Led_Test(void);
void sc_led_init(void);
void sc_led_off(unsigned int led);
void sc_led_on(unsigned int led);
int sc_read_btn_status(unsigned int btn);


/*--------------------------------------------------------------------
 * For RTC TEST definition
 *-------------------------------------------------------------------*/
/*
 * Delay Time
 */
#define RTC_TEST_DELAY		5


int RTC_Test(void);

/*-------------------------------------------------------------------
 * Flash ERASE/PROGRAM Opeations
 *-------------------------------------------------------------------*/
#define FLASH_OP_FAIL		(0)
#define FLASH_OP_OK		(1)
int sc_flash_erase(ulong address, int size);
int sc_flash_program(ulong address, char * buffer, int size);
void disable_DDR_ECC(void);
nand_info_t *sc_get_nand_info_ptr(void);


#endif /* _SC_MFG_CMD_H_ */

// vim: noet ai
