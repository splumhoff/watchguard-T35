/*
    ksymoops.c.

    Read a Linux kernel Oops file and make the best stab at converting
    the code to instructions and mapping stack values to kernel
    symbols.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.
*/

#define VERSION "2.4.11"

#include "ksymoops.h"
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <endian.h>

char *prefix;
char *path_nm = INSTALL_PREFIX"/bin/"CROSS"nm";          /* env KSYMOOPS_NM */
char *path_find = "/usr/bin/find";      /* env KSYMOOPS_FIND */
char *path_objdump = INSTALL_PREFIX"/bin/"CROSS"objdump";    /* env KSYMOOPS_OBJDUMP */
int debug = 0;
int errors = 0;
int warnings = 0;
addr_t truncate_mask = ~(addr_t)0;

SYMBOL_SET   ss_vmlinux;
SYMBOL_SET   ss_ksyms_base;
SYMBOL_SET **ss_ksyms_module;
int          ss_ksyms_modules;
int          ss_ksyms_modules_known_objects;
SYMBOL_SET   ss_lsmod;
SYMBOL_SET **ss_object;
int          ss_objects;
SYMBOL_SET   ss_system_map;

SYMBOL_SET   ss_merged;   /* merged map with info from all sources */
SYMBOL_SET   ss_Version;  /* Version_ numbers where available */

/* Regular expression stuff */

regex_t     re_nm;
regmatch_t *re_nm_pmatch;
regex_t     re_bracketed_address;
regmatch_t *re_bracketed_address_pmatch;
regex_t     re_revbracketed_address;
regmatch_t *re_revbracketed_address_pmatch;
regex_t     re_unbracketed_address;
regmatch_t *re_unbracketed_address_pmatch;

static void usage(void)
{
    static const char usage_text[] =
	   "\t[-v vmlinux] [--vmlinux=vmlinux]\n\t\t\t\t\tWhere to read vmlinux\n"
	   "\t[-V] [--no-vmlinux]\t\tNo vmlinux is available\n"
	   "\t[-k ksyms] [--ksyms=ksyms]\tWhere to read ksyms\n"
	   "\t[-K] [--no-ksyms]\t\tNo ksyms is available\n"
	   "\t[-l lsmod] [--lsmod=lsmod]\tWhere to read lsmod\n"
	   "\t[-L] [--no-lsmod]\t\tNo lsmod is available\n"
	   "\t[-o object] [--object=object]\tDirectory containing modules or an\n\t\t\t\t\tindividual object name\n"
	   "\t[-O] [--no-object]\t\tNo objects are available\n"
	   "\t[-m system.map] [--system-map=system.map]\n\t\t\t\t\tWhere to read System.map\n"
	   "\t[-M] [--no-system-map]\t\tNo System.map is available\n"
	   "\t[-s save.map] [--save-map=save.map]\n\t\t\t\t\tSave consolidated map\n"
	   "\t[-S] [--short-lines]\t\tShort or long lines toggle\n"
	   "\t[-e] [--endian-swap]\t\tToggle endianess of code bytes\n"
	   "\t[-x] [--hex]\t\t\tHex or decimal toggle\n"
	   "\t[-1] [--one-shot]\t\tOne shot toggle (exit after first Oops)\n"
	   "\t[-i] [--ignore-insmod-path]\tIgnore path from __insmod symbols, toggle\n"
	   "\t[-I] [--ignore-insmod-all]\tIgnore all __insmod symbols, toggle\n"
	   "\t[-d] [--debug]\t\t\tIncrease debug level by 1\n"
	   "\t[-h] [--help]\t\t\tPrint help text\n"
	   "\t[-t target] [--target=target]\tTarget of oops log\n"
	   "\t[-a architecture] [--architecture=architecture]\n\t\t\t\t\tArchitecture of oops log\n"
	   "\t[-A \"address list\"] [--addresses=\"address list\"]\n\t\t\t\t\tAdhoc addresses to decode\n"
	   "\t< Oops.file\t\t\tOops report to decode\n"
	   "\n"
	   "\t\tAll flags can occur more than once.  With the exception "
		"of -o,\n"
	   "\t\t-d and -A which are cumulative, the last occurrence of each "
		"flag is\n"
	   "\t\tused.  Note that \"-v my.vmlinux -V\" will be taken as "
		"\"No vmlinux\n"
	   "\t\tavailable\" but \"-V -v my.vmlinux\" will read "
		"my.vmlinux.  You\n"
	   "\t\twill be warned about such combinations.\n"
	   "\n"
	   "\t\tEach occurrence of -d increases the debug level.\n"
	   "\n"
	   "\t\tEach -o flag can refer to a directory or to a single "
		"object\n"
	   "\t\tfile.  If a directory is specified then all *.o files in "
		"that\n"
	   "\t\tdirectory and its subdirectories are assumed to be "
		"modules.\n"
	   "\n"
	   "\t\tIf any of the vmlinux, object, ksyms or system.map "
		"options\n"
	   "\t\tcontain the string *r (*m, *n, *s) then it is replaced "
		"at run\n"
	   "\t\ttime by the current value of `uname -r` (-m, -n, -s).\n"
	   "\n"
	   "\t\tThe defaults can be changed in the Makefile, current "
		"defaults\n"
	   "\t\tare\n\n"
	   "\t\t\t"
#ifdef DEF_VMLINUX
	   "-v " DEF_VMLINUX
#else
	   "-V"
#endif
	   "\n"
	   "\t\t\t"
#ifdef DEF_KSYMS
	   "-k " DEF_KSYMS
#else
	   "-K"
#endif
	   "\n"
	   "\t\t\t"
#ifdef DEF_LSMOD
	   "-l " DEF_LSMOD
#else
	   "-L"
#endif
	   "\n"
	   "\t\t\t"
#ifdef DEF_OBJECTS
	   "-o " DEF_OBJECTS
#else
	   "-O"
#endif
	   "\n"
	   "\t\t\t"
#ifdef DEF_MAP
	   "-m " DEF_MAP
#else
	   "-M"
#endif
	   "\n"
#ifdef DEF_TARGET
	   "\t\t\t"
	   "-t " DEF_TARGET
	   "\n"
#endif
#ifdef DEF_ARCH
	   "\t\t\t"
	   "-a " DEF_ARCH
	   "\n"
#endif
	   "\n"
	  ;
    printf("Version " VERSION "\n");
    printf("usage: %s\n", prefix);
    printf("%s", usage_text);
}

