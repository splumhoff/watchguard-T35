/***********************************************************************
 * 			NewCastle Uboot Test Software Version 1.0      *
 ***********************************************************************
 * Module	: Nand Test					       *
 ***********************************************************************/

/*
 * Header File
 */
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <linux/mtd/mtd.h>
#include <command.h>
#include <watchdog.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <jffs2/jffs2.h>
#include <nand.h>

#include "sc_mfg.h"

int BadBlockNumStore[TEST_RESULT_BUF_LEN];
unsigned char BLK_BUF[NAND_BLK_SIZE];

int Nand_Flash_Check(unsigned short blk_num, int test_mode)
{
	unsigned int offset;
	unsigned int length;
	int ret = 0;
	int result = 0;
	unsigned char tmp_buf[NAND_BLK_SIZE];
	//int ok_block_count = 0;

	/* nand info */
	nand_info_t *nand = sc_get_nand_info_ptr();
	/* erase opt */
	nand_erase_options_t erase_opt;

	memset(&erase_opt, 0, sizeof(erase_opt));    

	/* fill erase info */
	erase_opt.length = NAND_BLK_SIZE;
	erase_opt.offset = blk_num * NAND_BLK_SIZE;
	erase_opt.quiet  = test_mode;
	erase_opt.jffs2  = 0;
	erase_opt.scrub  = 0;

	offset = blk_num * NAND_BLK_SIZE;
	length = NAND_BLK_SIZE;

	if (nand_erase_opts(nand, &erase_opt)) {
		printf("Erase is Failed  \n");
		return SC_MFG_TEST_OP_FAIL;    
	}

	/* fill buffer with certain pattern */
	memset(BLK_BUF, NAND_FLASH_TEST_DATA, NAND_BLK_SIZE);

	// using jffs2, not yaffs
	if(nand_write_skip_bad(nand, offset, &length, BLK_BUF, 0)) {
		result = -1;
		goto check_result;
	}

	if(nand_read_skip_bad(nand, offset, &length, tmp_buf)) {
		result = -1;
		goto check_result;
	}

	if(memcmp(tmp_buf, BLK_BUF, NAND_BLK_SIZE)) {
		result = -1;
		goto check_result;
	}

check_result:	
	if (!result) {
		printf("(blcok:%d)...is ok \n", blk_num);
	}
	else {
		printf("(blcok:%d) is Bad \n", blk_num);
		ret = SC_MFG_TEST_OP_FAIL;
		goto check_final;
	}

	if (nand_erase_opts(nand, &erase_opt)) {
		printf("Erase is Failed  \n");
		ret = SC_MFG_TEST_OP_FAIL;    
		goto check_final;
	}
	else {
		ret = SC_MFG_TEST_OP_PASS;
		goto check_final;
	}
check_final:
	return ret;
}

int Nand_Flash_Test(int test_mode)
{
	int __attribute__((unused)) i = 0;

	/* nand info */
	nand_info_t *nand = sc_get_nand_info_ptr();;

	int BadBlockNum = 0;
	int Block_Test_Num;

	unsigned int offset = 0;

	int ret = 0;

	int Nand_Flash_Check_state;
    
	for (i=0; i<NAND_TOTAL_BLK; i++)
	{
		offset = i*NAND_BLK_SIZE;

		if (nand_block_isbad(nand, offset)) {
			printf("********Bad Block Found at %d*******\n", i);
			BadBlockNumStore[BadBlockNum] = i;
			BadBlockNum++;
		}
	}

	printf("Bad Blocks Num is: %d\n", BadBlockNum);

	if (BadBlockNum > MAX_BADBLK_ALLOW) {
		printf("There are too many bad blocks to allow the test to pass \n");
		printf(" NAND Flash Test \n");
		printf(" FAIL \n");
		memcpy(test_result, TEST_NAND_FAIL_STR, sizeof(TEST_NAND_FAIL_STR));
		test_result[TEST_RESULT_OFF] = NAND_FLASH_FAIL;

		set_result_state = Set_Test_Result( test_result, NAND_FLASH_TEST_ITEM );
		if (set_result_state) {
			printf("write the NAND_FLASH_TEST (fail) is fail \n");
			ret = SC_MFG_TEST_OP_FAIL;
			goto test_final;
		}
		else {
			printf("write the NAND_FLASH_TEST (fail) is OK \n");
			ret = SC_MFG_TEST_OP_FAIL;
			goto test_final;
		}
	} /* bad block check is over */
	for (Block_Test_Num = NAND_TEST_START_BLK; Block_Test_Num <= NAND_TEST_END_BLK; Block_Test_Num++)
	{
		/* skip bad block */
		for (i=0; i<BadBlockNum; i++)
		{
			if (BadBlockNumStore[i] == Block_Test_Num) {
				Block_Test_Num = Block_Test_Num+1;
				if(Block_Test_Num == NAND_TEST_END_BLK) {
					Nand_Flash_Check_state = SC_MFG_TEST_OP_PASS;
					goto done;
				}
			}
		}
		Nand_Flash_Check_state = Nand_Flash_Check( Block_Test_Num , test_mode);
done:
		if (Nand_Flash_Check_state == SC_MFG_TEST_OP_FAIL) {
			printf(" NAND Flash Test \n");
			printf(" FAIL \n");
			memcpy(test_result, TEST_NAND_FAIL_STR, sizeof(TEST_NAND_FAIL_STR));
			test_result[TEST_RESULT_OFF] = NAND_FLASH_FAIL;
			set_result_state=Set_Test_Result( test_result, NAND_FLASH_TEST_ITEM );
			if (set_result_state) {
				printf("write the NAND_FLASH_TEST (fail) is fail \n");
				ret = SC_MFG_TEST_OP_FAIL;
				goto test_final;
			} 
			else {
				printf("write the NAND_FLASH_TEST (fail) is OK \n");
				ret = SC_MFG_TEST_OP_FAIL;
				goto test_final;
			}
		}
		if ((Block_Test_Num==NAND_TEST_END_BLK-1)||(Block_Test_Num==NAND_TEST_END_BLK)) {
			printf(" NAND Flash Test \n");
			printf(" PASS \n");
			memcpy(test_result, TEST_NAND_PASS_STR, sizeof(TEST_NAND_PASS_STR));
			test_result[TEST_RESULT_OFF] = NAND_FLASH_PASS;
			set_result_state=Set_Test_Result( test_result, NAND_FLASH_TEST_ITEM );
			if (set_result_state) {
				printf("write the NAND_FLASH_TEST (OK) is fail \n");
				ret = SC_MFG_TEST_OP_FAIL;
				goto test_final;
			}
			else {
				printf("write the NAND_FLASH_TEST (OK) is OK \n");
				ret = SC_MFG_TEST_OP_PASS;
				goto test_final;
			}
		}
	}
test_final:
	return ret;
}
