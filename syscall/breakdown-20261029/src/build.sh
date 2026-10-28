#!/bin/bash
# tp_attach (bpftime's libbpf, static), writeloop, the dummy agent and the BPF
# objects: tp_other (getppid), tp_exit_empty (empty sys_exit_write), tp_count
# (counting sys_enter/sys_exit_write, from ../../arm64-trampoline-20261028).
set -eu
cd "$(dirname "$0")"
# BPFTIME_BUILD: a bpftime build directory (its libbpf/ is used); CC, CLANG
# and TARGET_ARCH (arm64 / x86) can be overridden.
BPFTIME_BUILD=${BPFTIME_BUILD:-/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily}
LB=$BPFTIME_BUILD/libbpf
CC=${CC:-gcc-13}
CLANG=${CLANG:-clang-15}
case "$(uname -m)" in x86_64) DEF_ARCH=x86 ;; aarch64) DEF_ARCH=arm64 ;; *) DEF_ARCH=$(uname -m) ;; esac
TARGET_ARCH=${TARGET_ARCH:-$DEF_ARCH}
$CC -O2 -g -o writeloop writeloop.c
$CC -O2 -g -I"$LB" -o tp_attach tp_attach.c "$LB/libbpf.a" -lelf -lz
$CC -O2 -g -shared -fPIC -o libdummy_agent.so dummy_agent.c
mkdir -p inc && ln -sfn "$LB/bpf" inc/bpf
for p in tp_other tp_exit_empty tp_count; do
  $CLANG -O2 -g -target bpf -D__TARGET_ARCH_$TARGET_ARCH -Iinc -I/usr/include/$(uname -m)-linux-gnu -c $p.bpf.c -o $p.bpf.o
done
