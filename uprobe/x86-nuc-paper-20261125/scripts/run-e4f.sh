#!/usr/bin/env bash
# Dn730f check (2026-11-26): loss b1,c at T=16,64 and timing a at T=1,4,16,64; legs Dn, Dn730, Dn730f; 5 reps.
# Same runner, CPUs and lock as e4/run-e4.sh; only the leg/T selection differs.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; E=$R/e4
SID=${1:?sid}; F=$E/raw/$SID; mkdir -p "$F"
date -Is > "$F/start-time.txt"; cat /proc/loadavg > "$F/load-before.txt"
{ uname -a; echo "preempt: $(cat /sys/kernel/debug/sched/preempt 2>/dev/null || sudo -n cat /sys/kernel/debug/sched/preempt 2>/dev/null || echo unknown)"; } > "$F/kernel.txt"
echo "$0 $*" > "$F/args.txt"
rc1=0; rc2=0
taskset -c 12-14 python3 "$E/run_e4.py" --lock "$R/.measure.lock" --out "$F/e4.jsonl" --legs Dn,Dn730,Dn730f \
  --variants b1,c --threads 16,64 --reps 5 > "$F/console-loss.log" 2>&1 || rc1=$?
taskset -c 12-14 python3 "$E/run_e4.py" --lock "$R/.measure.lock" --out "$F/e4.jsonl" --legs Dn,Dn730,Dn730f \
  --variants a --no-count --threads 1,4,16,64 --reps 5 > "$F/console-timing.log" 2>&1 || rc2=$?
echo "loss=$rc1 timing=$rc2" > "$F/exit-status.txt"; cat /proc/loadavg > "$F/load-after.txt"; date -Is > "$F/end-time.txt"
python3 "$E/summarize_e4.py" "$F/e4.jsonl" > "$F/summary.txt" 2>&1 || true
echo "E4F exit loss=$rc1 timing=$rc2"
