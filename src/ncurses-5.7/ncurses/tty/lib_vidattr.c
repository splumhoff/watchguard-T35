/****************************************************************************
 * Copyright (c) 1998-2006,2007 Free Software Foundation, Inc.              *
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
 *     and: Thomas E. Dickey                        1996 on                 *
 ****************************************************************************/

/*
 *	vidputs(newmode, outc)
 *
 *	newmode is taken to be the logical 'or' of the symbols in curses.h
 *	representing graphic renditions.  The terminal is set to be in all of
 *	the given modes, if possible.
 *
 *	if the new attribute is normal
 *		if exit-alt-char-set exists
 *			emit it
 *		emit exit-attribute-mode
 *	else if set-attributes exists
 *		use it to set exactly what you want
 *	else
 *		if exit-attribute-mode exists
 *			turn off everything
 *		else
 *			turn off those which can be turned off and aren't in
 *			newmode.
 *		turn on each mode which should be on and isn't, one by one
 *
 *	NOTE that this algorithm won't achieve the desired mix of attributes
 *	in some cases, but those are probably just those cases in which it is
 *	actually impossible, anyway, so...
 *
 * 	NOTE that we cannot assume that there's no interaction between color
 *	and other attribute resets.  So each time we reset color (or other
 *	attributes) we'll have to be prepared to restore the other.
 */

#include <curses.priv.h>
#include <term.h>

MODULE_ID("$Id$")

#define doPut(mode) TPUTS_TRACE(#mode); tputs(mode, 1, outc)

#define TurnOn(mask,mode) \
	if ((turn_on & mask) && mode) { doPut(mode); }

#define TurnOff(mask,mode) \
	if ((turn_off & mask) && mode) { doPut(mode); turn_off &= ~mask; }

	/* if there is no current screen, assume we *can* do color */
#define SetColorsIf(why,old_attr) \
	if (can_color && (why)) { \
		int old_pair = PAIR_NUMBER(old_attr); \
		TR(TRACE_ATTRS, ("old pair = %d -- new pair = %d", old_pair, pair)); \
		if ((pair != old_pair) \
		 || (fix_pair0 && (pair == 0)) \
		 || (reverse ^ ((old_attr & A_REVERSE) != 0))) { \
			_nc_do_color(old_pair, pair, reverse, outc); \
		} \
	}

#define PreviousAttr _nc_prescreen.previous_attr

