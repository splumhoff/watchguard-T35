/*
 * Header Files
 */
#include <asm/cache.h>
#include <common.h>
#include <command.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <exports.h>

#include "sc_mfg_cmd.h"

/*--------------------------------------------------------------------*/
/*
 * U_BOOT_CMD definition list for sc_mfg_test
 */

int do_sc_mfg_led_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_LED_TEST;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;	
}

U_BOOT_CMD(
    led_test_e,		1,	1,	do_sc_mfg_led_e,
    "sc mfg LED test",
    ""
);

/*--------------------------------------------------------------------*/
int do_sc_mfg_dram_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_DRAM_TEST;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;	
}

U_BOOT_CMD(
    ram_test_e,	1,		1,	do_sc_mfg_dram_e,
    "sc mfg DRAM test",
    ""
);
/*--------------------------------------------------------------------*/
int do_sc_mfg_nor_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_NOR_TEST;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;	
}

U_BOOT_CMD(
    nor_flash_check_e,	1,		1,	do_sc_mfg_nor_e,
    "sc mfg NOR flash test",
    ""
);

/*--------------------------------------------------------------------*/
int do_sc_mfg_nand_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_NAND_TEST;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;	
}

U_BOOT_CMD(
    nand_test_e,	1,		1,	do_sc_mfg_nand_e,
    "sc mfg NAND flash test",
    ""
);

/*--------------------------------------------------------------------*/
int do_sc_mfg_rtc_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_RTC_TEST;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;	
}

U_BOOT_CMD(
    rtc_test_e,	1,		1,	do_sc_mfg_rtc_e,
    "sc mfg RTC test",
    ""
);
/*--------------------------------------------------------------------*/
int do_sc_mfg_pci_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_PCI_TEST;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;		
}

U_BOOT_CMD(
    pci_test_e,	1,		1,	do_sc_mfg_pci_e,
    "sc mfg PCI test",
    ""
);
/*--------------------------------------------------------------------*/
int do_sc_mfg_read_results_e (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int arg_c;
	char *arg_v[10];

	memcpy((char *)STANDALONE_LOAD_ADDR, (char *)(SC_STANDALONE_BIN_ADDR + 0xEFF00000), SC_STANDALONE_BIN_SIZE);
	
	flush_dcache();
	invalidate_icache();	
	
	arg_c = 3;
	
	arg_v[0] = "go";
	arg_v[1] = STR_GO_ADDR;
	arg_v[2] = SC_MFG_TEST_RESULTS;
		
	do_go(NULL, 0, arg_c, arg_v);
	
	return 0;	
}

U_BOOT_CMD(
    read_test_result_e,	1,		1,	do_sc_mfg_read_results_e,
    "sc mfg read test results",
    ""
);

/* eof */
