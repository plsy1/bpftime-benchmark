#!/bin/bash
# Builds outattach (bpftime's libbpf 1.7.0, static), burstloop, and out-<kind>-<size>.bpf.o.
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
INC=/home/jetson/src/results/uprobe/map-table-20261028/src/inc
gcc-13 -O2 -g -pthread -I"$LB" -o outattach outattach.c "$LB/libbpf.a" -lelf -lz
gcc-13 -O2 -g -pthread -o burstloop burstloop.c
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -I$INC -I/usr/include/aarch64-linux-gnu"
$BPF -DKIND=0 -c out.bpf.c -o out-control.bpf.o
for kv in perf:5 rbout:6 rbres:7; do
  for size in 8 256 4096; do
    $BPF -DKIND=${kv#*:} -DSIZE=$size -c out.bpf.c -o out-${kv%:*}-$size.bpf.o
  done
done
ls -1 *.bpf.o | wc -l
