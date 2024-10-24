/********************************************************
 * ----------- LCD lib for chelan ----------------
 *
 * @file panel-chelan.c
 *
 * @brief front panel handling and library routines for chelan
 *
 * @author: Mukund Jampala <mukund.jampala@watchguard.com>
 * Copyright 2011, WatchGuard Technologies, Inc.
 * All Rights Reserved
 *
 * @date Wednesday Mar 02, 2011
 *
 * @par Function:
 *  1. library functions for LCD init and configure
 *  2. Change Baud rates
 *  3. Fetch the firmware Revision
 *
 * Future expansions:
 *  - save boot state and serial numbers
 *
 ********************************************************/

/*
 * Something about the chip.
 * From the lanner documents, there is a SUMSUNG S6A0069 Controller is being initialized by lanner coltroller (VC2025).
 * VC2025 talks to the serial port for user commands and controlling part.
 *
 * The S6A0069 LCD controller is pretty close the well known LCD controller HD44780.
*/

#include <common.h>

#include  "S6A0069-private.h"
#include  "serial.h"

/* GLOBALS */
int debug_level = FULL_DBG;

/* Buffer management stuff */
volatile UCHAR	serial_buffer[READ_BUFFER_SIZE];
int	head;
int	tail;
int	tail_peek;

extern ulong sys_start; /* for keypad detect window MGMT */

void send_packet (CMD_PACKET * pCMD, UCHAR * pdata);

#define FACTORY_KEYPAD_TEST_COUNT 1

int wait_response (CMD_PACKET * pCMD);

	
int
setup_keypad(void)
{
    CMD_PACKET *pCMD = NULL;
    CMD_PACKET CMD, CMD_wr;
    int status = -1;
    UCHAR data[4];

    data[0] = (KP_S1+KP_S2+KP_S3+KP_S4);
    data[1] = (KP_S1+KP_S2+KP_S3+KP_S4);
    data[3] = '\0';

    /* prepare keypad setting */
    DbgPrint (FULL_DBG, " Setting the Keypad ...\n");

    /* Format the Packet */
    pCMD = format_packet (&CMD_wr, CMD_CONFIG_KEY_MODE, (UCHAR *) data, 2, 0);
    if (pCMD == NULL)
        return status;

    /* Write the comand to LCD Controller */
    if ((status = writeLCD (pCMD)) < 0)
        return status;
    if ((status = wait_response (&CMD))) {
        DbgPrint (FULL_DBG,  "Timed out waiting for a response.\n");
    } else {
        DbgPrint (FULL_DBG,  "Got some Packet ...\n");
    }
    DbgPrint (FULL_DBG,  "Send packet completed\n");

    return 0;
}

UCHAR
poll_keypad_response (CMD_PACKET * rCMD)
{
    CMD_PACKET *pCMD = NULL;
    CMD_PACKET CMD_wr;
    int status = -1;

    DbgPrint (FULL_DBG, " Reading the Keypad ...\n");

    /* Format the Packet */
    pCMD = format_packet (&CMD_wr, CMD_READ_KEY_STATUS, NULL, 0, 0);
    if (pCMD == NULL)
	return status;

    DbgPrint (BASIC_DBG, "write Command\n");
    /* Write the comand to LCD Controller */
    if ((status = writeLCD (pCMD)) < 0)
	return status;

    /* Wait for response */
    if ((status = wait_response (rCMD))) {
	 DbgPrint (BASIC_DBG, "Timed out waiting for a response.\n");
    } else {
	 DbgPrint (BASIC_DBG, "Got some Packet ...\n");
    }

    return status;
}

int poll_keypad(UCHAR *key)
{

    CMD_PACKET CMD;
    int status = 0;

    if(!poll_keypad_response(&CMD)) {
#ifdef TEST
	dump_packet (&CMD);
#endif
        switch( CMD.data[0] ) {
            case KP_S1:
                DbgPrint (BASIC_DBG, "KP_S1 PRESSED\n");
                *key = 'a';
	        break;
            case KP_S2:
	        DbgPrint (BASIC_DBG, "KP_S2 PRESSED\n");
                *key = 'b';
                break;
            case KP_S3:
                DbgPrint (BASIC_DBG, "KP_S3 PRESSED\n");
                *key = 'c';
                break;
            case KP_S4:
                DbgPrint (BASIC_DBG, "KP_S4 PRESSED\n");
                *key = 'd';
                break;
            case KP_S1+16:
                DbgPrint (BASIC_DBG, "KP_S1 RELEASED\n");
                *key = 'A';
                break;
            case KP_S2+16:
                DbgPrint (BASIC_DBG, "KP_S2 RELEASED\n");
                *key = 'B';
                break;
            case KP_S3+16:
                DbgPrint (BASIC_DBG, "KP_S3 RELEASED\n");
                *key = 'C';
                break;
            case KP_S4+16:
                DbgPrint (BASIC_DBG, "KP_S4 RELEASED\n");
                *key = 'D';
                break;
            default:
                DbgPrint (BASIC_DBG, "NO Key activity detected\n");
		*key = 'Y';
                break;
        }
	if (*key == 'Y')
        	status = 0;
	else
		status = 1;

    } else {

        DbgPrint (FULL_DBG, "Timed out waiting for a response.\n");
        status = 0;
    }

    return status;
}

