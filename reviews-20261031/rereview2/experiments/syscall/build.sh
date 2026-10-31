#!/bin/sh
R=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$R/build-jetson-daily
exec g++-13 -std=c++20 -O1 -g -flto=auto -I $R/attach/text_segment_transformer -I $B/FridaGum-prefix/src/FridaGum -I $R/third_party/spdlog/include -DSPDLOG_COMPILED_LIB -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO "$1" $B/attach/text_segment_transformer/libbpftime_text_segment_transformer_core.a $B/third_party/spdlog/libspdlog.a $B/FridaGum-prefix/src/FridaGum/libfrida-gum.a -lpthread -ldl -o "${1%.cpp}"
