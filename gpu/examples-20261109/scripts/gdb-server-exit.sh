#!/bin/bash
# Backtrace of the loader's crash at exit: kernelretsnoop under the syscall server in gdb, SIGINT after load.
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-cuda
W=/home/jetson/src/bpftime-gpu-work
O=$W/runs/gdb-server-exit; mkdir -p $O
S=bpftime_gpu_gdbexit; rm -f /dev/shm/$S
cd $W/example/gpu/kernelretsnoop
export BPFTIME_GLOBAL_SHM_NAME=$S BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16
timeout -k 5 100 gdb -batch -nx -ex "set startup-with-shell off" -ex "set pagination off" \
  -ex "set environment LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so" \
  -ex "handle SIGINT nostop noprint pass" -ex run -ex bt -ex "info threads" -ex "thread apply all bt 12" \
  ./kernelretsnoop > $O/gdb.log 2>&1 &
GP=$!
for i in $(seq 1 60); do P=$(pgrep -x kernelretsnoop | head -1); [ -n "$P" ] && [ -e /dev/shm/$S ] && break; sleep 0.5; done
sleep 4
echo "inferior pid=$P"; kill -INT $P
wait $GP; echo gdb_rc=$?
rm -f /dev/shm/$S
pgrep -a -x kernelretsnoop || echo no-leftovers
