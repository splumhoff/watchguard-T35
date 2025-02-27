
# BSDI Makefile
#
# $Id$

PROG=	lsof

BIN=	${DESTDIR}

DOC=	${DESTDIR}

I=/usr/include
S=/usr/include/sys
L=/usr/include/local
P=

CDEF=
CDEFS=  ${CDEF} ${CFGF}
INCL=	${DINC}
CFLAGS=	${CDEFS} ${INCL} ${DEBUG}

GRP=

HDR=    lsof.h lsof_fields.h dlsof.h machine.h proto.h dproto.h

SRC=    dfile.c dmnt.c dnode.c dproc.c dsock.c dstore.c \
	arg.c main.c misc.c node.c print.c proc.c store.c usage.c

OBJ=	dfile.o dmnt.o dnode.o dproc.o dsock.o dstore.o \
	arg.o main.o misc.o node.o print.o proc.o store.o usage.o

MAN=	lsof.8
MANLCL=	lsof.0

OTHER=	

SHELL=	/bin/sh

SOURCE=	Makefile ${OTHER} ${MAN} ${HDR} ${SRC}

all: ${PROG}

${MANLCL}: ${MAN}
	rm -f ${MANLCL}
	nroff -mandoc -Tlp ${MAN} > ${MANLCL}

${PROG}: ${LIB} ${P} ${OBJ}
	${LD} -o $@ ${CFLAGS} ${OBJ} ${CFGL}

clean: FRC
	rm -f Makefile.bak ${PROG} a.out core errs lint.out tags *.o version.h
	rm -f machine.h.old new_machine.h
	(cd lib; ${MAKE} -f Makefile.skel clean)

install: all ${MANLCL} FRC
	@echo ''
	@echo 'Please write your own install rule.  Lsof should be installed'
	@echo 'setgid to the group that can can read /dev/kmem.  Normally'
	@echo 'that is the kmem group.  Your install rule actions might look'
	@echo 'something like this:'
	@echo ''
	@echo '    install -cs -m 2755 -g $${GRP} $${PROG} $${BIN}'
	@echo '    install -c -m 444 $${MANLCL} $${DOC}'
	@echo ''
	@echo 'You will have to complete the skeletons for the BIN, DOC, and'
	@echo 'GRP strings given at the beginning of this Makefile, e.g.,'
	@echo ''
	@echo '    BIN= $${DESTDIR}/usr/local/etc'
	@echo '    DOC= $${DESTDIR}/usr/local/man/man8'
	@echo '    GRP= kmem'
	@echo ''

${LIB}: FRC
	(cd lib; ${MAKE} DEBUG="${DEBUG}" CFGF="${CFGF}")

version.h:	FRC
	@echo Constructing version.h
	@rm -f version.h
	@echo '#define	LSOF_BLDCMT	"${LSOF_BLDCMT}"' > version.h;
	@echo '#define	LSOF_CC		"${CC}"' >> version.h
	@echo '#define	LSOF_CCV	"${CCV}"' >> version.h
	@echo '#define	LSOF_CCDATE	"'`date`'"' >> version.h
	@echo '#define	LSOF_CCFLAGS	"'`echo ${CFLAGS} | sed 's/\\\\(/\\(/g' | sed 's/\\\\)/\\)/g' | sed 's/"/\\\\"/g'`'"' >> version.h
	@if [ "X${LSOF_HOST}" = "X" ]; then \
	  echo '#define	LSOF_HOST	"'`uname -n`'"' >> version.h; \
	else \
	  if [ "${LSOF_HOST}" = "none" ]; then \
	    echo '#define	LSOF_HOST	""' >> version.h; \
	  else \
	    echo '#define	LSOF_HOST	"${LSOF_HOST}"' >> version.h; \
	  fi \
	fi
	@echo '#define	LSOF_LDFLAGS	"${CFGL}"' >> version.h
	@if [ "X${LSOF_LOGNAME}" = "X" ]; then \
	  echo '#define	LSOF_LOGNAME	"${LOGNAME}"' >> version.h; \
	else \
	  if [ "${LSOF_LOGNAME}" = "none" ]; then \
	    echo '#define	LSOF_LOGNAME	""' >> version.h; \
	  else \
	    echo '#define	LSOF_LOGNAME	"${LSOF_LOGNAME}"' >> version.h; \
	  fi; \
	fi
	@if [ "X${LSOF_SYSINFO}" = "X" ]; then \
	    echo '#define	LSOF_SYSINFO	"'`uname -a`'"' >> version.h; \
	else \
	  if [ "${LSOF_SYSINFO}" = "none" ]; then \
	    echo '#define	LSOF_SYSINFO	""' >> version.h; \
	  else \
	    echo '#define	LSOF_SYSINFO	"${LSOF_SYSINFO}"' >> version.h; \
	  fi \
	fi
	@if [ "X${LSOF_USER}" = "X" ]; then \
	  echo '#define	LSOF_USER	"${USER}"' >> version.h; \
	else \
	  if [ "${LSOF_USER}" = "none" ]; then \
	    echo '#define	LSOF_USER	""' >> version.h; \
	  else \
	    echo '#define	LSOF_USER	"${LSOF_USER}"' >> version.h; \
	  fi \
	fi
	@sed '/VN/s/.ds VN \(.*\)/#define	LSOF_VERSION	"\1"/' < version >> version.h

FRC:

# DO NOT DELETE THIS LINE - make depend DEPENDS ON IT

dfile.o:	${HDR} dfile.c

dmnt.o:		${HDR} dmnt.c

dnode.o:	${HDR} dnode.c

dproc.o:	${HDR} dproc.c

dsock.o:	${HDR} dsock.c

dstore.o:	${HDR} dstore.c

arg.o:		${HDR} arg.c

main.o:		${HDR} main.c

misc.o:		${HDR} misc.c

node.o:		${HDR} node.c

print.o:	${HDR} print.c

proc.o:		${HDR} proc.c

store.o:	${HDR} store.c

usage.o:	${HDR} version.h usage.c

# *** Do not add anything here - It will go away. ***
