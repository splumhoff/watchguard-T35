/*
    oops.c.

    Oops processing for ksymoops.

    Copyright 1999 Keith Owens <kaos@ocs.com.au>.
    Released under the GNU Public Licence, Version 2.

 */

#include "ksymoops.h"
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Error detected by bfd */
static void Oops_bfd_perror(const char *msg)
{
    static char const procname[] = "Oops_bfd_perror";
    bfd_error_type err = bfd_get_error();
    ERROR("%s %s", msg, bfd_errmsg(err));
}

/* Open the ksymoops binary so we have an input bfd to copy data from. */
static void Oops_open_input_bfd(char **me, bfd **ibfd, const OPTIONS *options)
{
    char **matches, **match;
    static char const procname[] = "Oops_open_input_bfd";

    *me = find_fullpath(prefix);

    if (!*ibfd && !(*ibfd = bfd_openr(*me, NULL))) {
	Oops_bfd_perror(*me);
	FATAL("%s", "giving up");
    }

    if (!bfd_check_format_matches(*ibfd, bfd_object, &matches)) {
	Oops_bfd_perror(*me);
	if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
	    printf("%s Matching formats:", procname);
	    match = matches;
	    while (*match)
		printf(" %s", *match++);
	    printf("\n");
	    free(matches);
	}
	FATAL("%s", "giving up");
    }
}

/* If target or architecture is not already set, default to the same as
 * ksymoops.
 */
static void Oops_set_default_ta(const char *me, const bfd *ibfd,
				OPTIONS *options)
{
    static char const procname[] = "Oops_set_default_ta";
    const char *bt;
    const bfd_arch_info_type *bai;
    int t = 0, a = 0;

    if (!options->target) {
	bt = bfd_get_target(ibfd);	/* Bah, undocumented bfd function */
	if (!bt) {
	    Oops_bfd_perror(me);
	    FATAL("cannot get bfd_target for %s:", me);
	}
	options->target = bt;
	t = 1;
    }

    if (!options->architecture) {
	/* bfd_get_arch_info should really take (const bfd *) */
	bai = bfd_get_arch_info((bfd *)ibfd);
	if (!bai) {
	    Oops_bfd_perror(me);
	    FATAL("cannot get bfd_arch_info for %s:", me);
	}
	options->architecture = bai->printable_name;
	a = 1;
    }
    if (t || a) {
	printf("Using defaults from ksymoops");
	if (t)
	    printf(" -t %s", options->target);
	if (a)
	    printf(" -a %s", options->architecture);
	printf("\n");
    }
}

/* Write the code values to a file using bfd. */
static int Oops_write_bfd_data(const bfd *ibfd, bfd *obfd,
			       const char *code, int size)
{
    asection *isec, *osec;
    asymbol *osym;

    /* bfd_get_section_by_name should really take (const bfd *,) */
    if (!(isec = bfd_get_section_by_name((bfd *)ibfd, ".text"))) {
	Oops_bfd_perror("get_section");
	return(0);
    }
    if (!bfd_set_start_address(obfd, 0)) {
	Oops_bfd_perror("set_start_address");
	return(0);
    }
    if (!(osec = bfd_make_section(obfd, ".text"))) {
	Oops_bfd_perror("make_section");
	return(0);
    }
    if (!bfd_set_section_flags(obfd, osec,
	bfd_get_section_flags(ibfd, isec))) {
	Oops_bfd_perror("set_section_flags");
	return(0);
    }
    if (!bfd_set_section_alignment(obfd, osec,
	bfd_get_section_alignment(ibfd, isec))) {
	Oops_bfd_perror("set_section_alignment");
	return(0);
    }
    osec->output_section = osec;
    if (!(osym = bfd_make_empty_symbol(obfd))) {
	Oops_bfd_perror("make_empty_symbol");
	return(0);
    }
    osym->name = "_XXX";
    osym->section = osec;
    osym->flags = BSF_GLOBAL;
    osym->value = 0;
    if (!bfd_set_symtab(obfd, &osym, 1)) {
	Oops_bfd_perror("set_symtab");
	return(0);
    }
    if (!bfd_set_section_size(obfd, osec, size)) {
	Oops_bfd_perror("set_section_size");
	return(0);
    }
    if (!bfd_set_section_vma(obfd, osec, 0)) {
	Oops_bfd_perror("set_section_vma");
	return(0);
    }
    if (!bfd_set_section_contents(obfd, osec, (PTR) code, 0, size)) {
	Oops_bfd_perror("set_section_contents");
	return(0);
    }
    if (!bfd_close(obfd)) {
	Oops_bfd_perror("close(obfd)");
	return(0);
    }
    return 1;
}

/* Write the Oops code to a temporary file with suitable header and trailer. */
static char *Oops_code_to_file(const char *code, int size, const bfd *ibfd,
			       OPTIONS *options)
{
    char *file, *tmpdir;
    int fd;
    bfd *obfd;
    const bfd_arch_info_type *bai;
    static const char temp_suffix[] = "/ksymoops.XXXXXX";
    static char const procname[] = "Oops_code_to_file";

    /* Security fix, use mkstemp and honour TMPDIR */
    if (!(tmpdir = getenv("TMPDIR")) || !*tmpdir) {
#ifdef P_tmpdir
	tmpdir = P_tmpdir;
#else
	tmpdir = "/tmp";
#endif
    }
    file = malloc(strlen(tmpdir) + sizeof(temp_suffix));
    if (!file)
	malloc_error(procname);
    strcpy(file, tmpdir);
    strcat(file, temp_suffix);
    if ((fd = mkstemp(file)) < 0) {
	ERROR("Unable to open mkstemp file '%s'\n", file);
	perror(prefix);
	return(NULL);
    }
    close(fd);

    /* Set the target on the output file */
    if (!(obfd = bfd_openw(file, options->target))) {
	Oops_bfd_perror(file);
	if (bfd_get_error() == bfd_error_wrong_format)
	    printf("Sorry, looks like your binutils cannot "
		   "handle target %s\n", options->target);
	return(NULL);
    }
    bfd_set_format(obfd, bfd_object);

    /* Set the architecture on the output file */
    if (!(bai = bfd_scan_arch(options->architecture))) {
	Oops_bfd_perror("scan_arch for specified architecture");
	if (bfd_get_error() == bfd_error_wrong_format)
	    printf("Sorry, looks like your binutils cannot "
		"handle the specified architecture\n");
	return(NULL);
    }
    bfd_set_arch_info(obfd, bai);
    options->address_bits = bfd_arch_bits_per_address(obfd);

    if (!Oops_write_bfd_data(ibfd, obfd, code, size))
	return(NULL);
    return(file);
}

/* Run objdump against the binary Oops code */
static FILE *Oops_objdump(const char *file, const OPTIONS *options)
{
    char *cmd;
    FILE *f;
    int cmd_strlen;
    static char const objdump_options[] = "-dhf --target=";
    static char const procname[] = "Oops_objdump";

    /* remember to leave space for spaces */
    cmd_strlen = strlen(path_objdump)+1+strlen(objdump_options)+strlen(options->target)+1+strlen(file)+1;
    cmd = malloc(cmd_strlen);
    if (!cmd)
	malloc_error(procname);
    strcpy(cmd, path_objdump);
    strcat(cmd, " ");
    strcat(cmd, objdump_options);
    strcat(cmd, options->target);
    strcat(cmd, " ");
    strcat(cmd, file);
    DEBUG(2, "command '%s'", cmd);
    f = popen_local(cmd, procname);
    free(cmd);
    return(f);
}


/* Forward references. */
static const char *Oops_arch_to_eip(const OPTIONS *options);
static void Oops_decode_one_add(SYMBOL_SET *ss, const addr_t address,
				const char *line1, const char type, const OPTIONS *options);
static addr_t Oops_truncate_address(addr_t address, const OPTIONS *options);

/* Process one code line from objdump, ignore everything else */
static void Oops_decode_one(SYMBOL_SET *ss, const char *line, addr_t eip,
			    int adjust, const char type, const OPTIONS *options)
{
    int i;
    addr_t address, eip_relative;
    char *line2, *map, **string = NULL;
    static regex_t     re_Oops_objdump;
    static regmatch_t *re_Oops_objdump_pmatch;
    static char const procname[] = "Oops_decode_one";

    /* objdump output.  Optional whitespace, hex digits, optional
     * ' <_XXX+offset>', ':'.  The '+offset' after _XXX is also optional.
     * Older binutils output 'xxxxxxxx <_XXX+offset>:', newer versions do
     * '00000000 <_XXX>:' first followed by '      xx:' lines.
     *
     * Just to complicate things even more, objdump recognises jmp, call,
     * etc., converts the code to something like this :-
     * "   f: e8 32 34 00 00  call   3446 <_XXX+0x3446>"
     * Recognise this and append the eip adjusted address, followed by the
     * map_address text for that address.
     *
     * With any luck, objdump will take care of all such references which
     * makes this routine architecture insensitive.  No need to test for
     * i386 jmp, call or m68k swl etc.
     */
    RE_COMPILE(&re_Oops_objdump,
	    "^ *"
	    "([0-9a-fA-F]+)"				/* 1 */
	    "( <_XXX[^>]*>)?"				/* 2 */
	    ":"
	    "("						/* 3 */
	    ".* +<_XXX\\+0?x?([0-9a-fA-F]+)> *$"	/* 4 */
	    ")?"
	    ".*"
	    ,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_objdump_pmatch);

    i = regexec(&re_Oops_objdump, line, re_Oops_objdump.re_nsub+1,
	re_Oops_objdump_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i != 0)
	return;

    re_strings(&re_Oops_objdump, line, re_Oops_objdump_pmatch, &string);
    address = hexstring(string[1]);
    if (errno) {
	ERROR("Invalid hex value in objdump line, treated as zero - '%s'\n"
	    "  objdump line '%s'",
	    string[1], line);
	perror(prefix);
	address = 0;
    }
    address = Oops_truncate_address(address + eip + adjust, options);
    if (string[4]) {
	/* EIP relative data to be adjusted. */
	eip_relative = hexstring(string[4]);
	if (errno) {
	    ERROR("Invalid hex value in objdump line, treated as zero - '%s'\n"
		  "objdump line '%s'", string[4], line);
	    perror(prefix);
	    eip_relative = 0;
	}
	eip_relative = Oops_truncate_address(eip_relative + eip + adjust,
		options);
	map = map_address(&ss_merged, eip_relative, options);
	/* new text is original line, eip_relative in hex, map text */
	i = strlen(line)+1+2*sizeof(eip_relative)+1+strlen(map)+1;
	line2 = malloc(i);
	if (!line2)
	    malloc_error(procname);
	snprintf(line2, i, "%s %s %s",
	    line, format_address(eip_relative, options), map);
	Oops_decode_one_add(ss, address, line2, type, options);
	free(line2);
    }
    else
	Oops_decode_one_add(ss, address, line, type, options);  /* as is */
    re_strings_free(&re_Oops_objdump, &string);
}

/* Maximum number of code bytes to process.  It needs to be a multiple of 2 for
 * endianess swapping (-e).  Sparc and alpha dump 36 bytes so use 64.
 */
#define CODE_SIZE 64

/* Extract the hex values from the Code: line and convert to binary.
 * Strange as it seems, this is actually architecture independent, nothing in
 * this proc cares who created the Code: line.
 */
