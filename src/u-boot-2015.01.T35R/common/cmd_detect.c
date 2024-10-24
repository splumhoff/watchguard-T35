/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * cmd detect the reset key on WG box
 */

#if defined(CONFIG_T35) 

#include <common.h>
#include <command.h>

/* We come here after U-Boot is initialised and ready to process commands */
static int do_detect(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
        unsigned char sysbReq;

	sysbReq = detect_SYSB_keypress();
	printf("detect_SYSB_keypress = %d\n", sysbReq);

	return 0;
}

U_BOOT_CMD(
	detect,	1,	1,	do_detect,
	"detect reset button",
);
#endif // if defined(CONFIG_T35) 
