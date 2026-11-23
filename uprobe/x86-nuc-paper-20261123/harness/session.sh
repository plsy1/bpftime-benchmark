#!/usr/bin/env bash
# Paper campaign, x86 NUC: one measurement session.
# Usage: session.sh <session-id> <plan-file>
# The plan file holds one run_paper.py argument line per step (without --config, --session,
# --out, --capdir); lines starting with # are skipped. Adapted from x86-nuc-ssl-nginx-20261026/run.sh.
# Guards (plan 2.0.1): frequency pinned and checked (APERF/MPERF), no leftover measured
# processes or bpftime shm, load < 0.3, build and sslsniff sha256 checked before and after,
# this session's Claude process tree moved to CPUs 12-14 and restored, everything restored on exit.
# Never touches network, sshd or tailscale.
set -euo pipefail
SID=${1:?session id}; PLAN=${2:?plan file}
R=/home/y/src/bpftime-x86-runs/paper-20261124
H=$R/harness
OUT=$R/raw/$SID
IMAGE=bpftime-ssl-nginx:x86-20261026
NAME=paper-x86
PINS=(2:3500000 4:3500000 6:3500000 8:2500000 9:2500000 10:2500000 11:2500000)
TSC=2112
mkdir "$OUT"
LOG=$OUT/log; mkdir "$LOG"
cp "$PLAN" "$OUT/plan.txt"
FREQLOG_PID=
CLAUDE_PIDS=()
finish()
{
    status=$?
    trap - EXIT
    sudo -n docker rm -f "$NAME" >>"$LOG/cleanup.log" 2>&1 || true
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    if [ -e "$LOG/pin-state.txt" ]; then "$H/pin-cpus.sh" restore "$LOG/pin-state.txt" || status=99; fi
    "$H/pin-cpus.sh" show 2 4 6 8 9 10 11 >"$LOG/pin-restored.txt" 2>&1 || true
    for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 0-15 "$p" >/dev/null 2>&1 || true; done
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT" 2>/dev/null || true
    printf '%s\n' "$status" >"$LOG/exit-status.txt"
    date --iso-8601=seconds >"$LOG/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$LOG/started-at.txt"

# --- start guards ---
check_sha() { (cd "$R/builds" && for d in */; do (cd "$d" && sha256sum --check --quiet sha256.txt) || exit 1; done) && (cd "$R/sslsniff" && sha256sum --check --quiet sha256.txt); }
check_sha
python3 - <<'PY'
import os, sys
bad = []
for pid in os.listdir('/proc'):
    if not pid.isdigit(): continue
    try: exe = os.readlink(f'/proc/{pid}/exe')
    except OSError: continue
    b = os.path.basename(exe)
    if b in ('nginx', 'wrk', 'test', 'uprobe', 'memleak', 'bpftrace', 'java') or b.startswith(('sslsniff', 'bpftime')):
        bad.append((pid, exe))
if bad: sys.exit(f'leftover processes: {bad}')
PY
test -z "$(find /dev/shm -mindepth 1 -name 'bpftime*' 2>/dev/null)" || { echo "bpftime shm left"; exit 1; }
awk '{ if ($1 >= 0.3) { print "load too high: " $1; exit 1 } }' /proc/loadavg
! sudo -n docker ps -a --format '{{.Names}}' | grep -qx "$NAME"
{ uname -r; cat /proc/cmdline; grep -r . /sys/devices/system/cpu/vulnerabilities/; } >"$LOG/kernel.txt"
systemctl list-timers --all --no-pager >"$LOG/timers.txt" 2>&1 || true
sudo -n docker image inspect -f '{{.Id}}' "$IMAGE" >"$LOG/image-id.txt"

# Move this Claude session's process tree off the measured CPUs.
p=$$
while [ "$p" -gt 1 ]; do
    c=$(cat /proc/$p/comm 2>/dev/null || true)
    case $c in claude|node|2.1.*) CLAUDE_PIDS+=("$p");; esac
    p=$(awk '{print $4}' /proc/$p/stat)
done
for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 12-14 "$p" >>"$LOG/claude-taskset.txt" 2>&1 || true; done
taskset -pc 12-14 $$ >/dev/null

"$H/pin-cpus.sh" pin "$LOG/pin-state.txt" "${PINS[@]}"
"$H/pin-cpus.sh" show 2 4 6 8 9 10 11 >"$LOG/pin-pinned.txt"
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$H/busy-check.py" "$TSC" "${PINS[@]}" >>"$LOG/check-before.txt" && break
    test "$try" -lt 15
done
top -bn2 -d2 | awk '/^top -/{n++} n==2' | sed -n '1,20p' >"$LOG/top-before.txt"
sudo -n python3 "$H/freq-log.py" 15 "$TSC" 2 4 6 8 9 10 11 >"$LOG/freq-log.txt" 2>&1 &
FREQLOG_PID=$!

sudo -n docker run -d --name "$NAME" --privileged --pid=host --shm-size=1g -v /sys:/sys \
    -v /lib/modules:/lib/modules:ro -v "$H":/bench -v "$R/builds":/builds:ro \
    -v /home/y/src/bpftime-x86-runs/ssl-nginx-20261026/builds:/builds177:ro \
    -v "$R/sslsniff":/sslsniff:ro -v "$OUT":/out "$IMAGE" sleep infinity >"$LOG/container-id.txt"

step=0
while IFS= read -r line; do
    case $line in ''|'#'*) continue;; esac
    step=$((step + 1))
    echo "step $step at freq-log line $(wc -l <"$LOG/freq-log.txt"): $line" >>"$LOG/freq-log-marks.txt"
    if [ "${line:0:1}" = '!' ]; then  # host command inside the pinned session (e.g. pinbench)
        OUT="$OUT" H="$H" timeout 3000 bash -c "${line:1}" 2>&1 | tee -a "$OUT/console.log"
        continue
    fi
    # shellcheck disable=SC2086
    sudo -n timeout 6000 docker exec "$NAME" taskset -c 12-14 python3 /bench/run_paper.py \
        --config /bench/config-x86.json --session "$SID" --out /out/rows.jsonl --capdir /out/cap \
        $line 2>&1 | tee -a "$OUT/console.log"
done <"$PLAN"

echo "end at freq-log line $(wc -l <"$LOG/freq-log.txt")" >>"$LOG/freq-log-marks.txt"
sudo -n python3 "$H/busy-check.py" "$TSC" "${PINS[@]}" >"$LOG/check-after.txt" || echo "check-after FAILED" >>"$LOG/check-after.txt"
check_sha
test -z "$(find /dev/shm -mindepth 1 -name 'bpftime*' 2>/dev/null)" || echo "shm left after session" >>"$LOG/warnings.txt"
