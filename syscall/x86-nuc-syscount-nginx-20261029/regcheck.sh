#!/usr/bin/env bash
# x86 NUC: src/regcheck (registers across a rewritten `syscall`) on the host for
# official 177a1dc (build-jit, transformer target added 10/29), dev 2e850c45 and
# 59e5e154: no tracing, transformer without programs, transformer while
# syscount's programs are loaded. Root, vm.mmap_min_addr = 0 (restored on exit).
# Usage: regcheck.sh > regcheck.log 2>&1
set -u
D=$(cd "$(dirname "$0")" && pwd)
SC=/home/y/src/bpftime-wt-uprobe-177a1dc/example/tracing/syscount/syscount
orig=$(cat /proc/sys/vm/mmap_min_addr)
trap 'echo "$orig" | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null; echo "vm.mmap_min_addr restored to $(cat /proc/sys/vm/mmap_min_addr) (original $orig)"' EXIT
echo 0 | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null
echo "== no tracing"; "$D/src/regcheck"; echo "exit $?"
for B in /home/y/src/bpftime-wt-official-177a1dc/build-jit \
         /home/y/src/bpftime-wt-uprobe-177a1dc/build-2e850c45 \
         /home/y/src/bpftime-wt-uprobe-177a1dc/build-59e5e154; do
    sudo -n "$B/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1
    echo "== $B: transformer, no programs"
    sudo -n env AGENT_SO="$B/runtime/agent/libbpftime-agent.so" \
        LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" \
        "$D/src/regcheck" 2>&1 | grep -v '^\['; echo "exit ${PIPESTATUS[0]}"
    echo "== $B: transformer, syscount programs loaded"
    sudo -n env LD_PRELOAD="$B/runtime/syscall-server/libbpftime-syscall-server.so" "$SC" -d 6 >/tmp/rc-sc.log 2>&1 &
    sleep 3
    sudo -n env AGENT_SO="$B/runtime/agent/libbpftime-agent.so" \
        LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" \
        "$D/src/regcheck" 2>&1 | grep -v '^\['; echo "exit ${PIPESTATUS[0]}"
    wait
    echo "-- syscount output:"; grep -v '^\[' /tmp/rc-sc.log | grep -v '^$'; rm -f /tmp/rc-sc.log
    sudo -n "$B/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1
done