static int Oops_code_values(const unsigned char* code_text, unsigned char *code,
			    int *adjust, const OPTIONS *options)
{
    int byte = 0, byte_prev, len, ret = 1;
    U16 u16;
    U32 u32;
    U64 u64, value;
    char **string = NULL;
    const char *p;
    static regex_t     re_Oops_code_value;
    static regmatch_t *re_Oops_code_value_pmatch;
    static const char procname[] = "Oops_code_values";

    /* Given by re_Oops_code: code_text is a message (e.g. "general protection")
     * or one or more hex fields separated by spaces.  Some architectures
     * bracket the current instruction with '<' and '>', others use '(' and ')'.
     * The first character is nonblank.
     */
    if (!isxdigit(*code_text)) {
	WARNING("%s", "Code looks like message, not hex digits.  "
	    "No disassembly attempted.");
	return(0);
    }
    memset(code, '\0', CODE_SIZE);
    p = code_text;
    *adjust = 0;    /* EIP points to code byte 0 */

    /* Code values.  Hex values separated by white space.  On some systems, the
     * current instruction is bracketed in '<' and '>' or '(' and ')'.
     */
    RE_COMPILE(&re_Oops_code_value,
	    "^"
	    "("				/* 1 */
	      "([<(]?)"			/* 2 */
	      "([0-9a-fA-F]+)"		/* 3 */
	      "[)>]?"
	      " *"
	    ")"
	    "|(Bad .*)"			/* 4 */
	    ,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_code_value_pmatch);

    while (regexec(&re_Oops_code_value, p, re_Oops_code_value.re_nsub+1,
	    re_Oops_code_value_pmatch, 0) == 0) {
	re_strings(&re_Oops_code_value, p,
	    re_Oops_code_value_pmatch, &string);
	if (string[4] && *(string[4])) {
	    ret = 0;
	    break;	/* Bad EIP value, no code bytes */
	}
	if (byte >= CODE_SIZE)
	    break;
	value = hexstring(string[3]);
	if (errno) {
	    ERROR("Invalid hex value in code_value line, treated as zero - "
		  "'%s'\ncode_value line '%s'", string[3], code_text);
	    perror(prefix);
	    value = 0;
	}
	if (string[2] && *(string[2]))
	    *adjust = -byte;    /* this byte is EIP */
	/* On some architectures Code: is a stream of bytes, on some it is a
	 * stream of shorts, on some it is a stream of ints.  Consistent we're
	 * not!
	 */
	len = strlen(string[3]);
	byte_prev = byte;
	if (byte+len/2 > CODE_SIZE) {
	    WARNING("extra values in Code line, ignored - '%s'", string[3]);
	    break;
	}
	switch (len) {
	    case 2:
		code[byte++] = value;
		break;
	    case 4:
		u16 = value;
		memcpy(code+byte, &u16, sizeof(u16));
		byte += sizeof(u16);
		break;
	    case 8:
		u32 = value;
		memcpy(code+byte, &u32, sizeof(u32));
		byte += sizeof(u32);
		break;
	    case 16:
		u64 = value;
		memcpy(code+byte, &u64, sizeof(u64));
		byte += sizeof(u64);
		break;
	    default:
		ERROR("invalid value 0x%s in Code line, must be 2, 4, 8 or 16 "
		      "digits, value ignored", string[3]);
		break;
	}
	if (options->endianess && len != 2) {
	    /* Reverse the endianess of the bytes just stored */
	    char c;
	    len /= 2;
	    while (len) {
		c = code[byte_prev];
		code[byte_prev] = code[byte_prev+len-1];
		code[byte_prev+len-1] = c;
		++byte_prev;
		len -= 2;
	    }
	}
	p += re_Oops_code_value_pmatch[0].rm_eo;
    }

    if (ret && *p)
	WARNING("garbage '%s' at end of code line ignored", p);

    re_strings_free(&re_Oops_code_value, &string);
    return(ret);
}

/* Add an objdump line to the symbol table.  Just to keep the non-ix86 users
 * happy, replace XXX with an target specific string.
 */
static void Oops_decode_one_add(SYMBOL_SET *ss, const addr_t address,
				const char *line1, const char type, const OPTIONS *options)
{
    int i;
    const char *p1;
    char *line2, *p2, *p3;
    static char const procname[] = "Oops_decode_one_add";

    i = strlen(line1)+1;
    line2 = malloc(i);
    if (!line2)
	malloc_error(procname);
    memset(line2, '\0', i);
    p1 = line1;
    p2 = line2;
    while (1) {
	if ((p3 = strstr(p1, "_XXX"))) {
	    memcpy(p2, p1, p3-p1+1);
	    p2 += p3 - p1 + 1;
	    p1 = p3 + 4;
	    strcpy(p2, Oops_arch_to_eip(options));
	    while (*p2)
		++p2;
	}
	else {
	    strcpy(p2, p1);
	    break;
	}
    }
    if (strlen(line2) > strlen(line1))
	ERROR("line2 overrun, should never happen line1='%s'\nline2='%s'",
	    line1, line2);
    add_symbol_n(ss, address, type, 1, line2);
    free(line2);
}

/* Set the eip from the EIP line */
static void Oops_set_eip(const char *value, addr_t *eip, SYMBOL_SET *ss,
			 const char *me, const bfd *ibfd,
			 OPTIONS *options)
{
    static const char procname[] = "Oops_set_eip";
    char eip_name[10], *p;

    /* Ensure target is set so EIP text is correct */
    Oops_set_default_ta(me, ibfd, options);

    *eip = hexstring(value);
    if (errno) {
	ERROR("Invalid hex value in %s line, ignored - '%s'",
	    Oops_arch_to_eip(options), value);
	perror(prefix);
	*eip = 0;
    }
    snprintf(eip_name, sizeof(eip_name), ">>%-3s;",
	 Oops_arch_to_eip(options));
    while ((p = strstr(eip_name, " ;")))
	strncpy(p, "; ", 2);
    *eip = Oops_truncate_address(*eip, options);
    add_symbol_n(ss, *eip, 'E', 1, eip_name);
}

/* Look for the sysrq-t (show_task) process line and extract the command name */
static int Oops_set_task_name(const char *line, char ***string, int string_max, SYMBOL_SET *ss)
{
    int i;
    char id[64];
    static regex_t     re_Oops_set_task_name;
    static regmatch_t *re_Oops_set_task_name_pmatch;
    static const char procname[] = "Oops_set_task_name";

    /* Task header from show_task contains (L-TLB) or (NOTLB), the first field
     * is the command name.
     */
    RE_COMPILE(&re_Oops_set_task_name,
	    "^([^ ]+) +[^ ] *([^ ]+).*\\((L-TLB|NOTLB)\\)",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_set_task_name_pmatch);

    i = regexec(&re_Oops_set_task_name, line, re_Oops_set_task_name.re_nsub+1,
	re_Oops_set_task_name_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i)
	return(0);
    re_string_check(re_Oops_set_task_name.re_nsub+1, string_max, procname);
    re_strings(&re_Oops_set_task_name, line, re_Oops_set_task_name_pmatch, string);
    snprintf(id, sizeof(id), "Proc;  %s", (*string)[1]);
    add_symbol_n(ss, 0, 'I', 1, id);
    return(1);
}

static regex_t     re_Oops_regs;
static regmatch_t *re_Oops_regs_pmatch;

/* Decide if the line contains registers.  Returns first register name. */
static const char *Oops_regs(const char *line)
{
    int i;
    const char *p;
    static const char procname[] = "Oops_regs";

    RE_COMPILE(&re_Oops_regs,
	    "^("			/* 1 */
	    "(GP|o)?r[0-9]{1,2}"	/* 2 */
	    "|[goli][0-9]{1,2}"
	    "|[eR][ABCD]X"
	    "|[eR][DS]I"
	    "|RBP"
	    "|e[bs]p"
	    "|[fsi]p"
	    "|IRP"
	    "|SRP"
	    "|D?CCR"
	    "|USP"
	    "|MOF"
	    "|ret_pc"
	    ")"
	    " *[:=] *"
	    UNBRACKETED_ADDRESS		/* 3 */
	    " *"
	    ,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_regs_pmatch);

    i = regexec(&re_Oops_regs, line, re_Oops_regs.re_nsub+1,
	re_Oops_regs_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i)
	return(NULL);
    /* Some lines just have one register at the start followed by a set of
     * values, those lines are handled as special cases because I have to
     * generate the register numbers.  This code only routines lines that
     * contain exactly one value or each value is preceded by ':' or '='.
     * Lazy check, any ':' or '=' in the rest of the line will do.
     */
    p = line + re_Oops_regs_pmatch[0].rm_eo;
    if (*p && !index(p, ':') && !index(p, '='))
	return(NULL);
    return(line + re_Oops_regs_pmatch[0].rm_so);
}

/* Process a register line, extract addresses */
static void Oops_set_regs(const char *line, const char *p,
			       SYMBOL_SET *ss, const OPTIONS *options)
{
    addr_t reg;
    char regname[20];
    char **string = NULL;
    static const char procname[] = "Oops_set_regs";

    /* Loop over register names and unbracketed addresses */
    while (1) {
	if (regexec(&re_Oops_regs, p, re_Oops_regs.re_nsub+1,
	    re_Oops_regs_pmatch, 0) == 0) {
	    re_strings(&re_Oops_regs, p, re_Oops_regs_pmatch, &string);
	    reg = hexstring(string[3]);		/* contents */
	    if (errno) {
		ERROR(" Invalid hex value in register line, ignored - '%s'",
		      string[3]);
		perror(prefix);
		reg = 0;
	    }
	    reg = Oops_truncate_address(reg, options);
	    strcpy(regname, ">>");
	    strncpy(regname+2, string[1], sizeof(regname)-2);
	    if (strlen(regname) < sizeof(regname)-1)
		strcat(regname, ";");
	    add_symbol_n(ss, reg, 'R', 1, regname);
	    p += re_Oops_regs_pmatch[0].rm_eo;
	}
	else
	    break;
    }

    if (*p)
	WARNING("garbage '%s' at end of register line ignored", p);
    re_strings_free(&re_Oops_regs, &string);
}

/******************************************************************************/
/*                     Start architecture sensitive code                      */
/******************************************************************************/

static regex_t     re_Oops_cris_regs;
static regmatch_t *re_Oops_cris_regs_pmatch;
static int         s390_reg_num = -1;

/* All address arithmetic is done in 64 bit mode, truncate to the result to
 * a valid range for the target.
 */
static addr_t Oops_truncate_address(addr_t address, const OPTIONS *options)
{
    if (options->address_bits == 32 ||
	(options->target && (
	 strstr(options->target, "i370") ||
	 strstr(options->target, "s390")))) {
	U32 u32 = address;
	if (strstr(options->target, "i370") ||
	    strstr(options->target, "s390"))
	    u32 &= 0x7fffffff;  /* Really 31 bit addressing on i370/s390 */
	address = u32;
    }
    address &= truncate_mask;
    return(address);
}

/* Return a string to represent the eip, depending on the target. */
static const char *Oops_arch_to_eip(const OPTIONS *options)
{
    if (!options->target)
	return("???");
    if (strstr(options->target, "i386"))
	return("EIP");
    if (strstr(options->target, "alpha") ||
	strstr(options->target, "arm")   ||
	strstr(options->target, "cris")  ||
	strstr(options->target, "m68k")  ||
	strstr(options->target, "mips"))
	    return("PC");
    if (strstr(options->target, "sparc")) {
	if (strstr(options->target, "64"))
	    return("TPC");
	else
	    return("PC");
    }
    if (strstr(options->target, "powerpc"))
	return("NIP");
    if (strstr(options->target, "i370") ||
	strstr(options->target, "s390"))
	return("PSW");
    if (strstr(options->target, "ia64"))
	return("IP");
    if (strstr(options->target, "x86_64") ||
	strstr(options->target, "x86-64") ||
	strstr(options->target, "x8664"))
	    return("RIP");
    return("???");
}

