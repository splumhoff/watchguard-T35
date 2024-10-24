/*
 * Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <watchdog.h>
#include "..//sc_mfg_standalone//sc_mfg.h"
//#include "..//drivers//rtc//s35390.h"


#if defined(CONFIG_FBX_T10) || defined(CONFIG_FBX_T15)
#define CONFIG_FACTORY_TEST
#define RESET_BTN_HOLD_TIME_CHECK_INTERVAL	10	//ms
#define RESET_BTN_HOLD_TIME					3000	//ms
#define FACTORY_RESET_GPIO					0x00200000


#define GPIO_USB_EN		0
#define GPIO_PHY_RST	11
#define GPIO_DDR_RST	14
#define GPIO_PCIE_MODULE_RST	15



extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
extern int run_command (const char *cmd, int flag);
extern void *malloc (size_t len);

extern int sc_rtc_set_time(SC_RTC_TIME * time);
extern void sc_i2c_init(int speed, int slaveadd);

int RTC_Test(void);

int mem_test(void);

int RTC_Battery_Test_Write(char *cRTCTime);
int RTC_Battery_Test_Read(void);


#endif

int do_test(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{

#if !defined(CONFIG_FACTORY_TEST)
	char * const *ap;
	int left, adv, expr, last_expr, neg, last_cmp;

	/* args? */
	if (argc < 3)
		return 1;

#if 0
	{
		printf("test:");
		left = 1;
		while (argv[left])
			printf(" %s", argv[left++]);
	}
#endif

	last_expr = 0;
	left = argc - 1; ap = argv + 1;
	if (left > 0 && strcmp(ap[0], "!") == 0) {
		neg = 1;
		ap++;
		left--;
	} else
		neg = 0;

	expr = -1;
	last_cmp = -1;
	last_expr = -1;
	while (left > 0) {

		if (strcmp(ap[0], "-o") == 0 || strcmp(ap[0], "-a") == 0)
			adv = 1;
		else if (strcmp(ap[0], "-z") == 0 || strcmp(ap[0], "-n") == 0)
			adv = 2;
		else
			adv = 3;

		if (left < adv) {
			expr = 1;
			break;
		}

		if (adv == 1) {
			if (strcmp(ap[0], "-o") == 0) {
				last_expr = expr;
				last_cmp = 0;
			} else if (strcmp(ap[0], "-a") == 0) {
				last_expr = expr;
				last_cmp = 1;
			} else {
				expr = 1;
				break;
			}
		}

		if (adv == 2) {
			if (strcmp(ap[0], "-z") == 0)
				expr = strlen(ap[1]) == 0 ? 1 : 0;
			else if (strcmp(ap[0], "-n") == 0)
				expr = strlen(ap[1]) == 0 ? 0 : 1;
			else {
				expr = 1;
				break;
			}

			if (last_cmp == 0)
				expr = last_expr || expr;
			else if (last_cmp == 1)
				expr = last_expr && expr;
			last_cmp = -1;
		}

		if (adv == 3) {
			if (strcmp(ap[1], "=") == 0)
				expr = strcmp(ap[0], ap[2]) == 0;
			else if (strcmp(ap[1], "!=") == 0)
				expr = strcmp(ap[0], ap[2]) != 0;
			else if (strcmp(ap[1], ">") == 0)
				expr = strcmp(ap[0], ap[2]) > 0;
			else if (strcmp(ap[1], "<") == 0)
				expr = strcmp(ap[0], ap[2]) < 0;
			else if (strcmp(ap[1], "-eq") == 0)
				expr = simple_strtol(ap[0], NULL, 10) == simple_strtol(ap[2], NULL, 10);
			else if (strcmp(ap[1], "-ne") == 0)
				expr = simple_strtol(ap[0], NULL, 10) != simple_strtol(ap[2], NULL, 10);
			else if (strcmp(ap[1], "-lt") == 0)
				expr = simple_strtol(ap[0], NULL, 10) < simple_strtol(ap[2], NULL, 10);
			else if (strcmp(ap[1], "-le") == 0)
				expr = simple_strtol(ap[0], NULL, 10) <= simple_strtol(ap[2], NULL, 10);
			else if (strcmp(ap[1], "-gt") == 0)
				expr = simple_strtol(ap[0], NULL, 10) > simple_strtol(ap[2], NULL, 10);
			else if (strcmp(ap[1], "-ge") == 0)
				expr = simple_strtol(ap[0], NULL, 10) >= simple_strtol(ap[2], NULL, 10);
			else {
				expr = 1;
				break;
			}

			if (last_cmp == 0)
				expr = last_expr || expr;
			else if (last_cmp == 1)
				expr = last_expr && expr;
			last_cmp = -1;
		}

		ap += adv; left -= adv;
	}

	if (neg)
		expr = !expr;

	expr = !expr;

	debug (": returns %d\n", expr);

	return expr;
