/****************************************************************************
 * Copyright (c) 1999-2007,2008 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey                    1999-on                     *
 ****************************************************************************/

#include <curses.priv.h>
#include <term.h>
#include <tic.h>

MODULE_ID("$Id$")

/*
 *	_nc_first_name(char *names)
 *
 *	Extract the primary name from a compiled entry.
 */
#define FirstName _nc_globals.first_name

NCURSES_EXPORT(char *)
_nc_first_name(const char *const sp)
/* get the first name from the given name list */
{
    unsigned n;

#if NO_LEAKS
    if (sp == 0) {
	if (FirstName != 0)
	    FreeAndNull(FirstName);
    } else
#endif
    {
	if (FirstName == 0)
	    FirstName = typeMalloc(char, MAX_NAME_SIZE + 1);

	if (FirstName != 0) {
	    for (n = 0; n < MAX_NAME_SIZE; n++) {
		if ((FirstName[n] = sp[n]) == '\0'
		    || (FirstName[n] == '|'))
		    break;
	    }
	    FirstName[n] = '\0';
	}
    }
    return (FirstName);
}

/*
 *	int _nc_name_match(namelist, name, delim)
 *
 *	Is the given name matched in namelist?
 */

NCURSES_EXPORT(int)
_nc_name_match(const char *const namelst, const char *const name, const char *const delim)
{
    const char *s, *d, *t;
    int code, found;

    if ((s = namelst) != 0) {
	while (*s != '\0') {
	    for (d = name; *d != '\0'; d++) {
		if (*s != *d)
		    break;
		s++;
	    }
	    found = FALSE;
	    for (code = TRUE; *s != '\0'; code = FALSE, s++) {
		for (t = delim; *t != '\0'; t++) {
		    if (*s == *t) {
			found = TRUE;
			break;
		    }
		}
		if (found)
		    break;
	    }
	    if (code && *d == '\0')
		return code;
	    if (*s++ == 0)
		break;
	}
    }
    return FALSE;
}
