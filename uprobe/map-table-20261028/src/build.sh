#!/bin/bash
# Builds the generic loader `attach` (bpftime's libbpf 1.7.0, static), trackloop with
# track(-control).bpf.o (tracking table) and countloop with count-<kind>.bpf.o (shared counters).
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -pthread -o trackloop trackloop.c
gcc-13 -O2 -g -I"$LB" -o attach attach.c "$LB/libbpf.a" -lelf -lz
# Only libbpf's bpf/ headers: its build dir also has a linux/ that shadows the uapi types.
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu"
$BPF -c track.bpf.c -o track.bpf.o
$BPF -DCONTROL=1 -c track.bpf.c -o track-control.bpf.o
gcc-13 -O2 -g -pthread -o countloop countloop.c
for kv in hash:1 percpu-hash:2 percpu-array:3 array:4 control:0; do
  $BPF -DKIND=${kv#*:} -c count.bpf.c -o count-${kv%:*}.bpf.o
done