/* Look for the EIP: line, returns start of the relevant hex value */
static char *Oops_eip(const char *line, char ***string, int string_max,
		      const bfd *ibfd, OPTIONS *options)
{
    int i;
    static regex_t     re_Oops_eip_sparc;
    static regmatch_t *re_Oops_eip_sparc_pmatch;
    static regex_t     re_Oops_eip_sparc64;
    static regmatch_t *re_Oops_eip_sparc64_pmatch;
    static regex_t     re_Oops_eip_ppc;
    static regmatch_t *re_Oops_eip_ppc_pmatch;
    static regex_t     re_Oops_eip_mips;
    static regmatch_t *re_Oops_eip_mips_pmatch;
    static regex_t     re_Oops_eip_kdb;
    static regmatch_t *re_Oops_eip_kdb_pmatch;
    static regex_t     re_Oops_eip_ia64;
    static regmatch_t *re_Oops_eip_ia64_pmatch;
    static regex_t     re_Oops_mca_ip_ia64;
    static regmatch_t *re_Oops_mca_ip_ia64_pmatch;
    static regex_t     re_Oops_eip_cris;
    static regmatch_t *re_Oops_eip_cris_pmatch;
    static regex_t     re_Oops_eip_other;
    static regmatch_t *re_Oops_eip_other_pmatch;
    static const char procname[] = "Oops_eip";

    /* Oops 'EIP:' line for sparc, actually PSR followed by PC */
    RE_COMPILE(&re_Oops_eip_sparc,
	    "^PSR: [0-9a-fA-F]+ PC: " UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_sparc_pmatch);

    i = regexec(&re_Oops_eip_sparc, line, re_Oops_eip_sparc.re_nsub+1,
	re_Oops_eip_sparc_pmatch, 0);
    DEBUG(4, "regexec sparc %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_sparc.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_sparc, line, re_Oops_eip_sparc_pmatch,
	    string);
	return((*string)[re_Oops_eip_sparc.re_nsub]);
    }

    /* Oops 'EIP:' line for sparc64, actually TSTATE followed by TPC */
    RE_COMPILE(&re_Oops_eip_sparc64,
	    "^TSTATE: [0-9a-fA-F]{16} TPC: " UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_sparc64_pmatch);

    re_string_check(re_Oops_eip_sparc64.re_nsub+1, string_max, procname);
    i = regexec(&re_Oops_eip_sparc64, line, re_Oops_eip_sparc64.re_nsub+1,
	re_Oops_eip_sparc64_pmatch, 0);
    DEBUG(4, "regexec sparc64 %d", i);
    if (i == 0) {
	/* Special case for sparc64.  If the output target is defaulting to the
	 * same format as ksymoops then the default is wrong, kernel is 64 bit,
	 * ksymoops is 32 bit.  When we see an EIP from sparc64, set the correct
	 * default.
	 */
	if (!options->target && !options->architecture &&
	    strcmp(bfd_get_target(ibfd), "elf32-sparc")) {
	    options->target = "elf64-sparc";
	    options->architecture = "sparc:v9a";
	}
	re_strings(&re_Oops_eip_sparc64, line, re_Oops_eip_sparc64_pmatch,
	    string);
	return((*string)[re_Oops_eip_sparc64.re_nsub]);
    }

    /* Oops 'EIP:' line for PPC, all over the place */
    RE_COMPILE(&re_Oops_eip_ppc,
	    "("
	      "kernel pc "
	      "|trap at PC: "
	      "|bad area pc "
	      "|NIP: "
	    ")"
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_ppc_pmatch);

    i = regexec(&re_Oops_eip_ppc, line, re_Oops_eip_ppc.re_nsub+1,
	re_Oops_eip_ppc_pmatch, 0);
    DEBUG(4, "regexec ppc %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_ppc.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_ppc, line, re_Oops_eip_ppc_pmatch,
	    string);
	return((*string)[re_Oops_eip_ppc.re_nsub]);
    }

    /* Oops 'EIP:' line for mips, epc, optional white space, ':',
     * optional white space, unbracketed address.
     */
    RE_COMPILE(&re_Oops_eip_mips,
	    "^epc *:+ *"
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_mips_pmatch);

    i = regexec(&re_Oops_eip_mips, line, re_Oops_eip_mips.re_nsub+1,
	re_Oops_eip_mips_pmatch, 0);
    DEBUG(4, "regexec mips %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_mips.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_mips, line, re_Oops_eip_mips_pmatch,
	    string);
	return((*string)[re_Oops_eip_mips.re_nsub]);
    }

    /* Oops 'IP' line for ia64, space, ip, optional white space, ':',
     * optional white space, bracketed address.
     */
    RE_COMPILE(&re_Oops_eip_ia64,
	    " ip *:+ *"
	    BRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_ia64_pmatch);

    i = regexec(&re_Oops_eip_ia64, line, re_Oops_eip_ia64.re_nsub+1,
	re_Oops_eip_ia64_pmatch, 0);
    DEBUG(4, "regexec eip_ia64 %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_ia64.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_ia64, line, re_Oops_eip_ia64_pmatch,
	    string);
	return((*string)[re_Oops_eip_ia64.re_nsub]);
    }

    /* MCA 'IP' line for ia64.  [x]ip at start of line, space, '('regname'),
     * spaces, :, space, 0x, UNBRACKETED_ADDRESS.
     */
    RE_COMPILE(&re_Oops_mca_ip_ia64,
	    "[xi]ip *\\([^)]*\\) *: *"
	    "0x" UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_mca_ip_ia64_pmatch);

    i = regexec(&re_Oops_mca_ip_ia64, line, re_Oops_mca_ip_ia64.re_nsub+1,
	re_Oops_mca_ip_ia64_pmatch, 0);
    DEBUG(4, "regexec mca_ip_ia64 %d", i);
    if (i == 0) {
	re_string_check(re_Oops_mca_ip_ia64.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_mca_ip_ia64, line, re_Oops_mca_ip_ia64_pmatch,
	    string);
	return((*string)[re_Oops_mca_ip_ia64.re_nsub]);
    }

    /* SGI kdb backtrace EIP.  At start of line, 0x unbracketed
     * address, 0x unbracketed address, anything, '+0x'.  The second
     * unbracketed address is the EIP.
     * The initial kdb reports has lines like
     * Entering kdb on processor 2 due to panic @ 0x5005f426
     */
    RE_COMPILE(&re_Oops_eip_kdb,
	    "("					/* 1 */
	      "^0x" UNBRACKETED_ADDRESS		/* 2 */
	      "0x" UNBRACKETED_ADDRESS		/* 3 */
	      ".*+0x"
	    ")"
	    "|("				/* 4 */
	      "^Entering kdb on processor.*0x"
	      UNBRACKETED_ADDRESS		/* 5 */
	    ")",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_kdb_pmatch);

    i = regexec(&re_Oops_eip_kdb, line, re_Oops_eip_kdb.re_nsub+1,
	re_Oops_eip_kdb_pmatch, 0);
    DEBUG(4, "regexec kdb %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_kdb.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_kdb, line, re_Oops_eip_kdb_pmatch,
	    string);
	if ((*string)[3] && *((*string)[3]))
	    return((*string)[3]);
	if ((*string)[5] && *((*string)[5]))
	    return((*string)[5]);
    }

    /* Oops 'IRP' line for CRIS: "^IRP: " unbracketed address.
     * Keep spacing optional.
     */
    RE_COMPILE(&re_Oops_eip_cris,
	    "^ *IRP *: *"
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_cris_pmatch);

    i = regexec(&re_Oops_eip_cris, line, re_Oops_eip_cris.re_nsub+1,
	re_Oops_eip_cris_pmatch, 0);
    DEBUG(4, "regexec cris %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_cris.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_cris, line, re_Oops_eip_cris_pmatch,
	    string);
	return((*string)[re_Oops_eip_cris.re_nsub]);
    }

    /* Oops 'EIP:' line for other architectures */
    RE_COMPILE(&re_Oops_eip_other,
		"^("
		  "EIP: +.*"	/* i386 */
		  "|RIP: +.*"	/* x86_64 */
		  "|PC *= *"	/* m68k, alpha */
		  "|pc *: *"	/* arm */
		")"
		BRACKETED_ADDRESS
		,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_other_pmatch);

    i = regexec(&re_Oops_eip_other, line, re_Oops_eip_other.re_nsub+1,
	re_Oops_eip_other_pmatch, 0);
    DEBUG(4, "regexec other %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_other.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_other, line, re_Oops_eip_other_pmatch,
	    string);
	return((*string)[re_Oops_eip_other.re_nsub]);
    }
    return(NULL);
}

/* Look for the arm lr line, returns start of the relevant hex value */
static char *Oops_arm_lr(const char *line, char ***string, int string_max)
{
    int i;
    static regex_t     re_Oops_arm_lr;
    static regmatch_t *re_Oops_arm_lr_pmatch;
    static const char procname[] = "Oops_arm_lr";

    RE_COMPILE(&re_Oops_arm_lr,
	    "pc *: *" BRACKETED_ADDRESS		/* 1 */
	    " *lr *: *" BRACKETED_ADDRESS,	/* 2 */
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_arm_lr_pmatch);

    i = regexec(&re_Oops_arm_lr, line, re_Oops_arm_lr.re_nsub+1,
	re_Oops_arm_lr_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_arm_lr.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_arm_lr, line, re_Oops_arm_lr_pmatch,
	    string);
	return((*string)[re_Oops_arm_lr.re_nsub]);
    }
    return(NULL);
}

/* Set the arm lr from the lr line */
static void Oops_set_arm_lr(const char *value, SYMBOL_SET *ss)
{
    static const char procname[] = "Oops_set_arm_lr";
    addr_t ra;
    ra = hexstring(value);
    if (errno) {
	ERROR(" Invalid hex value in ra line, ignored - '%s'", value);
	perror(prefix);
	ra = 0;
    }
    add_symbol_n(ss, ra, 'R', 1, ">>LR; ");
}

/* Look for the alpha ra line, returns start of the relevant hex value */
static char *Oops_alpha_ra(const char *line, char ***string, int string_max)
{
    int i;
    static regex_t     re_Oops_alpha_ra;
    static regmatch_t *re_Oops_alpha_ra_pmatch;
    static const char procname[] = "Oops_alpha_ra";

    RE_COMPILE(&re_Oops_alpha_ra,
	    "pc *= *" BRACKETED_ADDRESS		/* 1 */
	    " *ra *= *" BRACKETED_ADDRESS,	/* 2 */
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_alpha_ra_pmatch);

    i = regexec(&re_Oops_alpha_ra, line, re_Oops_alpha_ra.re_nsub+1,
	re_Oops_alpha_ra_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_alpha_ra.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_alpha_ra, line, re_Oops_alpha_ra_pmatch,
	    string);
	return((*string)[re_Oops_alpha_ra.re_nsub]);
    }
    return(NULL);
}

/* Set the alpha ra from the ra line */
static void Oops_set_alpha_ra(const char *value, SYMBOL_SET *ss)
{
    static const char procname[] = "Oops_set_alpha_ra";
    addr_t ra;
    ra = hexstring(value);
    if (errno) {
	ERROR(" Invalid hex value in ra line, ignored - '%s'", value);
	perror(prefix);
	ra = 0;
    }
    add_symbol_n(ss, ra, 'R', 1, ">>RA; ");
}

/* Look for the alpha spinlock stuck line, returns TRUE with string[1]
 * containing the PC, string[2] containing the previous PC.
 */
