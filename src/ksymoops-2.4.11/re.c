/*
    re.c.

    Regular expression processing for ksymoops.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

/* Compile a regular expression */
void re_compile(regex_t *preg, const char *regex, int cflags,
	regmatch_t **pmatch)
{
    int i, l;
    char *p;
    static char const procname[] = "re_compile";

    DEBUG_S(1, "'%s'", regex);
    if ((i = regcomp(preg, regex, cflags))) {
	l = regerror(i, preg, NULL, 0);
	++l;    /* doc is ambiguous, be safe */
	p = malloc(l);
	if (!p)
	    malloc_error("regerror text");
	regerror(i, preg, p, l);
	FATAL("on '%s' - %s", regex, p);
    }
    DEBUG_E(1, " %d sub expression(s)", preg->re_nsub);
    /* [0] is entire match, [1] is first substring */
    *pmatch = malloc((preg->re_nsub+1)*sizeof(**pmatch));
    if (!*pmatch)
	malloc_error("pmatch");

}

/* Compile common regular expressions */
void re_compile_common(void)
{

    /* nm: address, type, symbol, optional [module] */
    RE_COMPILE(&re_nm,
	"^([0-9a-fA-F]{4,}) +([^ ]) +([^ ]+)( +\\[([^ ]+)\\])?$",
	REG_NEWLINE|REG_EXTENDED,
	&re_nm_pmatch);

    /* bracketed address preceded by optional white space */
    RE_COMPILE(&re_bracketed_address,
	"^ *" BRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED,
	&re_bracketed_address_pmatch);

    /* reverse bracketed address preceded by optional white space */
    RE_COMPILE(&re_revbracketed_address,
	"^ *" REVBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED,
	&re_revbracketed_address_pmatch);

    /* unbracketed address preceded by optional white space */
    RE_COMPILE(&re_unbracketed_address,
	"^ *" UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED,
	&re_unbracketed_address_pmatch);

}

/* Split text into the matching re substrings - Perl is so much easier :).
 * Each element of *string is set to a malloced copy of the substring or
 * NULL if the substring did not match (undef).  A zero length substring match
 * is represented by a zero length **string.
 */
void re_strings(regex_t *preg, const char *text, regmatch_t *pmatch,
	char ***string)
{
    int i;
    static char const procname[] = "re_strings";
    if (!*string) {
	*string = malloc((preg->re_nsub+1)*sizeof(**string));
	if (!*string)
	    malloc_error("re_strings base");
	for (i = 0; i < preg->re_nsub+1; ++i)
	    (*string)[i] = NULL;
    }
    for (i = 0; i < preg->re_nsub+1; ++i) {
	DEBUG_S(5, "%d offsets %d %d", i, pmatch[i].rm_so, pmatch[i].rm_eo);
	if (pmatch[i].rm_so == -1) {
	    /* no match for this sub expression */
	    free((*string)[i]);
	    (*string)[i] = NULL;
	    DEBUG_E(5, "%s", " (undef)");
	}
	else {
	    int l = pmatch[i].rm_eo - pmatch[i].rm_so + 1;
	    char *p;
	    p = malloc(l);
	    if (!p)
		malloc_error("re_strings");
	    strncpy(p, text+pmatch[i].rm_so, l-1);
	    *(p+l-1) = '\0';
	    (*string)[i] = p;
	    DEBUG_E(5, " '%s'", p);
	}
    }
}

/* Free the matching re substrings */
void re_strings_free(const regex_t *preg, char ***string)
{
    if (*string) {
	int i;
	for (i = 0; i < preg->re_nsub+1; ++i)
	    free((*string)[i]);
	free(*string);
	*string = NULL;
    }
}

/* Check that there are enough strings for an re */
void re_string_check(int need, int available, const char *msg)
{
    static char const procname[] = "re_string_check";
    if (need > available)
	FATAL("not enough re_strings in %s.  Need %d, available %d",
	    msg, need, available);
}
