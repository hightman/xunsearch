#!/bin/sh
# timezone
zone_info=/usr/share/zoneinfo/$TZ
if [ -n "$TZ" ] && [ -e $zone_info ]; then
  ln -sf $zone_info /etc/localtime && echo $TZ > /etc/timezone
fi

# servers
sig_stop() {
  echo "ENTRY-POINT: stop signal received."
  bin/xs-indexd -k stop
  bin/xs-searchd -k stop
}
trap sig_stop TERM INT QUIT

echo "ENTRY-POINT: starting servers..."
echo -n > tmp/docker.log
bin/xs-indexd -l tmp/docker.log -k start
bin/xs-searchd -l tmp/docker.log -k start
tail -f tmp/docker.log &

wait

pid1=$(cat tmp/pid.8383)
pid2=$(cat tmp/pid.8384)
echo -n "ENTRY-POINT: waiting for servers to exit."
while true; do
  if kill -0 $pid1 2>/dev/null || kill -0 $pid2 2>/dev/null; then
    echo -n "."
    sleep 1
  else
    break
  fi
done
echo
