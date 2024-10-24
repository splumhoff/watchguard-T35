#include <common.h>
#include <command.h>
#include <miiphy.h>

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_RAMBOOT_PBL

static int do_sw(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned short value = 0;
	unsigned short port,addr,reg,data;
        int vid_1[7]={3, 2, 2, 2, 2, 2, 3};
        int fid30_1[2]={0x1113, 0x3331};
        int fid64_1[2]={0x311, 0x133};
        int timeout;
	int i;


	if (strcmp(argv[1],"write") == 0) {
		port = simple_strtoul(argv[2], NULL, 16);
		addr = simple_strtoul(argv[3], NULL, 16);
		reg = simple_strtoul(argv[4], NULL, 16);
		data = simple_strtoul(argv[5], NULL, 16);
		SMIRW(1, 0x10, addr, data, 0, 0, port, 0);
		printf("W Port=%2x addr=%02x reg=%02x data=%08x\n",
			port,addr,reg,data);
	}else if (strcmp(argv[1],"read") == 0) {
		port = simple_strtoul(argv[2], NULL, 16);
		addr = simple_strtoul(argv[3], NULL, 16);
		reg = simple_strtoul(argv[4], NULL, 16);
		SMIRW(0, 0x10, addr, 0, 0, 0, port, &value);
		printf("R Port=%2x addr=%02x reg=%02x data=%08x\n",
			port,addr,reg,value);
	}else if (strcmp(argv[1],"init") == 0) {
                DLAY SMIRW(1, 0x10, 4, 0xa000, 0, 0, 0x1b, 0);

                /* set rgmii delay  */
                DLAY SMIRW(1, 0x10, 1, 0xc03e, 0, 0, 0x15, 0);  /* P15 */
                DLAY SMIRW(1, 0x10, 1, 0xc03e, 0, 0, 0x16, 0);  /* P16 */
                /* set LED function */
                DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x10, 0);/* P0 */
                DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x11, 0);/* P1 */
                DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x12, 0);/* P1 */
                DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x13, 0);/* P3 */
                DLAY SMIRW(1, 0x10, 0x16, 0x9038, 0, 0,0x14, 0);/* P4 */
                /* set port to forwarding mode */
                DLAY SMIRW(1, 0x10, 4, 0x007f, 0, 0, 0x10, 0);
                DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x11, 0);
                DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x12, 0);
                DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x13, 0);
                DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x14, 0);
                DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x15, 0);
                DLAY SMIRW(1, 0x10, 4 ,0x007f, 0, 0, 0x16, 0);
                /* reset PHY */
                DLAY SMIRW(1, 0x10, 0 ,0x9140, 0, 0, 0x0, 0);
                DLAY SMIRW(1, 0x10, 0 ,0x9140, 0, 0, 0x1, 0);
                DLAY SMIRW(1, 0x10, 0 ,0x9140, 0, 0, 0x2, 0);
                DLAY SMIRW(1, 0x10, 0 ,0x9140, 0, 0, 0x3, 0);
                DLAY SMIRW(1, 0x10, 0 ,0x9140, 0, 0, 0x4, 0);
                DLAY
                DLAY
                DLAY
                /* Default power down mode , to disable */
                DLAY SMIRW(1, 0x10, 0 ,0x1140, 0, 0, 0x0, 0);
                DLAY SMIRW(0, 0x10, 0, 0x0, 0, 0, 0x0, &value);
                printf("Port0 reg0 =%x \n",value);

                DLAY SMIRW(1, 0x10, 0 ,0x1140, 0, 0, 0x1, 0);
                DLAY SMIRW(0, 0x10, 0, 0x0, 0, 0, 0x1, &value);
                printf("Port1 reg0 =%x \n",value);

                DLAY SMIRW(1, 0x10, 0 ,0x1140, 0, 0, 0x2, 0);
                DLAY SMIRW(0, 0x10, 0, 0x0, 0, 0, 0x2, &value);
                printf("Port2 reg0 =%x \n",value);

                DLAY SMIRW(1, 0x10, 0 ,0x1140, 0, 0, 0x3, 0);
                DLAY SMIRW(0, 0x10, 0, 0x0, 0, 0, 0x3, &value);
                printf("Port3 reg0 =%x \n",value);

                DLAY SMIRW(1, 0x10, 0 ,0x1140, 0, 0, 0x4, 0);
                DLAY SMIRW(0, 0x10, 0, 0x0, 0, 0, 0x4, &value);
                printf("Port4 reg0 =%x \n",value);
                printf("Done1\n");

	}else if (strcmp(argv[1],"vlan") == 0) {
                /* Set Switch 1 address 0x10 */
                for( i = 0; i< 7 ; i++)
                {
                        DLAY SMIRW(1, 0x10, 0x7, vid_1[i], 0, 0,0x10 + i, 0);
                        DLAY SMIRW(1, 0x10, 0x8, 0x2c80, 0, 0,0x10 + i, 0);
                DLAY SMIRW(0, 0x10, 0x7, 0x0, 0, 0, 0x10 + i, &value);
                printf("Port%d reg7 =%x \n",i,value);
                DLAY SMIRW(0, 0x10, 0x8, 0x0, 0, 0, 0x10 + i, &value);
                printf("Port%d reg8 =%x \n",i,value);
                }
                for( i = 0 ; i < 2 ;i++)
                {
                        /* current VID table 3 ~ 0 */
                        DLAY SMIRW(1, 0x10, 0x7, fid30_1[i], 0, 0,0x1b, 0);
                        /* current VID table 6 ~ 4 */
                        DLAY SMIRW(1, 0x10, 0x8, fid64_1[i], 0, 0,0x1b, 0);
                        /* Set VID ID,Start ID is 2 */
                        DLAY SMIRW(1, 0x10, 0x6, 0x1002 + i , 0, 0,0x1b, 0);
                        /* Set multiple address for vid */
                        DLAY SMIRW(1, 0x10, 0x2, 0x02 + i , 0, 0,0x1b, 0);
                        /* Store current VID */
                        DLAY SMIRW(1, 0x10, 0x5, 0xb000, 0, 0,0x1b, 0);
                        DLAY SMIRW(0, 0x10, 0x5, 0x0000, 0, 0,0x1b, &value);
                        timeout = 0;
                        while ( (value & 0x8000 ) != 0 && timeout > 200)
                        {
                                DLAY SMIRW(0, 0x10, 0x5, 0x0000, 0, 0,0x1b, &value);
                                timeout++;
                        }
                        if(timeout >= 200 )
                        {
                                printf("Switch 0x10 VLAN Table %01d Set timeout \n",i+3);
                        }
                }


	}


	return 1;
}


/***************************************************/

U_BOOT_CMD(
	sw,	6,	1,	do_sw,
	"read/write init and vlan",
	"  - read port addr reg\n"
	"  - write port addr reg data\n"
	"  - init  \n"
	"  - vlan\n"
);

#endif