/* Check if possibly conflicting options were specified */
static void multi_opt(int specl, int specu, char type, const char *using)
{
    static char procname[] = "multi_opt";
    if (specl && specu) {
	WARNING_S("you specified both -%c and -%c.  Using '",
	    type, toupper(type));
	if (using) {
	    WARNING_M("-%c %s", type, using);
	    if (type == 'o')
		WARNING_M("%s", " ...");
	    WARNING_E("%s", "'");
	}
	else
	    WARNING_E("-%c'", toupper(type));
    }
    else if (specl > 1 && type != 'o') {
	WARNING("you specified -%c more than once.  Using '-%c %s'",
	    type, type, using);
    }
    else if (specu > 1) {
	WARNING("you specified -%c more than once.  "
	    "Second and subsequent '-%c' ignored",
	    toupper(type), toupper(type));
    }
}

/* If a name contains *r (*m, *n, *s), replace with the current value of
 * `uname -r` (-m, -n, -s).  Actually uses uname system call rather than the
 * uname command but the result is the same.
 */
static void convert_uname(char **name)
{
    char *p, *newname, *oldname, *replacement;
    unsigned len;
    int free_oldname = 0;
    static char procname[] = "convert_uname";

    if (!*name)
	return;

    while ((p = strchr(*name, '*'))) {
	struct utsname buf;
	int i = uname(&buf);
	DEBUG(1, "%s in", *name);
	if (i) {
	    ERROR("uname failed, %s will not be processed", *name);
	    perror(prefix);
	    return;
	}
	switch (*(p+1)) {
	case 'r':
	    replacement = buf.release;
	    break;
	case 'm':
	    replacement = buf.machine;
	    break;
	case 'n':
	    replacement = buf.nodename;
	    break;
	case 's':
	    replacement = buf.sysname;
	    break;
	default:
	    ERROR("invalid replacement character '*%c' in %s", *(p+1), *name);
	    return;
	}
	len = strlen(*name)-2+strlen(replacement)+1;
	if (!(newname = malloc(len)))
	    malloc_error(procname);
	strncpy(newname, *name, (p-*name));
	strcpy(newname+(p-*name), replacement);
	strcpy(newname+(p-*name)+strlen(replacement), p+2);
	p = newname+(p-*name)+strlen(replacement);  /* no rescan */
	oldname = *name;
	*name = newname;
	if (free_oldname)
	    free(oldname);
	free_oldname = 1;
	DEBUG(1, "%s out", *name);
    }
    return;
}

