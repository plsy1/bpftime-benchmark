#!/bin/bash
# Builds the generic loader `attach` (bpftime's libbpf 1.7.0, static), countloop (with unpinned
# threads) and count-<kind>.bpf.o (copies of ../../map-table-20261028/src).
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -pthread -o countloop countloop.c
gcc-13 -O2 -g -I"$LB" -o attach attach.c "$LB/libbpf.a" -lelf -lz
# Only libbpf's bpf/ headers: its build dir also has a linux/ that shadows the uapi types.
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu"
for kv in percpu-hash:2 percpu-array:3 array:4 control:0; do
  $BPF -DKIND=${kv#*:} -c count.bpf.c -o count-${kv%:*}.bpf.o
done
