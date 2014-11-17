#!/bin/sh
# usage ./before_patch.sh [version]
#
version=$1
srcfile="xapian-core-${version}.tar.xz"

if ! test -f $srcfile ; then
  echo "Not exists source file: $srcfile"
  echo "Usage $0 [version]"
  exit
fi

if test -d xapian-core-${version} ; then
  echo "ERROR: Source directory exists: ./xapian-core-${version}"
  exit;
fi

echo "1. extracting the tar ball ..."
tar xzf $srcfile

echo "2. copy the new dir ..."
cp -rf xapian-core-${version} xapian-core-scws-${version}

echo "3. OK, you can run patch command now!! and then run the after_patch.sh"
echo "   cd xapian-core-scws-${version}"
echo "   patch -p1 < ../patch.xapian-core-scws"