#else
	ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);
	int nResetBtnHoldTime=0;

	static char command[CONFIG_SYS_CBSIZE] = { 0, };
	char cBuf[8]={0}, *cTemp;
	int nItemIndex = 1, nBufIndex=0, nCur=-1;
	u32 *uTemp=(u32 *)0x200000;
	cTemp=malloc(24);

	printf("Factory Test:\n");

/*CRC check of Kernel tests*/
/*	printf("CRC Test for NAND:\n");
	printf("\t%d.CRC:\t\t%d-1.CRC test of NAND/Kernel-Image\n", nItemIndex, nItemIndex);
	printf("\n");
	nItemIndex++;
*/

/*LED tests*/
	printf("\t%d.LED:\t\t%d-1.ATTN LED\n", nItemIndex, nItemIndex);
	printf("\t\t\t%d-2.Status LED\n", nItemIndex);
	printf("\t\t\t%d-3.Mode LED\n", nItemIndex);
	printf("\t\t\t%d-4.FailOver LED\n", nItemIndex);	
	printf("\t\t\t%d-5.WAP yellow LED\n", nItemIndex);	
	printf("\t\t\t%d-6.WAP Green LED\n", nItemIndex);
	printf("\n");
	nItemIndex++;

/*Flash Test*/
	printf("\t%d.Flash:\t%d-1.SPI R/W\n", nItemIndex, nItemIndex);
	printf("\t\t\t%d-2.Nand R/W\n", nItemIndex);
	printf("\n");
	nItemIndex++;
	
/*GPIO*/
	printf("\t%d.GPIO:\t\t%d-1.USB Power\t(Disable(0)/Enable(1))\n", nItemIndex, nItemIndex);
//	printf("\t\t\t%d-2.Simulate Overcurrent(0)/Normal Status(1)\n", nItemIndex);	
	printf("\t\t\t%d-2.PHY reset:\t(Reset all PHYs(0) / Normal Status(1) )\n", nItemIndex);	
	printf("\t\t\t%d-3.DDR reset:\t(Reset DDR(0) / Normal Status(1) )\n", nItemIndex);	
	printf("\t\t\t%d-4.PCIe module reset:(Reset(0) / Normal Status(1) ) \n", nItemIndex);	
	printf("\n");
	nItemIndex++;

/*MemTest*/
	printf("\t%d.MemTest:\t%d-1.Memory test.\n", nItemIndex, nItemIndex);
	printf("\n");
	nItemIndex++;

/*RTC Test*/
	printf("\t%d.I2C Test:\t%d-1.TPM: If the address is 0x29, then PASS. If it is not ,then FAIL.\n", nItemIndex, nItemIndex);
	printf("\t\t\t%d-2.RTC clock test.\n", nItemIndex);	
	printf("\t\t\t%d-3.RTC Battery Test: Write command example \"%d3:201402021530\" (YYYYMMDDHHMM)\n", nItemIndex, nItemIndex);	
	printf("\t\t\t%d-4.RTC Battery Test.\n", nItemIndex);	
	printf("\n");
	nItemIndex++;	
	
/*USB Test*/
	printf("\t%d.USB Test:\t%d-1.USB test.\n", nItemIndex, nItemIndex);
	printf("\t\t\t   (It is PASS if it shows '1 Storage Device(s) found'.).\n");
	printf("\t\t\t   (It is FAIL if it shows '0 Storage Device(s) found'.).\n");
	printf("\n");
	nItemIndex++;	

/*Ethernet*/
	printf("\t%d.Ethernet:\t%d-1.Ping test.\n", nItemIndex, nItemIndex);
	printf("\t\t\t    Please insert the RJ45 jack of the port you want to test.\n");
	printf("\t\t\t%d-2.Throughput: If the result shows \"Total of XXX bytes were the same\" then PASS. If it is not ,then FAIL.\n", nItemIndex);	
	printf("\n");
	nItemIndex++;

/*Factory reset btn*/
	printf("\t%d.Btn:\t\t%d-1.Factory button test.\n", nItemIndex, nItemIndex);
	printf("\t\t\t    (After entering this mode, please press and hold for 3 secs and the Factory reset button and the system will reset.\n");
	nItemIndex++;
	