/* boot time optimization: Make sure we dont wait more than this time */
#define OLD_CTRL_MAX_KEYPAD_TIMEOUT_MS 3900 /* OLD LCM module */ 
#define NEW_CTRL_MAX_KEYPAD_TIMEOUT_MS 2500 /* new LCM module */

#define MAX_PINGS 60

/*
 * @about: Mukund J
 *	This function is just supposed to detect the key and just that.
 *	But unfortunately, as the controller is not comming up for a 
 *	while after the boot, I have do do more. like ...
 *		So, it init the LCM when it detects a response from 
 *		the LCM controller for the first time (only).
 */
UCHAR detect_keypress(void)
{
	int i;
	UCHAR key;
	int ret;

	/* time mesurements */
	ulong keypad_timeo;
#ifdef TIMINGS_DEBUG
	ulong ping_detect_t = 0;
	ulong keypad_detect_t = 0, exit_t = 0;
#endif

	/* there is no point in KEY-PRESS detection event untill the controller is UP */
	for (i=0; i<=MAX_PINGS; i++) {
 		ret = Run_Command (CMD_PING, (UCHAR *) "T", 0);
		if (ret != -1 ) {
			DbgPrint (FULL_DBG, "PING response rec'd: b.out @ i = %d\n", i);
			/* reinit and turn on the display now and (only once) */
			lcm_panel_init();
#ifdef TIMINGS_DEBUG
			/* DEBUG: When NO key is pressed: sys_start = 84, ping_detect_t = 3508 */
			ping_detect_t = get_timer(sys_start);
			DbgPrint (FULL_DBG, "sys_start = %ld, ping_detect_t = %ld\n", sys_start, ping_detect_t);
#endif
			udelay(20000); /* assuming other BUGs reported are true,
					* so add 20 ms delay after lcm init'zed */
			break;
		}
		udelay(20000); /* 20 ms delay between each request */
	}

	if (i==MAX_PINGS) {
		printf ("ERROR: NO LCM response detected\n");
	}
 
	/*
	 * With the above PING command in place, by the time we to the below loop,
	 * we already have a GOOD responsive controller. So, all interations can
	 * be counted as good.
	 * We intened to exit the loop if no key is pressed or the MAX timeour set is
	 * reached.
	 */
	for(i=0; i<20; i++) {	/* MAX iterations == 20, timeout after that. */
		DbgPrint (FULL_DBG, "detect_keypress: i = %d\n", i);
		ret = poll_keypad(&key);
		if (ret) {
			/* poll keypad only if: (optimise boot time. litte BIT only)
			 * 1. (keypad_timeo < OLD_CTRL_MAX_KEYPAD_TIMEOUT_MS) ms timeout
			 * 2. (interation > 10): atleast give 10 iteration chance to the user
			 */
			keypad_timeo = get_timer(sys_start);
			if ((keypad_timeo > OLD_CTRL_MAX_KEYPAD_TIMEOUT_MS) && (i>10) )
				break;

			DbgPrint (FULL_DBG, "Key detected = %c, intr = %d\n",
				key, i);
#ifdef TIMINGS_DEBUG
			/* DEBUG: VALID KEY detected: sys_start = 84, keypad_detect_t = 3900 */
			keypad_detect_t = get_timer(sys_start);
			DbgPrint (FULL_DBG, "VALID KEY detected: sys_start = %ld, \
			keypad_detect_t = %ld\n", sys_start, keypad_detect_t);
#endif
			return key;
		}
		udelay(1200); /* 1.2 ms delay if not key being pressed, wait a bit between each try */
	}

	/* Display WG logo only when the key is NOT pressed or MAX timeout reached */
	printf ("WatchGuard\n");
	/* Finally, Display the Watchguard logo if we get here */
	lcd_clear();
	udelay(12000); /* delay needed between clear and display */
	lcd_puts (0, "    WatchGuard");

#ifdef TIMINGS_DEBUG
	/* DEBUG: When NO key is pressed: total intr=14: sys_start = 84, exit_t = 4259 */
	exit_t = get_timer(sys_start);
	DbgPrint (FULL_DBG, "total intr=%d: sys_start = %ld, exit_t= %ld\n", \
		sys_start, exit_t);
#endif	
	return key;
}

