/*
 * Header Files
 */
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <exports.h>

#include "sc_mfg.h"
/*
 * Global Variables For Test Results
 */
char test_result[TEST_RESULT_BUF_LEN];		/* test result, as string */
int set_result_state;

/*--------------------------------------------------------------------*/
/*
 * U_BOOT_CMD definition list for sc_mfg_test
 */

int do_sc_mfg_led (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = Led_Test();
	return ret;
}


/*--------------------------------------------------------------------*/
int do_sc_mfg_dram (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = Ram_Test();
	return ret;
}

/*--------------------------------------------------------------------*/
int do_sc_mfg_nor_fill (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = Fill_NOR_Flash(NOR_FLASH_TEST_DATA);

	return ret;
}


/*--------------------------------------------------------------------*/
int do_sc_mfg_nor (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = Check_NOR_Flash(NOR_FLASH_TEST_DATA);

	return ret;
}



/*--------------------------------------------------------------------*/
int do_sc_mfg_nand (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = Nand_Flash_Test(TEST_QUIET);
	return ret;
}



/*--------------------------------------------------------------------*/
int do_sc_mfg_rtc (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int ret = RTC_Test();
	return ret;
}


/*--------------------------------------------------------------------*/
int do_sc_mfg_pci (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	/* BUS 1 */
	pciinfo(1, 1);
	/* BUS 2 */
	pciinfo(2, 1);

	return 0;
}


/*--------------------------------------------------------------------*/
int do_sc_mfg_read_results (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	Read_Test_Result();
	return 0;
}

