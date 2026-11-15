#!/bin/bash
# build.sh: build captest (shared cudart, so the shim's interposers see its calls) and the
# copier shim into build/. No GPU work. About 20 s.
set -eu
D=$(cd "$(dirname "$0")" && pwd)
NVCC=/usr/local/cuda-13.2/bin/nvcc
mkdir -p "$D/build"
cd "$D"
$NVCC -O2 -arch=sm_87 -cudart shared -o build/captest src/captest.cu
gcc -O2 -Wall -Wextra -shared -fPIC -I/usr/local/cuda-13.2/include \
  -o build/libstep0b_copier.so src/step0b_copier.c -lcuda -ldl -lpthread
sha256sum src/captest.cu src/step0b_copier.c common/regmem.h build/captest build/libstep0b_copier.so > build/sha256.txt
echo "built: build/captest build/libstep0b_copier.so"
