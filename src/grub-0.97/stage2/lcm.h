/*
 * =====================================================================================
 *
 *       Filename:  lcm.h
 *
 *    Description:  :s
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

#ifndef _LCM_H_
#define _LCM_H_


#define LCD_MODULE_UNKNOWN       0   
#define LCD_MODULE_A             1   /* For Tacoma/Finley */
#define LCD_MODULE_B             2   /* For Kennewick/Vashon/Filton  */
#define LCD_MODULE_NONE_SEATTLE  3   /* NO LCM Module present, platform detected as seattle  */
#define LCD_MODULE_NONE_KIRKLAND 4   /* NO LCM Module present, platform detected as kirkland */
#define LCD_MODULE_NONE_COLFAX1  5   /* NO LCM Module present, platform detected as colfax-I */
#define LCD_MODULE_NONE_COLFAX2  6   /* NO LCM Module present, platform detected as colfax-II */
#define LCD_MODULE_NONE_WESTPORT 7   /* NO LCM Module present, platform detected as westport */
#define LCD_MODULE_NONE_WINTHROP 8   /* NO LCM Module present, platform detected as winthrop */
#define LCD_MODULE_NONE_MUKILTEO 9   /* NO LCM Module present, platform detected as mukilteo */
#define LCD_MODULE_NONE_TWISP    10  /* NO LCM Module present, platform detected as twisp */
#define LCD_MODULE_NONE          11  /* NO LCM Module present */

extern int lcd_module;
extern int LCDB_filton;

void load_timer2(unsigned int ticks);
void LCM_Init(void);
void LCM_Command(unsigned char RS, unsigned char RWn, unsigned char CMD, unsigned int uDelay, unsigned char *Ret);
void LCM_Backlight(void);
void LCM_Write(unsigned char Line, unsigned char *Text);
void LCM_Cls(void);
unsigned char LCM_GetButtons();

#endif