int
my_write ( char *str, int len)
{
	lcm_serial_puts (str, len);
	return 0;
}

#define READ_OVERBUFFER 10
int
my_read ( UCHAR *str, int size)
{
	int good = 0, bad = 0;
	int i=0;
	if (!str)
		return -1;

	/* for (i=0; i< size; ++i) { */
	for (i=0; i< (sizeof(CMD_PACKET)); ++i) {
		if (lcm_serial_tstc()) {	/* we got a key press	*/
			str[good] = lcm_serial_getc ();
			good++;
		} else {
			bad++;
		}
		if (bad)
			goto exit;
	}
exit:
	str[good] = '\0'; // append with a NULL char {string terminator}
	return good;
}

int
writeLCD (const void *cmd)
{
    int ret = -1;
    CMD_PACKET *cmdt = (CMD_PACKET *) cmd;
#ifdef LCM_IO_DEBUG
    unsigned char *data = (unsigned char *) cmd;
    int i;
#endif

    DbgPrint (BASIC_DBG, "Send length = %d\n", cmdt->data_length + CMD_SIZE);
#ifdef LCM_IO_DEBUG
    printf (" === Packet writeLCD <len=%d> === \n", (cmdt->data_length + CMD_SIZE));
    for (i = 0; i < (cmdt->data_length + CMD_SIZE); i++) {
	printf ("%2.2x ", *(data + i));
        if (!(i % 8))
           printf ("\n");
    }
    printf ("\n");
#endif

    // u-boot HACK, fix it
    // ret = my_write ((char *)cmd, (cmdt->data_length) + CMD_SIZE);
    ret = my_write ((char *)cmd, (cmdt->data_length) + CMD_SIZE);
    if (ret == -1)
        DbgPrint (BASIC_DBG, "ERROR writing to the serial port\n");

    DbgPrint (BASIC_DBG, "write-ret = %d\n", ret);

    DbgPrint (BASIC_DBG, "write packet\n");
    DbgPrint (BASIC_DBG, "write ret = %d\n", ret);
    DbgPrint (BASIC_DBG, "CMD: command =    %d\n", cmdt->command);
    DbgPrint (BASIC_DBG, "CMD: data_len =   %d\n", cmdt->data_length);
    DbgPrint (BASIC_DBG, "CMD: data =       %s\n", cmdt->data);

    return ret;
}

int
readLCD (void)
{
    UCHAR buffer[READ_BUFFER_SIZE];
    int BytesRead = 0, i;

    BytesRead = my_read (buffer, READ_BUFFER_SIZE);
    if (!BytesRead)
    	return BytesRead;
    else if (BytesRead != -1)
        DbgPrint (FULL_DBG, "read bytes = %d\n\n", BytesRead);

#ifdef LCM_IO_DEBUG
    printf (" === Packet readLCD <len=%d> === \n", BytesRead);
#endif

    for (i = 0; i < BytesRead; i++) {
#ifdef LCM_IO_DEBUG
        printf ("0x%x ", buffer[i]);
        if (!(i % 8))
           printf ("\n");
#endif
        serial_buffer[head] = buffer[i];

        /* track the HEAD */
        head++;
        if (READ_BUFFER_SIZE <= head)
            head=0;
    }

    // DbgPrint (FULL_DBG, "read packet\n");
    // DbgPrint (FULL_DBG, "CMD: command =    %d\n", cmd->command);
    // DbgPrint (FULL_DBG, "CMD: data_len =   %d\n", cmd->data_length);
    // DbgPrint (FULL_DBG, "CMD: data =       %s\n", cmd->data);

    return BytesRead;

}

/* 
 * returns the number of unprocessed bytes in the global buffer.
*/
int bytes_avail(void)
{
	int l_head;
  	int ret;
  
  	l_head = head;
  	if((ret = (l_head - (int)tail)) < 0)
    		ret += READ_BUFFER_SIZE;

  	return(ret);
}


