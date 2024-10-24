/*
 * =====================================================================================
 *
 *       Filename:  seattle_gpio.h
 *
 *    Description:  : header for sharing proto types and extern variables
 *
 *        Version:  1.0
 *        Created:  08/19/2009 05:14:31 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Denny Hu (China), Denny.Hu@watchguard.com
 *        Company:  watchguard
 *	    Copyright:  Copyright (c) 2009, Denny Hu
 *
 * =====================================================================================
 */

#ifndef _SEATTLE_GPIO_
#define _SEATTLE_GPIO_

unsigned char get_reset_button (void);
int sled_init(void);

#endif // _SEATTLE_GPIO_
