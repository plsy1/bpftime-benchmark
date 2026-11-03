#!/bin/bash
# Builds the loader `attach` (bpftime's libbpf 1.7.0, static), keyloop and the BPF objects
# keymap-k<KEY>-m<MAX_ENTRIES>.bpf.o and keymap-control.bpf.o.
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -pthread -o keyloop keyloop.c
gcc-13 -O2 -g -I"$LB" -o attach attach.c "$LB/libbpf.a" -lelf -lz
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu"
for k in 4 8; do
  for m in 10240 102400; do
    $BPF -DKEY_SIZE=$k -DMAX_ENTRIES=$m -c keymap.bpf.c -o keymap-k$k-m$m.bpf.o
  done
done
$BPF -DCONTROL=1 -c keymap.bpf.c -o keymap-control.bpf.o
