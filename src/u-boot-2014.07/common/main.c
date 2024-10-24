/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define	DEBUG	*/

#include <common.h>
#include <autoboot.h>
#include <cli.h>
#include <version.h>

#include <miiphy.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Board-specific Platform code can reimplement show_boot_progress () if needed
 */
void inline __show_boot_progress (int val) {}
void show_boot_progress (int val) __attribute__((weak, alias("__show_boot_progress")));

static void modem_init(void)
{
#ifdef CONFIG_MODEM_SUPPORT
	debug("DEBUG: main_loop:   gd->do_mdm_init=%lu\n", gd->do_mdm_init);
	if (gd->do_mdm_init) {
		char *str = getenv("mdm_cmd");

		setenv("preboot", str);  /* set or delete definition */
		mdm_init(); /* wait for modem connection */
	}
#endif  /* CONFIG_MODEM_SUPPORT */
}

static void run_preboot_environment_command(void)
{
#ifdef CONFIG_PREBOOT
	char *p;

	p = getenv("preboot");
	if (p != NULL) {
# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif

		run_command_list(p, -1, 0);

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif
	}
#endif /* CONFIG_PREBOOT */
}


#if defined(CONFIG_M200) || defined(CONFIG_M300)

#define BUTTON_RESET			(25)

/*
* Read button status by given gpio number
*/
int read_btn_status(unsigned int btn)
{
	unsigned int reg_val;
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	reg_val = pgpio->gpdat;
#if defined(CONFIG_M200) || defined(CONFIG_M300)
	/* Low active */
	if (reg_val & (1 << (31 - btn)) )
		return 0;
	else
		return 1;
#elif CONFIG_NEWPLATFORM
	/* return 1 if keypresse detected
	 * else return 0.
	 */
	return 0;
#else
	return 0;
#endif

}

unsigned char detect_SYSB_keypress(void)
{
	int status = 0;
	int i;
	for(i = 0; i < 3; i++) {

	        if (read_btn_status(BUTTON_RESET)) {
			printf("booting into SYSB ... \n");
			status = 1;
			break;
		}
        	udelay(1000);
	}

	return status;
}
#endif /* end of CONFIG_M200 or CONFIG_M300 */

/* We come here after U-Boot is initialised and ready to process commands */
void main_loop(void)
{
	const char *s;
#if defined(CONFIG_M200) || defined(CONFIG_M300)
       unsigned char sysbReq;
#endif
#ifdef CONFIG_BOREN_ETHNET_SETUP
	const char	*devname;
	setenv("ethact", "eTSEC3");
	devname = miiphy_get_current_dev();
	miiphy_write(devname, 0, 0x16, 0x12);
	miiphy_write(devname, 0, 0x14, 0x8001);
	miiphy_write(devname, 0, 0x16, 0x0);


	setenv("ethact", "eTSEC2");
	devname = miiphy_get_current_dev();
	miiphy_write(devname, 1, 0x16, 0x12);
	miiphy_write(devname, 1, 0x14, 0x8001);
	miiphy_write(devname, 1, 0x16, 0x0);
	setenv("ethact", "eTSEC1");

#endif
	bootstage_mark_name(BOOTSTAGE_ID_MAIN_LOOP, "main_loop");

#ifndef CONFIG_SYS_GENERIC_BOARD
	puts("Warning: Your board does not use generic board. Please read\n");
	puts("doc/README.generic-board and take action. Boards not\n");
	puts("upgraded by the late 2014 may break or be removed.\n");
#endif

	modem_init();
#ifdef CONFIG_VERSION_VARIABLE
	setenv("ver", version_string);  /* set version variable */
#endif /* CONFIG_VERSION_VARIABLE */

	cli_init();

	run_preboot_environment_command();

#if defined(CONFIG_UPDATE_TFTP)
	update_tftp(0UL);
#endif /* CONFIG_UPDATE_TFTP */

	s = bootdelay_process();
	if (cli_process_fdt(&s))
		cli_secure_boot_cmd(s);

#if defined(CONFIG_M200) || defined(CONFIG_M300)
	sysbReq = detect_SYSB_keypress();
	if(sysbReq == 1) {
		if(getenv("Burn_In_Time") != NULL) {
			 /* Assuming that mfgsys is the only user of "Burn_In_Time" */
			setenv("bootcmd", "run wgBootSercomm");
		} else {
			setenv("bootcmd", "run wgBootSysB");
		}

		s = getenv ("bootcmd");
		printf("string = %s\n", s);
		run_command (s, 0);
	}
#endif /* endof CONFIG_M200 or CONFIG_M300 */

	autoboot_command(s);

	cli_loop();
}