NCURSES_EXPORT(int)
vidputs(chtype newmode, int (*outc) (int))
{
    attr_t turn_on, turn_off;
    int pair;
    bool reverse = FALSE;
    bool can_color = (SP == 0 || SP->_coloron);
#if NCURSES_EXT_FUNCS
    bool fix_pair0 = (SP != 0 && SP->_coloron && !SP->_default_color);
#else
#define fix_pair0 FALSE
#endif

    newmode &= A_ATTRIBUTES;
    T((T_CALLED("vidputs(%s)"), _traceattr(newmode)));

    /* this allows us to go on whether or not newterm() has been called */
    if (SP)
	PreviousAttr = AttrOf(SCREEN_ATTRS(SP));

    TR(TRACE_ATTRS, ("previous attribute was %s", _traceattr(PreviousAttr)));

    if ((SP != 0)
	&& (magic_cookie_glitch > 0)) {
#if USE_XMC_SUPPORT
	static const chtype table[] =
	{
	    A_STANDOUT,
	    A_UNDERLINE,
	    A_REVERSE,
	    A_BLINK,
	    A_DIM,
	    A_BOLD,
	    A_INVIS,
	    A_PROTECT,
	};
	unsigned n;
	int used = 0;
	int limit = (max_attributes <= 0) ? 1 : max_attributes;
	chtype retain = 0;

	/*
	 * Limit the number of attribute bits set in the newmode according to
	 * the terminfo max_attributes value.
	 */
	for (n = 0; n < SIZEOF(table); ++n) {
	    if ((table[n] & SP->_ok_attributes) == 0) {
		newmode &= ~table[n];
	    } else if ((table[n] & newmode) != 0) {
		if (used++ >= limit) {
		    newmode &= ~table[n];
		    if (newmode == retain)
			break;
		} else {
		    retain = newmode;
		}
	    }
	}
#else
	newmode &= ~(SP->_xmc_suppress);
#endif
	TR(TRACE_ATTRS, ("suppressed attribute is %s", _traceattr(newmode)));
    }

    /*
     * If we have a terminal that cannot combine color with video
     * attributes, use the colors in preference.
     */
    if (((newmode & A_COLOR) != 0
	 || fix_pair0)
	&& (no_color_video > 0)) {
	/*
	 * If we had chosen the A_xxx definitions to correspond to the
	 * no_color_video mask, we could simply shift it up and mask off the
	 * attributes.  But we did not (actually copied Solaris' definitions).
	 * However, this is still simpler/faster than a lookup table.
	 *
	 * The 63 corresponds to A_STANDOUT, A_UNDERLINE, A_REVERSE, A_BLINK,
	 * A_DIM, A_BOLD which are 1:1 with no_color_video.  The bits that
	 * correspond to A_INVIS, A_PROTECT (192) must be shifted up 1 and
	 * A_ALTCHARSET (256) down 2 to line up.  We use the NCURSES_BITS
	 * macro so this will work properly for the wide-character layout.
	 */
	unsigned value = no_color_video;
	attr_t mask = NCURSES_BITS((value & 63)
				   | ((value & 192) << 1)
				   | ((value & 256) >> 2), 8);

	if ((mask & A_REVERSE) != 0
	    && (newmode & A_REVERSE) != 0) {
	    reverse = TRUE;
	    mask &= ~A_REVERSE;
	}
	newmode &= ~mask;
    }

    if (newmode == PreviousAttr)
	returnCode(OK);

    pair = PAIR_NUMBER(newmode);

    if (reverse) {
	newmode &= ~A_REVERSE;
    }

    turn_off = (~newmode & PreviousAttr) & ALL_BUT_COLOR;
    turn_on = (newmode & ~PreviousAttr) & ALL_BUT_COLOR;

    SetColorsIf(((pair == 0) && !fix_pair0), PreviousAttr);

    if (newmode == A_NORMAL) {
	if ((PreviousAttr & A_ALTCHARSET) && exit_alt_charset_mode) {
	    doPut(exit_alt_charset_mode);
	    PreviousAttr &= ~A_ALTCHARSET;
	}
	if (PreviousAttr) {
	    if (exit_attribute_mode) {
		doPut(exit_attribute_mode);
	    } else {
		if (!SP || SP->_use_rmul) {
		    TurnOff(A_UNDERLINE, exit_underline_mode);
		}
		if (!SP || SP->_use_rmso) {
		    TurnOff(A_STANDOUT, exit_standout_mode);
		}
	    }
	    PreviousAttr &= ALL_BUT_COLOR;
	}

	SetColorsIf((pair != 0) || fix_pair0, PreviousAttr);
    } else if (set_attributes) {
	if (turn_on || turn_off) {
	    TPUTS_TRACE("set_attributes");
	    tputs(tparm(set_attributes,
			(newmode & A_STANDOUT) != 0,
			(newmode & A_UNDERLINE) != 0,
			(newmode & A_REVERSE) != 0,
			(newmode & A_BLINK) != 0,
			(newmode & A_DIM) != 0,
			(newmode & A_BOLD) != 0,
			(newmode & A_INVIS) != 0,
			(newmode & A_PROTECT) != 0,
			(newmode & A_ALTCHARSET) != 0), 1, outc);
	    PreviousAttr &= ALL_BUT_COLOR;
	}
	SetColorsIf((pair != 0) || fix_pair0, PreviousAttr);
    } else {

	TR(TRACE_ATTRS, ("turning %s off", _traceattr(turn_off)));

	TurnOff(A_ALTCHARSET, exit_alt_charset_mode);

	if (!SP || SP->_use_rmul) {
	    TurnOff(A_UNDERLINE, exit_underline_mode);
	}

	if (!SP || SP->_use_rmso) {
	    TurnOff(A_STANDOUT, exit_standout_mode);
	}

	if (turn_off && exit_attribute_mode) {
	    doPut(exit_attribute_mode);
	    turn_on |= (newmode & ALL_BUT_COLOR);
	    PreviousAttr &= ALL_BUT_COLOR;
	}
	SetColorsIf((pair != 0) || fix_pair0, PreviousAttr);

	TR(TRACE_ATTRS, ("turning %s on", _traceattr(turn_on)));
	/* *INDENT-OFF* */
	TurnOn(A_ALTCHARSET,	enter_alt_charset_mode);
	TurnOn(A_BLINK,		enter_blink_mode);
	TurnOn(A_BOLD,		enter_bold_mode);
	TurnOn(A_DIM,		enter_dim_mode);
	TurnOn(A_REVERSE,	enter_reverse_mode);
	TurnOn(A_STANDOUT,	enter_standout_mode);
	TurnOn(A_PROTECT,	enter_protected_mode);
	TurnOn(A_INVIS,		enter_secure_mode);
	TurnOn(A_UNDERLINE,	enter_underline_mode);
#if USE_WIDEC_SUPPORT
	TurnOn(A_HORIZONTAL,	enter_horizontal_hl_mode);
	TurnOn(A_LEFT,		enter_left_hl_mode);
	TurnOn(A_LOW,		enter_low_hl_mode);
	TurnOn(A_RIGHT,		enter_right_hl_mode);
	TurnOn(A_TOP,		enter_top_hl_mode);
	TurnOn(A_VERTICAL,	enter_vertical_hl_mode);
#endif
	/* *INDENT-ON* */

    }

    if (reverse)
	newmode |= A_REVERSE;

    if (SP)
	SetAttr(SCREEN_ATTRS(SP), newmode);
    else
	PreviousAttr = newmode;

    returnCode(OK);
}

NCURSES_EXPORT(int)
vidattr(chtype newmode)
{
    T((T_CALLED("vidattr(%s)"), _traceattr(newmode)));

    returnCode(vidputs(newmode, _nc_outch));
}

NCURSES_EXPORT(chtype)
termattrs(void)
{
    chtype attrs = A_NORMAL;

    T((T_CALLED("termattrs()")));
    if (enter_alt_charset_mode)
	attrs |= A_ALTCHARSET;

    if (enter_blink_mode)
	attrs |= A_BLINK;

    if (enter_bold_mode)
	attrs |= A_BOLD;

    if (enter_dim_mode)
	attrs |= A_DIM;

    if (enter_reverse_mode)
	attrs |= A_REVERSE;

    if (enter_standout_mode)
	attrs |= A_STANDOUT;

    if (enter_protected_mode)
	attrs |= A_PROTECT;

    if (enter_secure_mode)
	attrs |= A_INVIS;

    if (enter_underline_mode)
	attrs |= A_UNDERLINE;

    if (SP->_coloron)
	attrs |= A_COLOR;

    returnChar(attrs);
}
