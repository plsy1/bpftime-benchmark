#!/bin/bash
# Build C4: cost kernels (cubin, PTX and SASS kept), bfc (cost host), pubc
# (correctness harness), and the PTX/SASS check. CPU only; no GPU work.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
mkdir -p build/keep build/pkeep
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -cubin -Xptxas -v --keep --keep-dir build/keep \
	-o build/kernels.cubin src/kernels.cu > build/kernels.ptxas.txt 2>&1
"$CUDA/bin/cuobjdump" -sass build/kernels.cubin > build/kernels.sass
g++-13 -O2 -std=c++17 -Wall -I"$CUDA/include" -o build/bfc src/bfc.cpp -lcuda
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -std=c++17 -Xptxas -v --keep --keep-dir build/pkeep \
	-o build/pubc src/pubc.cu -lcuda > build/pubc.ptxas.txt 2>&1
"$CUDA/bin/cuobjdump" -sass build/pubc > build/pubc.sass
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -cubin -o build/lowering_probe.cubin src/lowering_probe.cu
"$CUDA/bin/cuobjdump" -sass build/lowering_probe.cubin > build/lowering_probe.full.sass
grep -E 'Function|MEMBAR|ATOMG|RED|STG|LDG|CCTL|ERRBAR|FENCE|PROXY' build/lowering_probe.full.sass > build/lowering_probe.sass
python3 -I sass_check.py build/kernels.sass build/keep/kernels.ptx \
	build/pubc.sass build/pkeep/pubc.ptx build/sass_check.json > build/sass_check.txt || true
tail -1 build/sass_check.txt
grep -q 'ALL OK' build/sass_check.txt
echo "built: $D/build/bfc $D/build/pubc"
