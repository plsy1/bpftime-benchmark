#!/bin/bash
# x86 check of dev f0954773 (25f4134f): build and run the Frida attach tests only.
set -u
D=~/src/jetson-check-25f4134f
exec > >(tee -a ~/src/jetson-check-25f4134f.log) 2>&1
date; uname -r
git clone -q --reference ~/src/bpftime-benchmark --dissociate -b dev/uprobe-official-177a1dc465ff https://github.com/plsy1/bpftime-benchmark $D || exit 1
cd $D && git log --oneline -1 && git submodule update --init --recursive --jobs 8 -q || exit 1
cmake -S $D -B $D/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -DBPFTIME_BPF_CLANG=/usr/bin/clang-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DBUILD_BPFTIME_DAEMON=ON > $D/configure.log 2>&1 || { tail -20 $D/configure.log; exit 1; }
cmake --build $D/build --parallel 8 --target bpftime_frida_uprobe_attach_tests > $D/build.log 2>&1 || { grep -E "error" $D/build.log | head -20; exit 1; }
T=$D/build/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests
for i in 1 2 3; do timeout 600 $T > $D/suite.$i.txt 2>&1; echo "suite $i: $(grep -E '^All tests|test cases' $D/suite.$i.txt | tail -1)"; done
for n in "Detaching probes while threads call the function keeps them running" "No probe callback runs after its detach returned" "Probes attached again after a detach run"; do r=""; for i in 1 2 3 4 5; do timeout 120 $T "$n" > /dev/null 2>&1 && r="$r ok" || r="$r FAIL"; done; echo "$n:$r"; done
timeout 120 $T "Hook cost attached and parked" 2>&1 | grep -E "calls/s|parked="
date
