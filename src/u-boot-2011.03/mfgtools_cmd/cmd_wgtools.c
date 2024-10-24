/*
 * U-boot - wgtools.c
 *
 * Copyright (c) 2005-2008 WatchGuard Technologies.
 *
 */

#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <errno.h>
#include <stdio_dev.h>
#include <exports.h>

#if 0
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if 0
#include <net.h>
#endif

#include "mfgtools-private.h"
#include "macs.h"

#define IFNAMSIZ        16


extern int console_assign(int file, const char *devname);


/* WG Serial tools */
int do_storeserial(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i=0, j=0;
	char serial_copy[SERIAL_LENGTH+1];
	char *name;

	if (argc != 2)
		return -1;

	/*argv[1] shgould be 'serialnum' */
	name = argv[1];

	/* strip any white spaces and '-' */
	while ( name[i] ) {
		if ( isalnum(name[i]) ) {
			serial_copy[j]=name[i];
			j++;
		}
		i++;
	}
	serial_copy[j]=0;

	/* Validate the serial */
	if (mfg_valid_serial(serial_copy) == 0) {
		printf ("Invalid Serial: please recheck your serial\n");
 		return -2;
	}

	/* write the serial */
	return mfg_update_flashpart(MFG_CONFIG_SERIAL,
			serial_copy, (strlen(serial_copy) + 1));
}

U_BOOT_CMD(
	storeserial, 2, 0, do_storeserial,
	"store WG serial in WG specific way\n",
	"[wgserialnum]\n");

int do_readserial(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
	char serial[SERIAL_LENGTH];
	mfg_info_blk_st block;

	ret = read_config_block( MFG_CONFIG_SERIAL, &block);

	if ( block.hdr.length ) {
		memmove( serial, block.data, block.hdr.length );
		printf("WG Serial: %s\n", serial);
	}

	return ret;
}

U_BOOT_CMD(
	readserial, 1, 1, do_readserial,
	"read WG serial",
	"");


/* WG MAC tools */
int do_storemacs(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i=0;
    	int length = sizeof( struct mac_addr_info_st );
	struct mac_addr_info_st macs;
	int nr_macs = argc -1;

	if (nr_macs > (MAX_NUM_MACS))
		return -1;
	if (nr_macs < (MIN_NUM_MACS))
		return -1;

	/* do some basic validatation on the input MACS
	 * i.e. look for the first ':' in the mac address: i.e. 00':'90:0b:1e:7e:34
	 */
	for (i=0;i<nr_macs;i++) {
		if (argv[i+1][2] != ':' ) {
			printf("Invalid macaddres passed in: %s\n", argv[i+1]);
			printf("each MAC address provided should be seperated by ':'.\n"
			       "    example: 00:90:0b:1e:7e:34");
			return -1;
		}
	}

	for (i=0;i<nr_macs;i++) {
		strcpy(macs.interface[i].mac, argv[i+1]);
	}

	return mfg_update_flashpart(MFG_CONFIG_MACS, (char *)&macs, length);
}

U_BOOT_CMD(
	storemacs, 10, 0, do_storemacs,
	"store 9 WG MAC addresses in WG specific way",
	"[mac1] [mac2] [mac3]\n"
	"	- note: each MAC address provided should be seperated by ':'.\n"
	"	   example: 00:90:0b:1e:7e:34");

#define MAC_ADDR_EXP(data) \
    &(data)[0], &(data)[1], &(data)[2], &(data)[3], &(data)[4], &(data)[5]
 
static int showmac( char *device, char *macstr )
{
	if (!device || !macstr)
		return -1;

	if (macstr[2] != ':' ) {
		return -1;
	}

	printf("device %s: macaddres: %s\n", device, macstr);

	return 0; 
}

int do_readmacs(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i=0, ret;
	mfg_info_blk_st block;
	struct mac_addr_info_st macs;
	char temp[IFNAMSIZ];

	/* read the data blob */
	ret = read_config_block( MFG_CONFIG_MACS, &block);
	if ( block.hdr.length ) {
		memmove( &macs, block.data, block.hdr.length );

		/* interpret the mac addresses */
		for (i=0; i<MAX_NUM_MACS; i++) {
			sprintf(temp, "eth%d", i);
			showmac(temp, macs.interface[i].mac);
		}
	}

	return ret;
}

