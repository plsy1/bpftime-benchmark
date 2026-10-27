#!/bin/bash
# One user-uprobe run (HTTP 1 KB) with probes on the side: during a 10 s wrk window
# count the Go server's bpf() system calls and the goroutine uprobe sites it hits
# (bpftrace on the kernel), then a second 10 s window under perf record.
# Usage: why-slow.sh OUTDIR LEG   (LEG = user-uprobe or kernel-uprobe)
# Env: T=<socket_tracer of a DeepFlow variant>, TRACER_ENV="K=V ..." extra tracer env.
set -u
D=/home/jetson/src/deepflow-20261026; I=$D/bpftime-install
T=${T:-$D/deepflow-v7.2.3/agent/src/ebpf/samples/rust/socket-tracer/target/release/socket_tracer}
OUT=$1; LEG=$2; mkdir -p $OUT
PERF=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf
sudo -n pkill -9 -x wrk; sudo -n pkill -9 -x socket_tracer; sudo -n pkill -9 -x bpftime_daemon; sudo -n pkill -9 -x main; sleep 0.5
sudo -n $I/bpftimetool remove >/dev/null 2>&1
(cd $D/servers/http && sudo -n taskset -c 0-3 env BPFTIME_LOG_OUTPUT=console ./main 1024 > $OUT/server.txt 2>&1 &)
sleep 1.5; P=$(pgrep -n -x main); echo "server $P"
if [ $LEG = user-uprobe ]; then
  (sudo -n env BPFTIME_LOG_OUTPUT=console $I/bpftime_daemon -w 0x41770 -w 0x464f0 -w 0x46998 > $OUT/daemon.txt 2>&1 &); sleep 3
fi
(cd $OUT && sudo -n env DF_QUIET=1 DF_GO_PIDS=$P ${TRACER_ENV:-} $T > $OUT/tracer.txt 2>&1 &)
for i in $(seq 1 90); do grep -aq "adapt success" $OUT/tracer.txt 2>/dev/null && break; sleep 1; done
sleep 70
[ $LEG = user-uprobe ] && sudo -n env BPFTIME_LOG_OUTPUT=console $I/bpftime -i $I attach $P > $OUT/attach.txt 2>&1
sleep 2; curl -s -o /dev/null http://127.0.0.1:447/
# Window 1: counts. execute/newproc1 hits counted with kernel uprobes would add their own
# cost and, in the user leg, double the probes; count them in the kernel leg only.
if [ $LEG = kernel-uprobe ]; then
  sudo -n timeout -s INT 13 bpftrace -e "uprobe:$D/servers/http/main:runtime.execute /pid == $P/ { @execute = count(); } uprobe:$D/servers/http/main:runtime.newproc1 /pid == $P/ { @newproc1 = count(); }" > $OUT/bt-sites.txt 2>&1 &
else
  sudo -n timeout -s INT 13 bpftrace -e "tracepoint:syscalls:sys_enter_bpf /pid == $P/ { @bpf = count(); } tracepoint:raw_syscalls:sys_enter /pid == $P/ { @all_syscalls = count(); }" > $OUT/bt-bpf.txt 2>&1 &
fi
sleep 2
sudo -n taskset -c 4,5 wrk -t 2 -c 100 -d 10 http://127.0.0.1:447/ > $OUT/wrk1.txt 2>&1
sleep 2
# Window 2: where the server's CPU time goes.
sudo -n $PERF record -g -o $OUT/perf.data -p $P -- sleep 8 > $OUT/perf-record.txt 2>&1 &
sleep 1
sudo -n taskset -c 4,5 wrk -t 2 -c 100 -d 6 http://127.0.0.1:447/ > $OUT/wrk2.txt 2>&1
sleep 3
sudo -n pkill -9 -x socket_tracer; sudo -n pkill -9 -x bpftime_daemon; sudo -n pkill -9 -x main
sudo -n $I/bpftimetool remove >/dev/null 2>&1
grep -E "requests in|Requests/sec" $OUT/wrk1.txt
cat $OUT/bt-*.txt | grep -v "^Attaching"
