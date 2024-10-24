#ifndef _SC_MFG_CMD_H_
#define _SC_MFG_CMD_H_

#define SC_MFG_LED_TEST		"0"
#define SC_MFG_NOR_TEST		"1"
#define SC_MFG_NAND_TEST	"2"
#define SC_MFG_DRAM_TEST	"3"
#define SC_MFG_RTC_TEST		"4"
#define SC_MFG_PCI_TEST		"5"
#define SC_MFG_TEST_RESULTS	"6"

#ifndef STANDALONE_LOAD_ADDR
#define STANDALONE_LOAD_ADDR 0x1000000
#endif

#define STR_GO_ADDR		      "1000004"

int do_go (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

#endif /* _SC_MFG_CMD_H_ */

