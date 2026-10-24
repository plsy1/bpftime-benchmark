#!/bin/sh
# Host side: while run_profile.sh drives 256 KB load with sslsniff stage $1 (build $2), record
# the nginx worker's system calls (raw_syscalls:sys_enter) for 8 s, then cycles with call graphs
# for 8 s. Output: sys-$1.data, cyc-$1.data, wrk-stage$1.txt.
P=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf
R=$(dirname "$0"); STAGE=$1; BUILD=$2
sudo -n docker exec -d ssl-copyperf /perf/run_profile.sh "$STAGE" "$BUILD"
sleep 6
W=$(pgrep -n -x nginx)
echo "worker=$W"
sudo -n $P record -e raw_syscalls:sys_enter -p "$W" -o "$R/sys-$STAGE.data" -- sleep 8 2>&1 | tail -1
sudo -n $P record -F 2999 --call-graph fp -p "$W" -o "$R/cyc-$STAGE.data" -- sleep 8 2>&1 | tail -1
while pgrep -x wrk >/dev/null; do sleep 1; done
grep -E "Requests/sec|requests in" "$R/wrk-stage$STAGE.txt"
