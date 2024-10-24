/*
 * Copyright 2009-2012 Freescale Semiconductor, Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Freescale Semiconductor nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * This software is provided by Freescale Semiconductor "as is" and any
 * express or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose are
 * disclaimed. In no event shall Freescale Semiconductor be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of
 * this software, even if advised of the possibility of such damage.
 */

/**************************************************************************//**
 @File          part_integration_ext.h

 @Description   P4080/P5020/P3041/P1023 external definitions and structures.
*//***************************************************************************/
#ifndef __PART_INTEGRATION_EXT_H
#define __PART_INTEGRATION_EXT_H

#ifdef P1023
#include "part_integration_P1023.h"
#elif defined B4860 || defined T4240
#include "part_integration_B4_T4.h"
#else
#include "part_integration_P3_P4_P5.h"
#endif

/*****************************************************************************
 *  UNIFIED MODULE CODES
 *****************************************************************************/
  #define MODULE_UNKNOWN          0x00000000
  #define MODULE_FM               0x00010000
  #define MODULE_FM_MURAM         0x00020000
  #define MODULE_FM_PCD           0x00030000
  #define MODULE_FM_RTC           0x00040000
  #define MODULE_FM_MAC           0x00050000
  #define MODULE_FM_PORT          0x00060000
  #define MODULE_MM               0x00070000
  #define MODULE_FM_SP            0x00080000

#endif /* __PART_INTEGRATION_EXT_H */