/*--------------------------------------------------------------------*/
int Set_Test_Result( char *test_result, int test_item )
{
	unsigned char block_buffer_result[BLOCK_SIZE];
	unsigned char *buf_pos = NULL;
	unsigned int sector_base;
	
	sector_base = TEST_RESULT_OFFSET & (~(BLOCK_SIZE-1));

	/* Copy Test Result resident block content out */
	memcpy(block_buffer_result, (unsigned char *) (FLASH_ADDR_BASE + sector_base), BLOCK_SIZE);
	
	switch(test_item){
	case NAND_FLASH_TEST_ITEM:
		buf_pos = block_buffer_result + TEST_ITEM_NAND_OFFSET + (TEST_RESULT_OFFSET - sector_base);
		break;
	case NOR_FLASH_TEST_ITEM:
		buf_pos = block_buffer_result + TEST_ITEM_NOR_OFFSET + (TEST_RESULT_OFFSET - sector_base);
		break;
	case RTC_TEST_ITEM:
		buf_pos = block_buffer_result + TEST_ITEM_RTC_OFFSET + (TEST_RESULT_OFFSET - sector_base);
		break;
	case LED_TEST_ITEM:
		buf_pos = block_buffer_result + TEST_ITEM_LED_OFFSET + (TEST_RESULT_OFFSET - sector_base);
		break;
	case DRAM_TEST_ITEM:
		buf_pos = block_buffer_result + TEST_ITEM_RAM_OFFSET + (TEST_RESULT_OFFSET - sector_base);
		break;
	case PCI_TEST_ITEM:
		buf_pos = block_buffer_result + TEST_ITEM_PCI_OFFSET + (TEST_RESULT_OFFSET - sector_base);
		break;
	default:
		printf("Error: Wrong Test Item...\n");
		return SC_MFG_TEST_OP_FAIL; 
	}
	/* Overwrite test result to corresponding position in buffer */
	memcpy(buf_pos, test_result, TEST_RESULT_BUF_LEN);
	
	printf("Erasing eeprom...\n");

	if(sc_flash_erase(FLASH_ADDR_BASE + sector_base, BLOCK_SIZE) != FLASH_OP_OK)
	{
		printf("Error erasing at %x\n", (FLASH_ADDR_BASE + sector_base));
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("done\nPrograming ...");

	if(sc_flash_program(FLASH_ADDR_BASE + sector_base, (void *) block_buffer_result, BLOCK_SIZE) != FLASH_OP_OK) {
		printf("Can't program region at %x\n", FLASH_ADDR_BASE + sector_base);
		return SC_MFG_TEST_OP_FAIL;
	}
	udelay(1000000);
	printf("done\n");
	return SC_MFG_TEST_OP_PASS;
}

void Read_Test_Result(void)
{
	unsigned char * p_test_result_flag;
	unsigned char test_result_flag;

	int idx = -1;	
	unsigned int dram_err_address = 0;
	int dram_err_type = -1;
	char dram_err_chipmsk = 0;
	int i=0;

	/*
 	 * NAND Flash Test Flag
 	 */
	p_test_result_flag = (unsigned char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_NAND_OFFSET + TEST_RESULT_OFF);
	test_result_flag = *p_test_result_flag;
	
	printf("NAND Flash Test Flag: %d\n", test_result_flag);

	printf(" NAND Flash Test \n");

	if (test_result_flag == NAND_FLASH_PASS)
		printf(" PASS \n");
	else if (test_result_flag == NAND_FLASH_FAIL)
		printf(" FAIL \n");
	else
		printf(" UNKNOWN \n");
		
	/*
	 * NOR Flash Test Flag
	 */
	p_test_result_flag = (unsigned char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_NOR_OFFSET + TEST_RESULT_OFF);
	test_result_flag = *p_test_result_flag;

	printf("NOR Flash Test Flag: %d\n", test_result_flag);

	printf(" NOR Flash Test \n");

	if (test_result_flag == NOR_FLASH_PASS)
		printf(" PASS \n");
	else if (test_result_flag == NOR_FLASH_FAIL)
		printf(" FAIL \n");
	else
		printf(" UNKNOWN \n");

	/*
	* LED Test Flag
	*/
	p_test_result_flag = (unsigned char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_LED_OFFSET + TEST_RESULT_OFF);
	test_result_flag   = *p_test_result_flag;

	printf("LED Test Flag: %d\n", test_result_flag);

	printf(" LED Test \n");

	if (test_result_flag == LED_TEST_PASS)
		printf(" PASS \n");
	else if (test_result_flag == LED_TEST_FAIL)
		printf(" FAIL \n");
	else
		printf(" UNKNOWN \n");

	/*
	* RTC Test Flag
	*/
	p_test_result_flag = (unsigned char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_RTC_OFFSET + TEST_RESULT_OFF);
	test_result_flag   = *p_test_result_flag;

	printf("RTC Test Flag: %d\n", test_result_flag);

	printf(" RTC Test \n");

	if (test_result_flag == RTC_TEST_PASS)
		printf(" PASS \n");
	else if (test_result_flag == RTC_TEST_FAIL)
		printf(" FAIL \n");
	else
		printf(" UNKNOWN \n");

	/*
	* PCI Test Flag
	*/
	p_test_result_flag = (unsigned char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_PCI_OFFSET + TEST_RESULT_OFF);
	test_result_flag   = *p_test_result_flag;

	printf("PCI Test Flag: %d\n", test_result_flag);

	printf(" PCI Test \n");

	if (test_result_flag == PCI_TEST_PASS)
		printf(" PASS \n");
	else if (test_result_flag == PCI_TEST_FAIL)
		printf(" FAIL \n");
	else
		printf(" UNKNOWN \n");

	/*
	* DRAM Test Flag
	*/
	p_test_result_flag = (unsigned char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_RAM_OFFSET + TEST_RESULT_OFF);
	test_result_flag   = *p_test_result_flag;
	dram_err_address = *(unsigned int *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_RAM_OFFSET + DRAM_TEST_ERR_ADDR);
	dram_err_type = *(int *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_RAM_OFFSET + DRAM_TEST_ERR_TYPE);
	dram_err_chipmsk = *(char *)(FLASH_ADDR_BASE + TEST_RESULT_OFFSET + TEST_ITEM_RAM_OFFSET + DRAM_TEST_ERR_CHIP);
 
	printf("DRAM Test Flag: %d\n", test_result_flag);

	if(test_result_flag == DRAM_TEST_OK) {
		printf(" DRAM Test Pass \n");
	} else {
		printf(" DRAM Test Fail \n");
		idx = find_ram_test_item(test_result_flag);
		if (idx != -1)
			printf("%s", tseq[idx].msg);
		else
			printf(" UNKNOWN \n");

		printf(" : Address: %x, Type %x, Bad Chip Mask %x \n",\
				dram_err_address, dram_err_type, dram_err_chipmsk);	

		for(; i<4; i++) {
			if((dram_err_chipmsk >> i) & 0x1)
				printf(" Please Replace DRAM Chip U%d\n", (i+4));
		}
 	}
}

/* eof */
