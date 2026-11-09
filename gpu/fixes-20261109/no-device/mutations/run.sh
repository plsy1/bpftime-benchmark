#!/usr/bin/env bash
# Applies each mutation of mutate.py to the dev candidate, rebuilds bpftime_runtime_tests
# (build-cuda), runs [cuda-no-device], restores the source (checked against a copy).
set -u
R=/home/jetson/src/results/gpu/fixes-20261109/no-device/mutations
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
F=runtime/src/attach/bpf_attach_ctx_cuda.cpp
B=$C/build-cuda
cp $C/$F $R/source.backup
for m in none throw-again memset-first; do
  [ $m != none ] && python3 $R/mutate.py $m $C
  if timeout 3600 cmake --build $B --parallel 6 --target bpftime_runtime_tests > $R/build-$m.log 2>&1; then
    timeout 300 env BPFTIME_GLOBAL_SHM_NAME=bpftime_mut_nodev_$$ $B/runtime/unit-test/bpftime_runtime_tests '[cuda-no-device]' > $R/test-$m.log 2>&1
    rc=$?
    echo "$m: rc $rc $(grep -a -E '^All tests|^test cases' $R/test-$m.log | tail -n 1)"
    rm -f /dev/shm/bpftime_mut_nodev_$$
  else
    echo "$m: build failed"
  fi
  cp $R/source.backup $C/$F
done
cmp $R/source.backup $C/$F && echo restored-identical
rm -f $R/source.backup
