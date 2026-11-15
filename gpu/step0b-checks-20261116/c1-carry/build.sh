#!/bin/bash
# build.sh: everything of check C1 that runs on the CPU (no GPU work):
#   build/bpf/        eBPF objects (build_bpf.sh)
#   build/c1_helpers.ptx  the lookup wrapper (nvcc -rdc -ptx)
#   build/b/          part (b): kernels -> dev passes -> routing -> module,
#                     ptxas -v, SASS, SASS checks
#   build/c/          part (c): the cost module (tools/build_c.py), same steps
#   build/d/          part (d): llama.cpp rms_norm module variants, ptxas -v
#   build/c1check, build/c1bench, build/occ, build/hog, build/gate.cubin
# Expected: about 1-2 minutes (ptxas on the llama.cpp module dominates).
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
CUDA=/usr/local/cuda-13.2
NVCC=$CUDA/bin/nvcc
cd "$D"
mkdir -p build/b build/c build/d
./build_bpf.sh build/bpf
tools/forms_check.sh build/forms.txt
"$NVCC" -rdc=true -ptx -arch=sm_87 -O3 -o build/c1_helpers.ptx src/c1_helpers.cu
# parts (c) and (d): the same wrapper without the unsound frame type
"$NVCC" -rdc=true -ptx -arch=sm_87 -O3 -DC1_NO_FRAME -o build/c1_helpers_prod.ptx src/c1_helpers.cu
# (b)
"$NVCC" -O3 -ptx -arch=sm_87 -o build/b/kernels_b.ptx src/kernels_b.cu
python3 -I tools/build_b.py build/bpf build/b/kernels_b.ptx build/c1_helpers.ptx build/b > build/b/build_b.log 2>&1
"$CUDA/bin/ptxas" -arch=sm_87 -v build/b/b_module.ptx -o build/b/b_module.cubin > build/b/ptxas.txt 2>&1
"$CUDA/bin/cuobjdump" -sass build/b/b_module.cubin > build/b/b_module.sass
python3 -I tools/check_sass.py b build/b > build/b/sass_check.txt
tail -1 build/b/sass_check.txt
if [ -f tools/build_c.py ]; then
	"$NVCC" -cubin -arch=sm_87 -O3 -o build/gate.cubin src/gate.cu
	python3 -I tools/build_c.py build/bpf build/c1_helpers_prod.ptx build/c > build/c/build_c.log 2>&1
	"$CUDA/bin/ptxas" -arch=sm_87 -v build/c/c_module.ptx -o build/c/c_module.cubin > build/c/ptxas.txt 2>&1
	"$CUDA/bin/cuobjdump" -sass build/c/c_module.cubin > build/c/c_module.sass
	python3 -I tools/check_sass.py c build/c > build/c/sass_check.txt
	tail -1 build/c/sass_check.txt
	g++-13 -O2 -std=c++17 -Wall -I"$CUDA/include" -o build/c1bench src/c1bench.cpp -lcuda
fi
if [ -f tools/build_d.py ]; then
	python3 -I tools/build_d.py build/bpf build/c1_helpers_prod.ptx build/d > build/d/build_d.log 2>&1
	tail -3 build/d/build_d.log
	g++-13 -O2 -std=c++17 -Wall -I"$CUDA/include" -o build/occ src/occ.cpp -lcuda
fi
g++-13 -O2 -std=c++17 -Wall -I"$CUDA/include" -o build/c1check src/c1check.cpp -lcuda
"$NVCC" -O3 -arch=sm_87 -std=c++17 -o build/hog src/hog.cu
echo "build ok: $D/build"
