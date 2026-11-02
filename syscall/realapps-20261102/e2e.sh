#!/usr/bin/env bash
# Go programs with the agent and syscount attached: the hook-stack transformer
# (20 runs each) and a4ac0d31's (3 runs each); then one smoke-dyn run alone under
# syscount to compare its counts with what the program makes.
set -u
R=/home/jetson/src/results/syscall/realapps-20261102
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily
AG=$B/runtime/agent/libbpftime-agent.so
SERVER=$B/runtime/syscall-server/libbpftime-syscall-server.so
SC=/home/jetson/src/results/syscall/syscount-nginx-20261029/view/example/libbpf-tools/syscount/syscount
TOOL=$B/tools/bpftimetool/bpftimetool
OUT=$R/e2e; mkdir -p $OUT
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $OUT/clock-before.txt
start_syscount() { sudo -n $TOOL remove >/dev/null 2>&1; (sudo -n timeout 1500 env LD_PRELOAD=$SERVER $SC -d $1 > $2 2>&1 &); sleep 3; }
wait_syscount() { while pgrep -f "[s]yscount -d $1" >/dev/null; do sleep 2; done; sudo -n $TOOL remove >/dev/null 2>&1; }
for t in hookstack:20 a4ac0d31:3; do
  name=${t%%:*}; reps=${t##*:}
  T=$R/hook-perf/transformer-$name.so
  start_syscount 1200 $OUT/syscount-$name.out
  for bin in go/smoke-dyn go/deep/deep go/grow/grow go/httpd/httpd; do
    for i in $(seq 1 $reps); do
      sudo -n timeout 120 env AGENT_SO=$AG LD_PRELOAD=$T $R/$bin > $OUT/o.txt 2> $OUT/e.txt
      rc=$?
      echo "{\"transformer\": \"$name\", \"bin\": \"$bin\", \"run\": $i, \"rc\": $rc, \"out\": \"$(tail -1 $OUT/o.txt)\", \"err\": \"$(grep -m1 -E 'fatal|panic' $OUT/e.txt | tr -d '"\\')\"}" >> $OUT/runs.jsonl
    done
  done
  sudo -n pkill -INT -f '[s]yscount -d 1200'; wait_syscount 1200
done
# Counts: smoke-dyn makes 64 x 20000 getpid, writes and reads (pipe), stats.
start_syscount 40 $OUT/syscount-count.out
sudo -n timeout 120 env AGENT_SO=$AG LD_PRELOAD=$R/hook-perf/transformer-hookstack.so $R/go/smoke-dyn > $OUT/count-run.txt 2>&1
wait_syscount 40
rm -f $OUT/o.txt $OUT/e.txt
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $OUT/clock-after.txt
echo done > $OUT/finished
