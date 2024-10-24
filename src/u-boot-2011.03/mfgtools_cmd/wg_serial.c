/***************************************************************************
 * ----------- WG serial/ OEM Serial related functionality ----------------
 *
 * @file wg_serial.c
 *
 * @brief functionality for reading/writing WG/OEM serials, determining model
 *    info.
 *
 * @Original authors: Dan Wilder, Steve Huberty, Eivind Naess, Mukund Jampala
 * @refactor: Mukund Jampala <mukund.jampala@watchguard.com>
 * Copyright 2013, WatchGuard Technologies, Inc.
 * All Rights Reserved
 *
 * @date Wednesday Apr 10, 2013
 *
 * @par Function:
 *   int mfg_write_serial(const char *newserial);
 *   int mfg_read_serial(char *serial, int len);
 *   int mfg_valid_serial(const char *str);
 *
 ****************************************************************************/

#include <linux/stddef.h>

#include <common.h>
#include <linux/ctype.h>
#include <linux/err.h>

#include <errno.h>

#if 0
#include <fcntl.h> 
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include <wglinux.h>
#include <usertools/util.h>
#include <mfgtools/flash.h>
#endif

#include "mfgtools-private.h"
#include "crc16.h"

#if 0
#include "image-private.h"
#endif

#define FL_UNSIGNED   1
#define FL_NEG        2
#define FL_OVERFLOW   4
#define FL_READDIGIT  8

# define INT32_MIN		(-2147483647-1)
# define INT32_MAX		(2147483647)
# define UINT32_MAX		(4294967295U)
static unsigned long strtoxl(const char *nptr, char **endptr, int ibase, int flags) {
  const unsigned char *p;
  char c;
  unsigned long number;
  unsigned digval;
  unsigned long maxval;

  p = (const unsigned char *) nptr;
  number = 0;

  c = *p++;
  while (isspace(c)) c = *p++;

  if (c == '-') {
    flags |= FL_NEG;
    c = *p++;
  } else if (c == '+') {
    c = *p++;
  }

  if (ibase < 0 || ibase == 1 || ibase > 36) {
    if (endptr) *endptr = (char *) nptr;
    return 0L;
  } else if (ibase == 0) {
    if (c != '0') {
      ibase = 10;
    } else if (*p == 'x' || *p == 'X') {
      ibase = 16;
    } else {
      ibase = 8;
    }
  }

  if (ibase == 16) {
    if (c == '0' && (*p == 'x' || *p == 'X')) {
      ++p;
      c = *p++;
    }
  }

  maxval = UINT32_MAX / ibase;

  for (;;) {
    if (isdigit(c)) {
      digval = c - '0';
    } else if (isalpha(c)) {
      digval = toupper(c) - 'A' + 10;
    } else {
      break;
    }

    if (digval >= (unsigned) ibase) break;

    flags |= FL_READDIGIT;

    if (number < maxval || (number == maxval && (unsigned long) digval <= UINT32_MAX % ibase)) {
      number = number * ibase + digval;
    } else {
      flags |= FL_OVERFLOW;
    }

    c = *p++;
  }

  --p;

  if (!(flags & FL_READDIGIT)) {
    if (endptr) p = (const unsigned char *)nptr;
    number = 0;
  } else if ((flags & FL_OVERFLOW) || (!(flags & FL_UNSIGNED) && (((flags & FL_NEG) && (number < INT32_MIN)) || (!(flags & FL_NEG) && (number > INT32_MAX))))) {
#ifndef KERNEL
    errno = ERANGE;
#endif

    if (flags & FL_UNSIGNED) {
      number = UINT32_MAX;
    } else if (flags & FL_NEG) {
      number = INT32_MIN;
    } else {
      number = INT32_MAX;
    }
  }

  if (endptr) *endptr = (char *) p;

  if (flags & FL_NEG) number = (unsigned long) (-(long) number);

  return number;
}

