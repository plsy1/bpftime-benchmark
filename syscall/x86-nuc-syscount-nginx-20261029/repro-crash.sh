#!/bin/bash
# nginx (benchmark nginx.conf) started with the transformer while syscount's
# programs are loaded: backtrace of the crash. Runs in the container, cwd = view/.
set -u
B=$PWD/build
CONF=$PWD/benchmark/syscount-nginx/nginx.conf
$B/tools/bpftimetool/bpftimetool remove >/dev/null 2>&1
LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so \
  example/libbpf-tools/syscount/syscount -d 15 -T 40 > /tmp/sc.log 2>&1 &
sleep 3
timeout 60 gdb -q -batch -ex 'set pagination off' -ex 'set startup-with-shell off' \
  -ex "set environment AGENT_SO=$B/runtime/agent/libbpftime-agent.so" \
  -ex "set environment LD_PRELOAD=$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" \
  -ex 'handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint' \
  -ex 'break abort' -ex run -ex 'bt 40' -ex 'info registers pc' \
  --args nginx -c $CONF -p $(dirname $CONF)
pkill -9 -x syscount; wait
$B/tools/bpftimetool/bpftimetool remove >/dev/null 2>&1
