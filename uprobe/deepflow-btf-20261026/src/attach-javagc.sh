#!/bin/bash
# javagc (loaded under the syscall server, -p JVM) + bpftime attach into a root JVM.
set -u
D=/home/jetson/src/deepflow-20261026; I=$D/bpftime-install
W=/home/jetson/src/realprog-20261026-btf/workloads
OUT=$D/$1; JAVAGC=$2
sudo -n $I/bpftimetool remove >/dev/null 2>&1
sudo -n env BPFTIME_LOG_OUTPUT=console BPFTIME_TRACE_PRINTK_OUTPUT=stdout java -XX:+UseSerialGC -cp $W GcLoop 10 5 > $OUT.wl.txt 2>&1 &
sleep 2
P=$(pgrep -n -f "^java.*GcLoop 10 5")
sudo -n env LD_PRELOAD=$I/libbpftime-syscall-server.so BPFTIME_SHM_MEMORY_MB=1024 BPFTIME_LOG_OUTPUT=console \
  timeout -s INT 20 $JAVAGC -p $P -t 0 > $OUT.tool.txt 2>&1 &
sleep 3
sudo -n env BPFTIME_LOG_OUTPUT=console $I/bpftime -i $I attach $P > $OUT.cli.txt 2>&1
wait
sudo -n $I/bpftimetool remove >/dev/null 2>&1
echo "pid $P"; grep gc_collections $OUT.wl.txt
grep -E "dbg|bpf_trace_printk" $OUT.wl.txt | sed -E 's/[0-9]+ time/N time/' | sort | uniq -c | head
grep -cE "^[0-9]{2}:[0-9]{2}:[0-9]{2} +[0-9]+ +$P " $OUT.tool.txt
