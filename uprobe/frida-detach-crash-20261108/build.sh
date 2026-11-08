#!/bin/bash
# Configure (once) and build only the Frida attach test binary in the
# frida-detach-crash worktree, with the options of the probecheck build.
set -eu
W=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/frida-detach-crash
B=${1:-$W/build-fdc}
if [ ! -f "$B/CMakeCache.txt" ]; then
  cmake -S "$W" -B "$B" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
    -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_BUILD_WITH_LIBBPF=ON -DBUILD_BPFTIME_DAEMON=ON \
    -DBPFTIME_BUILD_KERNEL_BPF=ON -DBPFTIME_VERBOSE_OUTPUT=ON \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm
fi
cmake --build "$B" --target bpftime_frida_uprobe_attach_tests -j6
