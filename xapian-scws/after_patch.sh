#!/bin/sh
# usage ./after_patch.sh [version]
#
version=$1
srcdir="xapian-core-scws-${version}"

if ! test -d $srcdir ; then
  echo "Not exists source directory: $srcdir"
  echo "Usage $0 [version]"
  exit
fi

echo "1. generate the new patch ..."
gen=`echo $0 | sed -e 's#after_#gen_#'`
sh $gen $version

echo "2. replace version & bug url ..."
sed -i .bak \
	-e 's#.org/bugs\]#.xunsearch.com/bugs\]#' \
	-e 's#xapian-core\],#xapian-core-scws\],#' \
	$srcdir/configure.ac 

echo "3. Re-run the autoconf tools ..."
cd $srcdir
autoheader
rm -f configure.ac.bak config.h.in~
autoconf
cd ..

echo "4. Cleaning old tarball ..."
rm -f ../xunsearch-full/packages/xapian-*.tar.bz2

echo "5. Make the new tarball ..."
tar cjf ../xunsearch-full/packages/$srcdir.tar.bz2 $srcdir

echo "6. Cleaning unused source dirs ..."
rm -rf xapian-core-$version $srcdir

