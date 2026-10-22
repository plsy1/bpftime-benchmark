#!/usr/bin/env bash
# Builds the hash shape workloads from source/ with the toolchain outputs of
# the bdd9835 formal worktree (same as hash-lookup-helper-gap-20261021).
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/hash-shape-20261022
TOOLS=/home/jetson/src/bpftime-global-data-bdd9835-20261021
O="$TOOLS/benchmark/uprobe/.output"
build() { # name fill churn
    local v=$1 fill=$2 churn=$3
    local D="$OUT/workloads/$v"
    mkdir -p "$D"
    /usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 -DHASH_SHAPE_FILL=$fill \
        "-DHASH_SHAPE_PATH=\"$D/victim\"" \
        -I"$TOOLS/third_party/vmlinux/arm64" -I"$O" -I"$TOOLS/third_party/libbpf/include/uapi" \
        -c "$OUT/source/hash_shape.bpf.c" -o "$D/hash_shape.bpf.o"
    "$O/bpftool/bootstrap/bpftool" gen skeleton "$D/hash_shape.bpf.o" name hash_shape_bpf >"$D/hash_shape.skel.h"
    /usr/bin/gcc-13 -g -O2 -DHASH_SHAPE_FILL=$fill -DHASH_SHAPE_CHURN=$churn -I"$D" -I"$O" \
        "$OUT/source/hash_shape_loader.c" "$O/libbpf.a" -lelf -lz -o "$D/loader"
    /usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic "$OUT/source/hash_shape_victim.c" -o "$D/victim"
}
build full1024 1024 0
build official1000 1000 0
build half512 512 0
build churn512 512 64
sha256sum "$OUT"/workloads/*/{loader,victim,hash_shape.bpf.o} >"$OUT/workload-sha256.txt"
