
#ifndef __PANEL_CHELAN_H__
#define __PANEL_CHELAN_H__

#include <stdarg.h>
#include <malloc.h>
#include <linux/ctype.h>
#include <common.h>

#if 0
#define LCM_IO_DEBUG
#define LCM_DEBUG
#define DEBUG_LCM
#endif

/*
 * MACROS
 */
typedef unsigned char UCHAR;
typedef unsigned short USHORT;

#define DRIVER_API_VERSION  "0.01"

#define STREQ(a,b) (strcmp((a),(b)) == 0)
#define STRPREFIX(a,b) (strncmp((a),(b),strlen((b))) == 0)
#define FREE(a) free(a)

#define NO_DBG    0
#define BASIC_DBG 1
#define FULL_DBG  2

#ifdef DEBUG_LCM

#define DbgPrint(lvl, fmt, args...) \
 if (debug_level >= lvl ) \
  do { printf("DBG: " fmt , ##args); } while (0)
#define ErrPrint(fmt, args...) \
 do { printf("ERR: " fmt , ##args); } while (0)
#define SysErrPrint(errno, fmt, args...) \
 do { printf("SYSERR: " fmt , ##args); } while (0)

#else /* else DEBUG_LCM */

#define DbgPrint(lvl, fmt, args...)
#define ErrPrint(fmt, args...)
#define SysErrPrint(errno, fmt, args...)

#endif /* end DEBUG_LCM */


/* 
 * ALL #defines
 */
#define MAX_COMMAND		35
#define MAX_DATA_LENGTH 	25	/* Data lenght for each PACKET */
#define READ_BUFFER_SIZE 	2048

/* 
 * Device port settings
 */
#define DEV_LCM "/dev/ttyS1"
#define DEFAULT_BAUDRATE 19200

/* 
 * Software version info  
 */
#define SW_VERSION       "frontpanel-chelan"

/* All LCD commands */
enum lcd_commands {

    FETCH_LCD_VERSION = 0,
    CLEAR_LCD = 1,
    SET_LINE1 = 2,
    SET_LINE2 = 3,
    DISP_LCD_ANY_LOC_1 = 4,
    DISP_LCD_ANY_LOC_2 = 5,
    DISP_LCD_ANY_LOC_3 = 6,
    DISP_LCD_ANY_LOC_4 = 7,
    SET_LCD_STATUS_1 = 8,
    SET_LCD_STATUS_2 = 9,
    SET_LCD_STATUS_3 = 10,
    SET_LCD_STATUS_4 = 11,
    SET_CURSOR_STYLE = 12,
    SET_CURSOR_POS = 13,
    RESET_LCM = 14,
    RUN_PING = 15,
} LCMCMD;

/*
 * CMD packet strccture
 */
typedef struct {
    UCHAR command;
    UCHAR data_length;
    UCHAR data[MAX_DATA_LENGTH];
    union {
	UCHAR in_bytes[2];
	USHORT in_word;
    } CRC;

    /* Command processing function for this perticular 'command' */
    int (*cmd_processor) (int, char *data, int data_len);
    UCHAR data_remaining;


} CMD_PACKET;
#define CMD_SIZE 4
#define CMD_PACKET_SIZE (sizeof(CMD_PACKET) - sizeof(int *))

/* 
 * LCD packet command definition
 */

#define CMD_PING		0

#define CMD_GET_VERSION		1

#define CMD_WRITE_USER_FLASH	2

#define	CMD_READ_USER_FLASH	3

#define CMD_STORE_SETTING	4

#define	CMD_RESET_LCM_MODULE	5

#define	CMD_CLEAR_LCD		6

#define	CMD_DISP_LCD_LINE1	7

#define	CMD_DISP_LCD_LINE2	8

#define	CMD_SET_CURSOR_POSITION	11

#define	CMD_SET_CURSOR_STYLE	12

#define	CMD_SEND_CMD_TO_LCD	22

#define CMD_CONFIG_KEY_MODE	23

#define	CMD_READ_KEY_STATUS	24

#define	CMD_DISP_LCD_ANY_LOCATE	31

#define	CMD_SET_LED_STATUS	32

#define	CMD_SET_BAUDRATE	33

/* 
 * keypad definition
 */
#define KP_S1   0x01
#define KP_S2   0x02
#define KP_S3   0x04
#define KP_S4   0x08

/*
 * LED Color Enums & Rate Enums
*/
typedef enum {
        S6A_PLAIN_LED = 0,
        S6A_GREEN_LED = 1,
        S6A_RED_LED   = 2,
} LED_COLOR_t;

/*  LED blink rates */
#define BLINK_RATE_1SEC 1

#define S6A_LED_SOLID_OFF       0
#define S6A_LED_BLINK_ON        1
#define S6A_LED_SOLID_ON        1       // ??

/* BAUD rates as per the controller */
#define DATA_BAUDRATE_19200     0                                                      
#define DATA_BAUDRATE_115200    1                                                      
#define DATA_BAUDRATE_28800     2                                                      
#define DATA_BAUDRATE_38400     3                                                      
#define DATA_BAUDRATE_57600     4                                                      
#define DATA_BAUDRATE_9600      5

/*************************
 ** Internal functions ***
 ************************/
/* 
 * LCD controlling functions
 */
int setup_keypad(void);
int Ser_init (char *devname, int bitrate);
void Ser_deinit (char *devname);

int set_baudrate (int rate);
int poll_keypad(UCHAR *key);

int probe_lcm (void);
char *get_firmware_version (void);
void show_version (void);
int writeLCD (const void *cmd);
int readLCD (void);
void show_version (void);


void clear_display(void);
int set_led (LED_COLOR_t color, int rate);

int Run_Command (UCHAR command, UCHAR * data, int len);
void dump_packet (CMD_PACKET * pCMD);
/* 
 * Packet handling functions
 */
CMD_PACKET *format_packet (CMD_PACKET *cmd, UCHAR CMD, UCHAR * data, 
		UCHAR data_length, USHORT CRC);
int packet_available (CMD_PACKET * packet);


#endif /* eof __PANEL_CHELAN_H__ */
