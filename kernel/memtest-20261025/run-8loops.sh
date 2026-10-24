#!/bin/bash
# memtester 4.5 GB x 8 loops with a per-minute temperature log; must end before the 03:33 daily job.
D=$(dirname "$0")
( while pgrep -x memtester >/dev/null || [ ! -e "$D/loops8-finished.txt" ]; do
    printf '%s' "$(date +%H:%M:%S)"
    for z in /sys/class/thermal/thermal_zone*; do printf ' %s=%s' "$(cat $z/type)" "$(( $(cat $z/temp) / 1000 ))"; done
    echo " nvme=$(sudo -n nvme smart-log /dev/nvme0 2>/dev/null | awk '/^temperature/{print $3}')"
    sleep 60
    [ -e "$D/loops8-finished.txt" ] && break
  done ) > "$D/temps-8loops.log" 2>&1 &
date > "$D/loops8-started.txt"
sudo -n memtester 4500M 8 > "$D/memtester-4500M-8loops.log" 2>&1
echo "exit=$?" > "$D/loops8-exit.txt"
date > "$D/loops8-finished.txt"
