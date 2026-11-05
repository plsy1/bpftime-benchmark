#!/bin/bash
set -e
W=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily
FLAGS="-fno-omit-frame-pointer -O2 -g -DNDEBUG -std=gnu++20 -flto=auto -fno-fat-lto-objects -fPIE -DBPFTIME_BUILD_WITH_LIBBPF=1 -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO -DSPDLOG_COMPILED_LIB -I$W/attach/frida_uprobe_attach_impl/include -I$W/third_party/spdlog/include -I$B/FridaGum-prefix/src/FridaGum -I$W/attach/base_attach_impl"
LIBS="$B/attach/frida_uprobe_attach_impl/libbpftime_frida_uprobe_attach_impl.a $B/third_party/spdlog/libspdlog.a $B/FridaGum-prefix/src/FridaGum/libfrida-gum.a $B/third_party/spdlog/libspdlog.a -ldl -lpthread"
for s in "$@"; do
  echo "building $s"
  /usr/bin/g++-13 $FLAGS $s.cpp -o $s $LIBS 2>&1 | grep -v "^$" | head -30
done
