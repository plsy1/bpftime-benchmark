#!/usr/bin/env bash
# goexec (main.go) under the transformer with syscount attached (programs:
# 18 KB frames), 10 runs per build: dev-b60b99c3 (the code of 94f9ba8f) and
# fix-final (the fifth review's fixes). Agent, transformer and server of the
# same build each time.
set -u
D=/home/jetson/src/results/reviews-20261105/fixes/go-exec
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds
SC=/home/jetson/src/results/syscall/syscount-nginx-20261029/view/example/libbpf-tools/syscount/syscount
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
for name in dev-b60b99c3 fix-final; do
  sudo -n $B/$name/bpftimetool remove >/dev/null 2>&1
  (sudo -n timeout 900 env LD_PRELOAD=$B/$name/libbpftime-syscall-server.so $SC -d 600 > $D/syscount-$name.out 2>&1 &)
  sleep 3
  for i in $(seq 1 10); do
    sudo -n timeout 120 env AGENT_SO=$B/$name/libbpftime-agent.so LD_PRELOAD=$B/$name/libbpftime-agent-transformer.so $D/goexec > $D/o.txt 2> $D/e.txt
    rc=$?
    echo "{\"build\": \"$name\", \"run\": $i, \"rc\": $rc, \"out\": \"$(tail -1 $D/o.txt)\", \"err\": \"$(grep -m1 -E 'fatal|panic|SIGSEGV|unexpected' $D/e.txt | tr -d '"\\')\"}" >> $D/runs.jsonl
  done
  sudo -n pkill -INT -f '[s]yscount -d 600'
  while pgrep -f "[s]yscount -d 600" >/dev/null; do sleep 2; done
  sudo -n $B/$name/bpftimetool remove >/dev/null 2>&1
done
rm -f $D/o.txt $D/e.txt
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
