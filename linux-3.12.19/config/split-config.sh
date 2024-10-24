#!/bin/bash
#
# Split config files
#

#
# Function to tag config files with file name
#
tagconfig() {
# Tag it with file name
grep CONFIG_  "$1" | sort | sed -e "s|$| "$2" "$1" |" >>$CONFIGS/all.tag
}

export LC_ALL=C

# Go to linux/config
cd       $(dirname $0)

# Find tool
if [ -z "$KCONFIG" ] || [ ! -x $KCONFIG ]; then
  # If a valid KCONFIG isn't set in our environment:
  if [ -e ../../components/wg_linux/exports/host/bin/kconfig ]; then
  # Get kconfig tool from wg_linux
  KCONFIG=$(cd ../../components/wg_linux/exports/host/bin; pwd)/kconfig
  else
  # Fall back to default path
  KCONFIG=kconfig
  fi
fi

# Show what is configs we have
echo
echo  -n "Processing configs in "
pwd
echo
ls    -l */*.config
echo

# Get output directory
if [ -n "$1" ]; then
CONFIGS=$1
else
CONFIGS=../../components/kernel/exports/config
fi

# Get tag
if [ -n "$2" ]; then
TAG=$2
else
TAG="#"
fi

# Clean up output directory
mkdir -p $CONFIGS
rm    -f $CONFIGS/*.tag

# For all directories in linux/config tag *.config with file name
for cfgfile in $(find . -name *.config)
do
  tagconfig $cfgfile $TAG
done

# Go to output directory
cd       $CONFIGS

# Remove old database
rm    -f kconfig.db

# Build  new database
cat *.tag | sed 's/^# //' | sed 's/ is not set/=x/' | sort >kconfig.db

# Show what tool we are using
echo  -n "Using "
which    $KCONFIG

# Do the config split
$KCONFIG kconfig.db

# Show what we did
if [  -z "$2" ]; then
 echo -n "Split configs in "
 pwd
 echo
 ls   -l *.cfg
 echo
 rm   -f kconfig.db
fi

# Clean up
rm    -f *.tag
