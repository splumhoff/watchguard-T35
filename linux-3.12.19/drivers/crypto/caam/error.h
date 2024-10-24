/*
 * CAAM Error Reporting code header
 *
 * Copyright 2009-2011 Freescale Semiconductor, Inc.
 */

#ifndef CAAM_ERROR_H
#define CAAM_ERROR_H
#define CAAM_ERROR_STR_MAX 302
void caam_jr_strstatus(struct device *jrdev, u32 status);
#ifdef	CONFIG_WG_PLATFORM // WG:JB Return Linux errors
#define	linux_error(x) (((x & 0xF00000FF) == 0x4000001C) ? -ETIMEDOUT : -EIO)
#define	LINUX_ERROR(x) (unlikely(x) ? linux_error(x) : 0)
#endif
#endif /* CAAM_ERROR_H */
