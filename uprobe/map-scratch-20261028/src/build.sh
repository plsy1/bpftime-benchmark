#!/bin/bash
# Builds the generic loader `attach` (bpftime's libbpf 1.7.0, static), scratchloop and
# scratch(-control).bpf.o (large per-CPU scratch value).
set -eu
cd "$(dirname "$0")"
LB=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily/libbpf
gcc-13 -O2 -g -pthread -o scratchloop scratchloop.c
gcc-13 -O2 -g -I"$LB" -o attach attach.c "$LB/libbpf.a" -lelf -lz
# Only libbpf's bpf/ headers: its build dir also has a linux/ that shadows the uapi types.
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -Iinc -I/usr/include/aarch64-linux-gnu"
$BPF -c scratch.bpf.c -o scratch.bpf.o
$BPF -DCONTROL=1 -c scratch.bpf.c -o scratch-control.bpf.o
