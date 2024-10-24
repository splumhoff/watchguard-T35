/*
    io.c.

    Local I/O routines for ksymoops.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>

int regular_file(const char *file, const char *msg)
{
    struct stat statbuf;
    static char const procname[] = "regular_file";
    if (stat(file, &statbuf)) {
	ERROR("%s stat %s failed", msg, file);
	perror(prefix);
	return 0;
    }

    if (!S_ISREG(statbuf.st_mode)) {
	ERROR("%s %s is not a regular file, ignored", msg, file);
	return 0;
    }
    return 1;
}

FILE *fopen_local(const char *file, const char *mode, const char *msg)
{
    FILE *f;
    static char const procname[] = "fopen_local";
    if (!(f = fopen(file, mode))) {
	ERROR("%s fopen '%s' failed", msg, file);
	perror(prefix);
    }
    return f;
}

void fclose_local(FILE *f, const char *msg)
{
    int i;
    static char const procname[] = "fclose_local";
    if ((i = fclose(f))) {
	ERROR("%s fclose failed %d", msg, i);
	perror(prefix);
    }
}

/* Read a line, increasing the size of the line as necessary until \n is read.
 * Any nulls or carriage return in the input are silently removed, both
 * cause problems for string handling and printing.
 */
#define INCREMENT 10    /* arbitrary */
char *fgets_local(char **line, int *size, FILE *f, const char *msg)
{
    char *l, *p, *r;
    int longline = 1, s, u;
    static char const procname[] = "fgets_local";

    if (!*line) {
	*size = INCREMENT;
	*line = malloc(*size);
	if (!*line)
	    malloc_error("fgets_local alloc line");
    }

    l = *line;
    while (longline) {
	s = *size-(l-*line);
	memset(l, '\0', s);	/* in case the last line is not terminated */
	r = fgets(l, s, f);
	if (!r) {
	    if (ferror(f)) {
		ERROR("%s fgets failed", msg);
		perror(prefix);
	    }
	    if (l != *line)
		return(*line);
	    else
		return(NULL);
	}
	p = r = l;
	while (s--) {
	    if (*p && *p != '\r') {
		if (*p == '\n') {
		    *r++ = '\0';
		    longline = 0;
		    break;
		}
		else
		    *r++ = *p;
	    }
	    ++p;
	}
	if (longline) {
	    *size += INCREMENT;
	    u = r - *line;
	    *line = realloc(*line, *size);
	    if (!*line)
		malloc_error("fgets_local realloc line");
	    l = *line + u;
	}
    }

    /* convert tabs to spaces, assume tabwidth=8 */
#define TABWIDTH 8
    if ((p = strchr(*line, '\t'))) {
	int i = 0;
	while (p) {
	    ++i;
	    p = strchr(p+1, '\t');
	}
	if (strlen(*line) + 1 + i*(TABWIDTH-1) > *size)
	    *size += i*(TABWIDTH-1);    /* worst case expansion */
	l = malloc(*size);
	if (!l)
	    malloc_error("fgets_local tab expand alloc");
	memset(l, ' ', *size-1);
	*(l+*size-1) = '\0';
	p = *line;
	r = l;
	while (*p) {
	    if (*p == '\t')
		r += TABWIDTH - (p - *line) % TABWIDTH;
	    else
		*r++ = *p;
	    ++p;
	}
	*r = '\0';
	p = *line;
	*line = l;
	free(p);
    }

    DEBUG(4, "%s line '%s'", msg, *line);
    return(*line);
}

FILE *popen_local(const char *cmd, const char *msg)
{
    FILE *f;
    static char const procname[] = "popen_local";
    if (!(f = popen(cmd, "r"))) {
	ERROR("%s popen '%s' failed", msg, cmd);
	perror(prefix);
    }
    return f;
}

void pclose_local(FILE *f, const char *msg)
{
    int i;
    static char const procname[] = "pclose_local";
    errno = 0;
    if ((i = pclose(f))) {
	ERROR("%s pclose failed 0x%x", msg, i);
	if (errno)
	    perror(prefix);
    }
}
