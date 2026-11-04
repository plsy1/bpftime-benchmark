#!/bin/bash
# Manual SIGUSR1 detach: tool (memleak or funclatency, no -c) on the syscall-server, workload with
# the agent started directly so its exit status is visible. Usage: run.sh BUILD TOOL THREADS
set -u
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/$1; TOOL=$2; T=$3
D=/home/jetson/src/results/uprobe/memleak-fix-20261104/detach-manual
LT=/home/jetson/src/realprog-20261026-btf/libbpf-tools
WL=/home/jetson/src/realprog-20261026-btf/workloads-mt/t$T
stem=$D/$1.$TOOL.t$T
sudo -n pkill -9 -x memleak; sudo -n pkill -9 -x funclatency; sudo -n $B/bpftimetool remove >/dev/null 2>&1
case $TOOL in
  memleak) tcmd=($LT/memleak); wcmd=($WL/mallocloop 10 64 1024);;
  funclatency) tcmd=($LT/funclatency -u $WL/callloop:target_func); wcmd=($WL/callloop 10);;
esac
sudo -n timeout 40 env LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 "${tcmd[@]}" > $stem.tool.txt 2>&1 &
sleep 3
sudo -n timeout 30 env LD_PRELOAD=$B/libbpftime-agent.so BPFTIME_LOG_OUTPUT=console "${wcmd[@]}" > $stem.workload.txt 2>&1 &
wpid=$!
sleep 4
sudo -n pkill -USR1 -x ${wcmd[0]##*/}
wait $wpid; rc=$?
echo "$1 $TOOL t$T workload rc=$rc ops: $(grep -o 'ops_per_sec_median [0-9.]*' $stem.workload.txt)"
sudo -n pkill -INT -x $TOOL; sleep 1; sudo -n pkill -9 -x $TOOL; sudo -n $B/bpftimetool remove >/dev/null 2>&1