unsigned long strtoul(const char *nptr, char **endptr, int ibase) {
  return strtoxl(nptr, endptr, ibase, FL_UNSIGNED);
}


#define MFR_MODEL_SERIAL_LENGTH 9
#define WGSERIAL_CRC_LEN	4

#if 0
/*!
 * \brief Writes the WatchGuard serial number and Returns the status code
          Validates the 4-digit checksum before writing WG serial to the Hardware.
 *
 * \param serial    [IN]   The OEM serial information
 * 
 * \return 0 on success, -1 or <> 0 otherwise.
 */
int mfg_write_serial( const char *newserial ) 
{
    int retval = (-1);
    int i=0,j=0;
    char serial_copy[SERIAL_LENGTH+1];

    if ( !newserial ) {
        errno = EINVAL;
        return retval;
    }

    /* strip any white spaces and '-' */
    while ( newserial[i] ) {
        if ( isalnum(newserial[i]) ) {
            serial_copy[j]=newserial[i];
            j++;
        }
        i++;
    }
    serial_copy[j]=0;

    if (mfg_valid_serial(serial_copy) == 0) {
        errno = EINVAL;
        return retval-1;
    }

    return platform_mfg_write_serial(serial_copy);

}

/*!
 * \brief Returns the WatchGuard serial number.
 *        Tip:  You can override the serial number stored in the
 *        manufacturing area by putting it in /etc/wg/serial.
 *
 * \param serial    [OUT]   The  WatchGuard serial information
 * \param len       [IN]    Maximum serial buffer capacity.
 * 
 * \return 0 on success, -1 or <> 0 otherwise returning serial as the default
 *    serial
 */
int mfg_read_serial(char *serial, int len) {

    int rc   = -1;
    FILE *fp = NULL;
    char *pa = NULL;

    /* Input validation */
    if ( !serial || len <= 0 )
        return rc;

    memset(serial, 0, len);

    rc = platform_mfg_read_serial(serial, len);

    if ( rc || !strcmp(serial, "UNKNOWN") ) {
        strncpy(serial, DEFAULT_SERIAL_STRING, len);
        syslog(LOG_INFO, "Using default serial number");
    }

    return rc;

}
#endif

/*!
 * \brief Validates a WG serial number against its checksum.
 *
 * \param str       [IN]    The serial number
 *
 * \note Even though the serial string currently looks like a hex
 *     representation of an integer, the checksum uses the ASCII value of the
 *     digits of the serial number.  So the decimal value of "A" is 65 not 10.
 *     Use CRC16 (CCITTish) to generate the checksum for the first 9 digits of
 *     the serial string.
 *
 * \note Supports 13-digit serial number strings whose last 4 digits are the
 *     checksum of the first 9 digits.  Also allows for a 14-digit string with
 *     a hyphen between the serial number and the checksum (e.g.
 *     909912345-ABCD).
 *
 * \return 1 if valid, 0 if otherwise
 * NOTE : Never delete this function as part of refactoring: spokane heavily
          depends on this.
 */
int mfg_valid_serial(const char *str)
{
    u_int16_t exp_crc;
    u_int16_t act_crc;
    int len;
    int i;

    /* validate the entire length, accounting for a possible hyphen */
    len = strlen(str);
    if ((len != MFR_MODEL_SERIAL_LENGTH + WGSERIAL_CRC_LEN)
            && (len != MFR_MODEL_SERIAL_LENGTH + WGSERIAL_CRC_LEN + 1)){
        return(0); 
    }

    exp_crc = strtoul(str + len - WGSERIAL_CRC_LEN, NULL, 16);

    act_crc = 0;
    for (i = 0; i < MFR_MODEL_SERIAL_LENGTH; ++i) {
        act_crc = mfg_serial_crc_byte(act_crc, (u_int8_t) str[i]);
    }

    if (exp_crc != act_crc) {
        return(0);
    }

    return(1);
}
