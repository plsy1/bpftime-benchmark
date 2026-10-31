#!/bin/bash
# usage: run.sh <mb> <mode> [args...]
MB=$1; shift
NAME="fable3_exp_$$_$RANDOM"
export BPFTIME_GLOBAL_SHM_NAME="$NAME" BPFTIME_SHM_MEMORY_MB="$MB" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=error
export LD_PRELOAD=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/runtime/syscall-server/libbpftime-syscall-server.so
timeout 60 ./exp "$@" 2>&1
echo "exit=$?"
unset LD_PRELOAD
rm -f /dev/shm/"$NAME"*
