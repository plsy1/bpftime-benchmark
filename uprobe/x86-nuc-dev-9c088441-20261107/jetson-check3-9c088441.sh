#!/bin/bash
# x86 check of dev 9c088441, part 3 (build directory holds 9c088441):
#  - the 4 cases that skip or fail without root, under sudo, 3 times each;
#  - the whole runtime suite in one process with case durations printed, to see after which
#    case the "double free or corruption" abort comes (d46ceb8f and 9c088441 both abort);
#  - remove root-owned /dev/shm segments these runs created and the segments part 1-3 left.
set -u
D=~/src/jetson-check-9c088441
exec > >(tee -a ~/src/jetson-check-9c088441.log) 2>&1
echo "=== part 3"; date
cd $D || exit 1
git log --oneline -1
R=$D/build/runtime/unit-test/bpftime_runtime_tests
for c in "Test tail calling from userspace to kernel" \
         "Producers in two pid namespaces with the same pid write apart" \
         "Kernel-user percpu array gives a program its CPU's value" \
         "Kernel-user hash lookups are private to the calling thread"; do
    r=""
    for i in 1 2 3; do
        sudo -n timeout 120 $R "$c" > $D/root-case.log 2>&1 && r="$r ok" || r="$r rc=$?"
    done
    echo "sudo: $c:$r"
done
timeout 600 $R -d yes > $D/runtime.whole-durations.out 2>&1; echo "whole suite, durations: rc $?"
grep -aE '^[0-9.]+ s: ' $D/runtime.whole-durations.out | tail -3
grep -aE 'double free|corruption|^test cases' $D/runtime.whole-durations.out | tail -2
echo "root-owned in /dev/shm:"; find /dev/shm -mindepth 1 -maxdepth 1 -user root -printf '%f\n'
for f in $(find /dev/shm -mindepth 1 -maxdepth 1 -user root -newer ~/src/jetson-check-9c088441.sh -printf '%f\n'); do
    sudo -n fuser -s /dev/shm/$f || sudo -n rm -f -- /dev/shm/$f
done
for f in BPFTIME_TEST_ENTERNAL_MAP_SHM BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT bpftime_maps_shm LPMTrieMultiThreadTestShm; do
    [ -O /dev/shm/$f ] && { fuser -s /dev/shm/$f || rm -f -- /dev/shm/$f; }
done
echo "/dev/shm at the end:"; ls -l /dev/shm | tail -n +2
date
