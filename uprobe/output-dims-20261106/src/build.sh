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
# Ring buffer records that divide the 256 KiB ring (with the 8-byte header): no padding at its end.
for size in 248 4088; do
  $BPF -DKIND=7 -DSIZE=$size -c out.bpf.c -o out-rbres-$size.bpf.o
done
# opensnoop's record (8,240 bytes: 8,248 with the header, a padding every 31) and one of 8,184
# (8,192 with the header: divides the ring) (more-dims-20261106).
for size in 8184 8240; do
  $BPF -DKIND=7 -DSIZE=$size -c out.bpf.c -o out-rbres-$size.bpf.o
done
# Variable sizes 72/520/1480/3992 (more-dims-20261106).
for kv in perf:5 rbout:6 rbres:7; do
  $BPF -DKIND=${kv#*:} -DSIZE=4096 -DVAR=1 -c out.bpf.c -o out-${kv%:*}-var.bpf.o
done
# Soak object (more-dims-20261106/n6): hash churn + 256-byte ring buffer records.
$BPF -DKIND=8 -DSIZE=256 -c out.bpf.c -o out-soak.bpf.o
ls -1 *.bpf.o | wc -l
