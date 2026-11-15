#!/bin/bash
# Build C3 (CPU only, no GPU work): kernels (cubin, PTX and SASS kept), the
# three reference modules (ref/build_variants.py: the dev build's kretprobe
# pass + trampoline, as step 0's e2-cost), the SASS checks, the host program,
# and the diffs against step 0's originals.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
E2=/home/jetson/src/bpftime-gpu-work/step0/e2-cost
cd "$D"
mkdir -p build/keep build/ref diffs
"$CUDA/bin/nvcc" -arch=sm_87 -O3 -cubin -Xptxas -v --keep --keep-dir build/keep \
	-o build/kernels.cubin src/kernels.cu > build/kernels.ptxas.txt 2>&1
cp build/keep/kernels.ptx build/kernels.ptx
"$CUDA/bin/cuobjdump" -sass build/kernels.cubin > build/kernels.sass
python3 -I ref/build_variants.py build/ref > build/ref/build_variants.log 2>&1
tail -3 build/ref/build_variants.log
for m in refF refN refX; do
	"$CUDA/bin/ptxas" -arch=sm_87 -v build/ref/$m.ptx -o build/ref/$m.cubin > build/ref/$m.ptxas.txt 2>&1
	"$CUDA/bin/cuobjdump" -sass build/ref/$m.cubin > build/ref/$m.sass
done
python3 -I sass_check.py build/kernels.sass build/ref build/sass_check.json > build/sass_check.txt
tail -1 build/sass_check.txt
g++-13 -O2 -std=c++17 -Wall -I"$CUDA/include" -o build/bench3 src/bench3.cpp -lcuda
# what changed against step 0 (record only)
diff -u "$E2/src/kernels.cu" src/kernels.cu > diffs/kernels.cu.diff || true
diff -u "$E2/src/bench2.cpp" src/bench3.cpp > diffs/bench2_to_bench3.diff || true
diff -u "$E2/ref/build_ref.py" ref/build_variants.py > diffs/build_ref_to_build_variants.diff || true
diff -u "$E2/sass_check.py" sass_check.py > diffs/sass_check.py.diff || true
diff -u /home/jetson/src/bpftime-gpu-work/step0/common/step0_regmem.h src/step0b_regmem.h > diffs/step0b_regmem.h.diff || true
echo "built: $D/build/bench3"
