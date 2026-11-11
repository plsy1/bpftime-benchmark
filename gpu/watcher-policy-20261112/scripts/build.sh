#!/usr/bin/env bash
# Build of the exp/watcher-policy copy (46f5a7cb + watcher-sleep.patch + watcher-policy.patch), same flags as wake/build.sh.
set -u
S=/home/jetson/src/bpftime-gpu-work/policy/src
B=/home/jetson/src/bpftime-gpu-work/policy/build-cuda; L=/home/jetson/src/bpftime-gpu-work/policy/logs
date +%T
[ -f $B/CMakeCache.txt ] || timeout 600 cmake -S $S -B $B -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
  -DBPFTIME_ENABLE_LTO=OFF -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBUILD_BPFTIME_DAEMON=ON -DENABLE_EBPF_VERIFIER=OFF \
  -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG \
  -DBPFTIME_ENABLE_CUDA_ATTACH=ON -DBPFTIME_CUDA_ROOT=/usr/local/cuda-13.2 > $L/configure.log 2>&1 || { echo "configure failed"; tail -n 30 $L/configure.log; exit 1; }
nice -n 5 timeout 3600 cmake --build $B --parallel 5 --target ptxpass_kprobe_entry ptxpass_kretprobe ptxpass_kprobe_memcapture nv_attach_impl_ptx_compiler > $L/build.log 2>&1 || { echo "pass build failed"; grep -E 'error' $L/build.log | head; exit 1; }
nice -n 5 timeout 3600 cmake --build $B --parallel 5 --target bpftime-agent bpftime-syscall-server >> $L/build.log 2>&1 || { echo "build failed"; grep -E 'error' $L/build.log | head -20; exit 1; }
date +%T
ls -la $B/runtime/agent/libbpftime-agent.so $B/runtime/syscall-server/libbpftime-syscall-server.so
