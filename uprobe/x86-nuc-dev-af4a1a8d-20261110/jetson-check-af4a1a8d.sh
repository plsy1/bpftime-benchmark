#!/bin/bash
# x86 check of dev af4a1a8d, the last commit of the GPU fix round (fixes-20261109), on the pushed
# 45743763; af4a1a8d itself comes as a git bundle (not pushed). Without CUDA the round's CPU-side
# changes are: the llvmbpf overlay (a bpf-to-bpf call moves r10 by a whole frame, e94e2c04; the
# stack frame count, 45743763), the syscall server passing calls after its segment is gone to libc
# (17427685), cmake. Build with the daily's settings (probe checks off, LLVM 19, gcc-13), then:
#   - the whole runtime suite in one process, console reporter, failures listed;
#   - [jit_load] (the frame-overlap test, JIT and AOT) and the syscall server exit test 3 times;
#   - [aot-cache] and the array JIT groups once; the Frida attach suite 3 times;
#   - remove the /dev/shm segments these runs created (non-root runs only).
set -u
D=~/src/jetson-check-af4a1a8d
exec > >(tee -a ~/src/jetson-check-af4a1a8d.log) 2>&1
date; uname -r; nproc; df -h ~/src | tail -n 1
git clone -q --reference ~/src/bpftime-benchmark --dissociate -b dev/uprobe-official-177a1dc465ff https://github.com/plsy1/bpftime-benchmark $D || exit 1
cd $D && git checkout -q --detach 45743763463c474eae1563a458c97db863276741 || exit 1
git fetch -q ~/src/jetson-check-af4a1a8d.bundle HEAD || exit 1
git checkout -q --detach af4a1a8d094500043df8876e84d48e646c2638e9 && git log --oneline -3 || exit 1
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
    timeout 600 $R '[jit_load]' > $D/jit-load.$i.txt 2>&1
    echo "jit_load run $i: rc $? $(grep -aE '^All tests|^test cases' $D/jit-load.$i.txt | tail -1)"
    timeout 300 $R 'The syscall server passes calls made after its segment is gone to libc' > $D/server-exit.$i.txt 2>&1
    echo "server exit run $i: rc $? $(grep -aE '^All tests|^test cases' $D/server-exit.$i.txt | tail -1)"
done
timeout 600 $R '[aot-cache]' > $D/aot-cache.txt 2>&1
echo "aot-cache: rc $? $(grep -aE '^All tests|^test cases' $D/aot-cache.txt | tail -1)"
timeout 900 $R '[array-jit-lifetime],[array-jit-default],[array-jit-poc]' > $D/array-jit.txt 2>&1
echo "array-jit groups: rc $? $(grep -aE '^All tests|^test cases' $D/array-jit.txt | tail -1)"
for i in 1 2 3; do
    timeout 900 $F > $D/frida.$i.txt 2>&1
    echo "frida run $i: rc $? $(grep -aE '^All tests|^test cases' $D/frida.$i.txt | tail -1)"
done
for f in $(find /dev/shm -mindepth 1 -maxdepth 1 -user "$(id -u)" -newer ~/src/jetson-check-af4a1a8d.sh -printf '%f\n'); do
    fuser -s /dev/shm/$f || rm -f -- /dev/shm/$f
done
echo "/dev/shm at the end:"; ls -l /dev/shm | tail -n +2
date
