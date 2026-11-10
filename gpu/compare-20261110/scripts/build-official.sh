#!/usr/bin/env bash
# Official 4708f409 with CUDA attach, in the daily's official worktree (new build-cuda), with the
# settings of the dev candidate's build-cuda. Official's cmake/cuda.cmake looks for
# targets/aarch64-linux, which CUDA 13.2 does not install: a root of symlinks provides it.
set -u
T=/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e
O=$T/official; OB=$O/build-cuda
R=/home/jetson/src/results/gpu/compare-20261110/build
mkdir -p $OB/cuda-root/targets
ln -sfn /usr/local/cuda-13.2/targets/sbsa-linux $OB/cuda-root/targets/aarch64-linux
for x in bin include lib64 nvvm extras version.json; do ln -sfn /usr/local/cuda-13.2/$x $OB/cuda-root/$x; done
date +%T
timeout 600 cmake -S $O -B $OB -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
  -DBPFTIME_ENABLE_LTO=OFF -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBUILD_BPFTIME_DAEMON=ON -DENABLE_EBPF_VERIFIER=OFF \
  -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG \
  -DBPFTIME_ENABLE_CUDA_ATTACH=ON -DBPFTIME_CUDA_ROOT=$OB/cuda-root > $R/official-configure.log 2>&1 || { echo configure failed; tail -n 30 $R/official-configure.log; exit 1; }
grep -E 'Detected CUDA version|NVPTXCOMPILER|cupti' $R/official-configure.log | head -5
nice -n 5 timeout 3600 cmake --build $OB --parallel 2 --target ptxpass_kprobe_entry ptxpass_kretprobe ptxpass_kprobe_memcapture nv_attach_impl_ptx_compiler > $R/official-build.log 2>&1 || { echo "pass build failed"; grep -E 'error' $R/official-build.log | head; exit 1; }
nice -n 5 timeout 3600 cmake --build $OB --parallel 2 --target bpftime-agent bpftime-syscall-server >> $R/official-build.log 2>&1 || { echo "agent/server build failed"; grep -E 'error' $R/official-build.log | head; exit 1; }
date +%T
ls -la $OB/runtime/agent/libbpftime-agent.so $OB/runtime/syscall-server/libbpftime-syscall-server.so
