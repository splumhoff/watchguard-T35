/*
    ksyms.c.

    Process ksyms for ksymoops.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

/* Modutils adds symbols to assist ksymoops.  Decode them here.
 * Use type 'G' for generated symbols added by modutils (discarded in
 * the final map), type 'g' for derived generated symbols (kept in the
 * final map).
 */
static void assist_ksyms(SYMBOL_SET *ss, int symnum)
{
    int i;
    char **string = NULL;
    static regex_t     re_modutils;
    static regmatch_t *re_modutils_pmatch;
    static char const procname[] = "assist_ksyms";

    RE_COMPILE(&re_modutils,
	"^(" MODUTILS_PREFIX ".*)"	/* 1, prefix, module name */
	"("				/* 2 */
	   "_O(.*)"			/* 3 object filename */
	   "_M([0-9a-fA-F]+)"		/* 4 mtime, hex */
	   "_V(-?[0-9]+)"		/* 5 version, dec */
	"|"
	   "_S(.*)"			/* 6 section name */
	   "_L([0-9]+)"			/* 7 length, dec */
	")",
	REG_NEWLINE|REG_EXTENDED,
	&re_modutils_pmatch);

    i = regexec(&re_modutils, (ss->symbol)[symnum].name,
	    re_modutils.re_nsub+1, re_modutils_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i)
	return;
    re_strings(&re_modutils, (ss->symbol)[symnum].name, re_modutils_pmatch,
	&string);
    if (string[3]) {
	/* object filename, mtime, version */
	ss->object = string[3];
	string[3] = NULL;   /* do not free this substring */
	errno = 0;
	ss->mtime = hexstring(string[4]);
	if (errno) {
	    ERROR("mtime '%s' is not valid hex", string[4]);
	    perror(prefix);
	    ss->mtime = 0;
	}
	if (*(string[5]) != '-')    /* -ve means no version available */
	    add_Version(string[5], ss->source);
    }
    else if (string[6]) {
	/* section name, length */
	U64 end;
	int l;
	char *gsym;
	l = re_modutils_pmatch[6].rm_eo -
	      re_modutils_pmatch[6].rm_so + 1 +	/* section name */
	    5 +					/* ".start" */
	    1;					/* nul */
	gsym = malloc(l);
	if (!gsym)
	    malloc_error("alloc gsym");
	snprintf(gsym, l, "%s.start", string[6]);
	add_symbol_n(ss, (ss->symbol)[symnum].address, 'g', 1, gsym);
	errno = 0;
	end = strtoul(string[7], NULL, 10);
	if (errno) {
	    ERROR("mtime '%s' is not valid decimal", string[7]);
	    perror(prefix);
	    end = 0;
	}
	if (end) {
	    end = (ss->symbol)[symnum].address + end - 1;
	    snprintf(gsym, l, "%s.end", string[6]);
	    add_symbol_n(ss, end, 'g', 1, gsym);
	}
	free(gsym);
    }
    re_strings_free(&re_modutils, &string);
}

void add_ksyms(const char *address, char type, const char *symbol, const char *module,
	       const OPTIONS *options)
{
    int n;
    SYMBOL_SET *ssp;
    static char *prev_module = NULL;

    if (module) {
	if (!prev_module || strcmp(prev_module, module)) {
	    /* start of a new module in ksyms */
	    ++ss_ksyms_modules;
	    ss_ksyms_module = realloc(ss_ksyms_module,
		ss_ksyms_modules*sizeof(*ss_ksyms_module));
	    if (!ss_ksyms_module)
		malloc_error("realloc ss_ksyms_module");
	    ssp = ss_ksyms_module[ss_ksyms_modules-1] =
		malloc(sizeof(*(ss_ksyms_module[0])));
	    if (!ssp)
		malloc_error("alloc ss_ksyms_module[n]");
	    ss_init(ssp, module);
	    prev_module = strdup(module);
	    if (!prev_module)
		malloc_error("strdup prev_module");
	}
	ssp = ss_ksyms_module[ss_ksyms_modules-1];
    }
    else
	ssp = &ss_ksyms_base;

    if (type == ' ' && strncmp(symbol, MODUTILS_PREFIX, sizeof(MODUTILS_PREFIX)-1) == 0) {
	if (options->ignore_insmod_all)
	    return;
	type = 'G'; /* generated symbol to assist ksymoops */
    }
    n = add_symbol(ssp, address, type, 1, symbol);
    if (type == 'G')
	assist_ksyms(ssp, n);
}