/* Report if the option was specified or defaulted */
static void spec_or_default(int spec, int *some_spec) {
    if (spec) {
	printf(" (specified)\n");
	if (some_spec)
	    *some_spec = 1;
    }
    else
	printf(" (default)\n");
}

/* Extract adhoc addresses from a string and save for later.
 * An address is a string of at least 4 hex digits delimited by white space or
 * punctuation marks, possibly prefixed by 0x or 0X.
 */
static void adhoc_addresses(struct options *options, const unsigned char *string)
{
    int count, start;
    const unsigned char *p = string, *p1;
    char *address;
    while (*p) {
	while (*p && !isxdigit(*p))
	    ++p;
	if (!*p)
	    continue;
	start = p == string || isspace(*(p-1)) || ispunct(*(p-1));
	if (!start && p >= string+2 && *(p-2) == '0' && (*(p-1) == 'x' || *(p-1) == 'X')) {
	    start = p == string+2 || isspace(*(p-3)) || ispunct(*(p-3));
	}
	if (!start) {
	    ++p;
	    continue;
	}
	p1 = p;
	while (isxdigit(*++p)) {};
	if (p-p1 < 4)
	    continue;
	address = malloc(p-p1+1);
	if (!address)
	    malloc_error("adhoc_addresses");
	memcpy(address, p1, p-p1);
	address[p-p1] = '\0';
	for (count = 0; options->adhoc_addresses && options->adhoc_addresses[count]; ++count) {};
	options->adhoc_addresses = realloc(options->adhoc_addresses, (count+2)*sizeof(options->adhoc_addresses));
	if (!options->adhoc_addresses)
	    malloc_error("adhoc_addresses");
	options->adhoc_addresses[count] = address;
	options->adhoc_addresses[count+1] = NULL;
    }
}

