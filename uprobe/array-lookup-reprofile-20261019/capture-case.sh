#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD=/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors
RESULTS=/home/jetson/src/results/uprobe/array-lookup-reprofile-20261019
CASE=${1:?case required}
ROUND=${2:?round required}
PERIOD=${3:?cycle period required}
case "$CASE" in array_lookup|percpu_array_lookup) ;; *) exit 2 ;; esac
OUT="$RESULTS/profiles/$CASE-$ROUND"
LOADER="$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure"
VICTIM="$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim"
mkdir "$OUT"
test ! -e /dev/shm/bpftime_maps_shm
loader_pid=
profile_pid=
finish()
{
    status=$?
    trap - EXIT
    set +e
    if [[ -n "$profile_pid" ]]; then
        wait "$profile_pid"
    fi
    if [[ -n "$loader_pid" ]]; then
        sudo -n kill -TERM "$loader_pid" 2>/dev/null
        wait "$loader_pid" 2>/dev/null
    fi
    sudo -n "$BUILD/tools/bpftimetool/bpftimetool" remove >>"$OUT/cleanup.log" 2>&1
    printf '%s\n' "$status" >"$OUT/exit-status.txt"
    date --iso-8601=seconds >"$OUT/finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
date --iso-8601=seconds >"$OUT/started-at.txt"
printf '%s\n' "$PERIOD" >"$OUT/cycle-period.txt"
(exec sudo -n taskset -c 5 env \
    LD_PRELOAD="$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" \
    BPFTIME_VM_NAME=llvm SPDLOG_LEVEL=info "$LOADER") >"$OUT/loader.log" 2>&1 &
loader_pid=$!
for attempt in $(seq 1 300); do
    if rg -q 'Successfully started' "$OUT/loader.log"; then break; fi
    sudo -n kill -0 "$loader_pid" 2>/dev/null
    sleep 0.1
done
rg -q 'Successfully started' "$OUT/loader.log"
sleep 1
(exec sudo -n nsys profile --trace=none --sample=process-tree --run-as=root \
    --backtrace=dwarf --samples-per-backtrace=1 --sampling-period="$PERIOD" \
    --delay=5 --duration=5 --kill=none --wait=primary \
    --output="$OUT/capture" \
    taskset -c 5 env LD_PRELOAD="$BUILD/runtime/agent/libbpftime-agent.so" \
    BPFTIME_VM_NAME=llvm BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
    "$VICTIM" 2000000 0 3000 "$CASE") >"$OUT/victim.log" 2>"$OUT/nsys.log" &
profile_pid=$!
maps_saved=false
for attempt in $(seq 1 300); do
    if victim_pid=$(sudo -n pgrep -f "^$VICTIM 2000000 0 3000 $CASE$"); then
        if [[ "$victim_pid" =~ ^[0-9]+$ ]]; then
            sudo -n cat "/proc/$victim_pid/maps" >"$OUT/proc-maps.txt"
            if rg -q 'libbpftime-agent.so' "$OUT/proc-maps.txt"; then
                printf '%s\n' "$victim_pid" >"$OUT/victim-pid.txt"
                maps_saved=true
                break
            fi
        fi
    fi
    kill -0 "$profile_pid" 2>/dev/null
    sleep 0.1
done
test "$maps_saved" = true
profile_status=0
wait "$profile_pid" || profile_status=$?
printf '%s\n' "$profile_status" >"$OUT/nsys-exit-status.txt"
profile_pid=
test "$profile_status" -eq 0
# With --kill=none this nsys version can return before the victim flushes
# its final CSV. Keep the loader and shared memory alive until that row exists.
for attempt in $(seq 1 600); do
    if rg -q "^$CASE," "$OUT/victim.log"; then break; fi
    sleep 0.1
done
rg -q 'shm_open_type 1' "$OUT/victim.log" "$OUT/nsys.log"
rg -q "^$CASE," "$OUT/victim.log"
sudo -n nsys export --type sqlite --output="$OUT/capture.sqlite" \
    "$OUT/capture.nsys-rep" >"$OUT/export.log" 2>&1
printf 'Capture finished: %s round %s.\n' "$CASE" "$ROUND"