UCHAR fetch_byte(void)
{
    UCHAR ret = 0;

    int l_tail = tail;
    int l_head = head;

    /* See if there are any more bytes available. */
    if (l_tail != l_head) {

        /* There is at least one more ubyte. */
        ret = serial_buffer[l_tail];

        /* Increment the pointer and wrap if needed. */
        l_tail ++;
        if(READ_BUFFER_SIZE <= l_tail)
            l_tail = 0;

        tail=l_tail;
    }
    return(ret);
}


int peek_bytes_avail(void)
{
    int	l_head;
    int ret;
  
    l_head = head;
    if((ret = (l_head -(int)tail_peek) ) < 0)
        ret += READ_BUFFER_SIZE;

    return(ret);
}


void sync_peek_pointer(void)
{
    tail_peek = tail;
}


void accept_peeked_data(void)
{
    tail = tail_peek;
}


UCHAR peek_byte(void)
{
    int l_tail_peek;
    int l_head;
    UCHAR ret = 0;

    l_tail_peek = tail_peek;
    l_head = head;
  
    /* See if there are any more bytes available. */
    if (l_tail_peek != l_head) {

    /* There is at least one more ubyte. */
    ret = serial_buffer[l_tail_peek];

    /* Increment the pointer and wrap if needed. */
    l_tail_peek ++;
    if( READ_BUFFER_SIZE <= l_tail_peek)
        l_tail_peek = 0;

    tail_peek = l_tail_peek;

    }

    return(ret);
}

/*
 * wait_response: wait response from LCM-serial module
 *  input: 
 * 	none
 *
 * output: 
 *	0     : mean no response from LCM-serial module
 *	other : mean got correct response
 */
int
wait_response (CMD_PACKET * pCMD)
{
    volatile int k;
    int timed_out = -1;

    for (k = 0; k <= 1000000; k++) {
	if (packet_available (pCMD)) {
	     // DbgPrint (FULL_DBG,"package data ****AVAILABLE****\n");
	     return 0;
	} else {
	     // DbgPrint (FULL_DBG,"****** NO DATA ******\n");
	}
    }

    return timed_out;
}

void
clear_display (void)
{
    // CMD_PACKET *cmd;
    int ret = -1;
    ret = Run_Command (CMD_CLEAR_LCD, NULL, 0);
}

char *
get_firmware_version (void)
{
    CMD_PACKET *cmd;
    CMD_PACKET CMD_wr;
    int status;

    DbgPrint (FULL_DBG, "<%s>\n", __FUNCTION__);
    cmd = format_packet (&CMD_wr, CMD_GET_VERSION, NULL, 0, 0);	/* format for Write IO */
    if (cmd == NULL)
	return NULL;
    if (writeLCD (cmd) < 0)	/* Write the comand to LCD Controller */
	return NULL;

    if ((status = wait_response (cmd))) {

        DbgPrint (FULL_DBG,  "Timed out waiting for a response.\n");
    } else {
        DbgPrint (FULL_DBG,  "Got some Packet ...\n");
    }

    return NULL;		/* TODO: should be returning valid dat ptr instread */
}

/*
 * probe_lcm: try to detect lcm-serial module
 * input: 
 *	none
 * output: 
 *	-1    : mean no lcm-serial modules be detected
 *	other : mean device number
 */
int
cmd_processor (UCHAR cmd, UCHAR * data, int len)
{
    CMD_PACKET *pCMD = NULL;
    CMD_PACKET CMD, CMD_wr;
    int data_len = len;
    int status = -1;

     if ((!len) && (data != NULL)) {  /* SET data & data_len */
	    data_len = strlen ((const char *) data);
    }

    /* Format the Packet */
    pCMD = format_packet (&CMD_wr, cmd, data, data_len, 0);
    if (pCMD == NULL)
	return status;

    DbgPrint (BASIC_DBG, "writeLCD being called\n");
    /* Write the comand to LCD Controller */
    if ((status = writeLCD (pCMD)) < 0)
	return status;
    DbgPrint (BASIC_DBG, "Send packet completed\n");

    /* Wait for response */
    if ((status = wait_response (&CMD)))
     {
	 DbgPrint (BASIC_DBG, "Timed out waiting for a response.\n");
     }
    else
     {
	 DbgPrint (BASIC_DBG, "Got some Packet ...\n");
     }

    return status;
}

/*
 * Run_Command:
 *   run the command with this data and assicated data_len.
 */
