#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
EXPECTED_COMMIT=176eb291ead95eb5f8a56280deae626fac46eaa9

[[ $(git -C "$ROOT" rev-parse HEAD) == "$EXPECTED_COMMIT" ]]
make -C "$ROOT/benchmark/uprobe" \
    CLANG=/usr/lib/llvm-15/bin/clang \
    CC=/usr/bin/gcc-13 \
    kernel-map-runtime-diagnostic

sha256sum \
    "$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime" \
    "$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim" \
    "$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel_map_runtime.bpf.o"
