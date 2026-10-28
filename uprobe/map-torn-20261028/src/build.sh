#!/bin/bash
# Builds the generic loader `attach` (bpftime's libbpf 1.7.0, static), tornloop and
# torn(-noprealloc).bpf.o (torn reads through a lookup pointer).
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -pthread -o tornloop tornloop.c
gcc-13 -O2 -g -I"$LB" -o attach attach.c "$LB/libbpf.a" -lelf -lz
# Only libbpf's bpf/ headers: its build dir also has a linux/ that shadows the uapi types.
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu"
$BPF -c torn.bpf.c -o torn.bpf.o
$BPF -DNO_PREALLOC=1 -c torn.bpf.c -o torn-noprealloc.bpf.o