static int Oops_eip_alpha_spinlock(const char *line, char ***string,
				   int string_max, OPTIONS *options)
{
    int i;
    static regex_t     re_Oops_eip_alpha_spinlock;
    static regmatch_t *re_Oops_eip_alpha_spinlock_pmatch;
    static const char procname[] = "Oops_eip_alpha_spinlock";

    RE_COMPILE(&re_Oops_eip_alpha_spinlock,
	    "^spinlock stuck at " UNBRACKETED_ADDRESS
	    ".*owner.*at " UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_alpha_spinlock_pmatch);

    i = regexec(&re_Oops_eip_alpha_spinlock, line,
	re_Oops_eip_alpha_spinlock.re_nsub+1,
	re_Oops_eip_alpha_spinlock_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_alpha_spinlock.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_alpha_spinlock, line,
	    re_Oops_eip_alpha_spinlock_pmatch, string);
	return(1);
    }
    return(0);
}

/* Look for the sparc spinlock stuck line, returns TRUE with string[1]
 * containing the lock address, string[2] containing the caller PC,
 * string[3] containing the owning PC.
 */
static int Oops_eip_sparc_spinlock(const char *line, char ***string,
				   int string_max, OPTIONS *options)
{
    int i;
    static regex_t     re_Oops_eip_sparc_spinlock;
    static regmatch_t *re_Oops_eip_sparc_spinlock_pmatch;
    static const char procname[] = "Oops_eip_sparc_spinlock";

    RE_COMPILE(&re_Oops_eip_sparc_spinlock,
	    "^spin_lock[^ ]*\\(" UNBRACKETED_ADDRESS
	    ".*stuck at *" UNBRACKETED_ADDRESS
	    ".*PC\\(" UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_eip_sparc_spinlock_pmatch);

    i = regexec(&re_Oops_eip_sparc_spinlock, line,
	re_Oops_eip_sparc_spinlock.re_nsub+1,
	re_Oops_eip_sparc_spinlock_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_eip_sparc_spinlock.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_eip_sparc_spinlock, line,
	    re_Oops_eip_sparc_spinlock_pmatch, string);
	return(1);
    }
    return(0);
}

/* Look for the mips ra line, returns start of the relevant hex value */
static char *Oops_mips_ra(const char *line, char ***string, int string_max)
{
    int i;
    static regex_t     re_Oops_mips_ra;
    static regmatch_t *re_Oops_mips_ra_pmatch;
    static const char procname[] = "Oops_mips_ra";

    /* Oops 'ra:' line for mips, ra, optional white space, one or
     * more '=', optional white space, unbracketed address.
     */
    RE_COMPILE(&re_Oops_mips_ra,
	    "ra *=+ *"
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_mips_ra_pmatch);

    i = regexec(&re_Oops_mips_ra, line, re_Oops_mips_ra.re_nsub+1,
	re_Oops_mips_ra_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_mips_ra.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_mips_ra, line, re_Oops_mips_ra_pmatch,
	    string);
	return((*string)[re_Oops_mips_ra.re_nsub]);
    }
    return(NULL);
}

/* Set the mips ra from the ra line */
static void Oops_set_mips_ra(const char *value, SYMBOL_SET *ss)
{
    static const char procname[] = "Oops_set_mips_ra";
    addr_t ra;
    ra = hexstring(value);
    if (errno) {
	ERROR(" Invalid hex value in ra line, ignored - '%s'", value);
	perror(prefix);
	ra = 0;
    }
    add_symbol_n(ss, ra, 'R', 1, ">>RA; ");
}

/* Extract mips registers. */
static void Oops_mips_regs(const char *line, SYMBOL_SET *ss, const OPTIONS *options)
{
    int i, reg_num;
    addr_t reg;
    char regname[7];
    char **string = NULL;
    const char *p;
    static regex_t     re_Oops_mips_regs;
    static regmatch_t *re_Oops_mips_regs_pmatch;
    static const char procname[] = "Oops_mips_regs";

    RE_COMPILE(&re_Oops_mips_regs,
	    "^(\\$[0-9]{1,2}) *: *"	/* 1 */
	    UNBRACKETED_ADDRESS		/* 2 */
	    ,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_mips_regs_pmatch);

    i = regexec(&re_Oops_mips_regs, line, re_Oops_mips_regs.re_nsub+1,
	re_Oops_mips_regs_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i)
	return;
    p = line + re_Oops_mips_regs_pmatch[0].rm_so + 1;
    reg_num = strtoul(p, NULL, 10);
    p = line + re_Oops_mips_regs_pmatch[2].rm_so;
    /* Loop over unbracketed addresses */
    while (1) {
	if (regexec(&re_unbracketed_address, p, re_unbracketed_address.re_nsub+1,
	    re_unbracketed_address_pmatch, 0) == 0) {
	    re_strings(&re_unbracketed_address, p, re_unbracketed_address_pmatch, &string);
	    reg = hexstring(string[1]);		/* contents */
	    if (errno) {
		ERROR(" Invalid hex value in register line, ignored - '%s'",
		      string[1]);
		perror(prefix);
		break;
	    }
	    reg = Oops_truncate_address(reg, options);
	    snprintf(regname, sizeof(regname), ">>$%d;", reg_num);
	    add_symbol_n(ss, reg, 'R', 1, regname);
	    p += re_unbracketed_address_pmatch[0].rm_eo;
	    ++reg_num;
	    if (reg_num == 26)
		reg_num += 2;			/* skip k0, k1 */
	}
	else
	    break;
    }

    if (*p)
	WARNING("garbage '%s' at end of mips register line ignored", p);
    re_strings_free(&re_unbracketed_address, &string);
}

/* Look for the ia64 b0 line and set all the values */
static void Oops_set_ia64_b0(const char *line, char ***string, int string_max, SYMBOL_SET *ss)
{
    int i;
    addr_t b;
    char label[6];
    static regex_t     re_Oops_ia64_b0;
    static regmatch_t *re_Oops_ia64_b0_pmatch;
    static const char procname[] = "Oops_ia64_b0";

    /* Oops 'b0' line for ia64, b0 : unbracketed address, may be
     * repeated for b6, b7.  MCA has b0 '('regname')' : 0x unbracketed address.
     */
    RE_COMPILE(&re_Oops_ia64_b0,
	    "^b[0-7] *(\\([^)]*\\) *)?: *(0x)?"
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_ia64_b0_pmatch);

    /* Scan entire line for b0, b6, b7 */
    while(1) {
	i = regexec(&re_Oops_ia64_b0, line, re_Oops_ia64_b0.re_nsub+1,
	    re_Oops_ia64_b0_pmatch, 0);
	DEBUG(4, "regexec %d", i);
	if (i)
	    break;
	re_string_check(re_Oops_ia64_b0.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_ia64_b0, line, re_Oops_ia64_b0_pmatch, string);
	strcpy(label, ">>bx; ");
	label[3] = line[1];	/* register number */
	b = hexstring((*string)[re_Oops_ia64_b0.re_nsub]);
	if (errno) {
	    ERROR(" Invalid hex value in b%c line, ignored - '%s'",
		line[1], (*string)[re_Oops_ia64_b0.re_nsub]);
	    perror(prefix);
	    b = 0;
	}
	add_symbol_n(ss, b, 'R', 1, label);
	line += re_Oops_ia64_b0_pmatch[0].rm_eo;
    }
}

/* Look for the sparc register dump lines end */
static int Oops_sparc_regdump(const char *line, char ***string)
{
    int i;
    static regex_t     re_Oops_sparc_regdump;
    static regmatch_t *re_Oops_sparc_regdump_pmatch;
    static const char procname[] = "Oops_sparc_regdump";

    RE_COMPILE(&re_Oops_sparc_regdump,
	    "^("
		"i[04]: "
		"|Instruction DUMP: "
		"|Caller\\["
	    ")",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_sparc_regdump_pmatch);

    i = regexec(&re_Oops_sparc_regdump, line, re_Oops_sparc_regdump.re_nsub+1,
	re_Oops_sparc_regdump_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0)
	return 1;
    return 0;
}

/* Look for the i370 PSW line, returns start of the relevant hex value */
static char *Oops_i370_psw(const char *line, char ***string, int string_max)
{
    int i;
    static regex_t     re_Oops_i370_psw;
    static regmatch_t *re_Oops_i370_psw_pmatch;
    static const char procname[] = "Oops_i370_psw";

    /* Oops 'PSW:' line for i370, PSW at start of line, value is second
     * address.
     */
    RE_COMPILE(&re_Oops_i370_psw,
	    "^PSW *flags: *"
	    UNBRACKETED_ADDRESS		/* flags */
	    " *PSW addr: *"
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_i370_psw_pmatch);

    i = regexec(&re_Oops_i370_psw, line, re_Oops_i370_psw.re_nsub+1,
	re_Oops_i370_psw_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_i370_psw.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_i370_psw, line, re_Oops_i370_psw_pmatch,
	    string);
	return((*string)[re_Oops_i370_psw.re_nsub]);
    }
    return(NULL);
}

/* Set the i370 PSW from the PSW line */
static void Oops_set_i370_psw(const char *value, SYMBOL_SET *ss,
			      const char *me, const bfd *ibfd,
			      OPTIONS *options)
{
    static const char procname[] = "Oops_set_i370_psw";
    addr_t PSW;
    /* Ensure target is set so EIP text is correct */
    Oops_set_default_ta(me, ibfd, options);
    PSW = hexstring(value);
    if (errno) {
	ERROR(" Invalid hex value in PSW line, ignored - '%s'", value);
	perror(prefix);
	PSW = 0;
    }
    PSW = Oops_truncate_address(PSW, options);
    add_symbol_n(ss, PSW, 'E', 1, ">>PSW;");
}

/* Look for the s390 PSW line, returns start of the relevant hex value */
static char *Oops_s390_psw(const char *line, char ***string, int string_max)
{
    int i;
    static regex_t     re_Oops_s390_psw;
    static regmatch_t *re_Oops_s390_psw_pmatch;
    static const char procname[] = "Oops_s390_psw";

    /* Oops 'Kernel PSW:' line for s390, PSW at start of line, value is
     * second address.
     */
    RE_COMPILE(&re_Oops_s390_psw,
	    "^Kernel PSW: *"
	    UNBRACKETED_ADDRESS		/* flags */
	    UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_s390_psw_pmatch);

    i = regexec(&re_Oops_s390_psw, line, re_Oops_s390_psw.re_nsub+1,
	re_Oops_s390_psw_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_s390_psw.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_s390_psw, line, re_Oops_s390_psw_pmatch,
	    string);
	return((*string)[re_Oops_s390_psw.re_nsub]);
    }
    return(NULL);
}

/* No need for Oops_set_s390_psw, can use Oops_set_i370_psw for both */
#define Oops_set_s390_psw(line, string, me, ibfd, options) \
	Oops_set_i370_psw(line, string, me, ibfd, options)

