/****************************************************************************
 * Copyright (c) 1998-2001,2006 Free Software Foundation, Inc.              *
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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_hline.c
**
**	The routine whline().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id$")

NCURSES_EXPORT(int)
whline(WINDOW *win, chtype ch, int n)
{
    int code = ERR;
    NCURSES_SIZE_T start;
    NCURSES_SIZE_T end;

    T((T_CALLED("whline(%p,%s,%d)"), win, _tracechtype(ch), n));

    if (win) {
	struct ldat *line = &(win->_line[win->_cury]);
	NCURSES_CH_T wch;

	start = win->_curx;
	end = start + n - 1;
	if (end > win->_maxx)
	    end = win->_maxx;

	CHANGED_RANGE(line, start, end);

	if (ch == 0)
	    SetChar2(wch, ACS_HLINE);
	else
	    SetChar2(wch, ch);
	wch = _nc_render(win, wch);

	while (end >= start) {
	    line->text[end] = wch;
	    end--;
	}

	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
