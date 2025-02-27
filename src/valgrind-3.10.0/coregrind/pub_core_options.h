
/*--------------------------------------------------------------------*/
/*--- Command line options.                     pub_core_options.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_CORE_OPTIONS_H
#define __PUB_CORE_OPTIONS_H

//--------------------------------------------------------------------
// PURPOSE: This module holds the variables for all command line options,
// plus some functions and macros for manipulating them.  Almost every
// other module imports this one, if only for VG_(clo_verbosity).
//--------------------------------------------------------------------
#include "pub_tool_options.h"

/* The max number of suppression files. */
#define VG_CLO_MAX_SFILES 100

/* The max number of --require-text-symbol= specification strings. */
#define VG_CLO_MAX_REQ_TSYMS 100

/* The max number of --fullpath-after= parameters. */
#define VG_CLO_MAX_FULLPATH_AFTER 100

/* Should we stop collecting errors if too many appear?  default: YES */
extern Bool  VG_(clo_error_limit);
/* Alternative exit code to hand to parent if errors were found.
   default: 0 (no, return the application's exit code in the normal
   way. */
extern Int   VG_(clo_error_exitcode);

typedef 
   enum { 
      Vg_VgdbNo,   // Do not activate gdbserver.
      Vg_VgdbYes,  // Activate gdbserver (default).
      Vg_VgdbFull, // ACtivate gdbserver in full mode, allowing
                   // a precise handling of watchpoints and single stepping
                   // at any moment.
   } 
   VgVgdb;
/* if != Vg_VgdbNo, allows valgrind to serve vgdb/gdb. */
extern VgVgdb VG_(clo_vgdb);
/* if > 0, checks every VG_(clo_vgdb_poll) BBS if vgdb wants to be served. */
extern Int VG_(clo_vgdb_poll);

/* Specify when Valgrind gdbserver stops the execution and wait
   for a GDB to connect. */
typedef
   enum {                       // Stop :
      VgdbStopAt_Startup,       // just before the client starts to execute.
      VgdbStopAt_Exit,          // just before the client exits.
      VgdbStopAt_ValgrindAbExit // on abnormal valgrind exit.
   }
   VgdbStopAt;
// Build mask to check or set VgdbStop_At a membership
#define VgdbStopAt2S(a) (1 << (a))
// VgdbStopAt a is member of the Set s ?
#define VgdbStopAtiS(a,s) ((s) & VgdbStopAt2S(a))
extern UInt VG_(clo_vgdb_stop_at); // A set of VgdbStopAt reasons.

/* prefix for the named pipes (FIFOs) used by vgdb/gdb to communicate with valgrind */
extern const HChar *VG_(clo_vgdb_prefix);

/* if True, gdbserver in valgrind will expose a target description containing
   shadow registers */
extern Bool  VG_(clo_vgdb_shadow_registers);

/* Enquire about whether to attach to a debugger at errors?   default: NO */
extern Bool  VG_(clo_db_attach);
/* The debugger command?  default: whatever gdb ./configure found */
extern const HChar* VG_(clo_db_command);
/* Generating a suppression for each error?   default: 0 (NO)
   Other values: 1 (yes, but ask user), 2 (yes, don't ask user) */
extern Int   VG_(clo_gen_suppressions);
/* Sanity-check level: 0 = none, 1 (default), > 1 = expensive. */
extern Int   VG_(clo_sanity_level);
/* Automatically attempt to demangle C++ names?  default: YES */
extern Bool  VG_(clo_demangle);
/* Simulate child processes? default: NO */
/* Soname synonyms : a string containing a list of pairs
   xxxxx=yyyyy separated by commas.
   E.g. --soname-synonyms=somalloc=libtcmalloc*.so*,solibtruc=NONE */
extern const HChar* VG_(clo_soname_synonyms);
extern Bool  VG_(clo_trace_children);
/* String containing comma-separated patterns for executable names
   that should not be traced into even when --trace-children=yes */
extern const HChar* VG_(clo_trace_children_skip);
/* The same as VG_(clo_trace_children), except that these patterns are
   tested against the arguments for child processes, rather than the
   executable name. */