/* Extract ppc registers. */
static void Oops_ppc_regs(const char *line, SYMBOL_SET *ss, const OPTIONS *options)
{
    int i, reg_num;
    addr_t reg;
    char regname[9];
    char **string = NULL;
    const char *p;
    static regex_t     re_Oops_ppc_regs;
    static regmatch_t *re_Oops_ppc_regs_pmatch;
    static const char procname[] = "Oops_ppc_regs";

    RE_COMPILE(&re_Oops_ppc_regs,
	    "^(GPR[0-9]{1,2}) *: *"	/* 1 */
	    UNBRACKETED_ADDRESS		/* 2 */
	    ,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_ppc_regs_pmatch);

    i = regexec(&re_Oops_ppc_regs, line, re_Oops_ppc_regs.re_nsub+1,
	re_Oops_ppc_regs_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i)
	return;
    p = line + re_Oops_ppc_regs_pmatch[0].rm_so + 3;
    reg_num = strtoul(p, NULL, 10);
    p = line + re_Oops_ppc_regs_pmatch[2].rm_so;
    /* Loop over unbracketed addresses */
    while (1) {
	if (regexec(&re_unbracketed_address, p, re_unbracketed_address.re_nsub+1,
	    re_unbracketed_address_pmatch, 0) == 0) {
	    re_strings(&re_unbracketed_address, p, re_unbracketed_address_pmatch, &string);
	    reg = hexstring(string[1]);		/* contents */
	    if (errno) {
		ERROR(" Invalid hex value in register line, ignored - '%s'",
		      string[1]);
		perror(prefix);
		break;
	    }
	    reg = Oops_truncate_address(reg, options);
	    snprintf(regname, sizeof(regname), ">>GPR%d;", reg_num);
	    add_symbol_n(ss, reg, 'R', 1, regname);
	    p += re_unbracketed_address_pmatch[0].rm_eo;
	    ++reg_num;
	}
	else
	    break;
    }

    if (*p)
	WARNING("garbage '%s' at end of ppc register line ignored", p);
    re_strings_free(&re_unbracketed_address, &string);
}

/* Look for the Trace multilines :(.  Returns start of addresses. */
static const char *Oops_trace(const char *line)
{
    int i;
    const char *start = NULL;
    static int trace_line = 0;
    static regex_t     re_Oops_trace;
    static regmatch_t *re_Oops_trace_pmatch;
    static const char procname[] = "Oops_trace";

    /* Oops 'Trace' lines.  The ikd patch adds "(nn)" for the stack frame size
     * after each trace entry, it should appear on the same line as the
     * bracketed address.  Alas users have been known to split lines so "(nn)"
     * appears at the start of the continuation line, look for that as well.
     * Life would be so much easier without users.
     * The person who did the s390 trace obviously did not like any of the ways
     * that other Linux kernels report problems so they invented a system all on
     * their own.  It is different from the i370 trace and even more ambiguous
     * than the other trace formats.  That is what happens when code is written
     * in secret!
     */
    RE_COMPILE(&re_Oops_trace,
	    "^("						     /* 1 */
		    "(Call Trace: *)"				     /* 2 */
    /* alpha */     "|(Trace:)"					     /* 3 */
    /* various */   "|(" BRACKETED_ADDRESS ")"			     /* 4,5 */
    /* ppc */	    "|(Call backtrace:)"			     /* 6 */
    /* ppc, s390 */ "|" UNBRACKETED_ADDRESS			     /* 7 */
    /* arm */	    "|Function entered at (" BRACKETED_ADDRESS ")"   /* 8,9 */
    /* sparc64 */   "|Caller\\[" UNBRACKETED_ADDRESS "\\]"	     /* 10 */
    /* i386 */	    "|(" REVBRACKETED_ADDRESS ")"		     /* 11,12 */
    /* ikd patch */ "|(\\([0-9]+\\) *(" BRACKETED_ADDRESS "))"	     /* 13,14,15 */
    /* i370 */	    "|([0-9]+ +base=0x" UNBRACKETED_ADDRESS ")"	     /* 16,17 */
    /* s390 */	    "|(Kernel BackChain.*)"                          /* 18    */
	    ")",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_trace_pmatch);

    i = regexec(&re_Oops_trace, line, re_Oops_trace.re_nsub+1,
	re_Oops_trace_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
#undef MATCHED
#define MATCHED(n) (re_Oops_trace_pmatch[n].rm_so != -1)
	if (MATCHED(2) || MATCHED(3)) {
	    /* Most start lines */
	    trace_line = 1;
	    start = line + re_Oops_trace_pmatch[0].rm_eo;
	}
	else if (MATCHED(6)) {
	    /* ppc start line, not a bracketed address, just an address */
	    trace_line = 2;     /* ppc */
	    start = line + re_Oops_trace_pmatch[0].rm_eo;
	}
	else if (MATCHED(8)) {
	    /* arm only line, two bracketed addresses on each line */
	    trace_line = 3;
	    start = line + re_Oops_trace_pmatch[8].rm_so;
	}
	else if (MATCHED(10)) {
	    /* sparc64 only line, one unbracketed address */
	    trace_line = 3;
	    start = line + re_Oops_trace_pmatch[10].rm_so;
	}
	else if (MATCHED(17)) {
	    /* i370 only line, one unbracketed address */
	    trace_line = 3;
	    start = line + re_Oops_trace_pmatch[17].rm_so;
	}
	else if (MATCHED(18)) {
	    /* s390 only line, no addresses on this line */
	    trace_line = 4;
	    start = line + re_Oops_trace_pmatch[18].rm_eo;
	}
	else if (MATCHED(11)) {
	    /* Reversed addresses for spinloops :( */
	    trace_line = 1;
	    start = line + re_Oops_trace_pmatch[11].rm_so;
	}
	else if (trace_line == 1 && MATCHED(4))
	    /* Most continuation lines */
	    start = line + re_Oops_trace_pmatch[4].rm_so;
	else if (trace_line == 1 && MATCHED(14))
	    /* incorrectly split trace lines from ikd patch */
	    start = line + re_Oops_trace_pmatch[14].rm_so;
	else if ((trace_line == 2 || trace_line == 4) && MATCHED(7))
	    /* ppc/s390 continuation line */
	    start = line + re_Oops_trace_pmatch[7].rm_so;
	else
	    trace_line = 0;
    }
    else
	trace_line = 0;
    if (trace_line) {
	if (trace_line == 3)
	    trace_line = 0;	/* no continuation lines */
	return(start);
    }
    return(NULL);
}

/* Process a trace call line, extract addresses */
static void Oops_trace_line(const char *line, const char *p, SYMBOL_SET *ss,
			    const OPTIONS *options)
{
    char **string = NULL;
    regex_t *pregex;
    regmatch_t *pregmatch;
    static regex_t     re_ikd;
    static regmatch_t *re_ikd_pmatch;
    static const char procname[] = "Oops_trace_line";

    /* ikd adds "(nn)" for stack frame size after each 'Trace' field. */
    RE_COMPILE(&re_ikd,
	    "^(\\([0-9]+\\) *)",
	REG_NEWLINE|REG_EXTENDED,
	&re_ikd_pmatch);

    /* ppc/s390 does not bracket its addresses */
    if (isxdigit(*p)) {
	pregex = &re_unbracketed_address;
	pregmatch = re_unbracketed_address_pmatch;
    }
    else if (strncmp(p, "<[", 2) == 0) {
	/* I hate special cases! */
	pregex = &re_revbracketed_address;
	pregmatch = re_revbracketed_address_pmatch;
    }
    else {
	pregex = &re_bracketed_address;
	pregmatch = re_bracketed_address_pmatch;
    }

    /* Loop over (un|rev)?bracketed addresses */
    while (1) {
	if (regexec(pregex, p, pregex->re_nsub+1, pregmatch, 0) == 0) {
	    addr_t addr;
	    re_strings(pregex, p, pregmatch, &string);
	    addr = hexstring(string[1]);
	    addr = Oops_truncate_address(addr, options);
	    add_symbol_n(ss, addr, 'T', 1, "Trace;");
	    p += pregmatch[0].rm_eo;
	}
	else if (strncmp(p, "from ", 5) == 0)
	    p += 5;     /* arm does "address from address" */
	else if (regexec(&re_ikd, p, re_ikd.re_nsub+1, re_ikd_pmatch, 0) == 0)
	    p += re_ikd_pmatch[0].rm_eo;  /* step over ikd stack frame size */
	else
	    break;
    }

    if (*p && !strcmp(p, "..."))
	WARNING("garbage '%s' at end of trace line ignored", p);
    re_strings_free(pregex, &string);
}

/* Decide if the line contains CRIS registers.  Returns first register name. */
static const char *Oops_cris_regs(const char *line)
{
    int i;
    static const char procname[] = "Oops_cris_regs";

    /* Note that most of the register dump, the r0: to oR10: lines, are
       handled by Oops_regs.  */
    RE_COMPILE(&re_Oops_cris_regs,
	    "^(IRP|SRP|D?CCR|USP|MOF): *" UNBRACKETED_ADDRESS,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_cris_regs_pmatch);

    i = regexec(&re_Oops_cris_regs, line, re_Oops_cris_regs.re_nsub+1,
	re_Oops_cris_regs_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0)
	return(line + re_Oops_cris_regs_pmatch[0].rm_so);
    return(NULL);
}

/* Process a CRIS register line, extract addresses */
static void Oops_set_cris_regs(const char *line, const char *p,
			       SYMBOL_SET *ss, const OPTIONS *options)
{
    addr_t reg;
    char regname[7];
    char **string = NULL;
    static const char procname[] = "Oops_set_cris_regs";

    /* Loop over CRIS register names and unbracketed addresses.  */
    while (1) {
	if (regexec(&re_Oops_cris_regs, p, re_Oops_cris_regs.re_nsub+1,
	    re_Oops_cris_regs_pmatch, 0) == 0) {
	    re_strings(&re_Oops_cris_regs, p, re_Oops_cris_regs_pmatch, &string);
	    reg = hexstring(string[2]);		/* contents */
	    if (errno) {
		ERROR(" Invalid hex value in CRIS register line, ignored - '%s'",
		      string[2]);
		perror(prefix);
		reg = 0;
	    }
	    else {
	      reg = Oops_truncate_address(reg, options);
	      strcpy(regname, ">>");
	      strncpy(regname+2, string[1], sizeof(regname)-3);
	      strcat(regname, ";");
	      add_symbol_n(ss, reg, 'R', 1, regname);
	    }
	    p += re_Oops_cris_regs_pmatch[0].rm_eo;
	}
	else
	    break;
    }

    if (*p)
	WARNING("garbage '%s' at end of CRIS register line ignored", p);
    re_strings_free(&re_Oops_cris_regs, &string);
}

/* Decide if the line contains s390 registers.  Returns first register value.
 * This is messier than the trace lines.  The heading is on one line followed by
 * the registers with *NO* keywords.  Fine for a dedicated JESMSG file, very
 * poor design for a multi-user log system.
 */
static const char *Oops_s390_regs(const char *line)
{
    int i;
    static regex_t     re_Oops_s390_regs;
    static regmatch_t *re_Oops_s390_regs_pmatch;
    static const char procname[] = "Oops_s390_regs";

    RE_COMPILE(&re_Oops_s390_regs,
	    "^(Kernel GPRS.*)",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_s390_regs_pmatch);

    i = regexec(&re_Oops_s390_regs, line, re_Oops_s390_regs.re_nsub+1,
	re_Oops_s390_regs_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	s390_reg_num = 0;  /* Start with R0 */
	return(line + re_Oops_s390_regs_pmatch[0].rm_eo);
    }
    else if (s390_reg_num >= 0) /* Header already seen */ {
	i = regexec(&re_unbracketed_address, line,
	    re_unbracketed_address.re_nsub+1,
	    re_unbracketed_address_pmatch, 0);
	DEBUG(4, "regexec %d", i);
	if (i == 0)
	    return(line + re_unbracketed_address_pmatch[0].rm_so);
    }
    s390_reg_num = -1;  /* Not processing s390 registers */
    return(NULL);
}