/*Others
	printf("\t%d.Others:\t%d-1.Entering Linux kernel for other test items by downloading images.\n", nItemIndex, nItemIndex);
	printf("\t\t\t\tPlease make sure server ip is set and the TFTP server is ready.\n");	
*/

	memset(cTemp, 0, sizeof(cTemp));
	cTemp=getenv("serverip");
	if(strlen(cTemp))
		printf("\t\t\t    (Current server ip is %s)\n", cTemp);	
	printf("\n");
	nItemIndex++;
	
	printf("Press \'Q\' or \'q\' to quit factory test.\n\n");
	
ReTest:	
	printf("Please insert command:");
	while (1) {
		cBuf[++nCur] = getc();

		if ((cBuf[nCur] == 'q') || (cBuf[nCur] == 'Q')) 
		{
			printf("\n");

#undef CONFIG_FACTORY_TEST
			return 0;
		}

		if ((cBuf[nCur] == '\n') || (cBuf[nCur] == '\r')) 
		{
			printf("\n");
			goto FactoryTest;
		}
		else
		{
			printf("%c", cBuf[nCur]);
			continue;
		}
	}


FactoryTest:

	switch (cBuf[0]) 
	{
		/*
		case '1':
			//CRC tests
			switch (cBuf[1]) 
			{
				case '1': //
				memset(command, 0, sizeof(command));
				strcpy(command, "nand device 0;nand read 300000 0 9000000;crc32 300000 9000000 200000;md 200000 1");
				run_command (command, 0);
				
				break;
			}
			break;
		*/
		case '1':
		/*LED tests*/
		switch (cBuf[1]) 
			{
			case '1': //ATTN
				pgpio->gpdat=0xdc3f0000;
				break;
			case '2'://Status
				pgpio->gpdat=0xec3f0000;
				break;
			case '3'://mode
				pgpio->gpdat=0xf43f0000;
				break;
			case '4'://Fail Over
				pgpio->gpdat=0xf83f0000;
				break;
			case '5'://WAN, Yellow
				pgpio->gpdat=0xfc370000;
				break;
			case '6'://WAN, Green
				pgpio->gpdat=0xfc3B0000;
				break;
			
			}
		break;
	case '2':
		/*Flash tests*/

		memset(command, 0, sizeof(command));

		switch (cBuf[1])
		{
			case '1': //SPI
				strcpy(command, "mw 200000 12345678; mw 200004 99999999;sf probe 0;sf erase 90000 10000;sf write 200000 90000 4;sf read 200004 90000 4;");
				run_command (command, 0);

				uTemp=(u32 *)0x200000;
				if( (*uTemp) == (*(uTemp+1)))
					printf("SPI test PASS!\n\n");
				else
					printf("SPI test FAIL!\n\n");

				break;
			case '2'://NAND
				strcpy(command, "mw 200010 abcdef12; mw 200014 56565656;nand device 0;nand erase 1f000000 20000;nand write 200010 1f000000 4;nand read 200014 1f000000 4;");
				run_command (command, 0);
								
				uTemp=(u32 *)0x200010;
				if((*uTemp) == (*(uTemp+1)))
					printf("NAND test PASS!\n\n");
				else
				{
					printf("NAND test FAIL!\n\n");
				}
				break;
		}
		break;
	case '3':
		/*GPIO tests*/
		memset(command, 0, sizeof(command));
		
		switch (cBuf[1]) 
			{
			case '1': /*USB GPIO*/
				switch (cBuf[2]) 
				{
				case '0':
					pgpio->gpdat &= ~(0x1 << (31-GPIO_USB_EN));

					if(pgpio->gpdat | (0x1 << (31-GPIO_USB_EN)) )
						printf("GPIO USB Disable PASS!\n\n");
					else
						printf("GPIO USB Disable FAIL!\n\n");
					break;
				case '1':
					pgpio->gpdat |= 0x1 << (31-GPIO_USB_EN);

					if(pgpio->gpdat & ~(0x1 << (31-GPIO_USB_EN)))
						printf("GPIO USB Enable PASS!\n\n");
					else
						printf("GPIO USB Enable FAIL!\n\n");
					break;
				}
				break;
				
			case '2':/*PHY GPIO reset*/
				switch (cBuf[2]) 
				{
				case '0':
					pgpio->gpdat &= ~(0x1 << (31-GPIO_PHY_RST));

					if(pgpio->gpdat | (0x1 << (31-GPIO_PHY_RST)) )
						printf("GPIO PHY Reset Pin Clr PASS!\n\n");
					else
						printf("GPIO PHY Reset Pin Clr FAIL!\n\n");
					break;
				case '1':
					pgpio->gpdat |= 0x1 << (31-GPIO_PHY_RST);

					if(pgpio->gpdat & ~(0x1 << (31-GPIO_PHY_RST)))
						printf("GPIO PHY Reset Pin Set PASS!\n\n");
					else
						printf("GPIO PHY Reset Pin Set FAIL!\n\n");
					break;
				}
				break;
			case '3':/*DDR GPIO reset*/
				switch (cBuf[2]) 
				{
				case '0':
					strcpy(command, "mw ffe0f000 bc3f0000;mw ffe0f008 fc3d0000");
					run_command (command, 0);

					break;
				case '1':
					strcpy(command, "mw ffe0f000 bc3f0000;mw ffe0f008 fc3f0000");
					run_command (command, 0);
					
					break;
				}
				break;
			case '4':/*PCIe Module GPIO reset*/
				switch (cBuf[2]) 
				{
				case '0':
					pgpio->gpdat &= ~(0x1 << (31-GPIO_PCIE_MODULE_RST));

					if(pgpio->gpdat | (0x1 << (31-GPIO_PCIE_MODULE_RST)) )
						printf("GPIO PCIe Reset Pin Clr PASS!\n\n");
					else
						printf("GPIO PCIe Reset Pin Clr FAIL!\n\n");
					break;
				case '1':
					pgpio->gpdat |= 0x1 << (31-GPIO_PCIE_MODULE_RST);

					if(pgpio->gpdat & ~(0x1 << (31-GPIO_PCIE_MODULE_RST)))
						printf("GPIO PCIe Reset Pin Set PASS!\n\n");
					else
						printf("GPIO PCIe Reset Pin Set FAIL!\n\n");
					break;
				}
				break;
				
			}	
		break;
	case '4':
		/*Memory Test*/
		switch (cBuf[1]) 
			{
			case '1':
				mem_test();
				/*
				memset(command, 0, sizeof(command));
				strcpy(command, "mtest");
				run_command (command, 0);
				*/
				break;
			}
		break;
	case '5':
		/*I2C Test*/
		switch (cBuf[1]) 
		{
			case '1': /*TPM*/
				memset(command, 0, sizeof(command));
				strcpy(command, "i2c dev 1;i2c probe");
				run_command (command, 0);
				break;
			case '2':/*RTC*/
				RTC_Test();
				break;
			case '3':/*RTC Battery Test:Write*/
				if(cBuf[3])
					RTC_Battery_Test_Write(&cBuf[3]);
				else
					printf("Error! Please follow the pattern:\"201402021530\" (YYYYMMDDHHMM)\n");
				break;
			case '4':/*RTC Battery Test:Read*/
				RTC_Battery_Test_Read();
				break;
		}
		break;
	case '6':
		/*USB Test*/
		switch (cBuf[1]) 
		{
			case '1':/*RTC*/
				memset(command, 0, sizeof(command));
				strcpy(command, "usb reset");
				run_command (command, 0);
				break;
		}
		break;
	case '7':
		/*Ethernet tests*/
		memset(command, 0, sizeof(command));

		printf("If a message showed that \"Your (IP) is alive!\", then the enthernet is PASS.\n");
		printf("If it is not, then is FAIL.\n");
		switch (cBuf[1])
		{
			case '1': 
				if(strlen(cTemp))
				{
					printf("Now ping the server IP %s\n", cTemp);
					sprintf(command, "ping %s", cTemp);
				}
				else
				{
					printf("The serverip is not set, so now pinging default ip---192.168.1.101.\n");
					strcpy(command, "ping 192.168.1.101");
				}
				run_command (command, 0);
				break;
			case '2':
				memset(command, 0, sizeof(command));
				strcpy(command, "tftp 200000 rootfs.ext2.gz.uboot;tftp 4200000 rootfs.ext2.gz.uboot;cmp.b 200000 4200000 2200000");
				run_command (command, 0);			
				break;			
		}
		break;
	case '8':
			printf("\nPlease press and hold for 3 secs and the Factory reset button and the system will reset.)\n");
			printf("\n");
	
			while(1)
			{		
				if(pgpio->gpdat & FACTORY_RESET_GPIO)//normal status, so reset the timer
					nResetBtnHoldTime=0;
				else
					nResetBtnHoldTime+=RESET_BTN_HOLD_TIME_CHECK_INTERVAL;
				
				udelay(RESET_BTN_HOLD_TIME_CHECK_INTERVAL*1000);

				if(nResetBtnHoldTime>RESET_BTN_HOLD_TIME)
				{
					printf("Factory reset button is hold more than %d ms, now reset...\n", RESET_BTN_HOLD_TIME);
					do_reset (NULL, 0, 0, NULL);
				}
			}
			break;
	
//	case '8':
		/*Entering kernel*/
	/*	memset(command, 0, sizeof(command));

		switch (cBuf[1])
		{
			case '1': 
				if(strlen(cTemp))
				{
					printf("Please set the IP of your test station as %s\n", cTemp);
				}
				else
				{
					printf("Your 'serverip' environment variable is not set. Now set it as default 192.168.1.101\n");
					setenv("serverip", "192.168.1.101");
				}
				
				memset(command, 0, sizeof(command));
				strcpy(command, "tftp 0x2000000 p1010rdb.dtb;tftp 0x2100000 uImage;tftp 0x2600000 rootfs.ext2.gz.uboot;bootm 0x2100000 0x2600000 0x2000000;");
				run_command (command, 0);

				break;
			
		}
		break;
*/
	}

	for(nBufIndex=0; nBufIndex<nCur; nBufIndex++)
		cBuf[nBufIndex]='\0';
	nCur=-1;
	
	goto ReTest;

	return 0;
