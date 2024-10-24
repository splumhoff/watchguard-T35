The NANOG traceroute, modified, yet again		27-Feb-1999

If you get STUPID compile errors, use gcc.
STUPID compile errors are things like:
"traceroute.c", line 464: syntax error at or near type word "char"
"traceroute.c", line 464: const declared as parameter to non-function

Of course on some systems (Digital Unix 4.0) old GCC (2.7.2) gives
lots of stupid errors, but CC works just fine...  It's a good thing
this "ANSI C" thing means much.

AIX 3.2.5:
BSDI 1.1:
Digital UNIX 3.2c, 4.0:
FreeBSD:
Mach10:
Ultrix 4.3:
   % cc -o traceroute -O traceroute.c -lm

IRIX animaniacs 6.2:
   % gcc -O3 -s traceroute.c -lm -o traceroute, works for:

IRIX 6.4:
   % cc -mips4 -O3 -s traceroute.c -lm -o traceroute

Linux 2.0.36:
Linux 2.2.5:
   % gcc -lresolv -lm -o traceroute traceroute.c

Solaris:
   % gcc -o traceroute -O -DSOLARIS traceroute.c -lm -lresolv -lsocket -lnsl

Solaris X86:
   % gcc -o traceroute -O -DSOLARIS -DBYTESWAP_IP_LEN traceroute.c -lm -lresolv -lsocket -lnsl

SunOs 5.4:
   % cc -o traceroute -O traceroute.c -lm -lresolv

Unixware 1.0.3:
   % /usr/ucb/cc -o traceroute -O -DMAX_DATALEN=1500 traceroute.c -lm -lresolv


VMS:
   Pick up build.com and trace_*.opt from the vms subdirectory
   $ @build
   $ traceroute == "$location:traceroute """

Enjoy 

Ehud Gavron, gavron@wetwork.net
Wetwork - Network consulting - no bodies


Note TrACESroute V6.0GOLD = Traceroute V3.0
     V6.1GOLD follows


Tested Version Matrix	01/05/1999
-------------------------------------------------------------------------
Operating System		TR2.7	TR2.8	TR2.8.2   TR2.9.1  V6.1
-------------------------------------------------------------------------
AIX 3.2.5			2.7
BSDI 1.1			2.7
Digital UNIX 3.2c		2.7
Digital UNIX 4.0			2.8	2.8.2
FreeBSD 2.0.5			2.7
FreeBSD 2.2.x(1,6)			2.8	2.8.2
FreeBSD 2.2.7					2.8.2	2.9.1
FreeBSD 2.2.8					2.8.2
FeeeBSD 3.0					2.8.2
Irix 6.2					2.8.2
Irix 6.4					2.8.2
Linux 2.0.36						2.9.1
Mach10				2.7
Ultrix 4.3			2.7	2.8
Unixware 1.0.3			2.7
SunOs 5.4			2.7
Solaris 2.5.x(0,2)			2.7
Solaris 2.6				2.8	2.8.2   2.9.1
Solaris 7					2.8.2	2.9.2
Solaris X86 2.5.1				2.8.2   2.9.1
VMS6 or 7 MultiNet 3 or4	2.7	2.8	2.8.2   2.9.1
