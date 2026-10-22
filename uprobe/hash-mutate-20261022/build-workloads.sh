#!/usr/bin/env bash
# Builds the hash mutate workloads from source/ with the toolchain outputs of
# the bdd9835 formal worktree (same as hash-lookup-helper-gap-20261021).
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/hash-mutate-20261022
TOOLS=/home/jetson/src/bpftime-global-data-bdd9835-20261021
O="$TOOLS/benchmark/uprobe/.output"
build() { # name extra-clang-flags...
    local v=$1; shift
    local D="$OUT/workloads/$v"
    mkdir -p "$D"
    /usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 "$@" \
        "-DHASH_MUTATE_PATH=\"$D/victim\"" \
        -I"$TOOLS/third_party/vmlinux/arm64" -I"$O" -I"$TOOLS/third_party/libbpf/include/uapi" \
        -c "$OUT/source/hash_mutate.bpf.c" -o "$D/hash_mutate.bpf.o"
    "$O/bpftool/bootstrap/bpftool" gen skeleton "$D/hash_mutate.bpf.o" name hash_mutate_bpf >"$D/hash_mutate.skel.h"
    /usr/bin/gcc-13 -g -O2 -I"$D" -I"$O" "$OUT/source/hash_mutate_loader.c" "$O/libbpf.a" -lelf -lz -o "$D/loader"
    /usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic "$OUT/source/hash_mutate_victim.c" -o "$D/victim"
}
build hash-only
build hash-globals -DHASH_MUTATE_GLOBALS=1
build hash-array -DHASH_MUTATE_RESULT_MAP_TYPE=BPF_MAP_TYPE_ARRAY
sha256sum "$OUT"/workloads/*/{loader,victim,hash_mutate.bpf.o} >"$OUT/workload-sha256.txt"
