#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-array-jit-poc-20261019
OUT="$ROOT/benchmark/uprobe/.output/array-jit-poc"
OLD=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
mkdir -p "$OUT"
/usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 \
    -I"$ROOT/third_party/vmlinux/arm64" -I"$OLD/benchmark/uprobe/.output" \
    -I"$ROOT/third_party/libbpf/include/uapi" \
    -c "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc.bpf.c" -o "$OUT/array_jit_poc.bpf.o"
"$OLD/benchmark/uprobe/.output/bpftool/bootstrap/bpftool" gen skeleton \
    "$OUT/array_jit_poc.bpf.o" >"$OUT/array_jit_poc.skel.h"
/usr/bin/gcc-13 -g -O2 -I"$OUT" -I"$OLD/benchmark/uprobe/.output" \
    "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc_loader.c" \
    "$OLD/benchmark/uprobe/.output/libbpf.a" -lelf -lz -o "$OUT/loader"
/usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic \
    "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc_victim.c" -o "$OUT/victim"
printf 'Matched BPF object, loader and victim built.\n'
