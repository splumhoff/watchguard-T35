#!/bin/sh
#
# Run from your linux/config directory of a UTM build.
#
# Checks any p4 opened *.config files
# Expects to see no differences other than date stamp from running "make oldconfig"
# Prints PASSED: and FAILED: for all files it finds opened.
# Does not alter files: it is your job to produce correct config files.
# Consult Platform team if you require help.
#set -x

HERE="$(cd $(dirname $0) >& /dev/null; pwd)"
KERNEL_CMP_CFG="$HERE/../../components/kernel/config"
PASSLOG=$(mktemp)
FAILLOG=$(mktemp)
TMP=$(mktemp)

DEBUG=debug

cd $HERE

get_make_flags () {
  WG_TARGET_ARCH="$(basename $(dirname $1))"
  WG_PLATFORM="$(ls $KERNEL_CMP_CFG |grep $WG_TARGET_ARCH)"
  if [ -z "$WG_PLATFORM" ]; then
    WG_PLATFORM=$(basename $(dirname $(grep $WG_TARGET_ARCH $KERNEL_CMP_CFG/*/Config.mak |head -1 |cut -f1 -d':')))
  fi
  if [ -z "$WG_PLATFORM" ]; then
    echo "===== NO WG_PLATFORM: $1"
    return 1
  fi

  ARCH="$(WG_PLATFORM=$WG_PLATFORM WG_TARGET_ARCH=$WG_TARGET_ARCH CFG=$KERNEL_CMP_CFG make -s -C $HERE -f getarch)"

  if [ "$DEBUG" = "debug" ]; then
    echo ARGS         : $1
    echo WG_TARGET_ARCH  : $WG_TARGET_ARCH
    echo WG_PLATFORM     : $WG_PLATFORM
    echo ARCH         : $ARCH
  fi
  return 0
}

find_opened () {
p4 opened ... | sed -n '/tps/d;s/#..*//;/\.config/p' | xargs --no-run-if-empty p4 have | sed 's/..* //'
#p4 opened ... | sed -n '/tps/d;s/#..*//;/\.config/p' | sed 's/..* //'
}

check_file () {
  echo checking $1
  get_make_flags $1
  if [ $? -ne 0 ]; then
    echo "FAILED: $1" >> $FAILLOG
    echo "Could not determine platform for $WG_TARGET_ARCH" >> $FAILLOG
    FAILED="yes"
    return
  fi
  pushd .. >& /dev/null
  FAILED=""
  make distclean >& /dev/null
  cp $1 .config
  make ARCH=$ARCH oldconfig < /dev/null >& /dev/null
  if [ $? -ne 0 ]; then
    echo "FAILED: $1" >> $FAILLOG
    echo "make oldconfig failed" >> $FAILLOG
    FAILED="yes"
  else
    diff -du $1 .config |
    sed -r '
    /^[+]{3}/d
    /^[+-]# (Mon|Tue|Wed|Thu|Fri|Sat|Sun) (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 0-9]{2} [0-9:]{8} [0-9]{4}/s/^./#/
    ' |
    awk '
    /BEGIN/{printing=0; rc=0}
    /^---/{f=$2; printing=0; next}
    {if (printing) {print; next}}
    /^[+-]/{printf("FAILED: %s\n%s\n", f, $0); printing=1; rc=-1}
    ' > $TMP
    if grep FAILED $TMP >& /dev/null; then
      cat $TMP >> $FAILLOG
      echo "" >>$FAILLOG
      FAILED="yes"
    fi
  fi
  make distclean >& /dev/null
  popd >& /dev/null
}

FILEFOUND=""

if [ "$DEBUG" = "debug" ]; then
  echo opened ....
  echo "$(find_opened)"
  echo "======"
fi
for file in $(find_opened); do
  FILESFOUND="found"
#  get_make_flags $file
  check_file $file
  if [ "$FAILED" = "" ]; then
    echo "PASSED: $file" >> $PASSLOG
  fi
done

if [ "$FILESFOUND" = "" ]; then
  echo "No kernel config files open under p4"
fi

# Print successes
if [ -s $PASSLOG ]; then
echo =============================== Passed ===================================
cat $PASSLOG
fi

# Print failures
if [ -s $FAILLOG ]; then
echo =============================== Failed ===================================
grep FAILED $FAILLOG
echo
echo ">>> SEE $HERE/failed.tmp <<<"
echo ">>> This shows differences between your .config file before  <<<"
echo ">>> and after successful make oldconfig, or failed oldconfig <<<"
echo
cp $FAILLOG $HERE/failed.tmp
fi

rm -f $TMP $FAILLOG $PASSLOG