/* Parse the options.  Verbose but what's new with getopt? */
static void parse(int argc, char **argv, struct options *options, int *spec_h)
{
    int spec_v = 0, spec_V = 0,
	spec_o = 0, spec_O = 0,
	spec_k = 0, spec_K = 0,
	spec_l = 0, spec_L = 0,
	spec_m = 0, spec_M = 0,
	spec_s = 0;
    struct utsname buf;
    static char const procname[] = "parse";
    struct option long_opts[] = {
	{"vmlinux", 1, 0, 'v'},
	{"no-vmlinux", 0, 0, 'V'},
	{"ksyms", 1, 0, 'k'},
	{"no-ksyms", 0, 0, 'K'},
	{"lsmod", 1, 0, 'l'},
	{"no-lsmod", 0, 0, 'L'},
	{"object", 1, 0, 'o'},
	{"no-object", 0, 0, 'O'},
	{"system-map", 1, 0, 'm'},
	{"no-system-map", 0, 0, 'M'},
	{"save-map", 1, 0, 's'},
	{"short-lines", 0, 0, 'S'},
	{"endian-swap", 0, 0, 'e'},
	{"hex", 0, 0, 'x'},
	{"one-shot", 0, 0, '1'},
	{"ignore-insmod-path", 0, 0, 'i'},
	{"ignore-insmod-all", 0, 0, 'I'},
	{"truncate", 1, 0, 'T'},
	{"debug", 0, 0, 'd'},
	{"help", 0, 0, 'h'},
	{"target", 1, 0, 't'},
	{"architecture", 1, 0, 'a'},
	{"addresses", 1, 0, 'A'},
	{0, 0, 0, 0}
    };

    int c, i, some_spec = 0;
    char *p, *before;

    while ((c = getopt_long(argc, argv, "v:Vk:Kl:Lo:Om:Ms:e1xSiIT:hdt:a:A:",
	    long_opts, NULL)) != EOF) {
	if (c != 'd')
	    DEBUG(1, "'%c' '%s'", c, optarg);
	switch(c) {
	case 'v':
	    options->vmlinux = optarg;
	    ++spec_v;
	    break;
	case 'V':
	    options->vmlinux = NULL;
	    ++spec_V;
	    break;
	case 'k':
	    options->ksyms = optarg;
	    ++spec_k;
	    break;
	case 'K':
	    options->ksyms = NULL;
	    ++spec_K;
	    break;
	case 'l':
	    options->lsmod = optarg;
	    ++spec_l;
	    break;
	case 'L':
	    options->lsmod = NULL;
	    ++spec_L;
	    break;
	case 'o':
	    if (!spec_o) {
		/* First -o, discard default value(s) */
		for (i = 0; i < options->objects; ++i)
		    free((options->object)[i]);
		free(options->object);
		options->object = NULL;
		options->objects = 0;
	    }
	    options->object = realloc(options->object,
		((options->objects)+1)*sizeof(*(options->object)));
	    if (!options->object)
		malloc_error("object");
	    if (!(p = strdup(optarg)))
		malloc_error("strdup -o");
	    else {
		(options->object)[(options->objects)++] = p;
		++spec_o;
	    }
	    break;
	case 'O':
	    ++spec_O;
	    for (i = 0; i < options->objects; ++i)
		free((options->object)[i]);
	    free(options->object);
	    options->object = NULL;
	    options->objects = 0;
	    break;
	case 'm':
	    options->system_map = optarg;
	    ++spec_m;
	    break;
	case 'M':
	    options->system_map = NULL;
	    ++spec_M;
	    break;
	case 's':
	    options->save_system_map = optarg;
	    ++spec_s;
	    break;
	case 'e':
	    options->endianess = !options->endianess;
	    break;
	case '1':
	    options->one_shot = !options->one_shot;
	    break;
	case 'x':
	    options->hex = !options->hex;
	    break;
	case 'S':
	    options->short_lines = !options->short_lines;
	    break;
	case 'i':
	    options->ignore_insmod_path = !options->ignore_insmod_path;
	    break;
	case 'I':
	    options->ignore_insmod_all = !options->ignore_insmod_all;
	    break;
	case 'T':
	    options->truncate = atoi(optarg);
	    if (options->truncate >= 8*sizeof(addr_t) || options->truncate <= 0) {
		options->truncate = 0;
		truncate_mask = ~(addr_t)0;
	    }
	    else {
		truncate_mask = (~(addr_t)0) >> (8*sizeof(addr_t)-options->truncate);
	    }
	    break;
	case 'h':
	    usage();
	    ++*spec_h;
	    break;
	case 'd':
	    ++debug;
	    break;
	case 't':
	    options->target = optarg;
	    break;
	case 'a':
	    options->architecture = optarg;
	    break;
	case 'A':
	    adhoc_addresses(options, optarg);
	    break;
	case '?':
	    if (c == 'c')
		printf("Option -c is obsolete, use -e toggle instead\n");
	    usage();
	    exit(2);
	}
    }

    options->filecount = argc - optind;
    options->filename = argv + optind;

    if (options->adhoc_addresses && !options->filecount) {
	static char *null = "/dev/null";
	static char **null_list = { &null };
	options->filecount = 1;
	options->filename = null_list;
    }

    /* Expand any requests for the current uname values */
    convert_uname(&options->vmlinux);
    if (options->objects) {
	for (i = 0; i < options->objects; ++i)
	    convert_uname(options->object+i);
    }
    convert_uname(&options->ksyms);
    convert_uname(&options->lsmod);
    convert_uname(&options->system_map);

    /* Check for multiple options specified */
    multi_opt(spec_v, spec_V, 'v', options->vmlinux);
    multi_opt(spec_o, spec_O, 'o', options->object ? *options->object : NULL);
    multi_opt(spec_k, spec_K, 'k', options->ksyms);
    multi_opt(spec_l, spec_L, 'l', options->lsmod);
    multi_opt(spec_m, spec_M, 'm', options->system_map);

    printf("ksymoops %s", VERSION);
    if (uname(&buf) == 0)
	printf(" on %s %s", buf.machine, buf.release);
    printf(".  Options used\n");
    printf("    ");
    if (options->vmlinux)
	printf(" -v %s", options->vmlinux);
    else
	printf(" -V");
    spec_or_default(spec_v || spec_V, &some_spec);

    printf("    ");
    if (options->ksyms)
	printf(" -k %s", options->ksyms);
    else
	printf(" -K");
    spec_or_default(spec_k || spec_K, &some_spec);

    printf("    ");
    if (options->lsmod)
	printf(" -l %s", options->lsmod);
    else
	printf(" -L");
    spec_or_default(spec_l || spec_L, &some_spec);

    printf("    ");
    if (options->objects) {
	for (i = 0; i < options->objects; ++i)
	    printf(" -o %s", (options->object)[i]);
    }
    else
	printf(" -O");
    spec_or_default(spec_o || spec_O, &some_spec);

    printf("    ");
    if (options->system_map)
	printf(" -m %s", options->system_map);
    else
	printf(" -M");
    spec_or_default(spec_m || spec_M, &some_spec);

    /* Toggles on one line */
    before = "    ";
    if (!options->short_lines) {
	printf("%s -S", before);
	before = "";
    }
    if (options->endianess) {
	printf("%s -e", before);
	before = "";
    }
    if (!options->hex) {
	printf("%s -x", before);
	before = "";
    }
    if (options->one_shot) {
	printf("%s -1", before);
	before = "";
    }
    if (options->ignore_insmod_path) {
	printf("%s -i", before);
	before = "";
    }
    if (options->ignore_insmod_all) {
	printf("%s -I", before);
	before = "";
    }
    if (!*before)
	printf("\n");

    before = "    ";
    if (options->truncate) {
	printf("%s -T %d", before, options->truncate);
	before = "";
    }
    if (!*before)
	printf("\n");

    /* Target and architecture on one line */
    before = "    ";
    if (options->target) {
	printf("%s -t %s", before, options->target);
	before = "";
    }
    if (options->architecture) {
	printf("%s -a %s", before, options->architecture);
	before = "";
    }
    if (!*before)
	printf("\n");

    printf("\n");

    if (!some_spec) {
	/* special warning, no procname */
	++warnings;
	WARNING_E("%s",
"Warning: You did not tell me where to find symbol information.  I will\n"
"assume that the log matches the kernel and modules that are running\n"
"right now and I'll use the default options above for symbol resolution.\n"
"If the current kernel and/or modules do not match the log, you can get\n"
"more accurate output by telling me the kernel version and where to find\n"
"map, modules, ksyms etc.  ksymoops -h explains the options.\n"
	    );
    }
}

