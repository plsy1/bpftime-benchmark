#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-offical-no-btf
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SOURCE=third_party/bpftool/src/skeleton/profiler.bpf.c
OUTPUT=/home/jetson/src/.toolchains/bpftool-profile-arm64

cd "$ROOT"
if ! git diff --quiet -- "$SOURCE"; then
    echo "$SOURCE already has local changes" >&2
    exit 1
fi
restore() {
    if ! git diff --quiet -- "$SOURCE"; then
        patch -R -p1 <"$HERE/bpftool-profiler-no-core.patch"
    fi
}
trap restore EXIT INT TERM

patch -p1 <"$HERE/bpftool-profiler-no-core.patch"
mkdir -p "$OUTPUT"
make -C third_party/bpftool/src \
    OUTPUT="$OUTPUT/" \
    VMLINUX_H="$ROOT/third_party/vmlinux/arm64/vmlinux.h" \
    CLANG=/usr/lib/llvm-15/bin/clang \
    LLVM_CONFIG=/usr/lib/llvm-15/bin/llvm-config \
    LLVM_STRIP=/usr/lib/llvm-15/bin/llvm-strip \
    CC=/usr/bin/gcc-13 HOSTCC=/usr/bin/gcc-13 -j6

"$OUTPUT/bpftool" version
sha256sum "$OUTPUT/bpftool"
