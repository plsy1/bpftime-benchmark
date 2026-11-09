#!/usr/bin/env bash
# fixes-20261109 final regression, dev af4a1a8d, as uprobe/startup-20261108's:
# CPU build (candidate build-probecheck, CUDA off, checks on): every runtime test case as root in
# a process of its own (each.sh), [kernel_user] and the daemon tests as root, the transformer
# tests, the syscall trace tests as root (the daily filter), the Frida attach tests (5 runs), then
# the daily smoke (tracker.smoke_real_tools).
# CUDA build (build-cuda, never installed): bpftime_nv_attach_tests whole, and the whole runtime
# suite in one process (non-root, a segment of its own; the CUDA cases among them).
set -u
D=/home/jetson/src/results/gpu/fixes-20261109/verification/regression
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$C/build-probecheck
G=$C/build-cuda
export BPFTIME_LOG_OUTPUT=console
date +%T > $D/started
timeout 5400 bash $D/each.sh $B/runtime/unit-test/bpftime_runtime_tests > $D/each.out 2>&1
sudo -n timeout 600 env BPFTIME_LOG_OUTPUT=console $B/runtime/unit-test/bpftime_runtime_tests '[kernel_user]' > $D/kernel-user.log 2>&1; echo "kernel-user rc=$?" >> $D/kernel-user.log
sudo -n timeout 900 env BPFTIME_LOG_OUTPUT=console $B/daemon/test/bpftime_daemon_tests > $D/daemon.log 2>&1; echo "daemon rc=$?" >> $D/daemon.log
timeout 1200 $B/attach/text_segment_transformer/bpftime_text_segment_transformer_tests > $D/transformer.log 2>&1; echo "transformer rc=$?" >> $D/transformer.log
sudo -n timeout 600 env BPFTIME_LOG_OUTPUT=console $B/attach/syscall_trace_attach_impl/bpftime_syscall_trace_attach_tests '~"Test syscall dispatch - multiple"' '~"Test resolving private data"' > $D/syscall-trace.log 2>&1; echo "syscall-trace rc=$?" >> $D/syscall-trace.log
for i in 1 2 3 4 5; do
  timeout 900 $B/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests > $D/frida-$i.log 2>&1
  echo "frida run $i rc=$? $(grep -E 'test cases' $D/frida-$i.log | tail -1)" >> $D/frida.log
done
date +%T > $D/cpu-finished
timeout 1200 $G/attach/nv_attach_impl/test/bpftime_nv_attach_tests > $D/cuda-nv-attach.log 2>&1; echo "nv_attach rc=$?" >> $D/cuda-nv-attach.log
timeout 3600 env BPFTIME_GLOBAL_SHM_NAME=bpftime_verify_cuda_$$ $G/runtime/unit-test/bpftime_runtime_tests > $D/cuda-runtime.log 2>&1; echo "cuda runtime rc=$?" >> $D/cuda-runtime.log
rm -f /dev/shm/bpftime_verify_cuda_$$
cd /home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream || exit 1
timeout 600 python3 -c '
import sys
from pathlib import Path
import tracker
config, repo = tracker.load_config(Path("config.json").resolve())
tracker.smoke_real_tools(config, Path(sys.argv[1]), Path(sys.argv[2]))
' $C $B > $D/smoke.log 2>&1
echo "smoke rc=$?" >> $D/smoke.log
date +%T > $D/finished
