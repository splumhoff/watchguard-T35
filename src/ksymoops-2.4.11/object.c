/*
    object.c.

    object handling routines for ksymoops.  Read modules, vmlinux, etc.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>

/* Extract all symbols definitions from an object using nm */
static void read_nm_symbols(SYMBOL_SET *ss, const char *file, const OPTIONS *options)
{
    FILE *f;
    char *cmd, *line = NULL, **string = NULL;
    int i, cmd_strlen, size = 0;
    static char const procname[] = "read_nm_symbols";
    static char const nm_options[] = "--target=";

    if (!regular_file(file, procname))
	return;

    cmd_strlen = strlen(path_nm)+1 + strlen(file)+1;
    if (options->target)
	cmd_strlen += sizeof(nm_options) + strlen(options->target) + 1;
    cmd = malloc(cmd_strlen);
    if (!cmd)
	malloc_error("nm command");
    strcpy(cmd, path_nm);
    strcat(cmd, " ");
    if (options->target) {
	strcat(cmd, nm_options);
	strcat(cmd, options->target);
	strcat(cmd, " ");
    }
    strcat(cmd, file);
    DEBUG(2, "command '%s'", cmd);
    if (!(f = popen_local(cmd, procname)))
	return;
    free(cmd);

    while (fgets_local(&line, &size, f, procname)) {
	i = regexec(&re_nm, line, re_nm.re_nsub+1, re_nm_pmatch, 0);
	DEBUG(4, "regexec %d", i);
	if (i == 0) {
	    re_strings(&re_nm, line, re_nm_pmatch, &string);
	    add_symbol(ss, string[1], *string[2], 1, string[3]);
	}
    }

    pclose_local(f, procname);
    re_strings_free(&re_nm, &string);
    free(line);
    DEBUG(2, "%s used %d out of %d entries", ss->source, ss->used, ss->alloc);
}

/* Read the symbols from vmlinux */
void read_vmlinux(const OPTIONS *options)
{
    static char procname[] = "read_vmlinux";
    const char *vmlinux = options->vmlinux;
    if (!vmlinux)
	return;
    ss_init(&ss_vmlinux, "vmlinux");
    read_nm_symbols(&ss_vmlinux, vmlinux, options);
    if (ss_vmlinux.used) {
	ss_sort_na(&ss_vmlinux);
	extract_Version(&ss_vmlinux);
    }
    else
	WARNING("no kernel symbols in vmlinux, is %s a valid vmlinux file?",
	    vmlinux);
}

/* Read the symbols from one object (module) */
void read_object(const char *object, int i, const OPTIONS *options)
{
    static char procname[] = "read_object";
    ss_init(ss_object[i], object);
    read_nm_symbols(ss_object[i], object, options);
    if ((ss_object[i])->used) {
	ss_sort_na(ss_object[i]);
	extract_Version(ss_object[i]);
    }
    else
	WARNING("no symbols in %s", object);
}

/* Add a new entry to the list of objects */
static SYMBOL_SET *add_ss_object(const char *file)
{
    struct stat statbuf;
    int i;
    static char const procname[] = "add_ss_object";
    /* Avoid reading any object twice. */
    for (i = 0; i < ss_objects; ++i) {
	if (strcmp((ss_object[i])->source, file) == 0)
	    return(ss_object[i]);
    }
    ++ss_objects;
    ss_object = realloc(ss_object, ss_objects*sizeof(*ss_object));
    if (!ss_object)
	malloc_error("realloc ss_object");
    if (!(ss_object[ss_objects-1] = malloc(sizeof(*ss_object[0]))))
	malloc_error("alloc ss_object[n]");
    ss_init(ss_object[ss_objects-1], file);
    if (stat(file, &statbuf)) {
	ERROR("cannot stat(%s)", file);
	perror(prefix);
    }
    else {
	ss_object[ss_objects-1]->mtime = statbuf.st_mtime;
    }
    return(ss_object[ss_objects-1]);
}

/* Run a directory and its subdirectories, looking for *.o files */
static void find_objects(const char *dir)
{
    FILE *f;
    char *cmd, *line = NULL;
    int size = 0, files = 0;
    static char const procname[] = "find_objects";
    static char const options[] = " -follow '(' -path '*/build' -prune ')' -o -name '*.o' -print";

    cmd = malloc(strlen(path_find)+1+strlen(dir)+strlen(options)+1);
    if (!cmd)
	malloc_error("find command");
    strcpy(cmd, path_find);
    strcat(cmd, " ");
    strcat(cmd, dir);
    strcat(cmd, options);
    DEBUG(2, "command '%s'", cmd);
    if (!(f = popen_local(cmd, procname)))
	return;
    free(cmd);

    while (fgets_local(&line, &size, f, procname)) {
	DEBUG(2, "%s", line);
	add_ss_object(line);
	++files;
    }

    pclose_local(f, procname);
    if (!files)
	WARNING("no *.o files in %s.  Is %s a valid module directory?",
	    dir, dir);
}

/* Run the object paths, extracting anything that looks like a module name.
 */
static void run_object_paths(const OPTIONS *options)
{
    struct stat statbuf;
    int i;
    const char *file;
    static char const procname[] = "run_object_paths";
    DEBUG(1, "%s", "");

    for (i = 0; i < options->objects; ++i) {
	file = options->object[i];
	DEBUG_S(2, "checking %s - ", file);
	if (!stat(file, &statbuf) && S_ISDIR(statbuf.st_mode)) {
	    DEBUG_E(2, "%s", "directory, expanding");
	    find_objects(file);
	}
	else {
	    DEBUG_E(2, "%s", "not directory");
	    add_ss_object(file);
	}
    }
}