/* Process a s390 register line, extract addresses */
static void Oops_set_s390_regs(const char *line, const char *p,
			       SYMBOL_SET *ss, const OPTIONS *options)
{
    addr_t reg;
    char regname[7];
    char **string = NULL;
    static const char procname[] = "Oops_set_s390_regs";

    /* Loop over s390 unbracketed register values.  No keywords so we have to
     * synthesize the register numbers ourselves, yuck!
     */
    while (1) {
	if (regexec(&re_unbracketed_address, p,
	    re_unbracketed_address.re_nsub+1,
	    re_unbracketed_address_pmatch, 0) == 0) {
	    re_strings(&re_unbracketed_address, p,
		re_unbracketed_address_pmatch, &string);
	    reg = hexstring(string[1]);		/* contents */
	    if (errno) {
		ERROR(" Invalid hex value in s390 register line, ignored - '%s'",
		      string[1]);
		perror(prefix);
		reg = 0;
	    }
	    reg = Oops_truncate_address(reg, options);
	    strcpy(regname, ">>");
	    snprintf(regname, sizeof(regname), "r%-2d;  ", s390_reg_num);
	    add_symbol_n(ss, reg, 'R', 1, regname);
	    p += re_unbracketed_address_pmatch[0].rm_eo;
	    ++s390_reg_num;
	}
	else
	    break;
    }

    if (*p)
	WARNING("garbage '%s' at end of s390 register line ignored", p);
    re_strings_free(&re_unbracketed_address, &string);
}

/* Do pattern matching to decide if the line should be printed.  When reading a
 * syslog containing multiple Oops, you need the intermediate data (registers,
 * tss etc.) to go with the decoded text.  Sets text to the start of the useful
 * text, after any prefix.  Note that any leading white space is treated as part
 * of the prefix, later routines do not see any indentation.
 *
 * Note: If a line is not printed, it will not be scanned for any other text.
 */

#ifdef TIME_RE
/* time the re code */
#include <sys/time.h>
struct timeval before, after;
long long diff[30];
int count[30];
#define START gettimeofday(&before, NULL);
#define DIFF(n) gettimeofday(&after, NULL); \
	++count[n]; \
	diff[n] += (after.tv_sec - before.tv_sec)*1000001 + (after.tv_usec - before.tv_usec); \
	before = after;
#else
#define START
#define DIFF(n)
#endif

static int Oops_print(const char *line, const char **text, char ***string,
		      int string_max)
{
    int i, print = 0;
    static int stack_line = 0, trace_line = 0;
    static regex_t     re_Oops_prefix;
    static regmatch_t *re_Oops_prefix_pmatch;
    static regex_t     re_Oops_print_s;
    static regmatch_t *re_Oops_print_s_pmatch;
    static regex_t     re_Oops_print_a;
    static regmatch_t *re_Oops_print_a_pmatch;
    static const char procname[] = "Oops_print";

    START

    *text = line;

    /* Lines to be ignored.  For some reason the "amuse the user" print in
     * some die_if_kernel routines causes regexec to run very slowly.
     */

    if (strstr(*text, "\\|/ ____ \\|/")  ||
	strstr(*text, "\"@'/ ,. \\`@\"") ||
	strstr(*text, "\"@'/ .. \\`@\"") ||
	strstr(*text, "/_| \\__/ |_\\")  ||
	strstr(*text, "   \\__U_/"))
	return(1);  /* print but avoid regexec */

    DIFF(1);

    /* Trial and error with regex leads me to the following conclusions.
     *
     * regex does not like a lot of sub patterns that start with "^".  So split
     * the patterns into two groups, one set must appear at the start of the
     * line, the other set can appear anywhere.
     *
     * Lots of bracketed subexpressions really slows regex down.  Only use
     * brackets where absolutely necessary.  Examples are where you want to
     * extract matching substrings such as stack, call trace etc.  Or where
     * you have suboptions within a string, like the sparc die_if_kernel text.
     * Otherwise omit brackets, except that every re should have at least one
     * bracketed expression, that is how RE_COMPILE tells if the expression
     * has already been compiled.
     */

    /* Prefixes to be ignored.  These prefixes were originally done as
     * ^((pattern1)|(pattern2)|(pattern3))+  but that caused pathological
     * behaviour in regexec when faced with lines like
     *                     : "=r" (__cu_len), "=r" (__cu_from), "=r" (__cu_to)
     * Would you believe 11 seconds just to resolve that one line :(.  So just
     * look for one prefix at a time and loop until all prefixes are stripped.
     */
    RE_COMPILE(&re_Oops_prefix,
	    "^("			/* start of line */
	    " +"			/* leading spaces */
	    "|[^ ]{3} [ 0-9][0-9] [0-9]{2}:[0-9]{2}:[0-9]{2} "
	      "[^ ]+( kernel:)? +"	/* syslogd, syslog-ng */
	    "|<[0-9]+>"			/* kmsg */
	    "|[0-9]+\\|[^|]+\\|[^|]+\\|[^|]+\\|"	/* syslog-ng */
	    "|[0-9][AC] [0-9]{3} [0-9]{3}[cir][0-9]{2}:"	/* SNIA */
	    "\\+"			/* leading '+' */
	    ")",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_prefix_pmatch);

    do {
	i = regexec(&re_Oops_prefix, *text, re_Oops_prefix.re_nsub+1,
	    re_Oops_prefix_pmatch, 0);
	DEBUG(4, "regexec prefix %d", i);
	if (i == 0)
	    *text += re_Oops_prefix_pmatch[0].rm_eo;  /* step over prefix */
    } while (i == 0);

    DIFF(2);

    /* Lots of possibilities.  Expand as required for all architectures. */

    /* These patterns must appear at the start of the line, after stripping
     * the prefix above.
     *
     * The order below is required to handle multiline outupt.
     * string 2 is defined if the text is 'Stack from '.
     * string 3 is defined if the text is 'Stack: '.
     * string 4 is defined if the text might be a stack continuation.
     * string 5 is defined if the text is 'Call Trace: *'.
     * string 6 is defined if the text might be a trace continuation.
     * string 7 is the address part of the BRACKETED_ADDRESS.
     *
     * string 8 is defined if the text contains a version number.  No Oops
     * report contains this as of 2.1.125 but IMHO it should be added.  If
     * anybody wants to print a VERSION_nnnn line in their Oops, this code
     * is ready.
     *
     * string 9 is defined if the text is 'Trace:' (alpha).
     * string 10 is defined if the text is 'Call backtrace:' (ppc, i370).
     * string 11 is defined if the text is 'bh:' (i386).  Stack addresses are
     * on the next line.  In our typical inconsistent manner, the bh: stack
     * addresses are different from every other address, they are <[...]>
     * instead of [<...>] so define yet another re, string 12.  Sigh.
     *
     * The ikd patch adds "(nn)" for the stack frame size after each trace
     * entry, it should appear on the same line as the bracketed address.  Alas
     * users have been known to split lines so "(nn)" appears at the start of
     * the continuation line, look for that as well.  Life would be so much
     * easier without users.  Andi Kleen's multi cpu traceback patch adds the
     * module name as "(name)" after addresses.  Strings 13 and 14 are
     * a special case of strings 6 and 7 to handle "(nn)" and "(name)",
     * in fact anything in "()" between addresses is ignored.
     *
     * Both ppc and i370 use 'Call backtrace:' but the i370 data is completely
     * different from anything else.  It starts on a separate line and consists
     * of multiple lines, each containing a number (trace depth), base=, link=,
     * stack=.
     *
     * s390 decided to do their own thing instead of using the same format as
     * i370.  A bad case of "not invented here".  s390 data starts on a separate
     * line and consists of multiple addresses.
     *
     * The SGI kdb backtrace for ix86 prints EBP and EIP, one per line after the
     * header.
     *
     * Some strings indicate stack data (S), some indicate trace data (T).
     */
    RE_COMPILE(&re_Oops_print_s,
    /* arch type */				/* Required order */
		"^("					/*  1 */
    /* i386 */	    "(Stack: )"				/*  2  S */
    /* m68k */	    "|(Stack from )"			/*  3  S */
    /* various */   "|([0-9a-fA-F]{4,})"		/*  4  S,T */
    /* various */   "|(Call Trace: *)"			/*  5  T */
    /* various */   "|(" BRACKETED_ADDRESS ")"		/* 6,7 T */
    /* various */   "|(Version_[0-9]+)"			/*  8 */
    /* alpha */	    "|(Trace:)"				/*  9  T */
    /* ppc, i370 */ "|(Call backtrace:)"		/* 10  T */
    /* i386 */	    "|(bh:)"				/* 11  T */
    /* i386 */	    "|" REVBRACKETED_ADDRESS		/* 12  T */
    /* ikd/AK */    "|(\\([^ ]+\\) *"			/* 13  T */
		      BRACKETED_ADDRESS ")"		/* 14    */
    /* i370 */	    "|([0-9]+ +base=)"			/* 15  T */
    /* s390 */	    "|(Kernel BackChain)"		/* 16  T */
    /* kdb ix86 */  "|EBP *EIP"				/* 17  T */
    /* kdb ix86 */  "|0x" UNBRACKETED_ADDRESS "0x" UNBRACKETED_ADDRESS	/* 18, 19 T */


    /* order does not matter from here on */

    /* various */   "|Process .*stackpage="
    /* various */   "|Code *: "
    /* various */   "|Kernel panic"
    /* various */   "|In swapper task"
    /* various */   "|kmem_free"
    /* various */   "|swapper"
    /* various */   "|Pid:"
    /* various */   "|r[0-9]{1,2} *[:=]"		/* generic register dump */

    /* i386 2.0 */  "|Corrupted stack page"
    /* i386 */	    "|invalid operand: "
    /* i386 */	    "|Oops: "
    /* i386 */	    "|Cpu:* +[0-9]"
    /* i386 */	    "|current->tss"
    /* i386 */	    "|\\*pde +="
    /* i386 */	    "|EIP: "
    /* i386 */	    "|EFLAGS: "
    /* i386 */	    "|eax: "
    /* i386 */	    "|esi: "
    /* i386 */	    "|ds: "
    /* i386 */	    "|CR0: "
    /* i386 */	    "|wait_on_"
    /* i386 */	    "|irq: "
    /* i386 */	    "|Stack dumps:"

    /* x86_64 */    "|RAX: "
    /* x86_64 */    "|RSP: "
    /* x86_64 */    "|RIP: "
    /* x86_64 */    "|RDX: "
    /* x86_64 */    "|RBP: "
    /* x86_64 */    "|FS: "
    /* x86_64 */    "|CS: "
    /* x86_64 */    "|CR2: "
    /* x86_64 */    "|PML4"

    /* m68k */	    "|pc[:=]"
    /* m68k */	    "|68060 access"
    /* m68k */	    "|Exception at "
    /* m68k */	    "|d[04]: "
    /* m68k */	    "|Frame format="
    /* m68k */	    "|wb [0-9] stat"
    /* m68k */	    "|push data: "
    /* m68k */	    "|baddr="
    /* any other m68K lines to print? */

    /* alpha */	    "|Arithmetic fault"
    /* alpha */	    "|Instruction fault"
    /* alpha */	    "|Bad unaligned kernel"
    /* alpha */	    "|Forwarding unaligned exception"
    /* alpha */	    "|: unhandled unaligned exception"
    /* alpha */	    "|pc *="
    /* alpha */	    "|[rvtsa][0-9]+ *="	/* Changed from r to [vtsa] in 2.2.14 */
    /* alpha */	    "|gp *="
    /* alpha */	    "|spinlock stuck"
    /* any other alpha lines to print? */

    /* sparc */	    "|tsk->"
    /* sparc */	    "|PSR: "
    /* sparc */	    "|[goli]0: "
    /* sparc */	    "|Instruction DUMP: "
    /* sparc */	    "|spin_lock"
    /* any other sparc lines to print? */

    /* sparc64 */   "|TSTATE: "
    /* sparc64 */   "|[goli]4: "
    /* sparc64 */   "|Caller\\["
    /* sparc64 */   "|CPU\\["
    /* any other sparc64 lines to print? */

    /* ppc */	    "|MSR: "
    /* ppc */	    "|TASK = "
    /* ppc */	    "|last math "
    /* ppc */	    "|GPR[0-9]+: "
    /* any other ppc lines to print? */

    /* mips */	    "|.* in .*\\.[ch]:.*, line [0-9]+:"
    /* mips */	    "|\\$[0-9 ]+:"
    /* mips */	    "|Hi *:"
    /* mips */	    "|Lo *:"
    /* mips */	    "|epc *:"
    /* mips */	    "|badvaddr *:"
    /* mips */	    "|Status *:"
    /* mips */	    "|Cause *:"
    /* any other mips lines to print? */

    /* arm */	    "|Backtrace:"
    /* arm */	    "|Function entered at"
    /* arm */	    "|\\*pgd ="
    /* arm */	    "|Internal error"
    /* arm */	    "|pc :"
    /* arm */	    "|sp :"
    /* arm */	    "|Flags:"
    /* arm */	    "|Control:"
    /* any other arm lines to print? */

    /* ikd patch */ "|WARNING:"
    /* ikd patch */ "|this_stack:"
    /* ikd patch */ "|i:"
    /* any other ikd lines to print? */

    /* i370 */	    "|PSW"
    /* i370 */	    "|cr[0-9]+:"
    /* i370 */	    "|machine check"
    /* i370 */	    "|Exception in "
    /* i370 */	    "|Program Check "
    /* i370 */	    "|System restart "
    /* i370 */	    "|IUCV "
    /* i370 */	    "|unexpected external "
    /* i370 */	    "|Kernel stack "
    /* any other i370 lines to print? */

    /* s390 */	    "|User process fault:"
    /* s390 */	    "|failing address"
    /* s390 */	    "|User PSW"
    /* s390 */	    "|User GPRS"
    /* s390 */	    "|User ACRS"
    /* s390 */	    "|Kernel PSW"
    /* s390 */	    "|Kernel GPRS"
    /* s390 */	    "|Kernel ACRS"
    /* s390 */	    "|illegal operation"
    /* s390 */	    "|task:"
    /* any other s390 lines to print? */

    /* SGI kdb */ "|Entering kdb"
    /* SGI kdb */ "|eax *="
    /* SGI kdb */ "|esi *="
    /* SGI kdb */ "|ebp *="
    /* SGI kdb */ "|ds *="
    /* any other SGI kdb lines to print? */

    /* ia64 */	    "|psr "
    /* ia64 */	    "|unat  "
    /* ia64 */	    "|rnat "
    /* ia64 */	    "|ldrs "
    /* ia64 */	    "|xip "
    /* ia64 */	    "|iip "
    /* ia64 */	    "|ipsr "
    /* ia64 */	    "|ifa "
    /* ia64 */	    "|pr "
    /* ia64 */	    "|itc "
    /* ia64 */	    "|ifs "
    /* ia64 */	    "|bsp "
    /* ia64 */	    "|[bfr][0-9]+ "
    /* ia64 */	    "|irr[0-9] "
    /* ia64 */	    "|General Exception"
    /* ia64 */	    "|MCA"
    /* ia64 */	    "|SAL"
    /* ia64 */	    "|Processor State"
    /* ia64 */	    "|Bank [0-9]+"
    /* ia64 */	    "|Register Stack"
    /* ia64 */	    "|Processor Error Info"
    /* ia64 */	    "|proc err map"
    /* ia64 */	    "|proc state param"
    /* ia64 */	    "|proc lid"
    /* ia64 */	    "|processor structure"
    /* ia64 */	    "|check info"
    /* ia64 */	    "|target identifier"
    /* any other ia64 lines to print? */

    /* CRIS */	    "|IRP *: "
    /* any other cris lines to print? */

		")",
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_print_s_pmatch);

    i = regexec(&re_Oops_print_s, *text, re_Oops_print_s.re_nsub+1,
	re_Oops_print_s_pmatch, 0);
    DEBUG(4, "regexec start %d", i);
    DIFF(3);
    print = 0;
#undef MATCHED
#define MATCHED(n) (re_Oops_print_s_pmatch[n].rm_so != -1)
    DIFF(4);
    /* Handle multiline messages, messy.  Only treat strings that look like
     * addresses as useful when they immediately follow a definite stack or
     * trace header or a previously valid stack or trace line.
     * Because of the horribly ambiguous s390 register dump, unbracketed
     * addresses (string 4) can now be either stack or trace.  How not to design
     * dump output.
     */
    /* Stack lines */
    if (!MATCHED(2) && !MATCHED(3) && !MATCHED(4))
	stack_line = 0;
    else if (MATCHED(2) || MATCHED(3))
	stack_line = 1;
    else if (stack_line && !MATCHED(4))
	stack_line = 0;
    /* Trace lines */
    if (!MATCHED(4) && !MATCHED(5) && !MATCHED(6) && !MATCHED(9) &&
	!MATCHED(10) && !MATCHED(11) && !MATCHED(12) && !MATCHED(13) &&
	!MATCHED(15) && !MATCHED(16) && !MATCHED(17) && !MATCHED(19))
	trace_line = 0;
    else if (MATCHED(4) || MATCHED(5) || MATCHED(9) || MATCHED(10) ||
	MATCHED(11) || MATCHED(15) || MATCHED(16) || MATCHED(17))
	trace_line = 1;
    else if (trace_line && !MATCHED(4) && !MATCHED(6) && !MATCHED(12) &&
	!MATCHED(13) && !MATCHED(19))
	trace_line = 0;
    if (i == 0) {
	print = 1;
	if (MATCHED(4) && !stack_line && !trace_line) {
	    print = 0;
	    DEBUG(4, "%s", "ambiguous stack/trace line ignored");
	}
	if ((MATCHED(6) || MATCHED(12) || MATCHED(13)) && !trace_line) {
	    print = 0;
	    DEBUG(4, "%s", "ambiguous trace line ignored");
	}
    }
    if (print && MATCHED(4)) {
	/* Ambiguity with 3Com messages, a line like 3c503.c:v1.10 9/23/93
	 * starts with 4 or more hex digits but is not an error message, instead
	 * it is a filename.  Look for '.c:' before the first space and ignore
	 * if found.  Also ad1848, look for '/' before first space.  Do not look
	 * for ':' on its own, arm does "address: stack values" :(.
	 *
	 * Ambiguity of ambiguities, saith the preacher; all is ambiguity.
	 */
	const char *p1, *p2;
	p2 = strstr(*text, " ");
	if (p2) {
	    if ((p1 = strstr(*text, ".c:")) && p1 < p2)
		print = 0;  /* ignore ambiguous match */
	    else if ((p1 = strstr(*text, "/")) && p1 < p2)
		print = 0;  /* ignore ambiguous match */
	}
	if (!print)
	    DEBUG(4, "%s", "ambiguous start ignored");
    }
    /* delay splitting into strings until we really them */
    if (print && MATCHED(8)) {
	re_string_check(re_Oops_print_s.re_nsub+1, string_max,
	    procname);
	re_strings(&re_Oops_print_s, *text,
	    re_Oops_print_s_pmatch,
	    string);
	add_Version((*string)[8]+8, "Oops");
    }
    DIFF(5);

    /* These patterns can appear anywhere in the line, after stripping
     * the prefix above.
     */
    RE_COMPILE(&re_Oops_print_a,
   /* arch type */

    /* various */   "Unable to handle kernel"
    /* various */   "|Aiee"      /* anywhere in text is a bad sign (TM) */
    /* various */   "|die_if_kernel"  /* ditto */
    /* various */   "|NMI "
    /* various */   "|BUG "
    /* various */   "|\\(L-TLB\\)"
    /* various */   "|\\(NOTLB\\)"

    /* alpha */     "|\\([0-9]\\): Oops "
    /* alpha */     "|: memory violation"
    /* alpha */     "|: Exception at"
    /* alpha */     "|: Arithmetic fault"
    /* alpha */     "|: Instruction fault"
    /* alpha */     "|: arithmetic trap"
    /* alpha */     "|: unaligned trap"

    /* sparc      die_if_kernel has no fixed text, identify by (pid): text.
     *            Somebody has been playful with the texts.
     *
     *            Alas adding this next pattern increases run time by 15% on
     *            its own!  It would be considerably faster if sparc had
     *            consistent error texts.
     */
    /* sparc */     "|\\([0-9]+\\): "
			"("
			    "Whee"
			    "|Oops"
			    "|Kernel"
			    "|.*Penguin"
			    "|BOGUS"
			")"

    /* ppc */       "|kernel pc "
    /* ppc */       "|trap at PC: "
    /* ppc */       "|bad area pc "
    /* ppc */       "|NIP: "

    /* mips */      "| ra *=="

		,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_print_a_pmatch);

    if (!print) {
	i = regexec(&re_Oops_print_a, *text, re_Oops_print_a.re_nsub+1,
	    re_Oops_print_a_pmatch, 0);
	DEBUG(4, "regexec anywhere %d", i);
	if (i == 0)
	    print = 1;
    }
    DIFF(6);

    return(print);
}