/* Read environment variables */
static void read_env(const char *external, char **internal)
{
    char *p;
    static char const procname[] = "read_env";
    if ((p = getenv(external))) {
	*internal = p;
	DEBUG(1, "override %s=%s", external, *internal);
    }
    else
	DEBUG(1, "default %s=%s", external, *internal);
}

/* Print the available target and architectures. */
static void print_available_ta(const struct options *options)
{
    const char **list, **one;
    if (options->target && strcmp(options->target, "?") == 0) {
	printf("Targets supported by your libbfd\n");
	list = one = bfd_target_list();
	if (!list || !*list)
	    printf("    None, oh dear\n");
	else while (*one)
	    printf("    %s\n", *one++);
	free(list);
    }
    if (options->architecture && strcmp(options->architecture, "?") == 0) {
	printf("Architectures supported by your libbfd\n");
	list = one = bfd_arch_list();
	if (!list || !*list)
	    printf("    None, oh dear\n");
	else while (*one)
	    printf("    %s\n", *one++);
	free(list);
    }
    printf(
"Note that the above list comes from libbfd.  I have to assume that your\n"
"other binutils libraries (libiberty, libopcodes) and binutils programs\n"
"(nm and objdump) are in sync with libbfd.\n"
    );
}

/* Do all the hard work of reading the symbol sources */
void read_symbol_sources(const OPTIONS *options)
{
    int i;

    read_vmlinux(options);
    read_ksyms(options);
    /* No point in reading modules unless ksyms shows modules loaded */
    if (ss_ksyms_modules) {
	expand_objects(options);
	for (i = 0; i < ss_objects; ++i)
	    read_object(ss_object[i]->source, i, options);
    }
    else if (options->objects)
	printf("No modules in ksyms, skipping objects\n");
    /* No point in reading lsmod without ksyms */
    if (ss_ksyms_modules || ss_ksyms_base.used)
	read_lsmod(options);
    else if (options->lsmod)
	printf("No ksyms, skipping lsmod\n");
    read_system_map(options);
    merge_maps(options);
}

