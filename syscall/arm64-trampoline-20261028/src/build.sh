#!/bin/bash
# Builds tp_attach (bpftime's libbpf, static), writeloop and tp_count.bpf.o.
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -o writeloop writeloop.c
gcc-13 -O2 -g -I"$LB" -o tp_attach tp_attach.c "$LB/libbpf.a" -lelf -lz
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu -c tp_count.bpf.c -o tp_count.bpf.o
