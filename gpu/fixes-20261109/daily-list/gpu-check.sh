#!/usr/bin/env bash
# The 236-commit candidate (dev/uprobe-official-4708f4092d1e, c115b33d: the daily list on
# official 4708f409, with upstream #709) built with CUDA (its build-cuda, the settings of the dev
# candidate's build-cuda). The daily builds without CUDA, so this is the CUDA side's check:
# bpftime_nv_attach_tests, the CUDA runtime tests (each alone, a segment of its own), and the
# example matrix (fix-matrix.sh) on the default path and the late path
# (BPFTIME_CUDA_LOAD_SUBSTITUTION=0), with GPU_BUILD pointing run.sh at this build.
set -u
D=/home/jetson/src/results/gpu/fixes-20261109/daily-list
B=/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate/build-cuda
W=/home/jetson/src/bpftime-gpu-work
mkdir -p $D/gpu
timeout 1200 $B/attach/nv_attach_impl/test/bpftime_nv_attach_tests > $D/gpu/nv-attach.log 2>&1
echo "nv_attach rc=$? $(grep -a -E '^All tests|^test cases' $D/gpu/nv-attach.log | tail -n 1)"
i=0
for spec in '[cuda-no-device]' '[cuda-host-call]' '[cuda-ringbuf]' 'CUDA watcher stops before shared-memory teardown'; do
  i=$((i+1)); S=bpftime_d709_$$_$i
  timeout 300 env BPFTIME_GLOBAL_SHM_NAME=$S $B/runtime/unit-test/bpftime_runtime_tests "$spec" > $D/gpu/rt-$i.log 2>&1
  echo "rc=$? $spec | $(grep -a -E '^All tests|^test cases' $D/gpu/rt-$i.log | tail -n 1)"
  rm -f /dev/shm/$S
done
echo "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) start=$(date +%T)"
cd $W
GPU_BUILD=$B timeout 3600 bash ./fix-matrix.sh d709 > $D/gpu/matrix-default.out 2>&1
echo "== default path"; cat $W/runs/d709-summary.txt
GPU_BUILD=$B EXTRA_VIC_ENV=BPFTIME_CUDA_LOAD_SUBSTITUTION=0 timeout 3600 bash ./fix-matrix.sh d709-late > $D/gpu/matrix-late.out 2>&1
echo "== late path"; cat $W/runs/d709-late-summary.txt
echo "end=$(date +%T)"
cp $W/runs/d709-summary.txt $W/runs/d709-late-summary.txt $D/gpu/
grep -h -a 'GPU_BUILD\|server:' $W/runs/d709-krs-legacy/meta.txt | head -2
pgrep -a -x 'vec_add|vec_add_legacy|kernelretsnoop|host_map_test|kernel_trace|threadhist|threadscheduling|cuda_probe' || echo no-leftovers
ls /dev/shm | grep '^bpftime_gpu_' || echo shm-clean
