#!/usr/bin/env bash
# Baseline of the new development base: dev/uprobe-official-4708f4092d1e at c115b33d (the 236
# commits on official 4708f409), build-probecheck (CUDA off, checks on, LTO, the old dev worktree's
# settings). As gpu/fixes-20261109/verification/regression without the CUDA part: every runtime
# test case as root in a process of its own (each.sh), [kernel_user] and the daemon tests as root,
# the transformer tests, the syscall trace tests as root (the daily filter), the Frida attach tests
# (5 runs), then the daily smoke (tracker.smoke_real_tools). Root-owned test segments are removed
# afterwards (fuser checked).
set -u
D=/home/jetson/src/results/uprobe/base-4708f409-20261110/regression
C=/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate
B=$C/build-probecheck
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
cd /home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream || exit 1
timeout 600 python3 -c '
import sys
from pathlib import Path
import tracker
config, repo = tracker.load_config(Path("config.json").resolve())
tracker.smoke_real_tools(config, Path(sys.argv[1]), Path(sys.argv[2]))
' $C $B > $D/smoke.log 2>&1
echo "smoke rc=$?" >> $D/smoke.log
for f in bpftime_maps_shm BPFTIME_TEST_ENTERNAL_MAP_SHM BPFTIME_TEST_SHM_JSON_IMPORT_EXPORT my_shm_attach_test my_shm_maps_test; do
  [ -e /dev/shm/$f ] || continue
  sudo -n fuser -s /dev/shm/$f && echo "in use: $f" || { sudo -n rm -f -- /dev/shm/$f; echo "removed $f"; }
done > $D/shm-cleanup.txt
date +%T > $D/finished
