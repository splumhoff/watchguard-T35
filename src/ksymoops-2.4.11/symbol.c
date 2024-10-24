/*
    symbol.c.

    Symbol handling routines for ksymoops.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

/* Initialise a symbol source */
void ss_init(SYMBOL_SET *ss, const char *msg)
{
    memset(ss, '\0', sizeof(*ss));
    ss->source = strdup(msg);
    if (!ss->source)
	malloc_error(msg);
}

/* Free dynamic data from a symbol source */
void ss_free(SYMBOL_SET *ss)
{
    int i;
    SYMBOL *s;
    for (s = ss->symbol, i = 0; i < ss->used; ++i, ++s)
	free(s->name);
    free(ss->symbol);
    free(ss->source);
    memset(ss, '\0', sizeof(*ss));
}

/* Initialise common symbol sets */
void ss_init_common(void)
{
    ss_init(&ss_Version, "Version_");
}

/* Find a symbol name in a symbol source.  Brute force ascending order search,
 * no hashing.  If start is not NULL, it contains the starting point for the
 * scan and is updated to point to the found entry.  If the entry is not found,
 * return NULL with start pointing to the next highest entry.
 * NOTE: Assumes that ss is sorted by name.
 */
SYMBOL *find_symbol_name(const SYMBOL_SET *ss, const char *symbol, int *start)
{
    int i, l;
    SYMBOL *s;
    for (i = start ? *start : 0, s = ss->symbol+i; i < ss->used; ++i, ++s) {
	if ((l = strcmp(symbol, s->name)) == 0) {
	    if (start)
		*start = i;
	    return(s);
	}
	if (l < 0)
	    break;
    }
    if (start)
	*start = i;
    return NULL;
}

#define MIPS64_CKSEG0_MASK	0xffffffff80000000ULL
#define MIPS64_CKSEG0_BASE	0xffffffff80000000ULL
#define MIPS64_KSEG0_BASE	0x0000000080000000ULL
#define MIPS64_XKPHYS5_BASE	0xa800000000000000ULL

static addr_t normalize_address(addr_t address, const OPTIONS *options)
{
    if (options->target &&
	strstr(options->target, "mips") &&
	strstr(options->target, "64")) {
	/*
	* Some address gymnastics are necessary due to the weird way
	* in which mips64 kernels are built as elf32 then converted
	* to elf64 after linking.
	*/
	if ((address & MIPS64_CKSEG0_MASK) == MIPS64_CKSEG0_BASE ||
	    (address & MIPS64_CKSEG0_MASK) == MIPS64_KSEG0_BASE) {
	    /* normalise physical addrs in 32-bit kernel segments */
	    address = (address & ~MIPS64_CKSEG0_MASK) | MIPS64_XKPHYS5_BASE;
	}
    }
    address &= truncate_mask;
    return(address);
}

/* Find an address in a symbol source.  Brute force ascending order search, no
 * hashing.  If start is not NULL, it contains the starting point for the scan
 * and is updated to point to the found entry.  If the entry is not found,
 * return NULL with start pointing to the next highest entry.
 * NOTE: Assumes that ss is sorted by address.
 */
static SYMBOL *find_symbol_address(const SYMBOL_SET *ss,
				   addr_t address, int *start,
				   const OPTIONS *options)
{
    int i;
    SYMBOL *s;
    address = normalize_address(address, options);
    for (i = start ? *start : 0, s = ss->symbol+i; i < ss->used; ++i, ++s) {
	addr_t saddress = normalize_address(s->address, options);
	if (address > saddress)
	    continue;
	else if (address == saddress) {
	    if (i < ss->used-1 &&
		address == normalize_address((s+1)->address, options) &&
		(s->type == 'G' || s->type == 'g'))
		/* more than one symbol on the same address, skip the ones that
		 * came from modutils assist.
		 */
		continue;
	    if (start)
		*start = i;
	    return(s);
	}
	else
	    break;
    }
    if (start)
	*start = i;
    return NULL;
}

