#!/bin/bash
# Build e2-cost: kernels (cubin, PTX and SASS kept), the 1512 reference module
# (dev build's kretprobe pass + trampoline, see ref/build_ref.py), the SASS
# check, and the host program. CPU only; no GPU work.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
mkdir -p build/keep build/ref
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -cubin -Xptxas -v --keep --keep-dir build/keep \
	-o build/kernels.cubin src/kernels.cu > build/kernels.ptxas.txt 2>&1
cp build/keep/kernels.ptx build/kernels.ptx
"$CUDA/bin/cuobjdump" -sass build/kernels.cubin > build/kernels.sass
python3 -I ref/build_ref.py build/ref > build/ref/build_ref.log 2>&1
"$CUDA/bin/ptxas" -arch=sm_87 -v build/ref/ref1512.ptx -o build/ref/ref1512.cubin > build/ref/ptxas.txt 2>&1
"$CUDA/bin/cuobjdump" -sass build/ref/ref1512.cubin > build/ref/ref1512.sass
python3 -I sass_check.py build/kernels.sass build/ref/ref1512.sass build/sass_check.json > build/sass_check.txt
tail -1 build/sass_check.txt
g++-13 -O2 -std=c++17 -Wall -I"$CUDA/include" -o build/bench2 src/bench2.cpp -lcuda
echo "built: $D/build/bench2"
