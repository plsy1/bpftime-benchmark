#!/usr/bin/env bash
# x86 NUC: Jetson's keycheck (src/keycheck.c) on the host, before (build-2e850c45)
# and after (build-59e5e154) the pthread key fix. Per build: no tracing, the
# transformer without programs, the transformer while syscount's programs are
# loaded (LD_PRELOAD=syscall-server syscount -d 6 started first). Root and
# vm.mmap_min_addr = 0 (zpoline); the original value is restored on exit.
# Usage: keycheck.sh > keycheck.log 2>&1
set -u
D=$(cd "$(dirname "$0")" && pwd)
T=/home/y/src/bpftime-wt-uprobe-177a1dc
SC=$T/example/tracing/syscount/syscount
orig=$(cat /proc/sys/vm/mmap_min_addr)
trap 'echo "$orig" | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null; echo "vm.mmap_min_addr restored to $(cat /proc/sys/vm/mmap_min_addr) (original $orig)"' EXIT
echo 0 | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null
for b in build-2e850c45 build-59e5e154; do
    B=$T/$b
    sudo -n "$B/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1
    echo "== $b: no tracing"
    "$D/src/keycheck"; echo "exit $?"
    echo "== $b: transformer, no programs"
    sudo -n env AGENT_SO="$B/runtime/agent/libbpftime-agent.so" \
        LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" \
        "$D/src/keycheck" 2>&1 | grep -v '^\['; echo "exit ${PIPESTATUS[0]}"
    echo "== $b: transformer, syscount programs loaded"
    sudo -n env LD_PRELOAD="$B/runtime/syscall-server/libbpftime-syscall-server.so" "$SC" -d 6 >"/tmp/kc-$b.log" 2>&1 &
    sleep 3
    sudo -n env AGENT_SO="$B/runtime/agent/libbpftime-agent.so" \
        LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" \
        "$D/src/keycheck" 2>&1 | grep -v '^\['; echo "exit ${PIPESTATUS[0]}"
    wait
    echo "-- syscount output:"; grep -v '^\[' "/tmp/kc-$b.log"; rm -f "/tmp/kc-$b.log"
    sudo -n "$B/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1
done