/* Add a symbol to a symbol set, address in binary */
int add_symbol_n(SYMBOL_SET *ss, addr_t address,
		 const char type, const char keep, const char *symbol)
{
    int i;
    char **string = NULL;
    SYMBOL *s;
    static regex_t     re_symbol_ver;
    static regmatch_t *re_symbol_ver_pmatch;
    static const char procname[] = "add_symbol_n";

    address &= truncate_mask;
    /* Strip out any trailing symbol version _R.*xxxxxxxx. */
    RE_COMPILE(&re_symbol_ver,
	"^(.*)_R.*[0-9a-fA-F]{8,}$",
	REG_NEWLINE|REG_EXTENDED,
	&re_symbol_ver_pmatch);

    i = regexec(&re_symbol_ver, symbol,
	re_symbol_ver.re_nsub+1, re_symbol_ver_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0)
	re_strings(&re_symbol_ver, symbol, re_symbol_ver_pmatch,
	    &string);

    DEBUG(4, "%s %s '%c' %d '%s'",
	ss->source, format_address(address, NULL),
	type, keep, i == 0 ? string[1] : symbol);
    if (ss->used > ss->alloc)
	FATAL("ss %s used (%d) > alloc (%d)", ss->source, ss->used, ss->alloc);
    if (ss->used == ss->alloc) {
	/* increase by 20% or 10, whichever is larger, arbitrary */
	int newsize = ss->alloc*120/100;
	if (newsize < ss->alloc+10)
	    newsize = ss->alloc+10;
	DEBUG(4, "increasing %s from %d to %d entries",
	    ss->source, ss->alloc, newsize);
	ss->symbol = realloc(ss->symbol, newsize*sizeof(*(ss->symbol)));
	if (!ss->symbol)
	    malloc_error("realloc ss");
	ss->alloc = newsize;
    }
    s = ss->symbol+ss->used;
    if (i == 0) {
	s->name = string[1];
	string[1] = NULL;   /* don't free this one */
    }
    else {
	s->name = strdup(symbol);
	if (!s->name)
	    malloc_error("strdup symbol");
    }
    s->type = type;
    s->keep = keep;
    s->address = address;
    s->module = NULL;	/* only used in final map */
    re_strings_free(&re_symbol_ver, &string);
    return(ss->used++);
}

/* Add a symbol to a symbol set, address in character */
int add_symbol(SYMBOL_SET *ss, const char *address, const char type,
	       const char keep, const char *symbol)
{
    addr_t a;
    static char const procname[] = "add_symbol";
    errno = 0;
    a = hexstring(address);
    if (errno) {
	ERROR("address '%s' is in error", address);
	perror(prefix);
    }
    return(add_symbol_n(ss, a, type, 1, symbol));
}

/* Map an address to symbol, offset and length, address in binary */
char *map_address(const SYMBOL_SET *ss, addr_t address,
		  const OPTIONS *options)
{
    int i = 0, l;
    SYMBOL *s;
    static char *map = NULL;
    static int size = 0;
    static const char procname[] = "map_address";

    address &= truncate_mask;
    DEBUG(3, "%s %s", ss->source, format_address(address, options));
    s = find_symbol_address(ss, address, &i, options);
    if (!s && --i >= 0)
	s = ss->symbol+i;   /* address is between s and s+1 */

    /* Extra map text is always < 100 bytes */
    if (s) {
	l = strlen(s->name) + 100;
	if (s->module)
	    l += strlen(s->module)+2;  /* [module] */
    }
    else
	l = 100;
    if (l > size) {
	map = realloc(map, l);
	if (!map)
	    malloc_error(procname);
	size = l;
    }
    if (!s) {
	if (ss->used == 0)
	    snprintf(map, size, "No symbols available");
	else
	    snprintf(map, size, "Before first symbol");
    }
    else if ((i+1) >= ss->used) {
	/* Somewhere past last symbol.  Length of last section of code
	 * is unknown, arbitrary cutoff at 32K.
	 * Stupid gcc warns about trigraph ? ? > so split the strings.
	 */
	addr_t offset = normalize_address(address, options) -
			normalize_address(s->address, options);
	if (offset > 32768)
	    snprintf(map, size,
		     options->hex ? "<END_OF_CODE+%llx/???" "?>"
		    : "<END_OF_CODE+%lld/???" "?>",
		offset);
	else
	    snprintf(map, size,
		     options->hex ? "<%s+%llx/???" "?>"
		    : "<%s+%lld/???" "?>",
		s->name, offset);
    }
    else {
	snprintf(map, size,
		 options->hex ? "<%s%s%s%s+%llx/%llx>"
		: "<%s%s%s%s+%lld/%lld>",
	    s->module ? "[" : "",
	    s->module ? s->module : "",
	    s->module ? "]" : "",
	    s->name,
	    normalize_address(address, options) -
	    normalize_address(s->address, options),
	    normalize_address((s+1)->address, options) -
	    normalize_address(s->address, options));
    }
    return(map);
}

/* After sorting, obsolete symbols are at the top.  Delete them. */
static void ss_compress(SYMBOL_SET *ss)
{
    int i, j;
    SYMBOL *s;
    static const char procname[] = "ss_compress";

    DEBUG(2, "table %s, before %d", ss->source, ss->used);
    for (i = 0, s = ss->symbol+i; i < ss->used; ++i, ++s) {
	if (!s->keep) {
	    for (j = i; j < ss->used; ++j, ++s) {
		if (s->keep)
		    FATAL("table %s is not sorted", ss->source);
	    }
	    break;
	}
    }
    for (j = i, s = ss->symbol+j; j < ss->used; ++j, ++s) {
	DEBUG(4, "dropped %s", s->name);
	free(s->name);
    }
    ss->used = i;
    DEBUG(2, "table %s, after %d", ss->source, ss->used);
}

static int ss_compare_atn(const void *a, const void *b)
{
    SYMBOL *c = (SYMBOL *) a;
    SYMBOL *d = (SYMBOL *) b;
    int i;

    /* obsolete symbols to the top */
    if (c->keep != d->keep)
	return(d->keep - c->keep);
    if (c->address > d->address)
	return(1);
    if (c->address < d->address)
	return(-1);
    if (c->type > d->type)
	return(1);
    if (c->type < d->type)
	return(-1);
    if ((i = strcmp(c->name, d->name)))
	return(i);
    return(0);
}

