#!/bin/bash
set -eu
L=$HOME/src/bpftime-wt-uprobe-177a1dc/build-d5279469/libbpf
clang-19 -O2 -g -target bpf -I /usr/include/x86_64-linux-gnu -idirafter $L -c r1.bpf.c -o r1.bpf.o
gcc -O2 -Wall -I $L r1.c -o r1 $L/libbpf.a -lelf -lz -lpthread
