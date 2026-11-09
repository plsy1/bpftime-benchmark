#!/bin/bash
# mutation checks for 三; each: mutate, regenerate PTX if needed, build tests, run, restore
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/san-mut
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
cd $C || exit 1
restore() {
  cp $S/backup/default_trampoline.cu attach/nv_attach_impl/trampoline/default_trampoline.cu
  cp $S/backup/trampoline_ptx.h attach/nv_attach_impl/trampoline_ptx.h
  cp $S/backup/bpf_attach_ctx_cuda.cpp runtime/src/attach/bpf_attach_ctx_cuda.cpp
  cp $S/backup/nv_gpu_ringbuf_map.cpp runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp
}
trap restore EXIT
for m in "$@"; do
  restore
  echo "=== $m $(date +%T)"
  python3 $S/mutate.py "$m" || { echo "mutate failed"; continue; }
  if [ "$m" != none ] && [ "$m" != watcher-sleeps ] && [ "$m" != drain-one-per-poll ]; then
    make -s -C attach/nv_attach_impl/trampoline CLANG=clang++-16 ../trampoline_ptx.h >/dev/null 2>&1 || { echo "regen failed"; continue; }
  fi
  timeout 1500 cmake --build build-cuda -j4 --target bpftime_runtime_tests > $S/build-$m.log 2>&1 || { echo "build failed"; tail -5 $S/build-$m.log; continue; }
  timeout 300 build-cuda/runtime/unit-test/bpftime_runtime_tests "[cuda-ringbuf],[cuda-host-call]" > $S/test-$m.log 2>&1
  echo "exit=$?"
  grep -E "^/.*test_cuda_host_call.cpp:[0-9]+: FAILED|^test cases|All tests passed|host calls in" $S/test-$m.log | sort | uniq -c | head -12
  grep -B1 -A0 "TEST_CASE\|^-------" $S/test-$m.log >/dev/null
  grep -E "^[A-Z].*$" $S/test-$m.log | grep -E "^(A |Every|GPU threads|Per-thread)" | sort -u
done
restore
echo "=== restored; rebuilding $(date +%T)"
timeout 1500 cmake --build build-cuda -j4 --target bpftime_runtime_tests > $S/build-final.log 2>&1; echo "final build exit=$?"
timeout 300 build-cuda/runtime/unit-test/bpftime_runtime_tests "[cuda-ringbuf],[cuda-host-call]" 2>&1 | tail -2
