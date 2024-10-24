#!/bin/bash
# oldconfig-all.sh
# This script runs "make oldconfig" on every kernel.config in this tree.
# If you have made a bunch of changes to a Kconfig file, where we need a new
# kernel.config for everyone, this script is your friend!

set -x
set -e

HERE=$(cd $(dirname $0) >/dev/null; pwd)
cd $HERE

KERNEL_CMP_CFG="$HERE/../../components/kernel/config"
ARCH=

DEBUG=debug

ALL_CONFIGS=($(find . -name "*.config" |sed -e 's|\./||'))

if [ "$DEBUG" = "debug" ]; then
  echo "${ALL_CONFIGS[@]}"
fi

make_flags()
{
  WG_TARGET_ARCH="$(basename $(dirname $1))"
  WG_PLATFORM=$(basename $(dirname $(grep $WG_TARGET_ARCH $KERNEL_CMP_CFG/*/Config.mak |head -1 |cut -f1 -d':')))
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


main()
{
  pushd .. > /dev/null
  make -s distclean

  for cfgfile in ${ALL_CONFIGS[@]}
  do
    make_flags $cfgfile
    if [ $? -ne 0 ]; then
      echo "Warning: unknown config $cfgfile ... skipping"
      continue
    fi
    cp ${HERE}/${cfgfile} .config
    make -s ARCH=${ARCH} oldconfig
    p4 edit ${HERE}/${cfgfile}
    cp .config ${HERE}/${cfgfile}
    make -s distclean
  done
  popd > /dev/null
}

main

# vim: ts=2 sw=2 et ai
