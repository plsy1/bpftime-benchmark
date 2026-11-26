#!/bin/bash
# Build C4: four binaries from src/c4.cu (FAULT 0..3), with kept PTX, ptxas -v and SASS.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
names=(proto plainclaim nobound relaxpub plainnobound)
for f in 0 1 2 3 4; do
  n=${names[$f]}
  mkdir -p build/keep-$n
  "$CUDA/bin/nvcc" -arch=sm_87 -O3 -lineinfo -std=c++17 -DFAULT=$f -Xptxas -v \
    --keep --keep-dir build/keep-$n -o build/c4_$n src/c4.cu > build/ptxas-$n.txt 2>&1
  cp build/keep-$n/c4.ptx build/c4_$n.ptx
  "$CUDA/bin/cuobjdump" -sass build/c4_$n > build/c4_$n.sass
  echo "built build/c4_$n"
done
# protocol with the wait bound at 0: exercises the unstable path and "EEXIST then NULL"
n=proto_nowait
mkdir -p build/keep-$n
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -lineinfo -std=c++17 -DFAULT=0 -DWAIT_BOUND_NS=0u -Xptxas -v \
  --keep --keep-dir build/keep-$n -o build/c4_$n src/c4.cu > build/ptxas-$n.txt 2>&1
cp build/keep-$n/c4.ptx build/c4_$n.ptx
"$CUDA/bin/cuobjdump" -sass build/c4_$n > build/c4_$n.sass
echo "built build/c4_$n"
