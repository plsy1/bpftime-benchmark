#!/bin/bash
# Builds readstr and pr-<kind>-<size>.bpf.o (the loader is output-dims-20261106/src/outattach).
set -eu
cd "$(dirname "$0")"
INC=/home/jetson/src/results/uprobe/map-table-20261028/src/inc
gcc-13 -O2 -g -o readstr readstr.c
BPF="clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -I$INC -I/usr/include/aarch64-linux-gnu"
$BPF -DKIND=0 -DSIZE=8 -c pr.bpf.c -o pr-control.bpf.o
for size in 256 4096; do
  $BPF -DKIND=1 -DSIZE=$size -c pr.bpf.c -o pr-str-$size.bpf.o
done
for size in 64 256 4096; do
  $BPF -DKIND=2 -DSIZE=$size -c pr.bpf.c -o pr-read-$size.bpf.o
done
ls -1 *.bpf.o
