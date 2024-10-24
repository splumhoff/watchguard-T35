/*
	ksymoops.h.

	Copyright Keith Owens <kaos@ocs.com.au>.
	Released under the GNU Public Licence, Version 2.

*/

#include <bfd.h>
#include <sys/types.h>
#include <regex.h>
#include <stdio.h>

#define FATAL(format, args...) \
    do { \
	printf("Fatal Error (%s): " format "\n", procname, args); \
	exit(2); \
       } while(0)

#define ERROR(format, args...) \
    do { \
	printf("Error (%s): " format "\n", procname, args); \
	++errors; \
       } while(0)

#define WARNING(format, args...) \
    do { \
	 printf("Warning (%s): " format "\n", procname, args); \
	 ++warnings; \
       } while(0)

#define WARNING_S(format, args...) \
    do { \
	 printf("Warning (%s): " format, procname, args); \
	 ++warnings; \
       } while(0)

#define WARNING_M(format, args...) \
    do { \
	 printf(format, args); \
       } while(0)

#define WARNING_E(format, args...) \
    do { \
	 printf(format "\n", args); \
       } while(0)

#define DEBUG(level, format, args...) \
    do { \
	if(debug >= level) { \
	    printf("DEBUG (%s): " format "\n", procname, args); \
	} \
       } while(0)

#define DEBUG_S(level, format, args...) \
    do { \
	if(debug >= level) { \
	    printf("DEBUG (%s): " format, procname, args); \
	} \
       } while(0)

#define DEBUG_E(level, format, args...) \
    do { \
	if(debug >= level) { \
	    printf(format "\n", args); \
	} \
       } while(0)

/* Not all compilers/libraries define __unn so create my own.  */
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;


/* Assume worst case for an address - 64 bits. */
typedef U64 addr_t;

extern char *prefix;
extern char *path_nm;		/* env KSYMOOPS_NM */
extern char *path_find;		/* env KSYMOOPS_FIND */
extern char *path_objdump;	/* env KSYMOOPS_OBJDUMP */
extern int debug;
extern int errors;
extern int warnings;
extern addr_t truncate_mask;

typedef struct symbol SYMBOL;

struct symbol {
	char *name;		/* name of symbol */
	char type;		/* type of symbol from nm/System.map */
	char keep;		/* keep this symbol in merged map? */
	addr_t address;		/* address in kernel */
	char *module;           /* module name (if any) */
};

/* Header for symbols from one particular source */

typedef struct symbol_set SYMBOL_SET;

struct symbol_set {
	char *source;			/* where the symbols came from */
	int used;			/* number of symbols used */
	int alloc;			/* number of symbols allocated */
	SYMBOL *symbol;			/* dynamic array of symbols */
	SYMBOL_SET *related;		/* any related symbol set */
	const char *object;		/* name of related object */
	time_t mtime;			/* modification time */
};

extern SYMBOL_SET   ss_vmlinux;
extern SYMBOL_SET   ss_ksyms_base;
extern SYMBOL_SET **ss_ksyms_module;
extern int          ss_ksyms_modules;
extern int          ss_ksyms_modules_known_objects;

extern SYMBOL_SET   ss_lsmod;
extern SYMBOL_SET **ss_object;
extern int          ss_objects;
extern SYMBOL_SET   ss_system_map;

extern SYMBOL_SET   ss_merged;	/* merged map with info from all sources */
extern SYMBOL_SET   ss_Version;	/* Version_ numbers where available */

/* Regular expression stuff */

extern regex_t     re_nm;
extern regmatch_t *re_nm_pmatch;
extern regex_t     re_bracketed_address;
extern regmatch_t *re_bracketed_address_pmatch;
extern regex_t     re_revbracketed_address;
extern regmatch_t *re_revbracketed_address_pmatch;
extern regex_t     re_unbracketed_address;
extern regmatch_t *re_unbracketed_address_pmatch;

/* Bracketed address: optional '[', required '<', at least 4 hex characters,
 * required '>', optional ']', optional spaces.
 */
#define BRACKETED_ADDRESS	"\\[*<([0-9a-fA-F]{4,})>\\]* *"

/* Reverse bracketed address: required '<', required '[', at least 4
 * hex characters, required ']', required '>', optional spaces.
 */
