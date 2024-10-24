/***********************************************************************
 * 			NewCastle Uboot Test Software Version 1.0      *
 ***********************************************************************
 * Module	: Nor Test					       *
 ***********************************************************************/

/*
 * Header File
 */
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <flash.h>
#include "sc_mfg.h"

/* Static Variables - be careful about stack size */
static char block_buffer[BLOCK_SIZE];

/* Check Function */
int Check_NOR_Flash(int read_data)
{
	int i, j;
	int compare = SC_MFG_TEST_OP_PASS;
	unsigned int test_addr = 0;

	for (i=0; i<BUFFER_NUM; i++)
	{
		test_addr = FLASH_ADDR_BASE + NOR_TEST_START_OFFSET + i*BLOCK_SIZE;

		memcpy(block_buffer, (unsigned char *)test_addr, BLOCK_SIZE);

		for(j=0; j<BLOCK_SIZE; j++)
		{
			if(block_buffer[j] != read_data) {
				compare = SC_MFG_TEST_OP_FAIL;
				goto TEST_OVER;
			}
		}
	}

	if (i==BUFFER_NUM)
		compare = SC_MFG_TEST_OP_PASS;

TEST_OVER:
	if (compare == SC_MFG_TEST_OP_FAIL) {
		memcpy(test_result, TEST_NOR_FAIL_STR, sizeof(TEST_NOR_FAIL_STR));

		test_result[TEST_RESULT_OFF]=NOR_FLASH_FAIL;

		set_result_state = Set_Test_Result(test_result, NOR_FLASH_TEST_ITEM);
		printf(" NOR Flash Test \n");
		printf(" FAIL \n");
		if (set_result_state) {
			printf("write the NOR_FLASH_TEST (fail) is fail \n");
			return SC_MFG_TEST_OP_FAIL;
		}
		else {
			printf("write the NOR_FLASH_TEST (fail) is OK \n");
			return SC_MFG_TEST_OP_FAIL;
		}
	}
	else {
		memcpy(test_result, TEST_NOR_PASS_STR, sizeof(TEST_NOR_PASS_STR));
		test_result[TEST_RESULT_OFF]=NOR_FLASH_PASS;
		set_result_state = Set_Test_Result( test_result, NOR_FLASH_TEST_ITEM );
		printf(" NOR Flash Test \n");
		printf(" PASS \n");
		if (set_result_state) {
			printf("write the NOR_FLASH_TEST (OK) is fail \n");
			return SC_MFG_TEST_OP_FAIL;
		}
		else {
			printf("write the NOR_FLASH_TEST (OK) is OK \n");
			return SC_MFG_TEST_OP_PASS;
		}
	}
}

/* Fill Pattern */
int Fill_NOR_Flash(int write_data)
{
	int i;

	memcpy(block_buffer, (char *) (FLASH_ADDR_BASE + NOR_TEST_START_OFFSET), BLOCK_SIZE);

	for (i=0; i<BLOCK_SIZE; i++)
	{
		block_buffer[i] = write_data;

		if (i%500==0)
			udelay(2*10000); // delay

		if (i%5000==0)
			udelay(2*10000); // delay
	}

	printf("Erasing eeprom...\n");

	for (i=0; i<BUFFER_NUM; i++)
	{
		if (sc_flash_erase(FLASH_ADDR_BASE + NOR_TEST_START_OFFSET + i*BLOCK_SIZE, BLOCK_SIZE) != FLASH_OP_OK) {
			printf("Error erasing at %x\n", FLASH_ADDR_BASE + NOR_TEST_START_OFFSET + i*BLOCK_SIZE);
			return SC_MFG_TEST_OP_FAIL;
		}
	}

	printf("done\nPrograming ...\n");

	for (i=0; i<BUFFER_NUM; i++)
	{
		if (sc_flash_program(FLASH_ADDR_BASE + NOR_TEST_START_OFFSET + i*BLOCK_SIZE, block_buffer, BLOCK_SIZE) != FLASH_OP_OK) {
			printf("Can't program region at %x\n", FLASH_ADDR_BASE + NOR_TEST_START_OFFSET + i*BLOCK_SIZE);
			return SC_MFG_TEST_OP_FAIL;
		}
	}

	udelay(1000);

	printf("write datas into Nor Flash is OK...\n");
	return SC_MFG_TEST_OP_PASS;
}


/* Flash Erase/Program Operation */
int sc_flash_erase(ulong address, int size)
{
	unsigned long addr_first;
	unsigned long addr_last;

	addr_first = (unsigned long)(address);	
	addr_last  = (unsigned long)(address + size - 1);

	printf("address %lx, size %x\n", address, size);

	/* un-protect before real erase */
	if(flash_sect_protect(0, addr_first, addr_last)) {
		printf("up-protect flash fail!\n");
		return FLASH_OP_FAIL;
	}

	if(flash_sect_erase(addr_first, addr_last)) {
		printf("erase flash fail!\n");
		return FLASH_OP_FAIL;
	}

	/* protect again */
	if(flash_sect_protect(1, addr_first, addr_last)) {
		printf("protect flash fail!\n");
		return FLASH_OP_FAIL;
	}
	return FLASH_OP_OK;
}

int sc_flash_program(ulong address, char * buffer, int size)
{
	unsigned long addr_first;
	unsigned long addr_last;

	addr_first = (unsigned long)(address);	
	addr_last  = (unsigned long)(address + size - 1);

	/* un-protect before real program */
	if(flash_sect_protect(0, addr_first, addr_last)) {
		printf("up-protect flash fail!\n");
		return FLASH_OP_FAIL;
	}

	if(flash_write(buffer, address, size)) {
		printf("program flash fail!\n");
		return FLASH_OP_FAIL;
	}

	/* protect again */
	if(flash_sect_protect(1, addr_first, addr_last)) {
		printf("protect flash fail!\n");
		return FLASH_OP_FAIL;
	}
	return FLASH_OP_OK;
}

