#!/bin/sh
# usage: ./gen_patch.sh [version]
#
version=$1
old_dir="xapian-core-${version}"
new_dir="xapian-core-scws-${version}"
fpatch="patch.xapian-core-scws"
flist="configure.ac include/xapian/queryparser.h include/xapian/termgenerator.h"
flist="$flist api/omqueryinternal.cc"
flist="$flist queryparser/queryparser_internal.h queryparser/termgenerator_internal.h"
flist="$flist queryparser/queryparser.cc queryparser/queryparser_internal.cc "
flist="$flist queryparser/termgenerator.cc queryparser/termgenerator_internal.cc"

if ! test -d $old_dir ; then
  echo "Not exists orig source directory: $old_dir"
  echo "Usage: $0 [version]"
  exit
fi

if ! test -d $new_dir ; then
  echo "Not exists source directory: $new_dir"
  echo "Usage: $0 [version]"
  exit
fi

echo "Start the patch generating for $version"
mv -f $fpatch ${fpatch}.bak
echo -n > $fpatch
for f in $flist
do
  echo "diffing $f ... "
  diff -rcs $old_dir/$f $new_dir/$f >> $fpatch
done

echo "Done!"
exit