#endif
}

U_BOOT_CMD(
	test,	CONFIG_SYS_MAXARGS,	1,	do_test,
	"minimal test like /bin/sh",
	"[args..]"
);

int mem_test(void)
{
	vu_long	*addr, *start, *end;
	ulong	val;
	ulong	readback;
	ulong	errs = 0;
	int iterations = 1;
	int iteration_limit;

//#if defined(CONFIG_SYS_ALT_MEMTEST)
	vu_long	len;
	vu_long	offset;
	vu_long	test_offset;
	vu_long	pattern;
	vu_long	temp;
	vu_long	anti_pattern;
	vu_long	num_words;
#if defined(CONFIG_SYS_MEMTEST_SCRATCH)
	vu_long *dummy = (vu_long*)CONFIG_SYS_MEMTEST_SCRATCH;
#else
	vu_long *dummy = 0;	/* yes, this is address 0x0, not NULL */
#endif
	int	j;

	static const ulong bitpattern[] = {
		0x00000001,	/* single bit */
		0x00000003,	/* two adjacent bits */
		0x00000007,	/* three adjacent bits */
		0x0000000F,	/* four adjacent bits */
		0x00000005,	/* two non-adjacent bits */
		0x00000015,	/* three non-adjacent bits */
		0x00000055,	/* four non-adjacent bits */
		0xaaaaaaaa,	/* alternating 1/0 */
	};
/*
#else
	ulong	incr;
	ulong	pattern;
#endif
*/

	start = (ulong *)(CONFIG_SYS_MEMTEST_START);
	end = (ulong *)(CONFIG_SYS_MEMTEST_END);
	iteration_limit = 0;

	printf ("Testing %08x ... %08x:\n", (uint)start, (uint)end);
	printf("%s:%d: start 0x%p end 0x%p\n",
		__FUNCTION__, __LINE__, start, end);

	/*for (;;) */{
		if (ctrlc()) {
			putc ('\n');
			return 1;
		}


		if (iteration_limit && iterations > iteration_limit) {
			printf("Tested %d iteration(s) with %lu errors.\n",
				iterations-1, errs);
			return errs != 0;
		}

		/*
		printf("Iteration: %6d\r", iterations);
		PRINTF("\n");
		*/
		iterations++;

		/*
		 * Data line test: write a pattern to the first
		 * location, write the 1's complement to a 'parking'
		 * address (changes the state of the data bus so a
		 * floating bus doen't give a false OK), and then
		 * read the value back. Note that we read it back
		 * into a variable because the next time we read it,
		 * it might be right (been there, tough to explain to
		 * the quality guys why it prints a failure when the
		 * "is" and "should be" are obviously the same in the
		 * error message).
		 *
		 * Rather than exhaustively testing, we test some
		 * patterns by shifting '1' bits through a field of
		 * '0's and '0' bits through a field of '1's (i.e.
		 * pattern and ~pattern).
		 */
		printf("Data line testing...\n");
		addr = start;
		for (j = 0; j < sizeof(bitpattern)/sizeof(bitpattern[0]); j++) {
			
		    val = bitpattern[j];
			for(; val != 0; val <<= 1) {
			
			/*
			printf("testing val=%08x, addr=%x\n", val, addr);
			*/

			*addr  = val;
			*dummy  = ~val; /* clear the test data off of the bus */
			readback = *addr;
			if(readback != val) {
			    printf ("FAILURE (data line): "
				"expected %08lx, actual %08lx\n",
					  val, readback);
			    errs++;
			    if (ctrlc()) {
				putc ('\n');
				return 1;
			    }
			}
			*addr  = ~val;
			*dummy  = val;
			readback = *addr;
			if(readback != ~val) {
			    printf ("FAILURE (data line): "
				"Is %08lx, should be %08lx\n",
					readback, ~val);
			    errs++;
			    if (ctrlc()) {
				putc ('\n');
				return 1;
			    }
			}
		    }
		}

		/*
		 * Based on code whose Original Author and Copyright
		 * information follows: Copyright (c) 1998 by Michael
		 * Barr. This software is placed into the public
		 * domain and may be used for any purpose. However,
		 * this notice must not be changed or removed and no
		 * warranty is either expressed or implied by its
		 * publication or distribution.
		 */

		/*
		 * Address line test
		 *
		 * Description: Test the address bus wiring in a
		 *              memory region by performing a walking
		 *              1's test on the relevant bits of the
		 *              address and checking for aliasing.
		 *              This test will find single-bit
		 *              address failures such as stuck -high,
		 *              stuck-low, and shorted pins. The base
		 *              address and size of the region are
		 *              selected by the caller.
		 *
		 * Notes:	For best results, the selected base
		 *              address should have enough LSB 0's to
		 *              guarantee single address bit changes.
		 *              For example, to test a 64-Kbyte
		 *              region, select a base address on a
		 *              64-Kbyte boundary. Also, select the
		 *              region size as a power-of-two if at
		 *              all possible.
		 *
		 * Returns:     0 if the test succeeds, 1 if the test fails.
		 */
		len = ((ulong)end - (ulong)start)/sizeof(vu_long);
 		printf("Address line testing...\n");
		pattern = (vu_long) 0xaaaaaaaa;
		anti_pattern = (vu_long) 0x55555555;

		/*
		printf("%s:%d: length = 0x%.8lx\n",
			__FUNCTION__, __LINE__,
			len);
		*/
		/*
		 * Write the default pattern at each of the
		 * power-of-two offsets.
		 */
		for (offset = 1; offset < len; offset <<= 1) {
			
			start[offset] = pattern;
			/*
			printf("Write the default pattern. start[%08x]--> Addr:%08x, Value = %08x -->%08x \n", offset, &start[offset], start[offset], pattern);
			*/
		}

		/*
		 * Check for address bits stuck high.
		 */
		test_offset = 0;
		start[test_offset] = anti_pattern;

		for (offset = 1; offset < len; offset <<= 1) {
			/*
			printf("Check for address bits stuck high. start[%08x]--> Addr:%08x, Value = %08x -->%08x \n", offset, &start[offset], start[offset], anti_pattern);
			*/
		    temp = start[offset];
		    if (temp != pattern) {
			printf ("\nFAILURE: Address bit stuck high @ 0x%.8lx:"
				" expected 0x%.8lx, actual 0x%.8lx\n",
				(ulong)&start[offset], pattern, temp);
			errs++;
			if (ctrlc()) {
			    putc ('\n');
			    return 1;
			}
		    }
		}
		start[test_offset] = pattern;
		WATCHDOG_RESET();
		/*
		 * Check for addr bits stuck low or shorted.
		 */
		for (test_offset = 1; test_offset < len; test_offset <<= 1) {
		    start[test_offset] = anti_pattern;
			/*
			printf("Check for addr bits stuck low or shorted.. start[%08x]--> Addr:%08x, Value = %08x -->%08x \n", test_offset, &start[test_offset], start[test_offset], anti_pattern);
			*/

		    for (offset = 1; offset < len; offset <<= 1) {
			temp = start[offset];
			if ((temp != pattern) && (offset != test_offset)) {
			    printf ("\nFAILURE: Address bit stuck low or shorted @"
				" 0x%.8lx: expected 0x%.8lx, actual 0x%.8lx\n",
				(ulong)&start[offset], pattern, temp);
			    errs++;
			    if (ctrlc()) {
				putc ('\n');
				return 1;
			    }
			}
		    }
		    start[test_offset] = pattern;
		}

		/*
		 * Description: Test the integrity of a physical
		 *		memory device by performing an
		 *		increment/decrement test over the
		 *		entire region. In the process every
		 *		storage bit in the device is tested
		 *		as a zero and a one. The base address
		 *		and the size of the region are
		 *		selected by the caller.
		 *
		 * Returns:     0 if the test succeeds, 1 if the test fails.
		 */
		printf("Integrity testing...This will take a while. Please wait...\n");
		num_words = ((ulong)end - (ulong)start)/sizeof(vu_long) + 1;

		/*
		 * Fill memory with a known pattern.
		 */
		printf("Fill memory with a known pattern.");	
		for (pattern = 1, offset = 0; offset < num_words; pattern++, offset++) {
			WATCHDOG_RESET();
			start[offset] = pattern;
			/*
			printf("Fill memory with a known pattern. start[%08x]--> Addr:%08x, Value = %08x -->%08x \n", offset, &start[offset], start[offset],pattern);	
			*/
			
			if( !(pattern % 0xd0000))
				printf(".");
		}
	
		/*
		 * Check each location and invert it for the second pass.
		 */
		printf("\nCheck each location and invert it for the second stage.");	
		for (pattern = 1, offset = 0; offset < num_words; pattern++, offset++) {
			/*
			printf("Check each location and invert it for the second stage. start[%08x]--> Addr:%08x, Value = %08x -->%08x \n", offset, &start[offset], start[offset], pattern);
			*/
		    WATCHDOG_RESET();
		    temp = start[offset];
		    if (temp != pattern) {
			printf ("\nFAILURE (read/write) @ 0x%.8lx:"
				" expected 0x%.8lx, actual 0x%.8lx)\n",
				(ulong)&start[offset], pattern, temp);
			errs++;
			if (ctrlc()) {
			    putc ('\n');
			    return 1;
			}
		    }

		    anti_pattern = ~pattern;
		    start[offset] = anti_pattern;
			if( !(pattern % 0xd0000))
				printf(".");
		}

		/*
		 * Check each location for the inverted pattern and zero it.
		 */
		printf("\nCheck each location for the inverted pattern and zero it..");	
		for (pattern = 1, offset = 0; offset < num_words; pattern++, offset++) {
		    WATCHDOG_RESET();
		    anti_pattern = ~pattern;
		    temp = start[offset];
		    if (temp != anti_pattern) {
			printf ("\nFAILURE (read/write): @ 0x%.8lx:"
				" expected 0x%.8lx, actual 0x%.8lx)\n",
				(ulong)&start[offset], anti_pattern, temp);
			errs++;
			if (ctrlc()) {
			    putc ('\n');
			    return 1;
			}
		    }
		    start[offset] = 0;
			if( !(pattern % 0xd0000))
				printf(".");
		}
	}

	if(errs)
		printf ("\n%lu errors occurs! Memtest FAIL\n", errs);
	else
		printf ("\nMemtest PASS\n");
		
	return 0;
}

