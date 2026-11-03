#!/bin/bash
# x86 NUC version of build.sh (Jetson results uprobe/hash-keypattern-20261103/src/build.sh,
# kept here unchanged): the same sources and flags, with bpftime's libbpf 1.7.0 from the
# d5279469 build directory, clang-19 and the x86 target. Builds the loader `attach` (static
# libbpf), keyloop and the BPF objects keymap-k<KEY>-m<MAX_ENTRIES>.bpf.o and keymap-control.bpf.o.
set -eu
cd "$(dirname "$0")"
LB=/home/y/src/bpftime-wt-uprobe-177a1dc/build-d5279469/libbpf
gcc-13 -O2 -g -pthread -o keyloop keyloop.c
gcc-13 -O2 -g -I"$LB" -o attach attach.c "$LB/libbpf.a" -lelf -lz
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-19 -O2 -g -target bpf -D__TARGET_ARCH_x86 -Iinc -I/usr/include/x86_64-linux-gnu"
for k in 4 8; do
  for m in 10240 102400; do
    $BPF -DKEY_SIZE=$k -DMAX_ENTRIES=$m -c keymap.bpf.c -o keymap-k$k-m$m.bpf.o
  done
done
$BPF -DCONTROL=1 -c keymap.bpf.c -o keymap-control.bpf.o
