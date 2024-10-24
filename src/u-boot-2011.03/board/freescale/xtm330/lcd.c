/*
 * Mukund J
 * LCD display / LED controls / Keypad Code
 */

#include <common.h>
#include <asm/mmu.h>
#include <command.h>
#include <asm/immap_85xx.h>
#include <asm/io.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <version.h>

#include  "S6A0069-private.h"
#include  "serial.h"

DECLARE_GLOBAL_DATA_PTR;

extern int debug_level;

#ifdef DEBUG_XTM330_LCM

static int do_lcd_clear (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
        DbgPrint (BASIC_DBG, "DbgPrint: do_lcd_clear\n");

	ret = Run_Command (CMD_CLEAR_LCD, NULL, 0);
	return 0;
}

static int do_lcd_puts (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
	char *display_text;

	if (argc < 2)
		return cmd_usage(cmdtp);

	display_text = argv[1];
        DbgPrint (BASIC_DBG, "do_lcd_puts Invoked\n");
    	ret = Run_Command (CMD_DISP_LCD_LINE1, (UCHAR *) display_text, 0);
	return 0;
}

static int do_lcd_putc (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
	char *display_text;

	if (argc < 2)
		return cmd_usage(cmdtp);
	display_text = argv[1];

    	ret = Run_Command (CMD_DISP_LCD_LINE1, (UCHAR *) display_text, 0);
	puts("do_lcd_putc");
	return 0;
}

static int do_lcd_getc (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	char c = -1;

	if (lcm_serial_tstc())		/* we got a key press	*/
		c = lcm_serial_getc ();
	if (c != -1)
		DbgPrint (BASIC_DBG, "do_lcd_getc = %c (%d) \n", c, c);

	return 0;
}

static int do_detect_key (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	unsigned char key;
        while (1) {
                poll_keypad(&key);
		if ((key != 'X') && (key != 'Y')) {
                        printf( "<%s> Detected Valid Key Press, Key = %c\n", __FUNCTION__, key);
                }
        }

	return 0;
}

static int do_ping_flood (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int ret;

        DbgPrint (BASIC_DBG, "do_ping_flood Invoked\n");
    	ret = Run_Command (CMD_PING, (UCHAR *) "Prank", 0);

	return 0;
}


U_BOOT_CMD(
	lcd_cls, 1, 1, do_lcd_clear,
	"lcd clear display",
	""
);

U_BOOT_CMD(
	lcd_puts, 2, 1, do_lcd_puts,
	"display string on lcd",
	"<string> - <string> to be displayed"
);

U_BOOT_CMD(
	lcd_putc, 2, 1, do_lcd_putc,
	"display char on lcd",
	"<char> - <char> to be displayed"
);

U_BOOT_CMD(
	lcd_getc, 2, 1, do_lcd_getc,
	"fetch char on lcd",
	""
);

U_BOOT_CMD(
	detect_key, 2, 1, do_detect_key,
	"detect the Key press",
	""
);

U_BOOT_CMD(
	ping_flood, 2, 1, do_ping_flood,
	"Do the PING flood tets on the LCM",
	""
);

#endif /* end of DEBUG_XTM330_LCM */

int lcd_clear (void)
{
	int ret;
	ret = Run_Command (CMD_CLEAR_LCD, NULL, 0);
	return 0;
}

int lcd_puts (int line, char * text)
{
	int ret;
	char *display_text = text;

	if (!text)
		return -1;
	if (line == 0)
		 ret = Run_Command (CMD_DISP_LCD_LINE1, (UCHAR *) display_text, 0);
	else 
		 ret = Run_Command (CMD_DISP_LCD_LINE2, (UCHAR *) display_text, 0);

	return 0;
}

