#!/bin/bash
# Build check C2. CPU only, no GPU work:
#   build/kernels.cubin (+ kept PTX, SASS)    native path (nvcc -cubin, sm_87)
#   build/kernels_jit.ptx                      driver-JIT path: the C2_JIT_SUBSET kernels as PTX,
#                                              every call.uni turned into a plain call (as bpftime
#                                              inserts calls); also assembled offline for its SASS
#   build/ref/refprobe.cubin                   today's empty probe pair, built by the dev tree's
#                                              kprobe_entry and kretprobe passes (ref/build_refprobe.py)
#   build/c2chk, build/c2bench, build/hog      host programs
#   build/sass_check.{txt,json}                SASS checks (sass_check.py)
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
mkdir -p build/keep build/keepjit build/ref
NV="$CUDA/bin/nvcc -arch=sm_87 -O3 -std=c++17"
$NV -cubin -Xptxas -v --keep --keep-dir build/keep -o build/kernels.cubin src/kernels.cu > build/kernels.ptxas.txt 2>&1
cp build/keep/kernels.ptx build/kernels.ptx
"$CUDA/bin/cuobjdump" -sass build/kernels.cubin > build/kernels.sass
$NV -ptx -DC2_JIT_SUBSET -o build/kernels_jit_uni.ptx src/kernels.cu
sed 's/\bcall\.uni\b/call/g' build/kernels_jit_uni.ptx > build/kernels_jit.ptx
"$CUDA/bin/ptxas" -arch=sm_87 -O3 -o build/kernels_jit.cubin build/kernels_jit.ptx
"$CUDA/bin/cuobjdump" -sass build/kernels_jit.cubin > build/kernels_jit.sass
python3 -I ref/build_refprobe.py build/ref > build/ref/build_refprobe.log 2>&1
"$CUDA/bin/ptxas" -arch=sm_87 -v build/ref/refprobe.ptx -o build/ref/refprobe.cubin > build/ref/ptxas.txt 2>&1
"$CUDA/bin/cuobjdump" -sass build/ref/refprobe.cubin > build/ref/refprobe.sass
python3 -I sass_check.py build > build/sass_check.txt 2>&1 || true
tail -1 build/sass_check.txt
G="g++-13 -O2 -std=c++17 -Wall -I$CUDA/include"
$G -o build/c2chk src/c2chk.cpp -lcuda
$G -o build/c2bench src/c2bench.cpp -lcuda
$CUDA/bin/nvcc -O3 -arch=sm_87 -std=c++17 -o build/hog src/hog.cu
echo "built: $D/build"
