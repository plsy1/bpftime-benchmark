#!/bin/bash
# funclatency on the syscall server, workload under gdb with the agent; SIGUSR1 after 4 s.
set -u
b=$1; T=$2; S=$3
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/$b
LT=/home/jetson/src/realprog-20261026-btf/libbpf-tools
WL=/home/jetson/src/realprog-20261026-btf/workloads-mt/t$T
sudo -n pkill -9 -x funclatency; sudo -n $B/bpftimetool remove >/dev/null 2>&1
sudo -n timeout 60 env LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 $LT/funclatency -u $WL/callloop:target_func > $S/$b.t$T.tool.txt 2>&1 &
/bin/sleep 3
( /bin/sleep 6; sudo -n pkill -USR1 -x callloop ) &
sudo -n timeout 50 gdb -q -batch -ex 'handle SIGUSR1 nostop noprint pass' -ex 'set pagination off' \
  -ex "set exec-wrapper env LD_PRELOAD=$B/libbpftime-agent.so BPFTIME_LOG_OUTPUT=console" \
  -ex run -ex 'info threads' -ex bt -ex 'info registers pc sp x30' -ex 'x/8i $pc-16' -ex 'thread apply all bt 8' \
  --args $WL/callloop 10 > $S/$b.t$T.gdb.txt 2>&1
echo "gdb rc=$?"
sudo -n pkill -INT -x funclatency; /bin/sleep 1; sudo -n pkill -9 -x funclatency; sudo -n $B/bpftimetool remove >/dev/null 2>&1
