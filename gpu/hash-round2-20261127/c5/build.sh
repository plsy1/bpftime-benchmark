#!/bin/bash
# Build C5 (round two): protocol v2 and the controls from src/c5.cu, with kept PTX, ptxas -v and SASS.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
b() { # name fault extra...
  local n=$1 f=$2; shift 2
  mkdir -p build/keep-$n
  "$CUDA/bin/nvcc" -arch=sm_87 -O3 -lineinfo -std=c++17 -DFAULT=$f "$@" -Xptxas -v \
    --keep --keep-dir build/keep-$n -o build/c5_$n src/c5.cu > build/ptxas-$n.txt 2>&1
  cp build/keep-$n/c5.ptx build/c5_$n.ptx
  "$CUDA/bin/cuobjdump" -sass build/c5_$n > build/c5_$n.sass
  echo "built build/c5_$n"
}
b proto 0
b ctl1-plainclaim 1
b ctl2-nocheck 2
b ctl5-noreread 5
b ctl6-roundone 6
b proto_nowait 0 -DWAIT_BOUND_NS=0u
