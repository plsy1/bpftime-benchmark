#!/usr/bin/env bash
# E11 multi-thread churn + syscall layers, Jetson part (plan 2.2, P1).
#  1. churn: run_churn.py --backends kernel,O,D --objs churn-k4-v8.bpf.o,control.bpf.o
#     --threads 1,2,4,6 --reps 3 (--secs 8 default). Thread placement as 10-28: churnloop pins thread
#     i to CPU 5-i (one cluster up to 2 threads, cross-cluster from 4). No --cpus on the Jetson (the
#     plan's --cpus 0,2,4,6 is x86 only). Driver and loader unpinned (cpus 0-5), as 10-28.
#  2. layers: run_layers.py --rounds 5 (all 9 legs, n = 1,000,000) with BPFTIME_BUILD=builds/D.
#     Whole runner on cpu5 (10-29 ran unpinned; pinned here to cut migration noise: deviation).
#  3. optional (E11_O_LAYERS=1, default on): O on the agent legs, 1 round, in its own invocation whose
#     failure cannot stop the session: documents "O records 0 events on aarch64" (the official
#     transformer has no aarch64 svc rewriting). Rows will be ok=false by design.
# Prerequisite: prep/E11-build.sh (workloads/E11). Usage: E11-run.sh [session-id]
set -euo pipefail
SID=${1:-J-E11-20261124}
MIN=35                                  # expected ~15 min (churn 72 runs x ~9.5 s, layers ~1.5 min)
KILL_NAMES="churn churnloop tp_attach writeloop"
ARMS_USED="O D"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
W=$R/workloads/E11
start_session
check_workloads "$W"

step "churn start"
budget 1800
timeout --kill-after=30 "$B" taskset -c 0-5 python3 -u "$P/E11/run_churn.py" --backends kernel,O,D \
    --objs churn-k4-v8.bpf.o,control.bpf.o --threads 1,2,4,6 --reps 3 --out "$F/churn.jsonl" \
    >"$LOG/churn.log" 2>&1
step "churn end"
assert_clean

step "layers D start"
budget 900
BPFTIME_BUILD=$R/builds/D timeout --kill-after=30 "$B" taskset -c 5 python3 -u "$P/E11/run_layers.py" \
    --rounds 5 --out "$F/layers-D.jsonl" >"$LOG/layers-D.log" 2>&1
step "layers D end"
assert_clean

if [ "${E11_O_LAYERS:-1}" = 1 ]; then
    step "layers O start"
    budget 600
    BPFTIME_BUILD=$R/builds/O timeout --kill-after=30 "$B" taskset -c 5 python3 -u "$P/E11/run_layers.py" \
        --legs rewrite,agent-other,agent-exit,agent-count --rounds 1 --out "$F/layers-O.jsonl" \
        >"$LOG/layers-O.log" 2>&1 || echo "layers O exit $? (recorded, not fatal)" >>"$LOG/steps.txt"
    step "layers O end"
    for n in tp_attach writeloop; do sudo -n pkill -9 -x $n || true; done
    sudo -n "$R/builds/O/bpftimetool" remove >/dev/null 2>&1 || true
fi

# validity summary (no analysis): every churn row ok, every D layer row ok
python3 -I - "$F" <<'PY'
import json, sys, pathlib
f = pathlib.Path(sys.argv[1])
for name in ('churn.jsonl', 'layers-D.jsonl'):
    rows = [json.loads(l) for l in (f / name).read_text().splitlines()]
    bad = [r for r in rows if not r.get('ok')]
    print(name, len(rows), 'rows,', len(bad), 'not ok')
PY
check_workloads "$W"
echo "E11 done"