#define REVBRACKETED_ADDRESS	"<\\[([0-9a-fA-F]{4,})\\]> *"

#define UNBRACKETED_ADDRESS	"([0-9a-fA-F]{4,}) *"

/* The main set of options, save passing long sets of parameters around. */
struct options {
	char *vmlinux;
	char **object;
	int objects;
	char *ksyms;
	char *lsmod;
	char *system_map;
	char *save_system_map;
	char **filename;
	int filecount;
	int short_lines;
	int endianess;
	int hex;
	int one_shot;
	int ignore_insmod_path;
	int ignore_insmod_all;
	int truncate;
	const char *target;
	const char *architecture;
	char **adhoc_addresses;
	int address_bits;	/* not an option, derived from architecture */
	int vli;		/* not an option, derived from eip line */
};

typedef struct options OPTIONS;

/* ksymoops.c */
extern void read_symbol_sources(const OPTIONS *options);

/* io.c */
extern int regular_file(const char *file, const char *msg);
extern FILE *fopen_local(const char *file, const char *mode, const char *msg);
extern void fclose_local(FILE *f, const char *msg);
extern char *fgets_local(char **line, int *size, FILE *f, const char *msg);
extern int fwrite_local(void const *ptr, size_t size, size_t nmemb,
			FILE *stream, const char *msg);
extern FILE *popen_local(const char *cmd, const char *msg);
extern void pclose_local(FILE *f, const char *msg);

/* ksyms.c */
extern void read_ksyms(const OPTIONS *options);
extern void map_ksyms_to_modules(void);
extern void read_lsmod(const OPTIONS *options);
extern void compare_ksyms_lsmod(void);
extern void add_ksyms(const char *, char, const char *, const char *, const OPTIONS *options);

/* misc.c */
extern void malloc_error(const char *msg);
extern const char *format_address(addr_t address, const OPTIONS *options);
extern char *find_fullpath(const char *program);
extern U64 hexstring(const char *hex);

/* map.c */
extern void read_system_map(const OPTIONS *options);
extern void merge_maps(const OPTIONS *options);
extern void compare_maps(const SYMBOL_SET *ss1, const SYMBOL_SET *ss2,
			 int precedence);

/* object.c */
extern SYMBOL_SET *adjust_object_offsets(SYMBOL_SET *ss);
extern void read_vmlinux(const OPTIONS *options);
extern void expand_objects(const OPTIONS *options);
extern void read_object(const char *object, int i, const OPTIONS *options);

/* oops.c */
extern int Oops_read(OPTIONS *options);

/* re.c */
extern void re_compile(regex_t *preg, const char *regex, int cflags,
		       regmatch_t **pmatch);
extern void re_compile_common(void);
extern void re_strings(regex_t *preg, const char *text, regmatch_t *pmatch,
		       char ***string);
extern void re_strings_free(const regex_t *preg, char ***string);
extern void re_string_check(int need, int available, const char *msg);

#define RE_COMPILE(preg,regex,cflags,pmatch) \
    if(!(preg)->re_nsub) re_compile(preg, regex, cflags, pmatch);

/* symbol.c */
extern void ss_init(SYMBOL_SET *ss, const char *msg);
extern void ss_free(SYMBOL_SET *ss);
extern void ss_init_common(void);
extern SYMBOL *find_symbol_name(const SYMBOL_SET *ss, const char *symbol,
				int *start);
extern int add_symbol_n(SYMBOL_SET *ss, const addr_t address,
			const char type, const char keep, const char *symbol);
extern int add_symbol(SYMBOL_SET *ss, const char *address, const char type,
		      const char keep, const char *symbol);
extern char *map_address(const SYMBOL_SET *ss, const addr_t address,
			 const OPTIONS *options);
extern void ss_sort_atn(SYMBOL_SET *ss);
extern void ss_sort_na(SYMBOL_SET *ss);
extern SYMBOL_SET *ss_copy(const SYMBOL_SET *ss);
extern void add_Version(const char *version, const char *source);
extern void extract_Version(SYMBOL_SET *ss);
extern void compare_Version(void);

/* modutils interface */
#define MODUTILS_PREFIX "__insmod_"
