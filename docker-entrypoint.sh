#!/bin/sh
echo 'start xunsearch'
rm -f tmp/pid.* && bin/xs-indexd -l /dev/stdout -k start && bin/xs-searchd -l /dev/stdout -k start && tail -f /dev/null