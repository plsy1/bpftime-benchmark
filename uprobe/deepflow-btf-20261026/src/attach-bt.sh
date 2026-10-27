#!/bin/bash
# bpftrace (loaded under the syscall server) + bpftime attach into a root JVM.
set -u
D=/home/jetson/src/deepflow-20261026; I=$D/bpftime-install
W=/home/jetson/src/realprog-20261026-btf/workloads
J=/usr/lib/jvm/java-17-openjdk-arm64/lib/server/libjvm.so
OUT=$D/$1; PROG=$2
sudo -n $I/bpftimetool remove >/dev/null 2>&1
sudo -n env BPFTIME_LOG_OUTPUT=console java -XX:+UseSerialGC -cp $W GcLoop 10 50 > $OUT.wl.txt 2>&1 &
sleep 2
P=$(pgrep -n -f "^java.*GcLoop 10 50")
sudo -n env LD_PRELOAD=$I/libbpftime-syscall-server.so BPFTIME_SHM_MEMORY_MB=1024 BPFTIME_LOG_OUTPUT=console \
  timeout -s INT 20 bpftrace ${BTPID:+-p $P} -e "${PROG//JVM/$J}" > $OUT.tool.txt 2>&1 &
sleep 4
sudo -n env BPFTIME_LOG_OUTPUT=console $I/bpftime -i $I attach $P > $OUT.cli.txt 2>&1
wait
sudo -n $I/bpftimetool remove >/dev/null 2>&1
grep gc_collections $OUT.wl.txt
grep -vE "^\[20|^$|Attaching" $OUT.tool.txt | sort | uniq -c | head
