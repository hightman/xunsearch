#!/bin/sh
# usage: ./gen_patch.sh <version> [output]
#
version=$1
if test -z "$version"; then
  new_dir=`ls -t | grep -m1 ^xapian-core-scws-`
  version=${new_dir##*-}
else
  new_dir="xapian-core-scws-${version}"
fi
old_dir="xapian-core-${version}"
fpatch="patch.xapian-core-scws"
if ! test -z "$2" ; then
	fpatch=$2
fi

flist="configure.ac include/xapian/queryparser.h include/xapian/termgenerator.h"
if test -f "$old_dir/api/omqueryinternal.cc" ; then
  flist="$flist api/omqueryinternal.cc"
elif test -f "$old_dir/api/queryinternal.cc" ; then
  flist="$flist api/queryinternal.cc"
fi
flist="$flist queryparser/queryparser_internal.h queryparser/termgenerator_internal.h"
flist="$flist queryparser/queryparser.cc queryparser/queryparser_internal.cc"
flist="$flist queryparser/termgenerator.cc queryparser/termgenerator_internal.cc"
#flist="$flist queryparser/queryparser.lemony"

if test -z $old_dir || ! test -d $old_dir ; then
  echo "Non-exists orig source directory: $old_dir"
  echo "Usage: $0 [version]"
  exit
fi

if ! test -d $new_dir ; then
  echo "Non-exists source directory: $new_dir"
  echo "Usage: $0 [version]"
  exit
fi

echo "Start the patch generating, v${version} ..."
#mv -f $fpatch ${fpatch}.bak
echo -n > $fpatch
for f in $flist
do
  echo "diffing $f ... "
  diff -rcs $old_dir/$f $new_dir/$f >> $fpatch
done

echo "Done!"
exit