int
Run_Command (UCHAR command, UCHAR * data, int len)
{
    int invalid_cmd = 0;

    switch (command)
     {
     case CMD_PING:
	 DbgPrint (BASIC_DBG, "COMMAND 0 :PING\n");
	 break;
     case CMD_GET_VERSION:
	 DbgPrint (BASIC_DBG, "COMMAND 1 :Get Firmware Version\n");
	 break;
     case CMD_WRITE_USER_FLASH:
	 DbgPrint (BASIC_DBG, "COMMAND 2 :Write User Flash\n");
	 break;
     case CMD_READ_USER_FLASH:
	 DbgPrint (BASIC_DBG, "COMMAND 3 :Read User Flash\n");
	 break;
     case CMD_STORE_SETTING:
	 DbgPrint (BASIC_DBG, "COMMAND 4 :Store Setting\n");
	 break;
     case CMD_RESET_LCM_MODULE:
	 DbgPrint (BASIC_DBG, "COMMAND 5 :Reset LCM-serial Modules\n");
	 break;
     case CMD_CLEAR_LCD:
	 DbgPrint (BASIC_DBG, "COMMAND 6 :Clear LCM Screen\n");
	 break;
     case CMD_DISP_LCD_LINE1:
	 DbgPrint (BASIC_DBG, "COMMAND 7 :Display LCM Line 1\n");
	 break;
     case CMD_DISP_LCD_LINE2:
	 DbgPrint (BASIC_DBG, "COMMAND 8 :Display LCM Line 2\n");
	 break;
     case CMD_SET_CURSOR_POSITION:
	 DbgPrint (BASIC_DBG, "COMMAND 11 :Set Cursor Position\n");
	 break;
     case CMD_SET_CURSOR_STYLE:
	 DbgPrint (BASIC_DBG, "COMMAND 12 :Set Cursor Style\n");
	 break;
     case CMD_SEND_CMD_TO_LCD:
	 DbgPrint (BASIC_DBG,
		   "COMMAND 22 :Send Command PassThrough To LCM\n");
	 break;
     case CMD_CONFIG_KEY_MODE:
	 DbgPrint (BASIC_DBG, "COMMAND 23 :Config Key Mode\n");
	 break;
     case CMD_READ_KEY_STATUS:
	 DbgPrint (BASIC_DBG, "COMMAND 24 :Read Key Status\n");
	 break;
     case CMD_DISP_LCD_ANY_LOCATE:
	 DbgPrint (BASIC_DBG, "COMMAND 31 :Display On Any Location\n");
	 break;
     case CMD_SET_LED_STATUS:
	 DbgPrint (BASIC_DBG, "COMMAND 32 :Set LED Status/LCD backlight\n");
	 break;
     case CMD_SET_BAUDRATE:
	 DbgPrint (BASIC_DBG, "COMMAND 33 :Set Baudrate\n");
	 break;
     default:
	 invalid_cmd = -1;
	 DbgPrint (BASIC_DBG, "Invalid Command\n");
	 break;
     }

    /* Process only if it is known command */
    if (invalid_cmd != -1)
	return cmd_processor (command, data, len);

    return invalid_cmd;
}



int
Ser_init (char *devname, int bitrate)
{
	return 0;
}
void
Ser_deinit (char *devname)
{

}

int
set_led (LED_COLOR_t color, int rate)
{
    	UCHAR data[8]; /* = {1, 100, 1,}; LED packet */
	int ret;
 
	printf ("color = %d, rate = %d\n", color, rate);

	if(color < S6A_PLAIN_LED || color > S6A_RED_LED )
		return -1;
	if ( (rate < S6A_LED_SOLID_OFF) || (rate > S6A_LED_SOLID_ON))
		return -2;

	data[0] = (UCHAR) color; /* color */
	data[1] = (UCHAR) 100;	 /* duration ?? */
	data[2] = (UCHAR) rate;	 /* duration ?? */
	data[3] = (UCHAR) '\0';	 /* string terminator */

	ret = Run_Command (CMD_SET_LED_STATUS, data, 3);
	return ret;
}

int
set_baudrate (int rate)
{
    	UCHAR data[8]; /* = {1, 100, 1,}; LED packet */
	int ret;
 
	data[0] = (UCHAR) rate;			/* change to preset U-boot baud rate */
	data[1] = '\0';  			/* string terminator */
	ret = Run_Command (CMD_SET_BAUDRATE, (UCHAR *) data, 1);
	return ret;
}

