#!/bin/sh
# Host side: profile the nginx worker for 8 s while run_profile.sh drives load.
P=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf
R=$(dirname "$0"); STAGE=$1; BUILD=$2; CG=${3:-fp}
sudo -n docker exec -d ssl-perf /perf/run_profile.sh "$STAGE" "$BUILD"
sleep 14
W=$(pgrep -n -x nginx)
echo "worker=$W"
sudo -n $P record -F 2999 --call-graph "$CG" -p "$W" -o "$R/stage$STAGE-$BUILD.data" -- sleep 8 2>&1 | tail -1
while pgrep -x wrk >/dev/null; do sleep 1; done
grep Requests/sec "$R/wrk-stage$STAGE.txt"
