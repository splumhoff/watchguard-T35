/*
    map.c.

    Read System.map for ksymoops, create merged System.map.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <malloc.h>
#include <string.h>

/* Read the symbols from System.map */
void read_system_map(const OPTIONS *options)
{
    FILE *f;
    char *line = NULL, **string = NULL;
    int i, size = 0;
    static int merged = 0;
    static char const procname[] = "read_system_map";
    const char *system_map = options->system_map;

    if (!system_map)
	return;
    ss_init(&ss_system_map, "System.map");
    DEBUG(1, "%s", system_map);

    if (!regular_file(system_map, procname))
	return;

    if (!(f = fopen_local(system_map, "r", procname)))
	return;

    while (fgets_local(&line, &size, f, procname)) {
	i = regexec(&re_nm, line, re_nm.re_nsub+1, re_nm_pmatch, 0);
	DEBUG(4, "regexec %d", i);
	if (i == 0) {
	    re_strings(&re_nm, line, re_nm_pmatch, &string);
	    /* If the input is a merged system map then it can contain module
	     * data, add these as if they came from ksyms.  The system map is
	     * read last so we can check if there is any other module data,
	     * reading from both a merged map and module data does not make
	     * sense.
	     */
	    if (string[5]) {
		if (++merged == 1 && ss_ksyms_modules)
		    ERROR("%s appears to be a merged System.map, it contains [module] names.\n"
			  "\t\tYou also supplied ksyms, this combination makes no sense.\n"
			  "\t\tPlease use -VKLO -m %s.",
			  system_map, system_map);
		add_ksyms(string[1], string[2][0], string[3], string[5], options);
	    }
	    else
		add_symbol(&ss_system_map, string[1], *string[2],
		       1, string[3]);
	}
    }

    fclose_local(f, procname);
    re_strings_free(&re_nm, &string);
    free(line);
    if (ss_system_map.used) {
	ss_sort_na(&ss_system_map);
	extract_Version(&ss_system_map);
    }
    else {
	WARNING("no kernel symbols in System.map, is %s a valid System.map "
		"file?", system_map);
    }

    DEBUG(2, "%s used %d out of %d entries", ss_system_map.source,
	ss_system_map.used, ss_system_map.alloc);
}

/* Compare two maps, all symbols in the first should appear in the second. */
void compare_maps(const SYMBOL_SET *ss1, const SYMBOL_SET *ss2,
	     int precedence)
{
    int i, start = 0;
    SYMBOL *s1, *s2, **sdrop = precedence == 1 ? &s2 : &s1;
    const SYMBOL_SET **ssdrop = precedence == 1 ? &ss2 : &ss1;
    static char const procname[] = "compare_maps";

    if (!(ss1->used && ss2->used))
	return;

    DEBUG(2, "%s vs %s, %s takes precedence", ss1->source, ss2->source,
	precedence == 1 ? ss1->source : ss2->source);

    for (i = 0; i < ss1->used; ++i) {
	s1 = ss1->symbol+i;
	if (!(s1->keep))
	    continue;
	s2 = find_symbol_name(ss2, s1->name, &start);
	if (!s2) {
	    /* Some types only appear in nm output, not in things
	     * like System.map.  Silently ignore them.
	     */
	    if (s1->type == 'a' || s1->type == 't')
		continue;
	    /* modutils assist uses generated symbols */
	    if (s1->type == 'G' || s1->type == 'g')
		continue;
	    WARNING("%s symbol %s not found in %s.  Ignoring %s entry",
		ss1->source, s1->name,
		ss2->source, (*ssdrop)->source);
	    if (*sdrop)
		(*sdrop)->keep = 0;
	}
	else if (s1->address != s2->address) {
	    /* Type C symbols cannot be resolved from nm to ksyms,
	     * silently ignore them.
	     */
	    if (s1->type == 'C' || s2->type == 'C')
		continue;
	    /* Jakub Jelinek says that ___[fbsahi]_.* symbols are relocated at
	     * boot time so silently ignore any mismatches on these symbols.
	     */
	    if (strncmp(s1->name, "___", 3) == 0 &&
		s1->name[4] == '_' &&
		(s1->name[3] == 'f' ||
		 s1->name[3] == 'b' ||
		 s1->name[3] == 's' ||
		 s1->name[3] == 'a' ||
		 s1->name[3] == 'h' ||
		 s1->name[3] == 'i'))
		continue;
	    WARNING("mismatch on symbol %s %c, %s says %llx, %s says %llx.  "
		    "Ignoring %s entry",
		s1->name, s1->type, ss1->source, s1->address,
		ss2->source, s2->address, (*ssdrop)->source);
	    if (*sdrop)
		(*sdrop)->keep = 0;
	}
	else
	    ++start;    /* step to next entry in ss2 */
    }
}

