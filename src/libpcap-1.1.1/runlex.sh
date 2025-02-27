#! /bin/sh

#
# runlex.sh
# Script to run Lex/Flex.
# First argument is the (quoted) name of the command; if it's null, that
# means that neither Flex nor Lex was found, so we report an error and
# quit.
#
# @(#) $Header$
#

#
# Get the name of the command to run, and then shift to get the arguments.
#
if [ $# -eq 0 ]
then
	echo "Usage: runlex <lex/flex command to run> [ arguments ]" 1>&2
	exit 1
fi
LEX="$1"
shift

#
# Check whether we have Lex or Flex.
#
if [ -z "${LEX}" ]
then
	echo "Neither lex nor flex was found" 1>&2
	exit 1
fi

#
# Process the flags.  We don't use getopt because we don't want to
# embed complete knowledge of what options are supported by Lex/Flex.
#
flags=""
outfile=lex.yy.c
while [ $# -ne 0 ]
do
	case "$1" in

	-o*)
		#
		# Set the output file name.
		#
		outfile=`echo "$1" | sed 's/-o\(.*\)/\1/'`
		;;

	-*)
		#
		# Add this to the list of flags.
		#
		flags="$flags $1"
		;;

	--|*)
		#
		# End of flags.
		#
		break
		;;
	esac
	shift
done

#
# Is it Lex, or is it Flex?
#
if [ "${LEX}" = flex ]
then
	#
	# It's Flex.
	#
	have_flex=yes

	#
	# Does it support the --noFUNCTION options?  If so, we pass
	# --nounput, as at least some versions that support those
	# options don't support disabling yyunput by defining
	# YY_NO_UNPUT.
	#
	if flex --help | egrep noFUNCTION >/dev/null
	then
		flags="$flags --nounput"

		#
		# Does it support -R, for generating reentrant scanners?
		# If so, we're not currently using that feature, but
		# it'll generate some unused functions anyway - and there
		# won't be any header file declaring them, so there'll be
		# defined-but-not-declared warnings.  Therefore, we use
		# --noFUNCTION options to suppress generating those
		# functions.
		#
		if flex --help | egrep reentrant >/dev/null
		then
			flags="$flags --noyyget_lineno --noyyget_in --noyyget_out --noyyget_leng --noyyget_text --noyyset_lineno --noyyset_in --noyyset_out"
		fi
	fi
else
	#
	# It's Lex.
	#
	have_flex=no
fi

#
# OK, run it.
# If it's lex, it doesn't support -o, so we just write to
# lex.yy.c and, if it succeeds, rename it to the right name,
# otherwise we remove lex.yy.c.
# If it's flex, it supports -o, so we use that - flex with -P doesn't
# write to lex.yy.c, it writes to a lex.{prefix from -P}.c.
#
if [ $have_flex = yes ]
then
	${LEX} $flags -o"$outfile" "$@"

	#
	# Did it succeed?
	#
	status=$?
	if [ $status -ne 0 ]
	then
		#
		# No.  Exit with the failing exit status.
		#
		exit $status
	fi

	#
	# Flex has the annoying habit of stripping all but the last
	# component of the "-o" flag argument and using that as the
	# place to put the output.  This gets in the way of building
	# in a directory different from the source directory.  Try
	# to work around this.
	#
	# Is the outfile where we think it is?
	#
	outfile_base=`basename "$outfile"`
	if [ "$outfile_base" != "$outfile" -a \( ! -r "$outfile" \) -a -r "$outfile_base" ]
	then
		#
		# No, it's not, but it is in the current directory.  Put it
		# where it's supposed to be.
		#
		mv "$outfile_base" "$outfile"

		#
		# Did that succeed?
		#
		status=$?
		if [ $status -ne 0 ]
		then
			#
			# No.  Exit with the failing exit status.
			#
			exit $status
		fi
	fi
else
	${LEX} $flags "$@"

	#
	# Did it succeed?
	#
	status=$?
	if [ $status -ne 0 ]
	then
		#
		# No.  Get rid of any lex.yy.c file we generated, and
		# exit with the failing exit status.
		#
		rm -f lex.yy.c
		exit $status
	fi

	#
	# OK, rename lex.yy.c to the right output file.
	#
	mv lex.yy.c "$outfile" 

	#
	# Did that succeed?
	#
	status=$?
	if [ $status -ne 0 ]
	then
		#
		# No.  Get rid of any lex.yy.c file we generated, and
		# exit with the failing exit status.
		#
		rm -f lex.yy.c
		exit $status
	fi
fi

#
# OK, now let's generate a header file declaring the relevant functions
# defined by the .c file; if the .c file is .../foo.c, the header file
# will be .../foo.h.
#
# This works around some other Flex suckage, wherein it doesn't declare
# the lex routine before defining it, causing compiler warnings.
# XXX - newer versions of Flex support --header-file=, to generate the
# appropriate header file.  With those versions, we should use that option.
#

#
# Get the name of the prefix; scan the source files for a %option prefix
# line.  We use the last one.
#
prefix=`sed -n 's/%option[ 	][ 	]*prefix="\(.*\)".*/\1/p' "$@" | tail -1`
if [ ! -z "$prefix" ]
then
	prefixline="#define yylex ${prefix}lex"
fi

#
# Construct the name of the header file.
#
header_file=`dirname "$outfile"`/`basename "$outfile" .c`.h

#
# Spew out the declaration.
#
cat <<EOF >$header_file
/* This is generated by runlex.sh.  Do not edit it. */
$prefixline
#ifndef YY_DECL
#define YY_DECL int yylex(void)
#endif  
YY_DECL;
EOF