/* Look for the Code: line.  Returns start of the code bytes. */
static const char *Oops_code(const char *line, char ***string, int string_max)
{
    int i;
    char *p;
    static regex_t     re_Oops_code;
    static regmatch_t *re_Oops_code_pmatch;
    static const char procname[] = "Oops_code";

    /* Oops 'Code: ' hopefully followed by at least one hex code.  sparc
     * brackets the PC in '<' and '>'.  arm brackets the PC in '(' and ')'.
     */
    RE_COMPILE(&re_Oops_code,
		"^("				/*  1 */
    /* various */ "(Instruction DUMP)"		/*  2 */
    /* various */ "|(Code:? *)"			/*  3 */
		")"
		": +"
		"("				/*  4 */
		  "(general protection.*)"
		  "|(Bad E?IP value.*)"
		  "|(<[0-9]+>)"
		  "|(([<(]?[0-9a-fA-F]+[>)]? +)+[<(]?[0-9a-fA-F]+[>)]?)"
		")"
		"(.*)$"			/* trailing garbage */
		,
	REG_NEWLINE|REG_EXTENDED|REG_ICASE,
	&re_Oops_code_pmatch);

    i = regexec(&re_Oops_code, line, re_Oops_code.re_nsub+1,
	re_Oops_code_pmatch, 0);
    DEBUG(4, "regexec %d", i);
    if (i == 0) {
	re_string_check(re_Oops_code.re_nsub+1, string_max, procname);
	re_strings(&re_Oops_code, line, re_Oops_code_pmatch,
	    string);
	if ((*string)[re_Oops_code.re_nsub] &&
	    *((*string)[re_Oops_code.re_nsub])) {
	    /* Performance fix to Code: re means that trailing space is no
	     * longer part of the Code: text.  No point in complaining about
	     * trailing space.
	     */
	    for (p = (*string)[re_Oops_code.re_nsub]; isspace(*p); ++p)
		/* skip white space */ ;
	    if (*p) {
		WARNING("trailing garbage ignored on Code: line\n"
		    "  Text: '%s'\n"
		    "  Garbage: '%s'",
		    line, (*string)[re_Oops_code.re_nsub]);
	    }
	}
	return((*string)[4]);
    }
    return(NULL);
}

/******************************************************************************/
/*                      End architecture sensitive code                       */
/******************************************************************************/