int main(int argc, char **argv)
{
    int spec_h = 0;     /* -h was specified */
    int ret;
    struct options options = {
	NULL,   /* vmlinux */
	NULL,   /* object */
	0,	/* objects */
	NULL,   /* ksyms */
	NULL,   /* lsmod */
	NULL,   /* system_map */
	NULL,   /* save_system_map */
	NULL,   /* filename */
	0,	/* filecount */
	1,	/* short_lines */
	0,	/* endianess */
	1,	/* hex */
	0,	/* one_shot */
	0,	/* ignore_insmod_path */
	0,	/* ignore_insmod_all */
	0,	/* truncate */
	NULL,   /* target */
	NULL,   /* architecture */
	NULL,   /* addresses */
	0	/* address_bits */
    };
    static char const procname[] = "main";

    prefix = *argv;
    setvbuf(stdout, NULL, _IONBF, 0);

#ifdef DEF_VMLINUX
    options.vmlinux = DEF_VMLINUX;
#endif
#ifdef DEF_KSYMS
    options.ksyms = DEF_KSYMS;
#endif
#ifdef DEF_LSMOD
    options.lsmod = DEF_LSMOD;
#endif
#ifdef DEF_OBJECTS
    {
	char *p;
	options.object = realloc(options.object,
	    (options.objects+1)*sizeof(*options.object));
	if (!options.object)
	    malloc_error("DEF_OBJECTS");
	if (!(p = strdup(DEF_OBJECTS)))
	    malloc_error("DEF_OBJECTS");
	else
	    options.object[options.objects++] = p;
    }
#endif
#ifdef DEF_MAP
    options.system_map = DEF_MAP;
#endif
#ifdef DEF_TARGET
    options.target = DEF_TARGET;
    {
	int target_byteorder;

	if (strstr(options.target, "-big"))
	    target_byteorder = __BIG_ENDIAN;
	else if (strstr(options.target, "-little"))
	    target_byteorder = __LITTLE_ENDIAN;
	else
	    target_byteorder = __BYTE_ORDER;

	if (target_byteorder != __BYTE_ORDER)
	    options.endianess = 1;
    }
#endif
#ifdef DEF_ARCH
    options.architecture = DEF_ARCH;
#endif

    /* Not all include files define __u16, __u32, __u64 so do it the hard way.
     * Any decent optimizing compiler will discard these tests at compile time
     * unless there is a problem.
     */
    if (sizeof(U16) != 2)
	FATAL("%s", "sizeof(U16) != 2");
    if (sizeof(U32) != 4)
	FATAL("%s", "sizeof(U32) != 4");
    if (sizeof(U64) != 8)
	FATAL("%s", "sizeof(U64) != 8");

    parse(argc, argv, &options, &spec_h);

    if (spec_h && options.filecount == 0)
	return(0);  /* just the help text */

    bfd_init();

    if ((options.target && strcmp(options.target, "?") == 0) ||
	(options.architecture && strcmp(options.architecture, "?") == 0)) {
	print_available_ta(&options);
	return(0);  /* just the available target/architecture */
    }

    if (errors)
	return(1);

    DEBUG(1, "level %d", debug);

    read_env("KSYMOOPS_NM", &path_nm);
    read_env("KSYMOOPS_FIND", &path_find);
    read_env("KSYMOOPS_OBJDUMP", &path_objdump);

    re_compile_common();
    ss_init_common();

    if (!options.one_shot)
	read_symbol_sources(&options);

    /* After all that work, it is finally time to read the Oops report */
    ret = Oops_read(&options);

    if (warnings || errors) {
	printf("\n");
	if (warnings)
	    printf("%d warning%s ",
		   warnings, warnings == 1 ? "" : "s");
	if (warnings && errors)
	    printf("and ");
	if (errors)
	    printf("%d error%s ", errors, errors == 1 ? "" : "s");
	printf("issued.  Results may not be reliable.\n");
	if (!ret)
	    return(1);
    }

    return(ret);
}
