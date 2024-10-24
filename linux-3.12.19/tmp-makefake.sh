#!/bin/sh

# REMOVE ME WHEN THIS STUFF EITHER BUILDS OR ELSE HAS BEEN
# REMOVED FROM $WG_PLATFORM PKGSPEC FILES

# Script to make fake build products so make package in wg_linux
# does not crash

fakeit () {
mkdir  -p $(dirname $1)
if [ ! -e  "$1" ]; then
echo "touch $1"
#     touch $1
fi
}

#
cd $(dirname $0)
cd ../components/kernel
echo
pwd
#
echo
fakeit build/$WG_PLATFORM/linux/tools/perf/perf
fakeit build/$WG_PLATFORM/linux/tools/perf/perf-archive
fakeit build/$WG_PLATFORM/linux/tools/slub/slabinfo
fakeit exports/$WG_PLATFORM/tools/perf
fakeit exports/$WG_PLATFORM/tools/perf-archive
fakeit exports/$WG_PLATFORM/tools/slub/slabinfo
#
echo
fakeit                               ../wg_linux/exports/$WG_PLATFORM/lib/info.txt
#
mkdir  -p                            ../wg_linux/exports/host/bin
if [ ! -e                            ../wg_linux/exports/host/bin/genmanifest ]; then
 if [  -s /usr/local/bin/genmanifest ]; then
 cp    -p /usr/local/bin/genmanifest ../wg_linux/exports/host/bin/genmanifest
 fi
 if [  -s ~/bin/genmanifest          ]; then
 cp    -p ~/bin/genmanifest          ../wg_linux/exports/host/bin/genmanifest
 fi
fi
#
echo
fakeit ../../tps/Linkfarm/ppc_64-linux30-glibc219/bin/fw_printenv
fakeit ../../tps/Linkfarm/ppc_64-linux30-glibc219/bin/u-boot_XTM330.bin
#
echo