/* Take the user supplied list of objects which can include directories.
 * Expand directories into any *.o files.  The results are stored in
 * ss_object, leaving the user supplied options untouched.  If a ksyms
 * entry contains the name of its object file, use that by preference.
 * If --ignore-insmod-path is set then use the first object with the
 * same mtime and basename.
 */
void expand_objects(const OPTIONS *options)
{
    struct stat statbuf;
    int i, do_the_lot = 0, run_objects = 0;
    SYMBOL_SET *ss;
    const char *file;
    static char const procname[] = "expand_objects";

    for (i = 0; i < ss_ksyms_modules; ++i) {
	ss = ss_ksyms_module[i];
	if (!ss->object) {
	    do_the_lot = 1;
	    continue;
	}
	file = ss->object;
	if (options->ignore_insmod_path) {
	    /* Use the basename and mtime from insmod */
	    const char *basename = strrchr(file, '/'), *p;
	    int j;
	    if (basename)
		++basename;
	    else
		basename = file;
	    DEBUG(1, "looking for basename %s with mtime %ld for %s",
		basename, ss->mtime, ss->source);
	    if (!run_objects) {
		run_object_paths(options);
		run_objects = 1;
	    }
	    for (j = 0; j < ss_objects; ++j) {
		if (ss_object[j]->mtime == ss->mtime) {
		    p = strrchr(ss_object[j]->source, '/');
		    if (p)
			++p;
		    else
			p = ss_object[j]->source;
		    if (strcmp(p, basename) == 0) {
			DEBUG(1, "found %s for %s", ss_object[j]->source, file);
			free((char *)(ss->object));
			ss->object = strdup(ss_object[j]->source);
			if (!ss->object)
			    malloc_error(procname);
			file = ss->object;
			ss->related = add_ss_object(file);
			break;
		    }
		}
	    }
	    if (j == ss_objects) {
		DEBUG(1, "no match on basename %s with mtime %ld", basename, ss->mtime);
		do_the_lot = 1;
		continue;
	    }
	}
	else {
	    /* Use the full pathname from insmod */
	    DEBUG(1, "using %s for %s", file, ss->source);
	    if (stat(ss->object, &statbuf)) {
		ERROR("cannot stat(%s) for %s", file, ss->source);
		perror(prefix);
		do_the_lot = 1;
		continue;
	    }
	    if (statbuf.st_mtime != ss->mtime)
		WARNING("object %s for module %s has changed since load",
		    file, ss->source);
	    ss->related = add_ss_object(file);
	}
    }

    if (!do_the_lot) {
	DEBUG(1, "%s", "all ksyms modules map to specific object files");
	return;
    }
    DEBUG(1, "%s", "missing or mismatched modutils assists, doing it the hard way");
    if (!run_objects) {
	run_object_paths(options);
	run_objects = 1;
    }
}

/* Map a symbol type to a section code. 0 - text, 1 - data, 2 - read only data,
 * 3 - bss, 4 - sbss, 5 - C (cannot relocate), 6 - the rest.
 */
static int section(char type)
{
    switch (type) {
    case 'T':
    case 't':
    case 'W':		/* Resolved weak reference to text */
	return 0;
    case 'D':
    case 'd':
	return 1;
    case 'R':
    case 'r':
	return 2;
    case 'B':
    case 'b':
	return 3;
    case 'S':
    case 's':
	return 4;
    case 'C':
	return 5;
    default:
	return 6;
    }
}

/* Given ksyms module data which has a related object, create a copy of the
 * object data, adjusting the offsets to match where the module was loaded.
 */
SYMBOL_SET *adjust_object_offsets(SYMBOL_SET *ss)
{
    int i;
    addr_t adjust[] = {0, 0, 0, 0, 0, 0, 0};
    char type;
    SYMBOL *sk, *so;
    SYMBOL_SET *ssc;
    static char const procname[] = "adjust_object_offsets";

    ssc = ss_copy(ss->related);

    /* For common symbols, calculate the adjustment */
    for (i = 0; i < ss->used; ++i) {
	sk = ss->symbol+i;
	/* modutils assists have type G, the symbol contains the section name */
	if (sk->type == 'G') {
	    if (strstr(sk->name, "_S.text_"))
		type = 'T';
	    else if (strstr(sk->name, "_S.rodata_"))
		type = 'R';
	    else if (strstr(sk->name, "_S.data_"))
		type = 'D';
	    else if (strstr(sk->name, "_S.bss_"))
		type = 'B';
	    else if (strstr(sk->name, "_S.sbss_"))
		type = 'S';
	    else
		type = ' ';
	    /* Assumption: all sections start at relative offset 0 */
	    adjust[section(type)] = sk->address;
	}
	else if ((so = find_symbol_name(ssc, sk->name, NULL))) {
	    if (!adjust[section(so->type)])
		adjust[section(so->type)] = sk->address - so->address;
	}
    }
    for (i = 0; i < ssc->used; ++i) {
	so = ssc->symbol+i;
	/* Type C does not relocate well, silently ignore */
	if (so->type != 'C' && adjust[section(so->type)])
	    so->address += adjust[section(so->type)];
	else
	    so->keep = 0;  /* do not merge into final map */
    }
    /* Copy modutils assist symbols to object symbol table */
    for (i = 0; i < ss->used; ++i) {
	sk = ss->symbol+i;
	if (sk->type == 'G' || sk->type == 'g')
	    add_symbol_n(ssc, sk->address, sk->type, 1, sk->name);
    }
    ss->related = ssc;  /* map using adjusted copy */
    DEBUG(2, "%s text %llx rodata %llx data %llx bss %llx", ss->source, adjust[0], adjust[1], adjust[2], adjust[3]);
    return(ssc);
}
