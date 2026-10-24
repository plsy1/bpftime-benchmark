#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
OUT="$ROOT/benchmark/uprobe/.output/percpu-array-jit"
mkdir -p "$OUT"
/usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 \
    -I"$ROOT/third_party/vmlinux/arm64" -I"$ROOT/benchmark/uprobe/.output" \
    -I"$ROOT/third_party/libbpf/include/uapi" \
    -c "$ROOT/benchmark/uprobe/diagnostics/percpu_array_jit.bpf.c" -o "$OUT/percpu_array_jit.bpf.o"
"$ROOT/benchmark/uprobe/.output/bpftool/bootstrap/bpftool" gen skeleton \
    "$OUT/percpu_array_jit.bpf.o" >"$OUT/percpu_array_jit.skel.h"
/usr/bin/gcc-13 -g -O2 -I"$OUT" -I"$ROOT/benchmark/uprobe/.output" \
    "$ROOT/benchmark/uprobe/diagnostics/percpu_array_jit_loader.c" \
    "$ROOT/benchmark/uprobe/.output/libbpf.a" -lelf -lz -o "$OUT/loader"
/usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic \
    "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc_victim.c" -o "$OUT/victim"
printf 'Matched per-CPU BPF object, loader and victim built.\n'