/* Scan one line from ksyms.  Split lines into the base symbols and the module
 * symbols.  Separate ss for base and each module.
 */
static void scan_ksyms_line(const char *line, const OPTIONS *options)
{
    int i;
    char **string = NULL;
    static regex_t     re_ksyms;
    static regmatch_t *re_ksyms_pmatch;
    static char const procname[] = "scan_ksyms_line";

    /* ksyms: address, symbol, optional module */
    RE_COMPILE(&re_ksyms,
	"^([0-9a-fA-F]{4,})"		/* 1 address */
	" +"				/* white space */
	"([^ ]+)"			/* 2 symbol */
	"( +\\[([^ ]+)\\])?$",		/* 3,4 space, module (optional) */
	REG_NEWLINE|REG_EXTENDED,
	&re_ksyms_pmatch);

    i = regexec(&re_ksyms, line,
	    re_ksyms.re_nsub+1, re_ksyms_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i)
	return;

    re_strings(&re_ksyms, line, re_ksyms_pmatch, &string);
    if (strncmp(string[2], "GPLONLY_", 8) == 0)
	strcpy(string[2], string[2]+8);
    add_ksyms(string[1], ' ', string[2], string[4], options);
    re_strings_free(&re_ksyms, &string);
}

/* Read the symbols from ksyms.  */
void read_ksyms(const OPTIONS *options)
{
    FILE *f;
    char *line = NULL;
    int i, size;
    static char const procname[] = "read_ksyms";
    const char *ksyms = options->ksyms;

    if (!ksyms)
	return;
    ss_init(&ss_ksyms_base, "ksyms_base");
    DEBUG(1, "%s", ksyms);

    if (!regular_file(ksyms, procname))
	return;

    if (!(f = fopen_local(ksyms, "r", procname)))
	return;

    while (fgets_local(&line, &size, f, procname))
	scan_ksyms_line(line, options);

    fclose_local(f, procname);
    free(line);

    for (i = 0; i < ss_ksyms_modules; ++i) {
	ss_sort_na(ss_ksyms_module[i]);
	extract_Version(ss_ksyms_module[i]);
    }
    if (ss_ksyms_base.used) {
	ss_sort_na(&ss_ksyms_base);
	extract_Version(&ss_ksyms_base);
    }
    else {
	WARNING("no kernel symbols in ksyms, is %s a valid ksyms file?",
	    ksyms);
    }

     for (i = 0; i < ss_ksyms_modules; ++i) {
	 DEBUG(2, "%s used %d out of %d entries",
	     ss_ksyms_module[i]->source,
	     ss_ksyms_module[i]->used,
	     ss_ksyms_module[i]->alloc);
     }
     DEBUG(2, "%s used %d out of %d entries", ss_ksyms_base.source,
	 ss_ksyms_base.used, ss_ksyms_base.alloc);
}

/* Map each ksyms module entry to the corresponding object entry.  Tricky,
 * see the comments in the docs about needing a unique symbol in each
 * module.
 */
static void map_ksym_to_module(SYMBOL_SET *ss)
{
    int i, j, matches;
    char *name = NULL;
    static char const procname[] = "map_ksym_to_module";

    for (i = 0; i < ss->used; ++i) {
	matches = 0;
	for (j = 0; j < ss_objects; ++j) {
	    name = (ss->symbol)[i].name;
	    if (find_symbol_name(ss_object[j], name, NULL)) {
		++matches;
		ss->related = ss_object[j];
	    }
	}
	if (matches == 1)
	    break;      /* unique symbol over all objects */
	ss->related = NULL; /* keep looking */
    }
    if (!(ss->related)) {
	WARNING("cannot match loaded module %s to a unique module object.  "
		"Trace may not be reliable.", ss->source);
    }
    else DEBUG(1, "ksyms %s matches to %s based on unique symbol %s",
	       ss->source, ss->related->source, name);
}

