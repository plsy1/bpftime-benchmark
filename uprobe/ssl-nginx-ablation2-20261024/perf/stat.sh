#!/bin/sh
# Host side: perf stat of the nginx worker for 8 s under load; STAGE "none" = no tracing.
P=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf
R=$(dirname "$0"); STAGE=$1; BUILD=$2; TAG=$3
sudo -n docker exec -d ssl-perf /perf/run_profile.sh "$STAGE" "$BUILD"
sleep 14
W=$(pgrep -n -x nginx)
sudo -n $P stat -e cycles,instructions,branch-misses,L1-icache-load-misses,iTLB-load-misses,L1-dcache-load-misses,task-clock -p "$W" -o "$R/stat-$TAG.txt" -- sleep 8
while pgrep -x wrk >/dev/null; do sleep 1; done
grep Requests/sec "$R/wrk-stage$STAGE.txt"
