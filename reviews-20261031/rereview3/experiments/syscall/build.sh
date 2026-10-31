#!/bin/bash
# usage: build.sh src.cpp out  -- links against the prebuilt transformer core
R=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$R/build-jetson-daily
exec g++-13 -O2 -g -std=gnu++20 -fno-omit-frame-pointer -flto=auto \
  -DSPDLOG_COMPILED_LIB -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
  -I$R/attach/text_segment_transformer -I$B/FridaGum-prefix/src/FridaGum -I$R/third_party/spdlog/include \
  "$1" -o "$2" $B/attach/text_segment_transformer/libbpftime_text_segment_transformer_core.a \
  -lpthread $B/third_party/spdlog/libspdlog.a $B/FridaGum-prefix/src/FridaGum/libfrida-gum.a -ldl
