/*
 * crc16 routines for validatiing WG Serial number
 */

#ifndef __CRC16_H__
#define __CRC16_H__

#if 0
#include <sys/types.h>
#endif
#include <linux/types.h>

u_int16_t mfg_serial_crc_byte(u_int16_t crc, const u_int8_t nextbyte);

#endif /* __CRC16_H__ */
