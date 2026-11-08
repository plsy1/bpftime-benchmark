#!/bin/bash
# Build the GPU example loaders and the victims in a mirror tree outside the repo.
# Loaders: example/gpu/<ex> copied from the candidate, third_party symlinked.
# Victims: nvcc 13.2 default launch (handles) and legacy launch (-D__NV_LEGACY_LAUNCH twice).
set -u
R=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
W=/home/jetson/src/bpftime-gpu-work
export PATH=/usr/local/cuda-13.2/bin:$PATH
LEG="-D__NV_LEGACY_LAUNCH -Xcompiler -D__NV_LEGACY_LAUNCH"
mkdir -p $W/example/gpu $W/benchmark/gpu $W/logs
ln -sfn $R/third_party $W/third_party
FIRST=kernelretsnoop
for ex in kernelretsnoop kernel_trace host_map_test cuda-counter cudagraph threadhist threadscheduling; do
  if [ -e $W/example/gpu/$ex ]; then echo "exists $ex (skip copy)"; else cp -a $R/example/gpu/$ex $W/example/gpu/; fi
  if [ $ex != $FIRST ] && [ -e $W/example/gpu/$FIRST/.output/libbpf.a ] && [ ! -e $W/example/gpu/$ex/.output ]; then
    mkdir -p $W/example/gpu/$ex/.output
    cp -a $W/example/gpu/$FIRST/.output/libbpf.a $W/example/gpu/$FIRST/.output/libbpf $W/example/gpu/$FIRST/.output/bpf $W/example/gpu/$FIRST/.output/bpftool $W/example/gpu/$ex/.output/
  fi
  if timeout 900 make -C $W/example/gpu/$ex CLANG=clang-15 -j3 > $W/logs/build-$ex.log 2>&1; then echo "built $ex"; else echo "FAIL $ex (logs/build-$ex.log)"; fi
done
# victims
[ -e $W/benchmark/gpu/workload ] || cp -a $R/benchmark/gpu/workload $W/benchmark/gpu/
cd $W/benchmark/gpu/workload && \
  nvcc -cudart shared -O3 -g vec_add.cu -o vec_add > $W/logs/nvcc-vec_add.log 2>&1 && \
  nvcc -cudart shared -O3 -g $LEG vec_add.cu -o vec_add_legacy > $W/logs/nvcc-vec_add_legacy.log 2>&1 && echo "built workload vec_add vec_add_legacy" || echo "FAIL workload victims"
cd $W/example/gpu/kernel_trace && nvcc -cudart shared -g $LEG vec_add.cu -o vec_add_legacy > $W/logs/nvcc-kt-legacy.log 2>&1 && echo "built kernel_trace vec_add_legacy" || echo "FAIL kernel_trace legacy"
cd $W/example/gpu/cudagraph && nvcc -cudart shared -g vec_add_graph.cu -o vec_add_graph > $W/logs/nvcc-cg.log 2>&1 && nvcc -cudart shared -g $LEG vec_add_graph.cu -o vec_add_graph_legacy > $W/logs/nvcc-cg-legacy.log 2>&1 && echo "built cudagraph victims" || echo "FAIL cudagraph victims"
echo "done $(date +%H:%M:%S)"