extern const HChar* VG_(clo_trace_children_skip_by_arg);
/* After a fork, the child's output can become confusingly
   intermingled with the parent's output.  This is especially
   problematic when VG_(clo_xml) is True.  Setting
   VG_(clo_child_silent_after_fork) causes children to fall silent
   after fork() calls.  Although note they become un-silent again
   after the subsequent exec(). */
extern Bool  VG_(clo_child_silent_after_fork);

/* If the user specified --log-file=STR and/or --xml-file=STR, these
   hold STR after expansion of the %p and %q templates. */
extern HChar* VG_(clo_log_fname_expanded);
extern HChar* VG_(clo_xml_fname_expanded);

extern HChar* VG_(clo_log_fname_unexpanded);
extern HChar* VG_(clo_xml_fname_unexpanded);

/* Add timestamps to log messages?  default: NO */
extern Bool  VG_(clo_time_stamp);

/* The file descriptor to read for input.  default: 0 == stdin */
extern Int   VG_(clo_input_fd);

/* Whether or not to load the default suppressions. */
extern Bool  VG_(clo_default_supp);
/* The number of suppression files specified. */
extern Int   VG_(clo_n_suppressions);
/* The names of the suppression files. */
extern const HChar* VG_(clo_suppressions)[VG_CLO_MAX_SFILES];

/* An array of strings harvested from --fullpath-after= flags. */
extern Int   VG_(clo_n_fullpath_after);
extern const HChar* VG_(clo_fullpath_after)[VG_CLO_MAX_FULLPATH_AFTER];

/* Full path to additional path to search for debug symbols */
extern const HChar* VG_(clo_extra_debuginfo_path);

/* Address of a debuginfo server to use.  Either an IPv4 address of
   the form "d.d.d.d" or that plus a port spec, hence of the form
   "d.d.d.d:d", where d is one or more digits. */
extern const HChar* VG_(clo_debuginfo_server);

/* Do we allow reading debuginfo from debuginfo objects that don't
   match (in some sense) the main object?  This is dangerous, so the
   default is NO (False).  In any case it applies only to objects
   found either in _extra_debuginfo_path or via the
   _debuginfo_server. */
extern Bool VG_(clo_allow_mismatched_debuginfo);

/* DEBUG: print generated code?  default: 00000000 ( == NO ) */
extern UChar VG_(clo_trace_flags);

/* DEBUG: do SB profiling? default: False (== NO).  NOTE: does not
   have an associated command line flag.  Is set to True whenever
   --profile-flags= is specified. */
extern Bool  VG_(clo_profyle_sbs);
/* DEBUG: if doing SB profiling, provides bits for which JIT stages
   are shown.  Same meaning as for clo_trace_flags.  default: zero (==
   show block counts only) */
extern UChar VG_(clo_profyle_flags);
/* DEBUG: if doing SB profiling, dump blocks and zero counters after
   this-many back edges (event checks).  default: zero (== show
   profiling results only at the end of the run. */
extern ULong VG_(clo_profyle_interval);

