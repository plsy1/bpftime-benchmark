#!/usr/bin/env bash
# Futex calls (by op) of the workload and the tool during one ringbuf 1-thread output-ab run.
# $1: backend (builds/<name>), $2: threads
set -u
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/$1
H=/home/jetson/src/results/reviews-20261031/refixes2/output-ab
SRC=/home/jetson/src/results/uprobe/map-table-20261028/src
T=${2:-1}
sudo -n pkill -9 -x outattach; sudo -n pkill -9 -x countloop; sudo -n $B/bpftimetool remove >/dev/null 2>&1; sleep 0.3
sudo -n timeout 60 bpftrace -e 'tracepoint:syscalls:sys_enter_futex /comm == "countloop" || comm == "outattach"/ { @[comm, tid] = count(); }' > bt.out 2>&1 &
BT=$!
sleep 3
sudo -n timeout 60 env LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=256 $H/outattach $H/out-ringbuf.bpf.o $SRC/countloop on_count=count_event > tool.out 2>&1 &
TOOL=$!
for i in $(seq 1 100); do grep -q attached tool.out && break; sleep 0.2; done
sudo -n timeout 60 env LD_PRELOAD=$B/libbpftime-agent.so BPFTIME_LOG_OUTPUT=console $SRC/countloop $T 6 1 > work.out 2>&1
sudo -n pkill -INT -x outattach; sleep 2
sudo -n pkill -INT -x bpftrace; wait $BT 2>/dev/null
sudo -n pkill -9 -x outattach; sudo -n $B/bpftimetool remove >/dev/null 2>&1
grep -E "ops_per_sec_median|total_ops" work.out; grep -E "received|lost" tool.out | tail -2; cat bt.out | grep "@"
