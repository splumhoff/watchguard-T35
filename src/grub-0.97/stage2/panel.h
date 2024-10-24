/*
 * =====================================================================================
 *
 *       Filename:  panel.h
 *
 *    Description:  : header for definition of button controls
 *
 *        Created:  07/10/2014 03:34:31 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Mukund Jampala
 *        Company:  watchguard
 *
 * =====================================================================================
 */

/* Front panel button masks */
/* Mostly for NAR-7090 devices as per NAR-7090 manual and now used as masked values for other LCMs */
#define BTN_UP      0x1
#define BTN_DOWN    0x2
#define BTN_ENTER   0x4
#define BTN_ESCAPE  0x8
#define BTN_RESET   BTN_UP  /* Specific to platforms like seattle(NO LCM and only
			     * one reset BTN) where all you can do is pick a boot
			     * choice of 1 other system(sysa/sysa/sysa-safe):
			     * obviously, our choose would be sysb in such situations.
			     */
#define BTN_LEFT   BTN_ENTER
#define BTN_RIGHT  BTN_ESCAPE
