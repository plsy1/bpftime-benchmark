#!/bin/bash
# build_probe.sh: compile-only probe (no GPU): what the documented CUDA C++ constructs of
# claims 4 and 9 (../README.md) lower to with CUDA 13.2 on sm_87. About 20 s.
set -eu
cd "$(dirname "$0")"
NVCC=/usr/local/cuda-13.2/bin/nvcc
$NVCC -arch=sm_87 -std=c++17 -ptx -o probe.ptx probe.cu
$NVCC -arch=sm_87 -std=c++17 -cubin -o probe.cubin probe.cu
/usr/local/cuda-13.2/bin/cuobjdump -sass probe.cubin > probe.sass
{
  echo "# PTX (synchronization and publication instructions)"
  grep -n -E "bar.warp.sync|fence|membar|st\.(release|relaxed)|activemask|shfl|atom" probe.ptx
  echo "# SASS (per function)"
  awk '/Function : /{fn=$3} /NOP ;|MEMBAR|ERRBAR|CCTL|STRONG|WARPSYNC|SHFL|VOTE/{gsub(/\/\* 0x[0-9a-f]* \*\//,""); gsub(/  +/," "); print fn" ::"$0}' probe.sass
} > lowering.txt
cat lowering.txt
