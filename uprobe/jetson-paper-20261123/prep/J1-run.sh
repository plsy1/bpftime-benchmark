#!/usr/bin/env bash
# J1 (ATC week 1): opensnoop gap on Orin (E12 run-3 cell + run_cnt / perf openat / ringbuf size).
# K x10, Kpin x5 (tool cpu 3, workload cpus 0,1,4,5), D (9ccda7e8) x5. Usage: J1-run.sh [session-id]
set -euo pipefail
SID=${1:-J-J1-20261125}
MIN=25                                  # expected ~6 min
KILL_NAMES="opensnoop openloop perf"
ARMS_USED="D"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
start_session
(cd "$HOME/src/realprog-20261026-btf" && sha256sum --check --quiet <(grep -v ' /' "$R/logs/E12-examples/realprog-inputs-sha256.txt"))
cmp -s "$P/J1/audit.py" "$P/E12/audit.py"
cp "$P/J1/j1.py" "$P/J1/audit.py" "$F/"
old=$(sysctl -n kernel.bpf_stats_enabled)
sudo -n sysctl -w kernel.bpf_stats_enabled=1 >>"$LOG/steps.txt"
trap 'st=$?; sudo -n sysctl -w kernel.bpf_stats_enabled='"$old"' >/dev/null; (exit $st); finish' EXIT
assert_clean
step "J1 start"
budget 1200
(cd "$F" && timeout --kill-after=30 "$B" taskset -c 0-5 python3 -u "$P/J1/j1.py" "$F/rows.jsonl") >"$LOG/run.log" 2>&1 \
    || { echo "J1 exit $?" >>"$LOG/steps.txt"; exit 1; }
step "J1 end"
echo "J1 done"
