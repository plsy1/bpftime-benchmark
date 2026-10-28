#!/bin/bash
# Builds the workload, the loader (bpftime's libbpf 1.7.0, static) and the BPF objects:
# churn-k<KEY>-v<VALUE>.bpf.o and control.bpf.o (no map operations).
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -pthread -o churnloop churnloop.c
gcc-13 -O2 -g -I"$LB" -o churn churn.c "$LB/libbpf.a" -lelf -lz
# Only libbpf's bpf/ headers: its build dir also has a linux/ that shadows the uapi types.
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu"
for kv in 4:8 8:8 4:112 8:112; do
  k=${kv%:*}; v=${kv#*:}
  $BPF -DKEY_SIZE=$k -DVALUE_SIZE=$v -c churn.bpf.c -o churn-k$k-v$v.bpf.o
done
$BPF -DCONTROL=1 -c churn.bpf.c -o control.bpf.o
