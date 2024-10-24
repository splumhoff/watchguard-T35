#!/bin/bash
#
# Audit config files
#

# Go to linux/config
HERE=$(cd $(dirname $0); pwd)
cd $HERE

# Find tool
if [ -e ../../components/wg_linux/exports/host/bin/kconfig ]; then
# Get kconfig tool from wg_linux
KCONFIG=$(cd ../../components/wg_linux/exports/host/bin; pwd)/kconfig
else
# Fall back to default path
KCONFIG=kconfig
fi

# Check args for last approved change list number
if [ -n "$1" ]; then
 CLN=$1
elif [ -s  $HERE/lastgood.txt ]; then
 CLN=$(cat $HERE/lastgood.txt)
else
 CLN=365788
fi

# Get some space to work in
TMP=$(mktemp -d)
ORG=$TMP/org
UPD=$TMP/upd
mkdir -p  $ORG
mkdir -p  $UPD
if [  -d "$2" ]; then
 SRC=$(cd $2; pwd)
else
 SRC=$TMP/src
 mkdir -p $SRC
fi

# Split the updated kernel configs into tmp directory
export KCONFIG
$HERE/split-config.sh       $UPD "#" >/dev/null

# For all directories in linux/config get approved *.config files
rm       -f $SRC/.config
for cfgfile in $(find . -name *.config)
do
 if [    -e $cfgfile ]; then
  p4 print  $cfgfile@$CLN >$SRC/.config
  if [   -s $SRC/.config ]; then
   mkdir -p                $SRC/$(dirname $cfgfile)
   cp    -f $SRC/.config   $SRC/$cfgfile
  else
   rm    -f $SRC/.config
  fi
 fi
done
rm       -f $SRC/.config

# Check for no file found
if [ -z "$(ls $SRC/*/*.config)" ] ; then
 echo "No *.config files found @$CLN"
 exit 101
fi

# Change to approved tree
cd $SRC

# Copy script to approved tree
rm -f                       $SRC/split-config.sh
cp -p $HERE/split-config.sh $SRC/split-config.sh
chmod +x                    $SRC/split-config.sh

# Split the orginal kernel configs into tmp directory
$SRC/split-config.sh        $ORG "@" >/dev/null
rm -f                       $SRC/split-config.sh

# Check for empty databases
if [ ! -s $ORG/kconfig.db ]; then
 echo "Empty orginal database"
 exit 102
fi
if [ ! -s $UPD/kconfig.db ]; then
 echo "Empty updated database"
 exit 103
fi

# Run kconfig on the combined file
export LC_ALL=C
cat $ORG/kconfig.db $UPD/kconfig.db | sort >$TMP/db
$KCONFIG                                    $TMP/db | sort
ERR=$?

# Clean up
cd     /tmp
rm -rf $TMP

# Exit
exit   $ERR
