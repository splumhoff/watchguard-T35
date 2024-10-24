#include <common.h>
#include <exports.h>
#include "sc_mfg.h"

enum {
	SC_MFG_LED_TEST = 0,
	SC_MFG_NOR_TEST,
	SC_MFG_NAND_TEST,
	SC_MFG_DRAM_TEST,
	SC_MFG_RTC_TEST,
	SC_MFG_PCI_TEST,
	SC_MFG_TEST_RESULTS,
} SC_MFG_TYPE;


void sc_do_mfg (int argc, char *argv[])
{
	int test_item;

	/* Print the ABI version */
	app_startup(argv);
	printf ("Actual U-Boot ABI version %d\n", (int)get_version());

	printf ("SC MFG STANDALONE:\n");

	if(argc != 2)
	{
		printf("arg number is not correct!\n");
		return;
	}
	else
	{
		test_item = simple_strtoul(argv[1], NULL, 16);
		printf("test_item is %d\n", test_item);
	}

	switch(test_item)
	{
		case SC_MFG_LED_TEST:
			Led_Test();
			break;
		case SC_MFG_NOR_TEST:
			Fill_NOR_Flash(NOR_FLASH_TEST_DATA);
			Check_NOR_Flash(NOR_FLASH_TEST_DATA);
			break;
		case SC_MFG_NAND_TEST:
			Nand_Flash_Test(TEST_QUIET);
			break;
		case SC_MFG_DRAM_TEST:
			Ram_Test();
			break;
		case SC_MFG_RTC_TEST:
			RTC_Test();
			break;
		case SC_MFG_PCI_TEST:
			/* BUS 1 */
			pciinfo(1, 1);
			/* BUS 2 */
			pciinfo(2, 1);
			break;
		case SC_MFG_TEST_RESULTS:
			Read_Test_Result();
			break;
		default:
			printf("Wrong MFG TEST type!\n");
			break;
	}
}
