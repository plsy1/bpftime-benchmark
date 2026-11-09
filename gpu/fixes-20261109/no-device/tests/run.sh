#!/usr/bin/env bash
# The no-GPU fix on build-cuda (dev candidate, uncommitted at the run): build, then the CUDA
# runtime tests, the four CPU tests that failed in the CUDA build (verification/regression),
# and bpftime_nv_attach_tests; each runtime test alone with a segment of its own.
set -u
R=/home/jetson/src/results/gpu/fixes-20261109/no-device/tests
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$C/build-cuda
timeout 3600 cmake --build $B --parallel 6 --target bpftime_runtime_tests bpftime-agent bpftime-syscall-server bpftime_nv_attach_tests > $R/build.log 2>&1 || { echo build failed; exit 1; }
i=0
while IFS= read -r name; do
  i=$((i+1))
  spec=$(printf '%s' "$name" | sed 's/,/\\,/g')
  S=bpftime_nodev_tests_$$_$i
  timeout 300 env BPFTIME_GLOBAL_SHM_NAME=$S $B/runtime/unit-test/bpftime_runtime_tests "$spec" > $R/$i.log 2>&1
  printf 'rc=%-3s %s | %s\n' $? "$name" "$(grep -a -E '^All tests|^test cases' $R/$i.log | tail -n 1)"
  rm -f /dev/shm/$S
done <<'CASES'
[cuda-no-device]
[cuda-host-call]
[cuda-ringbuf]
CUDA watcher stops before shared-memory teardown
Typed hash helpers serve hash maps next to arrays and global data
Typed hash delete helper serves hash maps in plain and bound programs
Bound programs' AOT objects use the maps of the process running them
Agents link the AOT object the server compiled for a program
CASES
timeout 1200 $B/attach/nv_attach_impl/test/bpftime_nv_attach_tests > $R/nv-attach.log 2>&1
echo "nv_attach rc=$? $(grep -a -E '^All tests|^test cases' $R/nv-attach.log | tail -n 1)"