U_BOOT_CMD(
	readmacs, 1, 1, do_readmacs,
	"read 3 or 9 WG MAC addresses in WG specific way\n",
	"");

#define MAC_STR_SIZE	18
/* returns 1 if the format of mac is valid */
/* can do more (make sure hex numbers only, no multi/broad-cast mac) if really needed */
int is_valid_mac(char* buf) {
	if (strlen(buf) < MAC_STR_SIZE - 1 ||
		buf[2] != ':' || buf[5] != ':' || buf[8] != ':' || buf[11] != ':' || buf[14] != ':')
		return 0;
	else
		return 1;
}

#if defined(CONFIG_FBX_T10)

/* WG T10-D MAC tools for mac addr on the DSL chip */
int do_setdslmac(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i, j;
	char mac_str[MAC_STR_SIZE];

	if (argc < 2) {
		printf("mac addr missing, must provide DSL eth0 mac addr to write\n");
		return -1;
	}

	if (!is_valid_mac(argv[1])) {
		printf("invalid mac format, MAC addr should contain ':'\n");
		return -1;
	}

	for (i = 0, j = 0; i < MAC_STR_SIZE && argv[1][j] != '\0'; j++)
		if (argv[1][j] != ':') {
			mac_str[i] = argv[1][j];
			i++;
		}
	mac_str[MAC_STR_SIZE - 1] = '\0';

	/* printf("\nswitching console to eserial1 to set dsl eth0 mac to: %s\n", mac_str); */
	console_assign(stdout, "eserial1");
	printf(" \r");
	/* printf("sys mac %s\r", argv[1]); */
	printf("sys mac %s\r", mac_str);
	for (i = 0; i < 3; i++)
		udelay (1000000);
	console_assign(stdout, "serial");
	/* printf("console switched back to eserial0\n"); */

	printf("cmd sent to the DSL chip, pls wait 30 s for the chip to reboot\n");

	return 0;
}
U_BOOT_CMD(
	setdslmac, 2, 0, do_setdslmac,
	"set DSL chip eth0 MAC address in WG specific way",
	"	- note: each MAC address provided should be seperated by ':'.\n"
	"	           example: setdslmac 00:90:0b:1e:7e:34");



#define BUFF_LEN	256
#define LINE_LEN	80
/* WG T10-D MAC tools for mac addr on the DSL chip */
int do_readdslmac(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i;
	char* p;
	char ch[BUFF_LEN];
	char line[LINE_LEN];

	/* printf("switching console to eserial1 to run cmd to get DSL mac ...\n"); */
	console_assign(stdin, "eserial1");
	console_assign(stdout, "eserial1");
	printf("ifconfig eth0\r");
	console_assign(stdout, "serial");
  
	for (i = 0; i < BUFF_LEN; i++) {
		ch[i] = (char) fgetc(stdin);
		if (ch[i] <= 0)
			break;
	}
	if (i >= BUFF_LEN)
		i = BUFF_LEN - 1;
	ch[i] = '\0';
	console_assign(stdin, "serial");

	p = strstr(ch, "eth0 ");
	if (!p) {
		fputs(stdout, "#### DSL chip not ready yet. Please wait and re-try. Feedback received:\n");
		fputs(stdout, ch);
		fputs(stdout, "\n");
		return -1;
	}

	/* copy one line starting with "eth0" */
	for (i = 0; i < LINE_LEN && p[i] != '\n' && p[i] != '\0'; i++)
		line[i] = p[i];
	if (i >= LINE_LEN)
		i = LINE_LEN - 1;
	line[i] = '\0';

	p = strstr(line, "HWaddr");
	if (!p) {
		fputs(stdout, "#### error in reading DSL chip eth0 mac addr\n");
		fputs(stdout, line);
		return -1;
	}

	printf("device DSL eth0: macaddress: %s\n", (p + 7));

	return 0;
}

U_BOOT_CMD(
	readdslmac, 1, 0, do_readdslmac,
	"read DSL chip eth0 MAC addresses in WG specific way",
	""
	);
#endif // defined(CONFIG_FBX_T10)
