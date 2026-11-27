#!/usr/bin/env bash
# Build E5-real: BPF objects, loader (static libbpf from the official tree's sslsniff build output, read only), the
# Java workload. Run under the measurement lock: flock -n <R>/.measure.lock ./build.sh
set -euo pipefail
cd "$(dirname "$0")"
LIBBPF_OUT=/home/y/src/bpftime-paper/official-4708f409/example/tracing/sslsniff-paper-s4/.output
VMLINUX_DIR=/home/y/src/bpftime-paper/official-4708f409/third_party/vmlinux/x86
CLANG=${CLANG:-clang-19}
for o in e5real_k e5real_u; do
	$CLANG -g -O2 -target bpf -D__TARGET_ARCH_x86 -I"$LIBBPF_OUT" -I"$VMLINUX_DIR" -c $o.bpf.c -o $o.bpf.o
done
gcc -O2 -g -Wall -Wextra -I"$LIBBPF_OUT" e5rtool.c "$LIBBPF_OUT/libbpf.a" -lelf -lz -o e5rtool
javac -d ../java ../java/E5Threads.java
sha256sum e5real_k.bpf.o e5real_u.bpf.o e5rtool ../java/E5Threads.class >sha256.txt
{ $CLANG --version | head -1; gcc --version | head -1; javac -version 2>&1; } >toolchain.txt
cat sha256.txt
