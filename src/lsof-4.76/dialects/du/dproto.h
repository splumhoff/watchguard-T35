/*
 * dproto.h - DEC OSF/1, Digital UNIX, Tru64 UNIX function prototypes for lsof
 *
 * The _PROTOTYPE macro is defined in the common proto.h.
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * $Id$
 */


_PROTOTYPE(extern void clr_flinfo,(void));
_PROTOTYPE(extern char *get_nlist_path,(int ap));
_PROTOTYPE(extern int is_file_named,(char *p, int cd));
_PROTOTYPE(extern struct l_vfs *readvfs,(KA_T vm));

#if     defined(HASDCACHE)
_PROTOTYPE(extern void clr_sect,(void));
#endif  /* defined(HASDCACHE) */

#if	defined(HASIPv6)
_PROTOTYPE(extern struct hostent *gethostbyname2,(char *nm, int prot));
#endif	/* defined(HASIPv6) */

#if	defined(HASPRIVNMCACHE)
_PROTOTYPE(extern int tag_to_path,(char *fs, mlBfTagT t2pb, int nl, char *nlb));
#endif	/* defined(HASPRIVNMCACHE) */

#if	defined(USELOCALREADDIR)
_PROTOTYPE(extern int CloseDir,(DIR *dirp));
_PROTOTYPE(extern DIR *OpenDir,(char *dir));
_PROTOTYPE(extern struct DIRTYPE *ReadDir,(DIR *dirp));
#endif	/* defined(USELOCALREADDIR) */