/* DEBUG: if tracing codegen, be quiet until after this bb */
extern Int   VG_(clo_trace_notbelow);
/* DEBUG: if tracing codegen, be quiet after this bb  */
extern Int   VG_(clo_trace_notabove);
/* DEBUG: print system calls?  default: NO */
extern Bool  VG_(clo_trace_syscalls);
/* DEBUG: print signal details?  default: NO */
extern Bool  VG_(clo_trace_signals);
/* DEBUG: print symtab details?  default: NO */
extern Bool  VG_(clo_trace_symtab);
/* DEBUG: restrict symtab etc details to object name pattern.  Default: "*" */
extern const HChar* VG_(clo_trace_symtab_patt);
/* DEBUG: print call-frame-info details?  default: NO */
extern Bool  VG_(clo_trace_cfi);
/* DEBUG:  mimic /usr/bin/readelf --syms?  default: NO */
extern Bool  VG_(clo_debug_dump_syms);
/* DEBUG: mimic /usr/bin/readelf --debug-dump=line?  default: NO */
extern Bool  VG_(clo_debug_dump_line);
/* DEBUG: mimic  /usr/bin/readelf --debug-dump=frames?  default: NO */
extern Bool  VG_(clo_debug_dump_frames);
/* DEBUG: print redirection details?  default: NO */
extern Bool  VG_(clo_trace_redir);
/* Enable fair scheduling on multicore systems? default: NO */
enum FairSchedType { disable_fair_sched, enable_fair_sched, try_fair_sched };
extern enum FairSchedType VG_(clo_fair_sched);
/* DEBUG: print thread scheduling events?  default: NO */
extern Bool  VG_(clo_trace_sched);
/* DEBUG: do heap profiling?  default: NO */
extern Bool  VG_(clo_profile_heap);
#define MAX_REDZONE_SZB 128
// Maximum for the default values for core arenas and for client
// arena given by the tool.
// 128 is no special figure, just something not too big
#define MAX_CLO_REDZONE_SZB 4096
// We allow the user to increase the redzone size to 4Kb :
// This allows "off by one" in an array of pages to be detected.
#define CORE_REDZONE_DEFAULT_SZB 4
extern Int VG_(clo_core_redzone_size);
// VG_(clo_redzone_size) has default value -1, indicating to keep
// the tool provided value.
extern Int VG_(clo_redzone_size);
/* DEBUG: display gory details for the k'th most popular error.
   default: Infinity. */
extern Int   VG_(clo_dump_error);

/* Engage miscellaneous weird hacks needed for some progs. */
typedef
   enum {
      SimHint_lax_ioctls,
      SimHint_fuse_compatible,
      SimHint_enable_outer,
      SimHint_no_inner_prefix,
      SimHint_no_nptl_pthread_stackcache
   }
   SimHint;

// Build mask to check or set SimHint a membership
#define SimHint2S(a) (1 << (a))
// SimHint h is member of the Set s ?
#define SimHintiS(h,s) ((s) & SimHint2S(h))
extern UInt VG_(clo_sim_hints);

/* Show symbols in the form 'name+offset' ?  Default: NO */
extern Bool VG_(clo_sym_offsets);
/* Read DWARF3 inline info ? */
extern Bool VG_(clo_read_inline_info);
/* Read DWARF3 variable info even if tool doesn't ask for it? */
extern Bool VG_(clo_read_var_info);
/* Which prefix to strip from full source file paths, if any. */
extern const HChar* VG_(clo_prefix_to_strip);

/* An array of strings harvested from --require-text-symbol= 
   flags.

   Each string specifies a pair: a soname pattern and a text symbol
   name pattern, separated by a colon.  The patterns can be written
   using the normal "?" and "*" wildcards.  For example:
   ":*libc.so*:foo?bar".

   These flags take effect when reading debuginfo from objects.  If an
   object is loaded and the object's soname matches the soname
   component of one of the specified pairs, then Valgrind will examine
   all the text symbol names in the object.  If none of them match the
   symbol name component of that same specification, then the run is
   aborted, with an error message.

   The purpose of this is to support reliable usage of marked-up
   libraries.  For example, suppose we have a version of GCC's
   libgomp.so which has been marked up with annotations to support
   Helgrind.  It is only too easy and confusing to load the 'wrong'
   libgomp.so into the application.  So the idea is: add a text symbol
   in the marked-up library (eg), "annotated_for_helgrind_3_6", and
   then give the flag

     --require-text-symbol=:*libgomp*so*:annotated_for_helgrind_3_6

   so that when libgomp.so is loaded, we scan the symbol table, and if
   the symbol isn't present the run is aborted, rather than continuing
   silently with the un-marked-up library.  Note that you should put
   the entire flag in quotes to stop shells messing up the * and ?
   wildcards. */
extern Int    VG_(clo_n_req_tsyms);
extern const HChar* VG_(clo_req_tsyms)[VG_CLO_MAX_REQ_TSYMS];

/* Track open file descriptors? */
extern Bool  VG_(clo_track_fds);

