#!/usr/bin/env bash
# Builds for the xlarge experiments, same flags as ablate-20261110 (dev copies) and
# compare-20261110 (official, CUDA root of symlinks): build_x.sh fgt|oreg
#   fgt  = dev c115b33d + frame.diff + gate.diff + tramp.diff (official's device code on dev's runtime)
#   oreg = official 4708f409 + oreg.diff (dev's cuInit/primary-context/cuMemHostRegister registration)
set -u
K=$1; X=/home/jetson/src/bpftime-gpu-work/xlarge; S=$X/$K; B=$S/build-cuda; L=$X/logs; mkdir -p $L
CR=/usr/local/cuda-13.2
if [ $K = oreg ]; then
  mkdir -p $B/cuda-root/targets
  ln -sfn /usr/local/cuda-13.2/targets/sbsa-linux $B/cuda-root/targets/aarch64-linux
  for x in bin include lib64 nvvm extras version.json; do ln -sfn /usr/local/cuda-13.2/$x $B/cuda-root/$x; done
  CR=$B/cuda-root
fi
date +%T
timeout 600 cmake -S $S -B $B -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
  -DBPFTIME_ENABLE_LTO=OFF -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBUILD_BPFTIME_DAEMON=ON -DENABLE_EBPF_VERIFIER=OFF \
  -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG \
  -DBPFTIME_ENABLE_CUDA_ATTACH=ON -DBPFTIME_CUDA_ROOT=$CR > $L/$K-configure.log 2>&1 || { echo "$K configure failed"; tail -n 30 $L/$K-configure.log; exit 1; }
nice -n 5 timeout 3600 cmake --build $B --parallel 4 --target ptxpass_kprobe_entry ptxpass_kretprobe ptxpass_kprobe_memcapture nv_attach_impl_ptx_compiler > $L/$K-build.log 2>&1 || { echo "$K pass build failed"; grep -E 'error' $L/$K-build.log | head; exit 1; }
nice -n 5 timeout 3600 cmake --build $B --parallel 4 --target bpftime-agent bpftime-syscall-server >> $L/$K-build.log 2>&1 || { echo "$K build failed"; grep -E 'error' $L/$K-build.log | head; exit 1; }
date +%T
ls -la $B/runtime/agent/libbpftime-agent.so $B/runtime/syscall-server/libbpftime-syscall-server.so
