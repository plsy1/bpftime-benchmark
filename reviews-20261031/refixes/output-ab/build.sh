#!/bin/bash
# Builds outattach (bpftime's libbpf 1.7.0, static) and out-<kind>.bpf.o.
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
INC=/home/jetson/src/results/uprobe/map-table-20261028/src/inc
gcc-13 -O2 -g -pthread -I"$LB" -o outattach outattach.c "$LB/libbpf.a" -lelf -lz
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -I$INC -I/usr/include/aarch64-linux-gnu"
for kv in perf:5 ringbuf:6 control:0; do
  $BPF -DKIND=${kv#*:} -c out.bpf.c -o out-${kv%:*}.bpf.o
done
