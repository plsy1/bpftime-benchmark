#!/bin/bash
# Build C6: one source, one binary per variant (A B C D: rules table; S SD: harness checks),
# with kept PTX, ptxas -v and SASS of each.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
mkdir -p build
for v in A B C D S SD; do
  mkdir -p build/keep-$v
  "$CUDA/bin/nvcc" -arch=sm_87 -O3 -lineinfo -std=c++17 -DVARIANT=$v -Xptxas -v \
    --keep --keep-dir build/keep-$v -o build/c6_$v src/c6.cu > build/ptxas-$v.txt 2>&1
  cp build/keep-$v/c6.ptx build/c6_$v.ptx
  "$CUDA/bin/cuobjdump" -sass build/c6_$v > build/c6_$v.sass
  echo "built build/c6_$v"
done