/* Sort a symbol set by address, type and name */
void ss_sort_atn(SYMBOL_SET *ss)
{
    static char const procname[] = "ss_sort_atn";
    DEBUG(1, "%s", ss->source);
    qsort((char *) ss->symbol, (unsigned) ss->used,
	sizeof(*(ss->symbol)), ss_compare_atn);
    ss_compress(ss);
}

static int ss_compare_na(const void *a, const void *b)
{
    SYMBOL *c = (SYMBOL *) a;
    SYMBOL *d = (SYMBOL *) b;
    int i;

    /* obsolete symbols to the top */
    if (c->keep != d->keep)
	return(d->keep - c->keep);
    if ((i = strcmp(c->name, d->name)))
	return(i);
    if (c->address > d->address)
	return(1);
    if (c->address < d->address)
	return(-1);
    return(0);
}

/* Sort a symbol set by name and address, drop duplicates.  There should be
 * no duplicates but I have seen duplicates in ksyms on 2.0.35.
 */
void ss_sort_na(SYMBOL_SET *ss)
{
    int i;
    SYMBOL *s;
    static char const procname[] = "ss_sort_na";
    DEBUG(1, "%s", ss->source);
    qsort((char *) ss->symbol, (unsigned) ss->used,
	sizeof(*(ss->symbol)), ss_compare_na);
    ss_compress(ss);
    s = ss->symbol;
    for (i = 0; i < ss->used-1; ++i) {
	if (strcmp(s->name, (s+1)->name) == 0 &&
	    s->address == (s+1)->address) {
	    if (s->type != ' ')
		(s+1)->keep = 0;
	    else
		s->keep = 0;
	}
	++s;
    }
    qsort((char *) ss->symbol, (unsigned) ss->used,
	sizeof(*(ss->symbol)), ss_compare_na);
    ss_compress(ss);
}

/* Copy a symbol set, including all its strings */
SYMBOL_SET *ss_copy(const SYMBOL_SET *ss)
{
    SYMBOL_SET *ssc;
    static char const procname[] = "ss_copy";
    DEBUG(4, "%s", ss->source);
    ssc = malloc(sizeof(*ssc));
    if (!ssc)
	malloc_error("copy ssc");
    ss_init(ssc, ss->source);
    ssc->used = ss->used;
    ssc->alloc = ss->used;  /* shrink the copy */
    ssc->symbol = malloc(ssc->used*sizeof(*(ssc->symbol)));
    if (!(ssc->symbol))
	malloc_error("copy ssc symbols");
    memcpy(ssc->symbol, ss->symbol, ssc->used*sizeof(*(ssc->symbol)));
    return(ssc);
}

/* Convert version number to major, minor string.  */
static const char *format_Version(addr_t Version)
{
    static char string[12]; /* 255.255.255\0 worst case */
    snprintf(string, sizeof(string), "%d.%d.%d",
	(int) ((Version >> 16) & 0xff),
	(int) ((Version >> 8) & 0xff),
	(int) ((Version) & 0xff));
    return(string);
}

/* Save version number.  The "address" is the version number, the "symbol" is
 * the source of the version.
 */
void add_Version(const char *version, const char *source)
{
    static char const procname[] = "add_Version";
    int i = atoi(version);
    DEBUG(2, "%s %s %s", source, version, format_Version(i));
    add_symbol_n(&ss_Version, i, 'V', 1, source);
}

/* Extract Version_ number from a symbol set and save it.  */
void extract_Version(SYMBOL_SET *ss)
{
    int i = 0;
    SYMBOL *s;

    s = find_symbol_name(ss, "Version_", &i);
    if (!s && i < ss->used)
	s = ss->symbol+i;   /* first symbol after "Version_" */
    if (s && !strncmp(s->name, "Version_", 8))
	add_Version(s->name+8, ss->source);
}

/* Compare all extracted Version numbers.  Silent unless there is a problem. */
void compare_Version(void)
{
    int i = 0;
    SYMBOL *s, *s0;
    static int prev_used = 0;
    static char const procname[] = "compare_Version";

    if (!ss_Version.used)
	return;
    /* Only check if the Version table has changed in size */
    if (prev_used == ss_Version.used)
	return;

    ss_sort_na(&ss_Version);
    s0 = s = ss_Version.symbol;
    DEBUG(1, "Version %s", format_Version(s0->address));
    for (i = 0; i < ss_Version.used; ++i, ++s) {
	if (s->address != s0->address) {
	    /* format_Version uses static area, do separate calls */
	    WARNING_S("Version mismatch.  %s says %s,",
		s0->name, format_Version(s0->address));
	    WARNING_E(" %s says %s.  Expect lots of address mismatches.",
		s->name, format_Version(s->address));
	}
    }
    prev_used = ss_Version.used;
}
