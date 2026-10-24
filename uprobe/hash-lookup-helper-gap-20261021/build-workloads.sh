#!/usr/bin/env bash
# Hash consuming variants from committed sources (f24dc9bf93f9); toolchain outputs
# (libbpf.a, bpftool, headers) from the formal bdd9835 build worktree.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/hash-lookup-helper-gap-20261021
SRC_COMMIT=f24dc9bf93f960df0b7cf25b232c41ae1fc84b5d
REPO=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
TOOLS=/home/jetson/src/bpftime-global-data-bdd9835-20261021
O="$TOOLS/benchmark/uprobe/.output"
mkdir -p "$OUT/source" "$OUT/workloads"
for f in array_jit_poc.bpf.c array_jit_poc_loader.c array_jit_poc_victim.c; do
    git -C "$REPO" show "$SRC_COMMIT:benchmark/uprobe/diagnostics/$f" >"$OUT/source/$f"
done
git -C "$REPO" rev-parse "$SRC_COMMIT" >"$OUT/source/commit.txt"
build() { # variant extra-clang-flags...
    local v=$1; shift
    local D="$OUT/workloads/$v"
    mkdir -p "$D"
    /usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 "$@" \
        "-DBPFTIME_ARRAY_JIT_PATH=\"uprobe/$D/victim:\"" \
        -I"$TOOLS/third_party/vmlinux/arm64" -I"$O" -I"$TOOLS/third_party/libbpf/include/uapi" \
        -c "$OUT/source/array_jit_poc.bpf.c" -o "$D/array_jit_poc.bpf.o"
    "$O/bpftool/bootstrap/bpftool" gen skeleton "$D/array_jit_poc.bpf.o" name array_jit_poc_bpf >"$D/array_jit_poc.skel.h"
    /usr/bin/gcc-13 -g -O2 -I"$D" -I"$O" "$OUT/source/array_jit_poc_loader.c" "$O/libbpf.a" -lelf -lz -o "$D/loader"
    /usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic "$OUT/source/array_jit_poc_victim.c" -o "$D/victim"
}
build v1-hash-only -DBPFTIME_ARRAY_JIT_MAP_TYPE=BPF_MAP_TYPE_HASH
build v2-hash-globals -DBPFTIME_ARRAY_JIT_MAP_TYPE=BPF_MAP_TYPE_HASH -DBPFTIME_ARRAY_JIT_GLOBALS=1
build v3-hash-mixed -DBPFTIME_ARRAY_JIT_MAP_TYPE=BPF_MAP_TYPE_HASH -DBPFTIME_ARRAY_JIT_RESULT_MAP_TYPE=BPF_MAP_TYPE_ARRAY
sha256sum "$OUT"/workloads/*/{loader,victim,array_jit_poc.bpf.o} >"$OUT/workload-sha256.txt"
