#!/usr/bin/env bash
# Lazy mode costs: per-call filter (layers, normal vs lazy, same transformer)
# and first-call trap+rewrite (jitbench, 1000 sites).
set -u
D=/home/jetson/src/results/syscall/coverage-20261030/perf
S=/home/jetson/src/results/syscall/breakdown-20261029/src
T=$D/transformer-lazy.so
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before-lazy.txt
for r in 0 1 2 3 4; do
  python3 $D/run_layers.py --legs none --rounds 1 --out $D/lazy-none.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$T python3 $D/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/lazy-off.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$T LAYER_EXTRA_ENV=BPFTIME_SYSCALL_LAZY=1 python3 $D/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/lazy-on.jsonl > /dev/null 2>&1
  for mode in plain default eager lazy; do
    case $mode in
      plain) env=() ;;
      default) env=(AGENT_SO=$S/libdummy_agent.so LD_PRELOAD=$T) ;;
      eager) env=(AGENT_SO=$S/libdummy_agent.so LD_PRELOAD=$T BPFTIME_SYSCALL_REWRITE_ANON=1) ;;
      lazy) env=(AGENT_SO=$S/libdummy_agent.so LD_PRELOAD=$T BPFTIME_SYSCALL_LAZY=1) ;;
    esac
    out=$(timeout 60 taskset -c 5 env "${env[@]}" $D/jitbench 1000 2>/dev/null | tail -1)
    echo "{\"round\": $r, \"mode\": \"$mode\", \"out\": \"$out\"}" >> $D/jitbench.jsonl
  done
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after-lazy.txt
echo done > $D/finished-lazy
