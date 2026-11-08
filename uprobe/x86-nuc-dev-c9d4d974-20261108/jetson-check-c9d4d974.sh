#!/bin/bash
# x86 check of dev c9d4d974 (AOT cache: 33bba8fe, a90d8d0b, 9898ed19, c9d4d974 on 8a0afd36).
# The new commits come as a git bundle (not pushed). Build with the daily's settings (probe
# checks off, LLVM 19, gcc-13), then:
#   - the whole runtime suite in one process, console reporter (junit aborts on the threaded
#     Catch2 assertions, 11-08 section 10), failures listed;
#   - [aot-cache] 3 times (the new tests: another process image, fork child, keys, symbols);
#   - the array JIT and JIT load groups once; the Frida attach suite 3 times;
#   - remove the /dev/shm segments these runs created (non-root runs only).
set -u
D=~/src/jetson-check-c9d4d974
exec > >(tee -a ~/src/jetson-check-c9d4d974.log) 2>&1
date; uname -r; nproc
git clone -q --reference ~/src/bpftime-benchmark --dissociate -b dev/uprobe-official-177a1dc465ff https://github.com/plsy1/bpftime-benchmark $D || exit 1
cd $D && git checkout -q --detach 8a0afd3605f433a1ca3e9eeeb5e0866f6cd22446 || exit 1
git fetch -q ~/src/jetson-check-c9d4d974.bundle HEAD || exit 1
git checkout -q --detach c9d4d974bcf954bd7b309b46a7592c1b369b31d1 && git log --oneline -5 || exit 1
git submodule update --init --recursive --jobs 8 -q || exit 1
cmake -S $D -B $D/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -DBPFTIME_BPF_CLANG=/usr/bin/clang-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DBUILD_BPFTIME_DAEMON=ON > $D/configure.log 2>&1 || { tail -20 $D/configure.log; exit 1; }
R=$D/build/runtime/unit-test/bpftime_runtime_tests
F=$D/build/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests
date
nice -n 10 cmake --build $D/build --parallel 12 --target bpftime_runtime_tests bpftime_frida_uprobe_attach_tests > $D/build.log 2>&1 || { grep -E "error" $D/build.log | head -20; exit 1; }
date
timeout 1800 $R > $D/runtime.whole.out 2>&1; echo "runtime whole: rc $? $(grep -aE '^All tests|^test cases' $D/runtime.whole.out | tail -1)"
grep -aB12 'FAILED' $D/runtime.whole.out | grep -aE '^[A-Z].*[a-z]' | grep -av -E '^(Run with|Randomness)' | sort -u | sed 's/^/  failed or in a failed section: /' | head -20
for i in 1 2 3; do
    timeout 600 $R '[aot-cache]' > $D/aot-cache.$i.txt 2>&1
    echo "aot-cache run $i: rc $? $(grep -aE '^All tests|^test cases' $D/aot-cache.$i.txt | tail -1)"
done
timeout 900 $R '[array-jit-lifetime],[array-jit-default],[array-jit-poc],[jit_load]' > $D/array-jit.txt 2>&1
echo "array-jit groups: rc $? $(grep -aE '^All tests|^test cases' $D/array-jit.txt | tail -1)"
for i in 1 2 3; do
    timeout 900 $F > $D/frida.$i.txt 2>&1
    echo "frida run $i: rc $? $(grep -aE '^All tests|^test cases' $D/frida.$i.txt | tail -1)"
done
for f in $(find /dev/shm -mindepth 1 -maxdepth 1 -user "$(id -u)" -newer ~/src/jetson-check-c9d4d974.sh -printf '%f\n'); do
    fuser -s /dev/shm/$f || rm -f -- /dev/shm/$f
done
echo "/dev/shm at the end:"; ls -l /dev/shm | tail -n +2
date
