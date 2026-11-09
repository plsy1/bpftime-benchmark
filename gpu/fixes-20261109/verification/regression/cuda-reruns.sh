#!/usr/bin/env bash
# The cases that failed in cuda-runtime.log (build-cuda, whole suite in one non-root process, run
# right after each.sh, whose root runs had left root-owned segments with the tests' fixed names),
# each in a process of its own, non-root, with a segment of its own, on build-cuda and on
# build-probecheck (CUDA off), after removing those segments. Prints rc per case and build.
set -u
D=/home/jetson/src/results/gpu/fixes-20261109/verification/regression
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
export BPFTIME_LOG_OUTPUT=console
mkdir -p $D/reruns
i=0
while IFS= read -r name; do
  i=$((i+1))
  spec=$(printf '%s' "$name" | sed 's/,/\\,/g; s/\[/\\[/g')
  for b in build-cuda build-probecheck; do
    S=bpftime_rerun_$$_$i
    timeout 300 env BPFTIME_GLOBAL_SHM_NAME=$S $C/$b/runtime/unit-test/bpftime_runtime_tests "$spec" > $D/reruns/$i-$b.log 2>&1
    rc=$?
    printf '%-17s rc=%-3s %s | %s\n' $b $rc "$name" "$(grep -a -E '^All tests|^test cases' $D/reruns/$i-$b.log | tail -n 1)"
    rm -f /dev/shm/$S
  done
done <<'CASES'
Typed hash helpers serve hash maps next to arrays and global data
Typed hash delete helper serves hash maps in plain and bound programs
Test shm progs attach
Test shm hash maps with sub process
Test bpftime shm json import/export
Test basic operations of external hash map ops
Syscall server keeps shared memory while a peer server pid is alive
Syscall server keeps shared memory while an agent pid is alive
Bound programs' AOT objects use the maps of the process running them
A helper error on every call logs at 1, 10, 100, 1000
Agents link the AOT object the server compiled for a program
Test tail calling from userspace to kernel
CASES
