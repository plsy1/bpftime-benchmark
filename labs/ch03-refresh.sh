#!/bin/bash
# 第 3 章：agent 先于 handler 表启动时只扫一次、计数一直为 0；用 IPC refresh 让它重新扫表。
# 用法：BPFTIME_BUILD=<bpftime 构建目录> ./ch03-refresh.sh
set -u
cd "$(dirname "$0")"
B=${BPFTIME_BUILD:?请设置 BPFTIME_BUILD 为 bpftime 的构建目录，例如 .../bpftime/build}
SS=$B/runtime/syscall-server/libbpftime-syscall-server.so
AG=$B/runtime/agent/libbpftime-agent.so
export BPFTIME_GLOBAL_SHM_NAME=lab_ch03_$RANDOM BPFTIME_LOG_OUTPUT=$PWD/ch03.log
rm -f ch03.log
OFF=0x$(nm victim | awk '/ target_func$/{print $1}')
# loader 先建 map，等 4 秒再建 prog/link：agent 在这期间启动，扫到的表里还没有 link
DELAY=4 SECS=14 LD_PRELOAD=$SS ./loader "$PWD/victim" "$OFF" > ch03.loader.out 2>&1 &
LP=$!
sleep 1
LD_PRELOAD=$AG ./victim &
VP=$!
sleep 7
ipc() { python3 -c 'import socket,sys; s=socket.socket(socket.AF_UNIX); \
s.connect("\0bpftime-agent-"+sys.argv[1]); s.sendall(sys.argv[2].encode()); \
s.shutdown(socket.SHUT_WR); print(s.recv(99))' "$@"; }
echo "status:";  ipc $VP "status"
echo "refresh:"; ipc $VP "refresh "
sleep 4
kill $VP
wait $LP
echo "=== loader 输出（refresh 之后 counter 才开始增长）"; cat ch03.loader.out
grep -nE 'Attach successfully|ipc|refresh|epoch' ch03.log
rm -f /dev/shm/$BPFTIME_GLOBAL_SHM_NAME /tmp/bpftime-shm-$BPFTIME_GLOBAL_SHM_NAME.lock
