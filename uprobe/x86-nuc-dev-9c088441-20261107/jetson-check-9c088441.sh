#!/bin/bash
# x86 check of dev 9c088441 (the 16 commits d46ceb8f..9c088441: ringbuf reserve lock, perf
# wakeup/overflow shard, epoll nap/idle/signals, agent fail-fast, frida module cache).
# Build with the daily's settings (probe checks off), run on 9c088441:
#   - the whole runtime suite once, the Frida attach suite once;
#   - [ringbuf],[epoll],[perf_event],[allocation] three times (the changed areas);
# then the same whole-suite runs on d46ceb8f (last x86-unverified daily entry) as the baseline,
# so a failure is new only if d46ceb8f passes it here.
set -u
D=~/src/jetson-check-9c088441
exec > >(tee -a ~/src/jetson-check-9c088441.log) 2>&1
date; uname -r; nproc
git clone -q --reference ~/src/bpftime-benchmark --dissociate -b dev/uprobe-official-177a1dc465ff https://github.com/plsy1/bpftime-benchmark $D || exit 1
cd $D && git checkout -q --detach 9c0884414084af213448ab116f920917b9b9c8cb && git log --oneline -1 || exit 1
git submodule update --init --recursive --jobs 8 -q || exit 1
cmake -S $D -B $D/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -DBPFTIME_BPF_CLANG=/usr/bin/clang-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF -DBUILD_BPFTIME_DAEMON=ON > $D/configure.log 2>&1 || { tail -20 $D/configure.log; exit 1; }
R=$D/build/runtime/unit-test/bpftime_runtime_tests
F=$D/build/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests

failed_names() { # junit xml -> failed test case names (top level, unique)
    python3 - "$1" <<'PY'
import sys, xml.etree.ElementTree as ET
try:
    root = ET.parse(sys.argv[1]).getroot()
except Exception as e:
    print("  (no complete report: %s)" % e); sys.exit()
names = []
for tc in root.iter('testcase'):
    if tc.find('failure') is not None or tc.find('error') is not None:
        n = tc.get('name', '').split('/')[0]
        if n not in names: names.append(n)
for n in names: print("  FAILED:", n)
print("  failed cases:", len(names))
PY
}

build_and_run() { # $1 = label
    local L=$1
    date
    cmake --build $D/build --parallel 8 --target bpftime_runtime_tests bpftime_frida_uprobe_attach_tests > $D/build.$L.log 2>&1 || { grep -E "error" $D/build.$L.log | head -20; exit 1; }
    date
    timeout 1800 $R -r junit -o $D/runtime.$L.xml > $D/runtime.$L.out 2>&1; echo "runtime $L: rc $?"
    failed_names $D/runtime.$L.xml
    timeout 900 $F -r junit -o $D/frida.$L.xml > $D/frida.$L.out 2>&1; echo "frida $L: rc $?"
    failed_names $D/frida.$L.xml
}

build_and_run new
for i in 1 2 3; do
    timeout 900 $R "[ringbuf],[epoll],[perf_event],[allocation]" > $D/groups.$i.txt 2>&1
    echo "groups run $i: rc $? $(grep -aE '^All tests|^test cases' $D/groups.$i.txt | tail -1)"
done
git checkout -q --detach d46ceb8f4168d067d7c6aa579333a8fb29fc82d4 && git log --oneline -1 || exit 1
build_and_run base
echo "/dev/shm after the runs:"; ls -l /dev/shm | tail -n +2; date
