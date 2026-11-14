#!/bin/bash
# Build pub4 (PTX and SASS kept) and check the fences and stores per variant.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
cd "$D"
mkdir -p build/keep
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -std=c++17 -Xptxas -v --keep --keep-dir build/keep \
	-o build/pub4 src/pub4.cu -lcuda > build/ptxas.txt 2>&1
cp build/keep/pub4.ptx build/pub4.ptx
"$CUDA/bin/cuobjdump" -sass build/pub4 > build/pub4.sass
python3 -I sass_check.py build/pub4.sass build/sass_check.json build/pub4.ptx > build/sass_check.txt
tail -1 build/sass_check.txt
echo "built: $D/build/pub4"
