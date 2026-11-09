#!/bin/bash
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/wu-mut
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
cd $C || exit 1
restore() { cp $S/backup/nv_elf_introspect.cpp $S/backup/nv_attach_impl.cpp attach/nv_attach_impl/; }
trap restore EXIT
for m in "$@"; do
  restore; echo "=== $m $(date +%T)"
  python3 $S/mutate.py "$m" || { echo "mutate failed"; continue; }
  timeout 1500 cmake --build build-cuda -j4 --target bpftime_nv_attach_tests > $S/build-$m.log 2>&1 || { echo "build failed"; grep error $S/build-$m.log | head -3; continue; }
  timeout 300 build-cuda/attach/nv_attach_impl/test/bpftime_nv_attach_tests "[late-path]" > $S/test-$m.log 2>&1; echo "exit=$?"
  grep -E "FAILED:|test cases" -A2 $S/test-$m.log | grep -E "CHECK|FAILED|test cases" | head -8
done
restore; echo "=== restored $(date +%T)"
timeout 1500 cmake --build build-cuda -j4 --target bpftime_nv_attach_tests > $S/build-final.log 2>&1; echo "final build=$?"
timeout 300 build-cuda/attach/nv_attach_impl/test/bpftime_nv_attach_tests 2>&1 | grep -E "All tests|test cases"
