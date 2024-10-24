/*
 * Serial port rsetup for LCM controller
 */

#include <common.h>
#include <asm/u-boot.h>
#include <asm/processor.h>
#include <command.h>

#if (defined CONFIG_SYS_NS16550_COM2)
#include <ns16550.h>
#endif

#include  "S6A0069-private.h"

DECLARE_GLOBAL_DATA_PTR;

extern int debug_level;

#if (defined CONFIG_SYS_NS16550_COM2)
const NS16550_t LCM_COM_PORT = (NS16550_t) CONFIG_SYS_NS16550_COM2;
#endif

void lcm_serial_putc (const char c)
{
	NS16550_putc (LCM_COM_PORT, c);
}

int
lcm_serial_getc (void)
{
        // DbgPrint (BASIC_DBG, "lcm_serial_getc Invoked\n");
	udelay(50);
	return NS16550_getc (LCM_COM_PORT);

}

int
lcm_serial_tstc (void)
{
	return NS16550_tstc (LCM_COM_PORT);
}

#define LCM_BAUDRATE 19200

void lcm_serial_setbrg (void)
{
#ifdef CONFIG_APTIX
#define MODE_X_DIV 13
#else
#define MODE_X_DIV 16
#endif
	int clock_divisor = (CONFIG_SYS_NS16550_CLK + (LCM_BAUDRATE * (MODE_X_DIV / 2))) /
				(MODE_X_DIV * LCM_BAUDRATE);

#if (defined CONFIG_SYS_NS16550_COM2)
        DbgPrint (BASIC_DBG, "lcm_serial_setbrg: in ifdef, GOOD\n");
	NS16550_reinit (LCM_COM_PORT, clock_divisor);
#endif
}

void lcm_serial_puts (const char *s, int len)
{
	int i;

	for (i=0; i<len; i++) {
		lcm_serial_putc (*s++);
	}
}

int lcm_panel_init(void)
{
	/* setup the serial port COM2 */
	lcm_serial_setbrg ();
	udelay(10000);

	/* set LCM controller BAUDRATE */
	set_baudrate (DATA_BAUDRATE_19200);

	/* setup Keypad */
	setup_keypad();

	return 0;
}

