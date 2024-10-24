/* nar7090.h - Portwell NAR-7090 board support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002  Free Software Foundation, Inc.
 *  Copyright (C) 2008  WatchGuard Technologies, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* This header depends on macros in "serial.h" */
#include <serial.h>

/*
 * The NAR-7090 has the front panel LCD and control buttons hooked up to
 * ttyS1, a.k.a. its EZIO port.
 */
#define EZIO_PORT_INDEX     1
#define EZIO_PORT_SPEED     2400
#define EZIO_PORT_WORDLEN   UART_8BITS_WORD
#define EZIO_PORT_PARITY    UART_NO_PARITY
#define EZIO_PORT_STOPBITS  UART_1_STOP_BIT


/*
 * Each front panel button-press is prefixed by the following magic byte.
 */
#define EZIO_READKEY_HDR  0xFD


/* EZIO command values */
#define CMD_PREFIX            0xFE
#define CMD_INIT              0x28
#define CMD_END               0x37
#define CMD_CLS               0x01
#define CMD_HOME              0x02
#define CMD_READKEY           0x06
#define CMD_HIDE_CHARS        0x08
#define CMD_SHOW_CHARS        0x0C
#define CMD_CURSOR_BLINK      0x0D
#define CMD_CURSOR_UNDERLINE  0x0E
#define CMD_MOVE_LEFT         0x10
#define CMD_MOVE_RIGHT        0x14
#define CMD_SCROLL_LEFT       0x18
#define CMD_SCROLL_RIGHT      0x1C
#define CMD_SETPOS            0x80
#define CMD_SETDIS            0x40

#define LCD_ROW2_OFFSET  0x40

#define BTN_HIGH_NIBBLE  0xB0
/*
 * The NAR-7090 manual uses the following low nibbles to describe the
 * button values:
 *
 *     BTN_UP      0xE
 *     BTN_DOWN    0xD
 *     BTN_ENTER   0xB
 *     BTN_ESCAPE  0x7
 *
 * Basically, when a bit is zero/low/off, it means that button is
 * depressed.  To make it more intuitive in the code, we negate and use
 * the mask values defined below.
 */

void ezio_init (void);
void ezio_cmd (unsigned char command);
unsigned char ezio_hw_getchar (void);
void ezio_hw_putchar (char c);
void ezio_putchars (char *message);
unsigned char ezio_getbuttons (void);
void ezio_sample (void);
