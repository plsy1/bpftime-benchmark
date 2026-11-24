#!/usr/bin/env bash
# E13 start-up time, Jetson (plan 2.2; K=1 is HotOS P0, the rest P1).
#  1. burst (P0/P1): spawnburst K=1 and K=16, backends kernel, O, D (AOT cache on, default),
#     D:noaot (BPFTIME_AOT_CACHE=0 in the tool), E13_ROUNDS rounds (default 200) x 2 reps (order
#     reversed in rep 2, as 11-08). Reported later: child p50 and max.
#     Reading of the plan's "N=200 x 2 reps": 200 rounds per (backend, K) cell, i.e. 200 starts at
#     K=1 and 3,200 at K=16 (11-08 used 20 rounds). Set E13_ROUNDS to change.
#  2. Dpc point: the same runner, backend Dpc, K in E13_DPC_KS (default 1), for the bridge to the
#     11-08 "-probecheck" (checks on) numbers. Separate invocation.
#  3. seq: n2-startup's spawnloop, N=200 one after another x 2 reps, out-control.bpf.o, builds O,D
#     (the runner adds none, kernel and agent-without-tool cells itself). E13_SEQ=0 skips it.
#  4. P1 (E13_P1=1, default on; failures recorded, not fatal): memleak-start 10 runs (cells kernel,
#     O, D, D:noaot) and n4-many-probes M=1/10/100/1000 (backends none,kernel,O,D, 2 reps).
# Nothing pinned: K=16 needs all six CPUs (taskset -c 0-5 undoes this shell's cpu2 mask). As 11-08.
# Prerequisite: prep/E13-build.sh (workloads/E13). Usage: E13-run.sh [session-id]
set -euo pipefail
SID=${1:-J-E13-20261124}
MIN=60                                  # expected ~15 min for 1-3, ~25-30 min with P1
KILL_NAMES="outattach shortlived spawnburst spawnloop memleak stampmain many-1 many-10 many-100 many-1000"
ARMS_USED="O D Dpc"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
W=$R/workloads/E13
ROUNDS=${E13_ROUNDS:-200}
start_session
check_workloads "$W"
(cd / && sha256sum --check --quiet "$W/external-sha256.txt")
printf 'rounds=%s dpc_ks=%s seq=%s p1=%s\n' "$ROUNDS" "${E13_DPC_KS:-1}" "${E13_SEQ:-1}" "${E13_P1:-1}" >"$LOG/params.txt"
T="taskset -c 0-5"

assert_clean; step "burst start"
budget 2400
timeout --kill-after=30 "$B" $T python3 -u "$P/E13/run_burst.py" --backends kernel,O,D,D:noaot --ks 1,16 --rounds "$ROUNDS" \
    --reps 2 --out "$F/burst.jsonl" >"$LOG/burst.log" 2>&1
step "burst end"

assert_clean; step "burst Dpc start"
budget 900
timeout --kill-after=30 "$B" $T python3 -u "$P/E13/run_burst.py" --backends Dpc --ks "${E13_DPC_KS:-1}" --rounds "$ROUNDS" \
    --reps 2 --out "$F/burst-Dpc.jsonl" >"$LOG/burst-Dpc.log" 2>&1
step "burst Dpc end"

if [ "${E13_SEQ:-1}" = 1 ]; then
    assert_clean; step "seq start"
    budget 1800
    timeout --kill-after=30 "$B" $T python3 -u "$P/E13/n2_startup.py" --n 200 --reps 2 --objs out-control.bpf.o --builds O,D \
        --out "$F/seq.jsonl" >"$LOG/seq.log" 2>&1
    step "seq end"
fi

if [ "${E13_P1:-1}" = 1 ]; then
    assert_clean; step "memleak-start start"
    budget 1200
    timeout --kill-after=30 "$B" $T bash "$P/E13/memleak_start.sh" "$F/memleak-start" 10 kernel O D D:noaot \
        >"$F/memleak-start.txt" 2>"$LOG/memleak-start.err" || echo "memleak-start exit $? (not fatal)" >>"$LOG/steps.txt"
    step "memleak-start end"
    sudo -n pkill -9 -x memleak || true
    for a in O D; do sudo -n "$R/builds/$a/bpftimetool" remove >/dev/null 2>&1 || true; done
    assert_clean; step "n4 start"
    budget 1800
    timeout --kill-after=30 "$B" $T python3 -u "$P/E13/n4_many.py" --ms 1,10,100,1000 --backends none,kernel,O,D --reps 2 \
        --out "$F/n4-many.jsonl" >"$LOG/n4-many.log" 2>&1 || echo "n4 exit $? (not fatal)" >>"$LOG/steps.txt"
    step "n4 end"
fi

python3 -I - "$F" <<'PY'
import json, sys, pathlib
f = pathlib.Path(sys.argv[1])
for p in sorted(f.glob('*.jsonl')):
    rows = [json.loads(l) for l in p.read_text().splitlines()]
    print(p.name, len(rows), 'rows,', sum(not r.get('ok') for r in rows), 'not ok')
PY
check_workloads "$W"
echo "E13 done"