int rtc_set_get_test(void)
{
	SC_RTC_TIME time;
	SC_RTC_TIME time_get;

	time.year	= 8; //year 2008
	time.month	= 8; //month
	time.day	= 8; //day
	time.week	= 5; //day of week data,2008 -08 -08 is Fri
	time.hour	= 8; //hour
	time.min	= 8; //minute
	time.sec	= 8; //second

	printf("***Will set time to 2008-08-08 08:08:08***\n");

	if (sc_rtc_set_time(&time) == RTC_OP_FAIL) {
		printf("***Set time to 2008-08-08 08:08:08 error***\n");
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("***Set time to 2008-08-08 08:08:08 success, will delay %d second...***\n", RTC_TEST_DELAY);
	udelay(1000000 * RTC_TEST_DELAY);

	if (sc_rtc_get_time(&time_get) == RTC_OP_FAIL) {
		printf("***After set time,read back error***");
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("Read back time:%d-%d-%d %d(day of week) %d:%d:%d\n",
	time_get.year + 2000,
	time_get.month,
	time_get.day,
	time_get.week,
	time_get.hour,
	time_get.min,
	time_get.sec);

	if ((time_get.year != time.year ) || (time_get.month != time.month) ||
	    (time_get.day != time.day ) || (time_get.week != time.week) ||
	    (time_get.hour != time.hour) || (time_get.min != time.min )) {
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("Set second:%d, Read second:%d \n", time.sec, time_get.sec);
	if ( (time_get.sec - time.sec) == RTC_TEST_DELAY ) {
		return SC_MFG_TEST_OP_PASS;
	}
	else {
		return SC_MFG_TEST_OP_FAIL;
	}
}

/*
* RTC Test main entry
*/
int RTC_Test(void)
{
	int RTC_result;

	/* I2C bus Init */
	sc_i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	/* RTC Chip Init */
	sc_rtc_init();

	RTC_result = rtc_set_get_test();

	if ( RTC_result != SC_MFG_TEST_OP_PASS ) {
		printf("RTC Test FAIL\n");
	}
	else {
		printf("RTC Test PASS\n");
	}
	return 0;
}

int RTC_Battery_Test_Write(char *cRTCTime)
{
	SC_RTC_TIME time;
	int i=0, nTmp=0;
	char cTmp[5]={0};
	
	if(strlen(cRTCTime)<strlen("201402021530"))
		goto RTC_BATTERY_TEST_ERROR;
		
	for(;i<strlen(cRTCTime);i++)
	{
		if(!cRTCTime[i])
			goto RTC_BATTERY_TEST_ERROR;
	}
	
	i=0;
	
	strncpy(cTmp, &cRTCTime[i], 4); 
	nTmp=simple_strtoul(cTmp, NULL, 10);
	time.year	= nTmp-2000; 
	i+=4;
	
	memset(cTmp, 0, sizeof(cTmp));
	strncpy(cTmp, &cRTCTime[i], 2); 
	nTmp=simple_strtoul(cTmp, NULL, 10);
	time.month	= nTmp;
	i+=2;
	
	memset(cTmp, 0, sizeof(cTmp));
	strncpy(cTmp, &cRTCTime[i], 2); 
	nTmp=simple_strtoul(cTmp, NULL, 10);
	time.day	= nTmp; 
	i+=2;
	
	memset(cTmp, 0, sizeof(cTmp));
	strncpy(cTmp, &cRTCTime[i], 2); 
	nTmp=simple_strtoul(cTmp, NULL, 10);
	time.hour	= nTmp;
	i+=2;
	
	memset(cTmp, 0, sizeof(cTmp));
	strncpy(cTmp, &cRTCTime[i], 2); 
	nTmp=simple_strtoul(cTmp, NULL, 10);
	time.min	= nTmp; 
	i+=2;
	
	time.sec	= 0; 
	time.week	= 5; //day of week data,2008 -08 -08 is Fri
	
	printf(	"***Will set time to %04d-%02d-%02d %02d:%02d:%02d***\n", 
			time.year+2000, 
			time.month, 
			time.day,
			time.hour,
			time.min,
			time.sec);

	if (sc_rtc_set_time(&time) == RTC_OP_FAIL) {
		printf("***Set time FAIL***\n");
		return SC_MFG_TEST_OP_FAIL;
	}

	printf("***Set time OK***\n");
	return SC_MFG_TEST_OP_PASS;
	
	RTC_BATTERY_TEST_ERROR:
		printf("Error! Please follow the pattern like this:\"201402021530\" (YYYYMMDDHHMM)\n");
		return SC_MFG_TEST_OP_FAIL;		
}

int RTC_Battery_Test_Read(void)
{
	SC_RTC_TIME time_get;
	
	if (sc_rtc_get_time(&time_get) == RTC_OP_FAIL) {
		printf("***Get time FAIL***");
		return SC_MFG_TEST_OP_FAIL;
	}

	if(time_get.hour >= 40) //the bit represent am/pm
	{
		printf("Read back time:%04d-%02d-%02d %02d:%02d:%02d\n",
		time_get.year + 2000,
		time_get.month,
		time_get.day,
		time_get.hour-40,
		time_get.min,
		time_get.sec);
	}
	else
	{
		printf("Read back time:%04d-%02d-%02d %02d:%02d:%02d\n",
		time_get.year + 2000,
		time_get.month,
		time_get.day,
		time_get.hour,
		time_get.min,
		time_get.sec);
	}	
	/*
	if ((time_get.year != time.year ) || (time_get.month != time.month) ||
	    (time_get.day != time.day ) || (time_get.week != time.week) ||
	    (time_get.hour != time.hour) || (time_get.min != time.min )) {
		return SC_MFG_TEST_OP_FAIL;
	}
	*/
	
	printf("***Get time OK***\n");
	return SC_MFG_TEST_OP_PASS;

}

int do_false(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return 1;
}

U_BOOT_CMD(
	false,	CONFIG_SYS_MAXARGS,	1,	do_false,
	"do nothing, unsuccessfully",
	NULL
);

int do_true(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return 0;
}

U_BOOT_CMD(
	true,	CONFIG_SYS_MAXARGS,	1,	do_true,
	"do nothing, successfully",
	NULL
);