/* Map all ksyms module entries to their corresponding objects */
void map_ksyms_to_modules(void)
{
    int i;
    SYMBOL_SET *ss, *ssc;
    static char const procname[] = "map_ksyms_to_modules";

    for (i = 0; i < ss_ksyms_modules; ++i) {
	ss = ss_ksyms_module[i];
	if (ss->related) {
	    DEBUG(1, "ksyms %s matches to %s based on modutils assist",
		ss->source, ss->related->source);
	}
	else
	    map_ksym_to_module(ss);
	if (ss->related) {
	    ssc = adjust_object_offsets(ss);
	    compare_maps(ss, ssc, 1);
	}
    }
}

/* Read the modules from lsmod.  */
void read_lsmod(const OPTIONS *options)
{
    FILE *f;
    char *line = NULL;
    int i, size;
    char **string = NULL;
    static regex_t     re_lsmod;
    static regmatch_t *re_lsmod_pmatch;
    static char const procname[] = "read_lsmod";
    const char *lsmod = options->lsmod;

    if (!lsmod)
	return;
    ss_init(&ss_lsmod, "lsmod");
    DEBUG(1, "%s", lsmod);

    if (!regular_file(lsmod, procname))
	return;

    if (!(f = fopen_local(lsmod, "r", procname)))
	return;

    /* lsmod: module, size, use count, optional used by */
    RE_COMPILE(&re_lsmod,
	"^"
	" *([^ ]+)"                 /* 1 module */
	" *([^ ]+)"                 /* 2 size */
	" *([^ ]+)"                 /* 3 count */
	" *(.*)"                    /* 4 used by */
	"$",
	REG_NEWLINE|REG_EXTENDED,
	&re_lsmod_pmatch);

    while (fgets_local(&line, &size, f, procname)) {
	i = regexec(&re_lsmod, line,
		re_lsmod.re_nsub+1, re_lsmod_pmatch, 0);
	DEBUG(4, "regexec %d", i);
	if (i)
	    continue;
	re_strings(&re_lsmod, line, re_lsmod_pmatch, &string);
	add_symbol(&ss_lsmod, string[2], ' ', 1, string[1]);
    }

    fclose_local(f, procname);
    free(line);
    re_strings_free(&re_lsmod, &string);
    if (ss_lsmod.used)
	ss_sort_na(&ss_lsmod);
    else {
	WARNING("no symbols in lsmod, is %s a valid lsmod file?",
	    lsmod);
    }

    DEBUG(2, "%s used %d out of %d entries", ss_lsmod.source, ss_lsmod.used,
	ss_lsmod.alloc);
}

/* Compare modules from ksyms against module list in lsmod and vice versa.
 * There is one ss_ for each ksyms module and a single ss_lsmod to cross
 * check.
 */
void compare_ksyms_lsmod(void)
{
    int i, j;
    SYMBOL_SET *ss;
    SYMBOL *s;
    static char const procname[] = "compare_ksyms_lsmod";

    if (!(ss_lsmod.used && ss_ksyms_modules))
	return;

    s = ss_lsmod.symbol;
    for (i = 0; i < ss_lsmod.used; ++i, ++s) {
	for (j = 0; j < ss_ksyms_modules; ++j) {
	    ss = ss_ksyms_module[j];
	    if (strcmp(s->name, ss->source) == 0)
		break;
	}
	if (j >= ss_ksyms_modules) {
	    WARNING("module %s is in lsmod but not in ksyms, probably no "
		    "symbols exported", s->name);
	}
    }

    for (i = 0; i < ss_ksyms_modules; ++i) {
	ss = ss_ksyms_module[i];
	if (!find_symbol_name(&ss_lsmod, ss->source, NULL))
	    ERROR("module %s is in ksyms but not in lsmod", ss->source);
    }
}