/* Should we run __libc_freeres at exit?  Sometimes causes crashes.
   Default: YES.  Note this is subservient to VG_(needs).libc_freeres;
   if the latter says False, then the setting of VG_(clo_run_libc_freeres)
   is ignored.  Ie if a tool says no, I don't want this to run, that
   cannot be overridden from the command line. */
extern Bool  VG_(clo_run_libc_freeres);

/* Should we show VEX emulation warnings?  Default: NO */
extern Bool VG_(clo_show_emwarns);

/* How much does the stack pointer have to change before tools
   consider a stack switch to have happened?  Default: 2000000 bytes
   NB: must be host-word-sized to be correct (hence Word). */
extern Word VG_(clo_max_stackframe);
/* How large should Valgrind allow the primary thread's guest stack to
   be? */
extern Word VG_(clo_main_stacksize);

/* If the same IP is found twice in a backtrace in a sequence of max
   VG_(clo_merge_recursive_frames) frames, then the recursive call
   is merged in the backtrace.
   Note also that the merge is done during unwinding, to obtain
   an much as possible significant backtrace.
   Note that the value is changeable by a gdbsrv command. */
extern Int VG_(clo_merge_recursive_frames);

/* Max number of sectors that will be used by the translation code cache. */
extern UInt VG_(clo_num_transtab_sectors);

/* Only client requested fixed mapping can be done below 
   VG_(clo_aspacem_minAddr). */
extern Addr VG_(clo_aspacem_minAddr);

/* Delay startup to allow GDB to be attached?  Default: NO */
extern Bool VG_(clo_wait_for_gdb);

/* To what extent should self-checking translations be made?  These
   are needed to deal with self-modifying code on uncooperative
   platforms. */
typedef 
   enum { 
      Vg_SmcNone,  // never generate self-checking translations
      Vg_SmcStack, // generate s-c-t's for code found in stacks
                   // (this is the default)
      Vg_SmcAll,   // make all translations self-checking.
      Vg_SmcAllNonFile // make all translations derived from
                   // non-file-backed memory self checking
   } 
   VgSmc;

/* Describe extent to which self-modifying-code should be
   auto-detected. */
extern VgSmc VG_(clo_smc_check);

/* A set of minor kernel variants,
   so they can be properly handled by m_syswrap. */
typedef
   enum {
      KernelVariant_bproc,
      KernelVariant_android_no_hw_tls,
      KernelVariant_android_gpu_sgx5xx,
      KernelVariant_android_gpu_adreno3xx
   }
   KernelVariant;
// Build mask to check or set KernelVariant a membership
#define KernelVariant2S(v) (1 << (v))
// KernelVariant v is member of the Set s ?
#define KernelVariantiS(v,s) ((s) & KernelVariant2S(v))
extern UInt VG_(clo_kernel_variant);

/* Darwin-specific: automatically run /usr/bin/dsymutil to update
   .dSYM directories as necessary? */
extern Bool VG_(clo_dsymutil);

/* Should we trace into this child executable (across execve etc) ?
   This involves considering --trace-children=,
   --trace-children-skip=, --trace-children-skip-by-arg=, and the name
   of the executable.  'child_argv' must not include the name of the
   executable itself; iow child_argv[0] must be the first arg, if any,
   for the child. */
extern Bool VG_(should_we_trace_this_child) ( HChar* child_exe_name,
                                              HChar** child_argv );

/* Whether illegal instructions should be reported/diagnosed.
   Can be explicitly set through --sigill-diagnostics otherwise
   depends on verbosity (False if -q). */
extern Bool VG_(clo_sigill_diag);

/* Unwind using stack scanning (a nasty hack at the best of times)
   when the normal CFI/FP-chain scan fails.  If the number of
   "normally" recovered frames is below this number, stack scanning
   will be used (on platforms on which it is supported, currently only
   arm-linux).  The default value of zero has the effect of disabling
   stack scanning.  Default: zero*/
extern UInt VG_(clo_unw_stack_scan_thresh);

/* If stack scanning is used, this is how many frames it may recover.
   Since it tends to pick up a lot of junk, this value is set pretty
   low by default.  Default: 5 */
extern UInt VG_(clo_unw_stack_scan_frames);

#endif   // __PUB_CORE_OPTIONS_H

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
