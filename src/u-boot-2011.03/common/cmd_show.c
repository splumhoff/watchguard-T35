/*
 * Lanner switch burn test tool
 * vlan switch 0x10 
 * port 1,2 vid=3
 * port 3,5 vid=4
 * port 4,6 vid=5
 * vlan switch 0x11
 * port 5,6 vid=7
 * port 0,1 vid=8
 * port 2,3 vod=9
 * under OS brige eth0 and eth1
 */

#include <common.h>
#include <command.h>
#include <miiphy.h>

int vid_1[7]={1, 3, 3, 4, 5, 4, 5};
int fid30_1[3]={0x3113, 0x1333, 0x3333};
int fid64_1[3]={0x333, 0x313, 0x131};

int vid_2[7]={8, 8, 9, 9, 1, 7, 7};
int fid30_2[3]={0x3333, 0x3311, 0x1133};
int fid64_2[3]={0x113, 0x333, 0x333};

int do_vid_set (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i;
	int timeout = 0;
	unsigned short value = 0;

/* Set Seitch 1 address 0x10 */
	for( i = 0; i< 7 ; i++)
	{
		DLAY SMIRW(1, 0x10, 0x7, vid_1[i], 0, 0,0x10 + i, 0);
		DLAY SMIRW(1, 0x10, 0x8, 0x2c80, 0, 0,0x10 + i, 0);
	}

	for( i = 0 ; i < 3 ;i++)
	{
		/* current VID table 3 ~ 0 */
		DLAY SMIRW(1, 0x10, 0x7, fid30_1[i], 0, 0,0x1b, 0);
		/* current VID table 6 ~ 4 */
		DLAY SMIRW(1, 0x10, 0x8, fid64_1[i], 0, 0,0x1b, 0);
		/* Set VID ID,Start ID is 3 */
		DLAY SMIRW(1, 0x10, 0x6, 0x1003 + i , 0, 0,0x1b, 0);
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
/* Set Switch 0 address 0x11 */
	for( i = 0; i< 7 ; i++)
	{
		DLAY SMIRW(1, 0x11, 0x7, vid_2[i], 0, 0,0x10 + i, 0);
		DLAY SMIRW(1, 0x11, 0x8, 0x2c80, 0, 0,0x10 + i, 0);
	}

	for( i = 0 ; i < 3 ;i++)
	{
		/* current VID table 3 ~ 0 */
		DLAY SMIRW(1, 0x11, 0x7, fid30_2[i], 0, 0,0x1b, 0);
		/* current VID table 6 ~ 4 */
		DLAY SMIRW(1, 0x11, 0x8, fid64_2[i], 0, 0,0x1b, 0);
		/* Set VID ID ,Start ID is 7 */
		DLAY SMIRW(1, 0x11, 0x6, 0x1007 + i , 0, 0,0x1b, 0);
		/* Set multiple address for vid */
		DLAY SMIRW(1, 0x11, 0x2, 0x02 + i , 0, 0,0x1b, 0);
		/* Store current VID */
		DLAY SMIRW(1, 0x11, 0x5, 0xb000, 0, 0,0x1b, 0);

		DLAY SMIRW(0, 0x11, 0x5, 0x0000, 0, 0,0x1b, &value);
		timeout = 0;
		while ( (value & 0x8000 ) != 0 && timeout > 200)
		{
			DLAY SMIRW(0, 0x11, 0x5, 0x0000, 0, 0,0x1b, &value);
			timeout++;
		}
		if(timeout >= 200 )
		{
			printf("Switch 0x11 VLAN Table %01d Set timeout \n",i+3);
		}
	}
/* Enable internal switch connect  */
	DLAY SMIRW(1, 0x10, 0x0, 0x9140, 0, 0,0x04,0);
	DLAY SMIRW(1, 0x10, 0x1, 0x803e, 0, 0,0x16,0);
	DLAY SMIRW(1, 0x11, 0x1, 0x803e, 0, 0,0x16,0);
	return 0;
}

U_BOOT_CMD(
        vid_set,       2,      1,      do_vid_set,
        "Set switch 0x10 and 0x11 to vid",
        "lanner switch test\n"
);

