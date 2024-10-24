/* nar7090.c - Portwell NAR-7090 board support */
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

#ifdef SUPPORT_SERIAL

#include <nar7090.h>
#include <panel.h>


/*
 * Conditional on GRUB_UTIL because this stuff doesn't need to be a part of the
 * stand-alone ``grub`` utility program.
 */
#ifndef GRUB_UTIL


static unsigned short ezio_port = 0;

/* Initialize the front panel */
void
ezio_init (void)
{
  unsigned short orig_port;
  ezio_port = serial_hw_get_port (EZIO_PORT_INDEX);

  orig_port = serial_get_consoleport ();
  serial_hw_init (ezio_port, EZIO_PORT_SPEED, EZIO_PORT_WORDLEN,
      EZIO_PORT_PARITY, EZIO_PORT_STOPBITS);
  /*
   * reset the default serial port because serial_hw_init leaves it as
   * ezio_port and that'll never do.
   */
  serial_set_consoleport (orig_port);

  /*
   * According to the NAR-7090 manual, you have to send the init command
   * twice before it believes you.
   * */
  ezio_cmd (CMD_INIT);
  ezio_cmd (CMD_INIT);

  /*
   * Not sure why we need to pass this, but it's in the reference source
   * code, and it doesn't work without it.
   */
  ezio_cmd (CMD_SETDIS);
}

/* Send an EZIO command */
void
ezio_cmd (unsigned char command)
{
  ezio_hw_putchar (CMD_PREFIX);
  ezio_hw_putchar (command);
}

/* Get a char. */
unsigned char
ezio_hw_getchar (void)
{
  if (inb (ezio_port + UART_LSR) & UART_DATA_READY)
    {
      /* grab the EZIO_READKEY_HDR */
      inb (ezio_port + UART_RX);
      /* return the payload */
      return inb (ezio_port + UART_RX);
    }

  /* A real EZIO panel is normally high, and won't return 0 */
  return 0;
}

/* Put a char. */
void
ezio_hw_putchar (char c)
{
  int timeout = 100000;

  /* Wait until the transmitter holding register is empty.  */
  while ((inb (ezio_port + UART_LSR) & UART_EMPTY_TRANSMITTER) == 0)
    {
      if (--timeout == 0)
        {
          /* There is something wrong. But what can I do?  */
          return;
        }
    }

  outb (ezio_port + UART_TX, c);
}

/* Write a string to the display */
void
ezio_putchars (char *message)
{
    char c;
    while ((c = *(message++)))
      {
        ezio_hw_putchar (c);
      }
}

/* Find out what buttons are pressed. */
unsigned char
ezio_getbuttons (void)
{
  unsigned char value;
  ezio_cmd (CMD_READKEY);
  value = ezio_hw_getchar ();
  if (value & BTN_HIGH_NIBBLE)
    {
      /*
       * the front panel uses reverse logic, so negate on the way out,
       * plus we only care about the low nibble
       */
      return (~value) & 0xf;
    }
  return 0;
}

/* Sample LCD display */
void
ezio_sample (void)
{
  ezio_cmd (CMD_CLS);
  ezio_putchars ("Hey Everyone");
}

#endif /* GRUB_UTIL */

#endif /* SUPPORT_SERIAL */
