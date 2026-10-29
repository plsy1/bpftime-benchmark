#!/bin/bash
# perf of the nginx process in each leg: profile_leg.py (in the container) sets up the leg and
# runs wrk -c 100 -d 20; from 3 s after nginx serves, perf stat counts nginx's cycles and
# instructions for 8 s and perf record samples it (frame-pointer call graphs). Per-request
# figures use wrk's requests/s for the 8 s window. Run from the host with container sc-nginx up.
set -u
R=/home/jetson/src/results/syscall/syscount-nginx-20261029
PERF=/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf
out=$R/perf
TAG=${TAG:-}
for leg in "$@"; do
  log=$out/$leg$TAG.leg.txt
  sudo -n docker exec sc-nginx python3 $R/profile_leg.py $leg > $log 2>&1 &
  exec_pid=$!
  for i in $(seq 300); do grep -q NGINX_PID $log && break; sleep 0.1; done
  pid=$(awk '/NGINX_PID/{print $2}' $log)
  if [ -z "$pid" ]; then echo "$leg: no nginx"; cat $log; wait $exec_pid; continue; fi
  sleep 3
  sudo -n timeout 30 $PERF stat -e cycles,instructions,cache-misses,branch-misses -p $pid \
    -o $out/$leg$TAG.stat.txt -- sleep 8 &
  stat_pid=$!
  sudo -n timeout 30 $PERF record -F 2999 -g -p $pid -o $out/$leg$TAG.perf.data -- sleep 8 > /dev/null 2>&1
  wait $stat_pid
  wait $exec_pid
  sudo -n chown jetson:jetson $out/$leg$TAG.perf.data $out/$leg$TAG.stat.txt
  $PERF report -i $out/$leg$TAG.perf.data --no-children --sort dso,sym --stdio -g none \
    --percent-limit 0.5 > $out/$leg$TAG.report.txt 2>/dev/null
  echo "$leg $(grep RPS $log)"
done
