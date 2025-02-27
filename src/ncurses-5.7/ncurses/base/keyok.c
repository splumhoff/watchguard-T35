/****************************************************************************
 * Copyright (c) 1998-2000,2006 Free Software Foundation, Inc.              *
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
 *  Author: Thomas E. Dickey            1997-on                             *
 ****************************************************************************/

#include <curses.priv.h>

MODULE_ID("$Id$")

/*
 * Enable (or disable) ncurses' interpretation of a keycode by adding (or
 * removing) the corresponding 'tries' entry.
 *
 * Do this by storing a second tree of tries, which records the disabled keys. 
 * The simplest way to copy is to make a function that returns the string (with
 * nulls set to 0200), then use that to reinsert the string into the
 * corresponding tree.
 */

NCURSES_EXPORT(int)
keyok(int c, bool flag)
{
    int code = ERR;
    int count = 0;
    char *s;

    T((T_CALLED("keyok(%d,%d)"), c, flag));
    if (c >= 0) {
	unsigned ch = (unsigned) c;
	if (flag) {
	    while ((s = _nc_expand_try(SP->_key_ok, ch, &count, 0)) != 0
		   && _nc_remove_key(&(SP->_key_ok), ch)) {
		code = _nc_add_to_try(&(SP->_keytry), s, ch);
		free(s);
		count = 0;
		if (code != OK)
		    break;
	    }
	} else {
	    while ((s = _nc_expand_try(SP->_keytry, ch, &count, 0)) != 0
		   && _nc_remove_key(&(SP->_keytry), ch)) {
		code = _nc_add_to_try(&(SP->_key_ok), s, ch);
		free(s);
		count = 0;
		if (code != OK)
		    break;
	    }
	}
    }
    returnCode(code);
}
