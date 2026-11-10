#!/usr/bin/env bash
# Build one ablation copy of dev c115b33d with the candidate build-cuda's settings:
# build.sh frame|gate|tramp
set -u
K=$1; A=/home/jetson/src/bpftime-gpu-work/ablate; S=$A/$K; B=$S/build-cuda; L=$A/logs; mkdir -p $L
date +%T
timeout 600 cmake -S $S -B $B -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
  -DBPFTIME_ENABLE_LTO=OFF -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBUILD_BPFTIME_DAEMON=ON -DENABLE_EBPF_VERIFIER=OFF \
  -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG \
  -DBPFTIME_ENABLE_CUDA_ATTACH=ON -DBPFTIME_CUDA_ROOT=/usr/local/cuda-13.2 > $L/$K-configure.log 2>&1 || { echo "$K configure failed"; tail -n 30 $L/$K-configure.log; exit 1; }
nice -n 5 timeout 5400 cmake --build $B --parallel 2 --target ptxpass_kprobe_entry ptxpass_kretprobe ptxpass_kprobe_memcapture nv_attach_impl_ptx_compiler bpftime-agent bpftime-syscall-server > $L/$K-build.log 2>&1 || { echo "$K build failed"; grep -E 'error' $L/$K-build.log | head; exit 1; }
date +%T
ls -la $B/runtime/agent/libbpftime-agent.so $B/runtime/syscall-server/libbpftime-syscall-server.so