/* Append the second symbol set onto the first */
static void append_map(SYMBOL_SET *symbol, const SYMBOL_SET *related, char *module)
{
    int i, symnum;
    SYMBOL *s;
    static char const procname[] = "append_map";

    if (!related || !related->used)
	return;
    DEBUG_S(2, "%s to %s", related->source, symbol->source);
    if (module)
	DEBUG_E(2, " [%s]", module);
    else
	DEBUG_E(2, "%s", "");

    for (i = 0; i < related->used; ++i) {
	s = related->symbol+i;
	if (s->keep) {
	    symnum = add_symbol_n(symbol, s->address, s->type, 1, s->name);
	    (symbol->symbol)[symnum].module = module;
	}
    }
}

/* Compare the various sources and build a merged system map */
void merge_maps(const OPTIONS *options)
{
    int i;
    SYMBOL *s;
    FILE *f;
    static char const procname[] = "merge_maps";
    const char *save_system_map = options->save_system_map;

    DEBUG(1, "%s", "");

    /* Using_Versions only appears in ksyms, copy to other tables */
    if ((s = find_symbol_name(&ss_ksyms_base,
	    "Using_Versions", 0))) {
	if (ss_system_map.used) {
	    add_symbol_n(&ss_system_map, s->address,
		s->type, s->keep, s->name);
	    ss_sort_na(&ss_system_map);
	}
	if (ss_vmlinux.used) {
	    add_symbol_n(&ss_vmlinux, s->address, s->type,
		s->keep, s->name);
	    ss_sort_na(&ss_vmlinux);
	}
    }

    compare_Version();  /* highlight any version problems first */
    compare_ksyms_lsmod();  /* highlight any missing modules next */
    compare_maps(&ss_ksyms_base, &ss_vmlinux, 2);
    compare_maps(&ss_system_map, &ss_vmlinux, 2);
    compare_maps(&ss_vmlinux, &ss_system_map, 1);
    compare_maps(&ss_ksyms_base, &ss_system_map, 2);

    if (ss_objects) {
	map_ksyms_to_modules();
    }

    ss_init(&ss_merged, "merged");
    append_map(&ss_merged, &ss_vmlinux, NULL);
    append_map(&ss_merged, &ss_ksyms_base, NULL);
    append_map(&ss_merged, &ss_system_map, NULL);
    for (i = 0; i < ss_ksyms_modules; ++i)
	append_map(&ss_merged,
	    (ss_ksyms_module[i]->related ?
		ss_ksyms_module[i]->related :
		ss_ksyms_module[i]),
	    ss_ksyms_module[i]->source);
    if (!ss_merged.used)
	WARNING("%s", "no symbols in merged map");

    /* drop duplicates, type a (registers), type G (modutils assist) and
     * gcc2_compiled.
     */
    ss_sort_atn(&ss_merged);
    s = ss_merged.symbol;
    for (i = 0; i < ss_merged.used-1; ++i) {
	if (s->type == 'a' || s->type == 'G' ||
	    (s->type == 't' && !strcmp(s->name, "gcc2_compiled."))) {
	    DEBUG(3, "dropping non duplicate %s %c", s->name, s->type);
	    s->keep = 0;
	}
	else if (strcmp(s->name, (s+1)->name) == 0 &&
	    s->address == (s+1)->address) {
	    if (s->type != ' ')
		(s+1)->keep = 0;
	    else
		s->keep = 0;
	}
	++s;
    }
    ss_sort_atn(&ss_merged);    /* will remove dropped variables */

    if (save_system_map) {
	DEBUG(1, "writing merged map to %s", save_system_map);
	if (!(f = fopen_local(save_system_map, "w", procname)))
	    return;
	s = ss_merged.symbol;
	for (i = 0; i < ss_merged.used; ++i) {
	    if (s->keep) {
		fprintf(f, "%s %c %s",
		    format_address(s->address, options),
		    s->type, s->name);
		    if (s->module)
			fprintf(f, "\t[%s]", s->module);
		    fprintf(f, "\n");
	    }
	    ++s;
	}
    }

    /* The merged map may contain symbols with an address of 0, e.g.
     * Using_Versions.  These give incorrect results for low addresses in
     * map_address, such addresses map to "Using_Versions+xxx".  Remove
     * any addresses below (arbitrary) 4096 from the merged map.  AFAIK,
     * Linux does not use the first page on any arch.
     */
    for (i = 0; i < ss_merged.used; ++i) {
	if ((ss_merged.symbol+i)->address < 4096) {
	    DEBUG(3, "dropping low address %s %c", s->name, s->type);
	    (ss_merged.symbol+i)->keep = 0;
	}
	else
	    break;
    }
    if (i)
	ss_sort_atn(&ss_merged);    /* remove dropped variables */
}
