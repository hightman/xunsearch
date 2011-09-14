#!/bin/sh
# Generate a constant definition file for PHP
# INPUT FILES: ../src/xs_cmd.h, ../config.h
# $Id: $ 

echo "<?php"
echo "/* Automatically generated at "`date +"%Y/%m/%d %H:%M" `" */"
grep "^#define[	 ]CMD_" ../src/xs_cmd.h | grep -v "[\"(]" | awk '{ print "define(\047" $2 "\047,\t" $3 ");" }'
if test -f ../config.h ; then
  grep "PACKAGE_" ../config.h | grep -v "STRING" | awk '{ print "define(\047" $2 "\047,\t" $3 ");" }'
fi
echo "/* end the cmd defination */"
echo
