#!/bin/bash
# x86 NUC ssl-nginx fair-scope round (10/26): baseline, kernel, official 177a1dc, a7d54d7e,
# same CO-RE sslsniff (bpftime example, unmodified) for every leg. Adapted from Jetson
# btf-kernel-20261026/ssl-nginx/run.sh (image from ssl-nginx-2x2-20261023/docker/Dockerfile).
# x86 additions: nginx on CPU 2 and sslsniff on CPU 4 (P cores, siblings 3 and 5 idle) at
# 3.5 GHz, wrk on E cores 8-9 at 2.5 GHz, run_fair.py itself on E core 10; turbo on,
# min = max, C6+ disabled (4.4 GHz is not held with several cores busy). Frequency logged
# every second from E core 15; checked before and after; restored at the end.
set -euo pipefail
R=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-ssl-nginx-20261026
B=/home/y/src/bpftime-x86-runs/ssl-nginx-20261026/builds
IMAGE=bpftime-ssl-nginx:x86-20261026
NAME=ssl-x86
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
    -v /lib/modules:/lib/modules:ro -v "$R":/bench -v "$B":/builds:ro -v "$R/sslsniff":/sslsniff:ro \
    "$IMAGE" sleep infinity >"$LOG/container-id.txt"
run() {
    sudo -n docker exec -e NGINX_CPUS=2 -e SNIFF_CPUS=4 -e WRK_CPUS=8,9 "$NAME" \
        taskset -c 10 python3 /bench/run_fair.py "$@"
}
mark() { echo "$1 at freq-log line $(wc -l <"$LOG/freq-log.txt")" >>"$LOG/freq-log-marks.txt"; }
mark audit
run --payload 16 --legs kernel,official,a7d54d7e --audit --out /bench/audit-16.json >"$R/audit-16.log" 2>&1
run --payload 262144 --legs kernel,official,a7d54d7e --audit --out /bench/audit-256k.json >"$R/audit-256k.log" 2>&1
python3 -I - "$R" <<'PY'
import json, sys
from pathlib import Path
r = Path(sys.argv[1])
for p in ('16', '256k'):
    a = json.load(open(r / f'audit-{p}.json'))
    for leg, v in a.items():
        assert v['events'] > 0 and v['events_per_request'] > 0.9, (p, leg, v)
        assert not v['wrk_errors'], (p, leg, v)
print('Event audit passed for every tracing leg.', flush=True)
PY
for r in r1 r2; do
    mark "fair-16-$r"
    run --payload 16 --reps 10 --legs baseline,kernel,official,a7d54d7e --out /bench/fair-16-$r.json >"$R/fair-16-$r.log" 2>&1
    mark "fair-256k-$r"
    run --payload 262144 --reps 10 --legs baseline,kernel,official,a7d54d7e --out /bench/fair-256k-$r.json >"$R/fair-256k-$r.log" 2>&1
done
mark end
sudo -n python3 "$R/busy-check.py" "$TSC" "${PINS[@]}" >"$LOG/check-after.txt"
(cd "$R" && sha256sum --check --quiet artifact-sha256.txt)
python3 "$R/summarize.py" >"$R/summary.md"
