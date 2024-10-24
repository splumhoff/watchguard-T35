/*
    misc.c.

    Miscellaneous routines for ksymoops.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void malloc_error(const char *msg)
{
    static char const procname[] = "malloc_error";
    FATAL("for %s", msg);
}

/* Format an address with the correct number of leading zeroes */
const char *format_address(addr_t address, const OPTIONS *options)
{
    /* Well oversized */
    static char text[200];
    snprintf(text, sizeof(text), "%0*llx",
	options && options->address_bits ? options->address_bits/4
		: address > 0xffffffff ? 16 : 8,
	address);
    return(text);
}

/* Find the full pathname of a program.  Code heavily based on
 * glibc-2.0.5/posix/execvp.c.
 */
char *find_fullpath(const char *program)
{
    char *fullpath = NULL;
    char *path, *p;
    size_t len;
    static const char procname[] = "find_fullpath";

    /* Don't search when it contains a slash.  */
    if (strchr(program, '/')) {
	if (!(fullpath = strdup(program)))
	    malloc_error(procname);
	DEBUG(2, "%s", fullpath);
	return(fullpath);
    }

    path = getenv ("PATH");
    if (!path) {
	/* There is no `PATH' in the environment.  The default search
	   path is the current directory followed by the path `confstr'
	   returns for `_CS_PATH'.
	 */
	len = confstr(_CS_PATH, (char *) NULL, 0);
	if (!(path = malloc(1 + len)))
	    malloc_error(procname);
	path[0] = ':';
	confstr(_CS_PATH, path+1, len);
    }

    len = strlen(program) + 1;
    if (!(fullpath = malloc(strlen(path) + len)))
	malloc_error(procname);
    p = path;
    do {
	path = p;
	p = strchr(path, ':');
	if (p == NULL)
	    p = strchr(path, '\0');

	/* Two adjacent colons, or a colon at the beginning or the end
	 * of `PATH' means to search the current directory.
	 */
	if (p == path)
	    memcpy(fullpath, program, len);
	else {
	    /* Construct the pathname to try.  */
	    memcpy(fullpath, path, p - path);
	    fullpath[p - path] = '/';
	    memcpy(&fullpath[(p - path) + 1], program, len);
	}

	/* If we have execute access, assume this is the program. */
	if (access(fullpath, X_OK) == 0) {
	    DEBUG(2, "%s", fullpath);
	    return(fullpath);
	}
    } while (*p++ != '\0');

    FATAL("could not find executable %s", program);
}

/* Convert a hex string to a number.  Addresses can be 64 bit, not all libraries
 * have strtoull so roll my own.  I assume that hex is a valid hex string, the
 * regular expression parsing takes care of that.
 */
U64 hexstring(const char *hex)
{
    addr_t address = 0;
    char chunk[9];
    int len;
    /* Unlike the IMHO broken strtoul[l], clear errno here instead of forcing
     * the user to do it.
     */
    errno = 0;
    while (*hex == '0')
	++hex;
    len = strlen(hex);
    if (len > 2*sizeof(addr_t)) {
	errno = ERANGE;
	return(ULONG_MAX);
    }
    while (len > 0) {
	strncpy(chunk, hex, len > 8 ? 8 : len+1);
	chunk[8] = '\0';
	address = (address << strlen(chunk)*4) + strtoul(chunk, NULL, 16);
	len -= 8;
	hex += 8;
    }
    return(address);
}
