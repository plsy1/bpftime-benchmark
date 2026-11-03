#!/usr/bin/env bash
# x86 NUC, 11/03: the hash map key-pattern matrix of Jetson results uprobe/hash-keypattern-20261103
# (run_keys.py, src/: unchanged sources, built by src/build-x86.sh) for the kernel and the frozen
# dev d5279469 build, then the addr pattern with and without BPFTIME_HASH_ZERO_SEED=1.
# keyloop pins its threads to CPUs 5, 4, ..., 0 (P cores 2, 1, 0, both threads each): those six
# CPUs run at 3.5 GHz, turbo on, min = max, C6 and deeper disabled (4.4 GHz is not held with
# several cores busy; as ../../x86-nuc-dev-6ab9cbfe-20261102/ssl-nginx). Frequency logged every
# second from E core 15; checked before; restored at the end. plymouthd on E cores 12-14 meanwhile.
# Each step has its own total timeout; each keyloop run has run_keys.py's (secs + 120 s).
set -euo pipefail
K=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-d5279469-20261103/keypattern
PINS=(0:3500000 1:3500000 2:3500000 3:3500000 4:3500000 5:3500000)
TSC=2112
L=$K/log
mkdir "$L"
FREQLOG_PID= PLY=
finish()
{
    status=$?
    trap - EXIT
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    for name in attach keyloop; do sudo -n pkill -9 -x "$name" 2>/dev/null || true; done
    sudo -n /home/y/src/bpftime-x86-runs/dev-d5279469-20261103/keypattern/builds/dev-d5279469/bpftimetool remove >>"$L/cleanup.log" 2>&1 || true
    if [ -e "$L/pin-state.txt" ]; then "$K/pin-cpus.sh" restore "$L/pin-state.txt" || status=99; fi
    "$K/pin-cpus.sh" show 0 1 2 3 4 5 >"$L/pin-restored.txt" 2>&1 || true
    if [ -n "$PLY" ]; then sudo -n taskset -pc 0-15 "$PLY" >>"$L/plymouthd-affinity.txt" 2>&1 || true; fi
    printf '%s\n' "$status" >"$L/exit-status.txt"
    date --iso-8601=seconds >"$L/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$L/started-at.txt"
# Nothing of an earlier run may be left (this directory's own scripts excepted).
pgrep -af 'benchmark/test|uprobe/uprobe|keyloop|/attach |sslsniff|nginx|wrk|bpftime_|stress-ng' | grep -v -e pgrep -e "$K/" >"$L/leftover-processes.txt" || true
find /dev/shm -mindepth 1 -maxdepth 1 -user root >"$L/root-shm.txt"
if [ -s "$L/leftover-processes.txt" ] || [ -s "$L/root-shm.txt" ] || [ -e /dev/shm/bpftime_maps_shm ]; then
    echo "leftover processes or shared memory" >&2; exit 1
fi
(cd "$K/src" && sha256sum --check --quiet jetson-sources-sha256.txt)
sha256sum --check --quiet "$K/artifact-sha256.txt"
uname -r >"$L/kernel.txt"
PLY=$(pgrep -x plymouthd || true)
if [ -n "$PLY" ]; then
    taskset -pc "$PLY" >"$L/plymouthd-affinity.txt"
    sudo -n taskset -pc 12-14 "$PLY" >>"$L/plymouthd-affinity.txt"
fi
"$K/pin-cpus.sh" pin "$L/pin-state.txt" "${PINS[@]}"
"$K/pin-cpus.sh" show 0 1 2 3 4 5 >"$L/pin-pinned.txt"
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$K/busy-check.py" "$TSC" "${PINS[@]}" >>"$L/check-before.txt" && break
    test "$try" -lt 15
done
top -bn2 -d2 | awk '/^top -/{n++} n==2' | sed -n '1,15p' >"$L/top-before.txt"
sudo -n python3 "$K/freq-log.py" 15 "$TSC" 0 1 2 3 4 5 >"$L/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
mark() { echo "$1 at freq-log line $(wc -l <"$L/freq-log.txt")" >>"$L/freq-log-marks.txt"; }
mark matrix
timeout 4500 python3 "$K/run_keys.py" --backends kernel,dev-d5279469 --threads 1,6 --reps 3 --secs 5 \
    --out "$K/keys.jsonl" >"$K/keys.log" 2>&1
mark zeroseed
timeout 1800 python3 "$K/run_keys.py" --backends dev-d5279469,dev-d5279469-zeroseed --patterns addr \
    --threads 1,6 --reps 3 --secs 5 --out "$K/keys-zeroseed.jsonl" >"$K/keys-zeroseed.log" 2>&1
mark end
sha256sum --check --quiet "$K/artifact-sha256.txt"
pgrep -af 'keyloop|/attach ' | grep -v -e pgrep -e "$K/" >"$L/leftover-after.txt" || true
