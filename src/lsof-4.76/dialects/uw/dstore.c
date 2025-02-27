/*
 * dstore.c - SCO UnixWare global storage for lsof
 */


/*
 * Copyright 1996 Purdue Research Foundation, West Lafayette, Indiana
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

#ifndef lint
static char copyright[] =
"@(#) Copyright 1996 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id$";
#endif


#include "lsof.h"


int CloneMaj;				/* clone major device number (see
					 * HaveCloneMaj) */


/*
 * Drive_Nl -- table to drive the building of Nl[] via build_Nl()
 *             (See lsof.h and misc.c.)
 */

struct drive_Nl Drive_Nl[] = {
	{ "cdev",	"cdevsw"	},	/* UW < 7 */
	{ "cmaj",	"clonemajor"	},	/* UW >= 7 */
	{ "ncdev",	"cdevswsz"	},	/* UW < 7 */
	{ X_NCACHE,	"ncache"	},
	{ X_NCSIZE,	"ncsize"	},
	{ "proc",	"practive"	},
	{ "sgvnops",	"segvn_ops"	},
	{ "sgdnops",	"segdev_ops"	},
	{ "var",	"v"		},
	{ "",		""		},
	{ NULL,		NULL		}
};

char **Fsinfo = NULL;			/* file system information */
int Fsinfomax = 0;			/* maximum file system type */
int HaveCloneMaj = 0;			/* CloneMaj status */
int Kd = -1;				/* /dev/kmem file descriptor */
short Nfstyp = 0;			/* number of fstypsw[] entries */

#if	defined(HASFSTRUCT)
/*
 * Pff_tab[] - table for printing file flags
 */

struct pff_tab Pff_tab[] = {
	{ (long)FREAD,		FF_READ		},
	{ (long)FWRITE,		FF_WRITE	},
	{ (long)FNDELAY,	FF_NDELAY	},

# if	defined(FDIRECT)
	{ (long)FDIRECT,	FF_DIRECT	},
# endif	/* defined(FDIRECT) */

	{ (long)FAPPEND,	FF_APPEND	},

# if	defined(FASYNC)
	{ (long)FASYNC,		FF_ASYNC	},
# endif	/* defined(FASYNC) */

	{ (long)FSYNC,		FF_SYNC		},

# if	defined(FDSYNC)
	{ (long)FDSYNC,		FF_DSYNC	},
# endif	/* defined(FDSYNC) */

# if	defined(FLARGEFILE)
	{ (long)FLARGEFILE,	FF_LARGEFILE	},
# endif	/* defined(FLARGEFILE) */

# if	defined(FCLONE)
	{ (long)FCLONE,		FF_CLONE	},
# endif	/* defined(FCLONE) */

# if	defined(FILE_MBLK)
	{ (long)FILE_MBLK,	FF_FILE_MBLK	},
# endif	/* defined(FILE_MBLK) */

	{ (long)FNONBLOCK,	FF_NBLOCK	},
	{ (long)FNOCTTY,	FF_NOCTTY	},

# if	defined(FNMFS)
	{ (long)FNMFS,		FF_NMFS		},
# endif	/* defined(FNMFS) */

	{ (long)0,		NULL		}
};


/*
 * Pof_tab[] - table for print process open file flags
 */

struct pff_tab Pof_tab[] = {

	{ (long)FCLOSEXEC,	POF_CLOEXEC	},

# if	defined(UF_FDLOCK)
	{ (long)UF_FDLOCK,	POF_FDLOCK	},
# endif	/* defined(UF_FDLOCK) */

	{ (long)0,		NULL		}
};
#endif	/* defined(HASFSTRUCT) */