/* Decode part or all of the Oops Code: via objdump. */
static void Oops_decode_part(const char* code, int code_size,
			     addr_t eip, int adjust,
			     const char type, SYMBOL_SET *ss,
			     char ***string, int string_max,
			     const bfd *ibfd, OPTIONS *options)
{
    FILE *f;
    char *file, *line = NULL;
    int size = 0, lines = 0;
    static char const procname[] = "Oops_decode_part";

    DEBUG(1, "%s", "");
    /* binary to same format as ksymoops */
    if (!(file = Oops_code_to_file(code, code_size, ibfd, options)))
	return;
    /* objdump the pseudo object */
    if (!(f = Oops_objdump(file, options)))
	return;
    while (fgets_local(&line, &size, f, procname)) {
	DEBUG(2, "%s", line);
	++lines;
	Oops_decode_one(ss, line, eip, adjust, type, options);
    }
    pclose_local(f, procname);  /* opened in Oops_objdump */
    free(line);
    if (!lines)
	ERROR("no objdump lines read for %s", file);
    else if (unlink(file)) {
	ERROR("could not unlink %s", file);
	perror(prefix);
    }
}

/* Decode the Oops Code: via objdump. */
static void Oops_decode(const unsigned char* code_text, addr_t eip,
			SYMBOL_SET *ss, char ***string, int string_max,
			const bfd *ibfd, OPTIONS *options)
{
    char code[CODE_SIZE];
    int adjust;
    static char const procname[] = "Oops_decode";

    DEBUG(1, "%s", "");
    /* text to binary */
    if (!Oops_code_values(code_text, code, &adjust, options))
	return;
    /* adjust is -ve number of bytes before eip */
    if (adjust && options->vli) {
	    Oops_decode_part(code, -adjust, eip, adjust, 'U', ss, string, string_max, ibfd, options);
	    Oops_decode_part(code-adjust, CODE_SIZE+adjust, eip, 0, 'C', ss, string, string_max, ibfd, options);
    } else {
	    Oops_decode_part(code, CODE_SIZE, eip, adjust, 'C', ss, string, string_max, ibfd, options);
    }
}

/* Reached the end of an Oops report, format the extracted data. */
static void Oops_format(const SYMBOL_SET *ss_format, OPTIONS *options)
{
    int i;
    SYMBOL *s;
    addr_t eip = 0;
    char *eye_catcher, prev_type = '\0';
    static const char procname[] = "Oops_format";

    DEBUG(1, "%s", "");

    compare_Version();  /* Oops might have a version one day */
    printf("\n");
    for (s = ss_format->symbol, i = 0; i < ss_format->used; ++i, ++s) {
	/* For type C and U data, print Code:, address, map, "name" (actually
	 * the text of an objdump line).  For type I (id) print the string.
	 * For other types print name, address, map.
	 */
	if (s->type == 'C' || s->type == 'U') {
	    if (prev_type != s->type) {
		printf("\n");
		if (s->type == 'U')
		    printf("This architecture has variable length instructions, decoding before eip\n"
			   "is unreliable, take these instructions with a pinch of salt.\n\n");
		else if (prev_type == 'U')
		    printf("This decode from eip onwards should be reliable\n\n");
	    }
	    eye_catcher = "";
	    if (eip && i < ss_format->used-1 && (s+1)->type == s->type &&
		eip >= s->address && eip < (s+1)->address)
		eye_catcher = "   <=====";
	    if (options->short_lines)
		printf("Code;  %s %s%s\n%s%s\n",
		    format_address(s->address, options),
		    map_address(&ss_merged, s->address, options), eye_catcher,
		    s->name, eye_catcher);
	    else
		printf("Code;  %s %-30s %s%s\n",
		    format_address(s->address, options),
		    map_address(&ss_merged, s->address, options),
		    s->name, eye_catcher);
	}
	else if (s->type == 'I') {
	    printf("%s\n", s->name);
	}
	else {
	    /* Suppress registers that do not resolve to symbols */
	    char *map = map_address(&ss_merged, s->address, options);
	    if (s->type == 'R') {
		if (index(map, '+') == NULL || strncmp(map, "<END_OF_CODE+", 13) == 0) {
		    continue;
		}
	    }
	    if (prev_type != s->type)
		printf("\n");
	    printf("%s %s %s",
		s->name, format_address(s->address, options), map);
	    if (s->type == 'E') {
		eip = s->address;
		if (eip)
		    printf("   <=====");
	    }
	    printf("\n");
	}
	prev_type = s->type;
    }
    printf("\n");
}

/* Select next Oops input file */
static FILE *Oops_next_file(OPTIONS *options)
{
    static FILE *f = NULL;
    static int first_file = 1;
    static const char procname[] = "Oops_next_file";

    if (first_file) {
	f = stdin;
	first_file = 0;
    }
    while (options->filecount) {
	if (f)
	    fclose_local(f, procname);
	f = NULL;
	if (strcmp(*(options->filename), "/dev/null") == 0 ||
	    regular_file(*(options->filename), procname))
	    f = fopen_local(*(options->filename), "r", procname);
	if (f)
	    DEBUG(1, "reading Oops report from %s", *(options->filename));
	++options->filename;
	--options->filecount;
	if (f)
	    return(f);
    }
    return(f);
}

/* Read the Oops report */
#define MAX_STRINGS 300 /* Maximum strings in any Oops re */
int Oops_read(OPTIONS *options)
{
    char *line = NULL, **string = NULL, *me;
    const char *start, *text;
    int i, size = 0, lineno = 0, lastprint = 0, print;
    addr_t eip = 0;
    int sparc_regdump = 0;
    FILE *f;
    SYMBOL_SET ss_format;
    bfd *ibfd = NULL;
    static const char procname[] = "Oops_read";

    ss_init(&ss_format, "Oops log data");
    Oops_open_input_bfd(&me, &ibfd, options);

    if (options->adhoc_addresses) {
	char **adhoc;
	for (adhoc = options->adhoc_addresses; *adhoc; ++adhoc) {
	    add_symbol(&ss_format, *adhoc, 'A', 1, "Adhoc");
	}
    }

    if (!options->filecount && isatty(0))
	printf("Reading Oops report from the terminal\n");

    string = malloc(MAX_STRINGS*sizeof(*string));
    if (!string)
	malloc_error(procname);
    memset(string, '\0', MAX_STRINGS*sizeof(*string));

    do {
	if (!(f = Oops_next_file(options)))
	    continue;
	while (fgets_local(&line, &size, f, procname)) {
	    DEBUG(3, "%s", line);
	    ++lineno;
	    print = Oops_print(line, &text, &string, MAX_STRINGS);
	    if (print && Oops_sparc_regdump(text, &string)) {
		sparc_regdump = 1;
	    } else {
		if (options->target &&
		    strstr(options->target, "sparc") &&
		    sparc_regdump &&
		    ss_format.used) {
		    Oops_format(&ss_format, options);
		    ss_free(&ss_format);
		}
		sparc_regdump = 0;
	    }
	    if (print) {
		if (options->one_shot && lastprint == 0)
		    read_symbol_sources(options);
		puts(line);
		lastprint = lineno;
		if ((start = Oops_eip(text, &string, MAX_STRINGS, ibfd,
				      options))) {
		    const char *ret_addr;
		    if ((ret_addr = Oops_arm_lr(text, &string, MAX_STRINGS)))
			Oops_set_arm_lr(ret_addr, &ss_format);
		    if ((ret_addr = Oops_alpha_ra(text, &string, MAX_STRINGS)))
			Oops_set_alpha_ra(ret_addr, &ss_format);
		    Oops_set_eip(start, &eip, &ss_format, me, ibfd, options);
		    options->vli = strstr(line, " VLI") != NULL;
		}
		if (Oops_eip_alpha_spinlock(text, &string, MAX_STRINGS,
					    options)) {
		    /* Set the eip and previous user from the alpha spinlock
		     * stuck line.  YASC, this line contains both the eip and
		     * the equivalent of a trace entry.
		     */
		    Oops_set_eip(string[1], &eip, &ss_format, me, ibfd, options);
		    Oops_trace_line(line, string[2], &ss_format, options);
		}
		if (Oops_eip_sparc_spinlock(text, &string, MAX_STRINGS,
					    options)) {
		    /* Set the eip and previous user from the sparc spinlock
		     * stuck line.  YASC, this line contains the lock and two
		     * PC entries.
		     */
		    Oops_set_eip(string[2], &eip, &ss_format, me, ibfd, options);
		    Oops_set_eip(string[3], &eip, &ss_format, me, ibfd, options);
		    Oops_trace_line(line, string[1], &ss_format, options);
		}
		if ((start = Oops_mips_ra(text, &string, MAX_STRINGS)))
		    Oops_set_mips_ra(start, &ss_format);
		Oops_mips_regs(text, &ss_format, options);
		Oops_ppc_regs(text, &ss_format, options);
		if ((start = Oops_i370_psw(text, &string, MAX_STRINGS)))
		    Oops_set_i370_psw(start, &ss_format, me, ibfd, options);
		if ((start = Oops_s390_psw(text, &string, MAX_STRINGS)))
		    Oops_set_s390_psw(start, &ss_format, me, ibfd, options);
		if ((start = Oops_trace(text)))
		    Oops_trace_line(text, start, &ss_format, options);
		if ((start = Oops_regs(text)))
		    Oops_set_regs(text, start, &ss_format, options);
		if ((start = Oops_s390_regs(text)))
		    Oops_set_s390_regs(text, start, &ss_format, options);
		if ((start = Oops_cris_regs(text)))
		    Oops_set_cris_regs(text, start, &ss_format, options);
		Oops_set_ia64_b0(text, &string, MAX_STRINGS, &ss_format);
		if (Oops_set_task_name(text, &string, MAX_STRINGS, &ss_format))
		    Oops_set_eip(string[2], &eip, &ss_format, me, ibfd, options);
		if ((start = Oops_code(text, &string, MAX_STRINGS))) {
		    /* In case no EIP line was seen */
		    Oops_set_default_ta(me, ibfd, options);
		    Oops_decode(start, eip, &ss_format, &string, MAX_STRINGS,
				ibfd, options);
		    Oops_format(&ss_format, options);
		    ss_free(&ss_format);
		    if (options->one_shot)
			return(0);
		}
	    }
	    /* More than 5 (arbitrary) lines which were not printed
	     * and there is some saved data, assume we missed the
	     * Code: line.
	     */
	    if (ss_format.used && lineno > lastprint+5) {
		WARNING("%s", "Code line not seen, dumping what data is available");
		Oops_format(&ss_format, options);
		ss_free(&ss_format);
		if (options->one_shot)
		    return(0);
	    }
	}
	if (ss_format.used) {
	    if (options->target &&
		strstr(options->target, "sparc") == 0 &&
		sparc_regdump)
		;	/* sparc regdump comes *after* code, ignore :( */
	    else if (!options->adhoc_addresses)
		WARNING("%s", "Code line not seen, dumping what data is available");
	    Oops_format(&ss_format, options);
	    ss_free(&ss_format);
	    if (options->one_shot)
		return(0);
	}
    } while (options->filecount != 0);

#ifdef TIME_RE
    for (i = 0; i < sizeof(diff)/sizeof(diff[0]); ++i)
	printf("diff %d %d %lld %lld\n", i, count[i], diff[i], count[i] ? diff[i]/count[i] : 0);
#endif

    if (ibfd && !bfd_close(ibfd)) {
	Oops_bfd_perror("close(ibfd)");
	return(0);
    }

    for (i = 0; i < sizeof(string); ++i) {
	free(string[i]);
	string[i] = NULL;
    }
    free(line);
    if (options->one_shot)
	return(3);  /* one shot mode, end of input, no data */
    return(0);
}
