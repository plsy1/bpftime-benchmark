#!/usr/bin/env bash
# E4 full campaign: per-CPU lost updates, legs K/Ol/Ol730x/Dn/Dn730, T 1/4/16/64.
#   phase 1 (loss):   variants a,b1,c with the exact per-run counter (rows verifiable)
#     (b0 is byte-identical to a in BPF bytecode, ld/add/st, and both JIT to one incq; add it
#     only as a check via E4_LOSS_VARIANTS)
#   phase 2 (timing): variant a with --no-count (no counter on the hot path; #730 lock price)
# Each single run takes the measurement lock itself (run_e4.py --lock; flock -n semantics, 60 s
# retry x20). Touches no CPU frequency, IRQ or governor settings. run_e4.py sha-checks every
# selected build and e4/src before starting. Output: e4/raw/<SID>/{e4.jsonl,summary.txt,...}
# Usage: run-e4.sh SID [extra run_e4.py args for both phases, e.g. --reps 5]
#   env E4_LOSS_VARIANTS (default a,b1,c), E4_SKIP_TIMING=1 to skip phase 2
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; E=$R/e4
SID=${1:?sid}; shift; F=$E/raw/$SID; mkdir -p "$F"
date -Is > "$F/start-time.txt"
cat /proc/loadavg > "$F/load-before.txt"
{ uname -a; echo "preempt: $(cat /sys/kernel/debug/sched/preempt 2>/dev/null || sudo -n cat /sys/kernel/debug/sched/preempt 2>/dev/null || echo unknown)";
  echo "cmdline: $(cat /proc/cmdline)"; } > "$F/kernel.txt"
echo "$0 $SID $*" > "$F/args.txt"
rc1=0; rc2=0
taskset -c 12-14 python3 "$E/run_e4.py" --lock "$R/.measure.lock" --out "$F/e4.jsonl" \
  --variants "${E4_LOSS_VARIANTS:-a,b1,c}" "$@" > "$F/console-loss.log" 2>&1 || rc1=$?
if [ "${E4_SKIP_TIMING:-0}" != 1 ]; then
  taskset -c 12-14 python3 "$E/run_e4.py" --lock "$R/.measure.lock" --out "$F/e4.jsonl" \
    --variants a --no-count "$@" > "$F/console-timing.log" 2>&1 || rc2=$?
fi
echo "loss=$rc1 timing=$rc2" > "$F/exit-status.txt"
cat /proc/loadavg > "$F/load-after.txt"
date -Is > "$F/end-time.txt"
python3 "$E/summarize_e4.py" "$F/e4.jsonl" > "$F/summary.txt" 2>&1 || true
[ $rc1 -eq 0 ] && [ $rc2 -eq 0 ]
