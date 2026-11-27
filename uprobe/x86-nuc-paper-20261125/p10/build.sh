#!/usr/bin/env bash
# Build P10 (BPF object, loader with the official tree's static libbpf, victim). Run under the measurement lock.
set -euo pipefail
cd "$(dirname "$0")"
LIBBPF_OUT=/home/y/src/bpftime-paper/official-4708f409/example/tracing/sslsniff-paper-s4/.output
VMLINUX_DIR=/home/y/src/bpftime-paper/official-4708f409/third_party/vmlinux/x86
CLANG=${CLANG:-clang-19}
$CLANG -g -O2 -target bpf -D__TARGET_ARCH_x86 -I"$LIBBPF_OUT" -I"$VMLINUX_DIR" -c p10.bpf.c -o p10.bpf.o
gcc -O2 -g -Wall -Wextra -I"$LIBBPF_OUT" p10tool.c "$LIBBPF_OUT/libbpf.a" -lelf -lz -o p10tool
gcc -O2 -g -Wall -Wextra p10victim.c -o p10victim
sha256sum p10.bpf.o p10tool p10victim p10run.py >sha256.txt
cat sha256.txt
