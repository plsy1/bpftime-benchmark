#!/bin/bash
# 第 2 章：看共享内存段、假 fd、锁文件、handler 表导出，以及第二个 syscall-server 清表。
# 用法：BPFTIME_BUILD=<bpftime 构建目录> ./ch02-shm.sh
set -u
cd "$(dirname "$0")"
B=${BPFTIME_BUILD:?请设置 BPFTIME_BUILD 为 bpftime 的构建目录，例如 .../bpftime/build}
SS=$B/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$B/tools/bpftimetool/bpftimetool
export BPFTIME_GLOBAL_SHM_NAME=lab_ch02_$RANDOM
export BPFTIME_LOG_OUTPUT=$PWD/ch02.log
OFF=0x$(nm victim | awk '/ target_func$/{print $1}')
echo "SHM=$BPFTIME_GLOBAL_SHM_NAME  target_func offset=$OFF"
SECS=12 LD_PRELOAD=$SS ./loader "$PWD/victim" "$OFF" > ch02.loader.out 2>&1 &
LP=$!
sleep 2
echo "=== 段文件与实际占用";   ls -l /dev/shm/$BPFTIME_GLOBAL_SHM_NAME; du -h /dev/shm/$BPFTIME_GLOBAL_SHM_NAME
echo "=== loader 的 fd（假 fd 指向 /dev/null）"; ls -l /proc/$LP/fd
echo "=== 锁文件";            ls -l /tmp/bpftime-shm-$BPFTIME_GLOBAL_SHM_NAME.lock
echo "=== 导出 handler 表";   $TOOL export "$PWD/ch02.json" && python3 -m json.tool ch02.json
echo "=== 第二个 syscall-server 进程（只调一次 close(-1)）"
LD_PRELOAD=$SS ./poke; echo "poke rc=$?"
$TOOL export "$PWD/ch02-after.json"; echo "再导出："; cat ch02-after.json; echo
wait $LP
echo "=== loader 输出（poke 之后 lookup 返回 -1）"; cat ch02.loader.out
rm -f /dev/shm/$BPFTIME_GLOBAL_SHM_NAME /tmp/bpftime-shm-$BPFTIME_GLOBAL_SHM_NAME.lock
