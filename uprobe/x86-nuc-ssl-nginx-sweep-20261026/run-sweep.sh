#!/bin/bash
# x86 NUC ssl-nginx payload sweep (10/26): the 7 sizes of the official draw_figture.py, 5 reps
# per size, legs baseline, kernel, official 177a1dc, a7d54d7e rotated; a 3 s event audit per size
# first. Adapted from Jetson ssl-nginx-sweep-bca2ebce-20261024 (run_sweep.sh) and from
# ../x86-nuc-ssl-nginx-20261026/run.sh, whose pinning and frequencies it keeps unchanged.
# x86 additions: nginx on CPU 2 and sslsniff on CPU 4 (P cores, siblings 3 and 5 idle) at
# 3.5 GHz, wrk on E cores 8-9 at 2.5 GHz, run_fair.py itself on E core 10; turbo on,
# min = max, C6+ disabled (4.4 GHz is not held with several cores busy). Frequency logged
# every second from E core 15; checked before and after; restored at the end.
set -euo pipefail
R=/home/y/src/bpftime-wt-results/uprobe/x86-nuc-ssl-nginx-sweep-20261026
B=/home/y/src/bpftime-x86-runs/ssl-nginx-20261026/builds
IMAGE=bpftime-ssl-nginx:x86-20261026
NAME=ssl-x86-sweep
SSLSNIFF_DIR=/home/y/src/bpftime-x86-runs/ssl-nginx-20261026/raw/sslsniff
PINS=(2:3500000 4:3500000 8:2500000 9:2500000)
TSC=2112
LOG=$R/log
mkdir "$LOG"
FREQLOG_PID=
finish()
{
    status=$?
    trap - EXIT
    sudo -n docker rm -f "$NAME" >>"$LOG/cleanup.log" 2>&1 || true
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    if [ -e "$LOG/pin-state.txt" ]; then "$R/pin-cpus.sh" restore "$LOG/pin-state.txt" || status=99; fi
    "$R/pin-cpus.sh" show 2 4 8 9 >"$LOG/pin-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$LOG/exit-status.txt"
    date --iso-8601=seconds >"$LOG/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$LOG/started-at.txt"
(cd "$R" && sha256sum --check --quiet artifact-sha256.txt)
! sudo -n docker ps -a --format '{{.Names}}' | grep -qx "$NAME"
uname -r >"$LOG/kernel.txt"
"$R/pin-cpus.sh" pin "$LOG/pin-state.txt" "${PINS[@]}"
"$R/pin-cpus.sh" show 2 4 8 9 >"$LOG/pin-pinned.txt"
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$R/busy-check.py" "$TSC" "${PINS[@]}" >>"$LOG/check-before.txt" && break
    test "$try" -lt 15
done
top -bn2 -d2 | awk '/^top -/{n++} n==2' | sed -n '1,15p' >"$LOG/top-before.txt"
sudo -n python3 "$R/freq-log.py" 15 "$TSC" 2 4 8 9 >"$LOG/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
sudo -n docker run -d --name "$NAME" --privileged --pid=host --shm-size=1g -v /sys:/sys \
    -v /lib/modules:/lib/modules:ro -v "$R":/bench -v "$B":/builds:ro -v "$SSLSNIFF_DIR":/sslsniff:ro \
    "$IMAGE" sleep infinity >"$LOG/container-id.txt"
run() {
    sudo -n docker exec -e NGINX_CPUS=2 -e SNIFF_CPUS=4 -e WRK_CPUS=8,9 "$NAME" \
        taskset -c 10 python3 /bench/run_fair.py "$@"
}
mark() { echo "$1 at freq-log line $(wc -l <"$LOG/freq-log.txt")" >>"$LOG/freq-log-marks.txt"; }
for size in 16b:16 1kb:1024 2kb:2048 4kb:4096 16kb:16384 128kb:131072 256kb:262144; do
    name=${size%%:*}; bytes=${size#*:}
    mark "audit-$name"
    run --payload "$bytes" --legs kernel,official,a7d54d7e --audit --out "/bench/audit-$name.json" >"$R/audit-$name.log" 2>&1
    python3 -I - "$R/audit-$name.json" <<'PY'
import json, sys
for leg, v in json.load(open(sys.argv[1])).items():
    assert v['events'] > 0 and v['events_per_request'] > 0.9, (leg, v)
    assert not v['wrk_errors'], (leg, v)
PY
    mark "sweep-$name"
    run --payload "$bytes" --reps 5 --legs baseline,kernel,official,a7d54d7e --out "/bench/sweep-$name.json" >"$R/sweep-$name.log" 2>&1
    echo "$(date +%H:%M:%S) $name done"
done
mark end
sudo -n python3 "$R/busy-check.py" "$TSC" "${PINS[@]}" >"$LOG/check-after.txt"
(cd "$R" && sha256sum --check --quiet artifact-sha256.txt)
python3 "$R/plot_sweep.py" >"$R/summary-sweep.md"
