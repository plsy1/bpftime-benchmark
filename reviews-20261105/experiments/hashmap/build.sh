#!/bin/bash
# build.sh <src.cpp> <out>: compile a standalone experiment against the hash map header at 94f9ba8f
set -e
W=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
g++-13 -std=gnu++20 -O2 -g -fno-omit-frame-pointer -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
  -I$W/runtime/include -I$W/runtime/src -I$W/third_party/spdlog/include \
  "$1" -o "$2" -lpthread -lrt
