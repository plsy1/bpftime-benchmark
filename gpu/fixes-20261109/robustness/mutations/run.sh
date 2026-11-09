#!/bin/bash
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/liu-mut
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
cd $C || exit 1
restore() {
  cp $S/backup/nv_attach_utils.cpp $S/backup/nv_attach_impl.cpp attach/nv_attach_impl/
  cp $S/backup/ptx_compiler.cpp attach/nv_attach_impl/ptx_compiler/
  cp $S/backup/bpf_attach_ctx_cuda.cpp runtime/src/attach/
}
trap restore EXIT
for m in "$@"; do
  restore; echo "=== $m $(date +%T)"
  python3 $S/mutate.py "$m" || { echo "mutate failed"; continue; }
  case $m in
    map-value-unchecked|watcher-no-context) T=bpftime_runtime_tests; B=build-cuda/runtime/unit-test/bpftime_runtime_tests; F="[cuda-host-call]";;
    *) T=bpftime_nv_attach_tests; B=build-cuda/attach/nv_attach_impl/test/bpftime_nv_attach_tests; F="[sm-arch],[ptx-work-dir],[ptx-compiler]";;
  esac
  timeout 2000 cmake --build build-cuda -j4 --target $T > $S/build-$m.log 2>&1 || { echo "build failed"; grep error $S/build-$m.log | head -3; continue; }
  timeout 300 $B "$F" > $S/test-$m.log 2>&1; echo "exit=$?"
  grep -E "FAILED:|test cases" -A2 $S/test-$m.log | grep -E "CHECK|REQUIRE|FAILED|test cases|expansion" | head -6
done
restore; echo "=== restored $(date +%T)"
timeout 2500 cmake --build build-cuda -j4 --target bpftime_nv_attach_tests bpftime_runtime_tests > $S/build-final.log 2>&1; echo "final build=$?"
timeout 600 build-cuda/attach/nv_attach_impl/test/bpftime_nv_attach_tests 2>&1 | grep -E "All tests|test cases" | tail -1
timeout 300 build-cuda/runtime/unit-test/bpftime_runtime_tests "[cuda-ringbuf],[cuda-host-call]" 2>&1 | grep -E "All tests|test cases"
