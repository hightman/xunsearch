#!/bin/sh -e
# timezone
zone_info=/usr/share/zoneinfo/$TZ
if [ -n "$TZ" ] && [ -e $zone_info ]; then
  ln -sf $zone_info /etc/localtime && echo $TZ > /etc/timezone
fi
# servers
rm -f tmp/pid.*
echo -n > tmp/docker.log
bin/xs-indexd -l tmp/docker.log -k start
bin/xs-searchd -l tmp/docker.log -k start
# display log
tail -f tmp/docker.log
