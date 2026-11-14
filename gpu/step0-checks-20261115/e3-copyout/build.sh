#!/bin/bash
# build.sh: build every check-3 binary into build/, keep PTX and SASS, and
# check that the stores, fences, loads and calls the tests rely on survived
# compilation (check_sass.sh). No GPU work. About 1 minute.
set -eu
E3=$(cd "$(dirname "$0")" && pwd)
NVCC=/usr/local/cuda-13.2/bin/nvcc
CUOBJDUMP=/usr/local/cuda-13.2/bin/cuobjdump
B=$E3/build
mkdir -p "$B/keepA" "$B/keepB" "$B/keepC"
cd "$E3"
echo "[build] partA copyorder"
$NVCC -O2 -std=c++17 -arch=sm_87 -keep -keep-dir "$B/keepA" -o "$B/copyorder" partA/copyorder.cu -lcuda
echo "[build] partB captest (shared cudart, so the shim's interposers see its calls)"
$NVCC -O2 -arch=sm_87 -cudart shared -keep -keep-dir "$B/keepB" -o "$B/captest" partB/captest.cu
echo "[build] partB libstep0_copier.so"
gcc -O2 -Wall -Wextra -shared -fPIC -I/usr/local/cuda-13.2/include \
  -o "$B/libstep0_copier.so" partB/step0_copier.c -lcuda -ldl -lpthread
echo "[build] partC vmmexport"
$NVCC -O2 -std=c++17 -arch=sm_87 -keep -keep-dir "$B/keepC" -o "$B/vmmexport" partC/vmmexport.cu -lcuda
$CUOBJDUMP -sass "$B/copyorder" > "$B/keepA/copyorder.sass"
$CUOBJDUMP -sass "$B/captest" > "$B/keepB/captest.sass"
$CUOBJDUMP -sass "$B/vmmexport" > "$B/keepC/vmmexport.sass"
bash "$E3/check_sass.sh"
