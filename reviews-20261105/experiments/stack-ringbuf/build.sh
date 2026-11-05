#!/bin/bash
# Builds the experiments against the existing build's static core library,
# with the flags of bpftime_text_segment_transformer_tests.
set -e
W=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$W/build-jetson-daily
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
for src in altstack_race busy_stuck thread_churn; do
	/usr/bin/g++-13 -fno-omit-frame-pointer -O2 -g -DNDEBUG -std=gnu++20 \
		-flto=auto -fno-fat-lto-objects -fPIE \
		-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO -DSPDLOG_COMPILED_LIB \
		-I$W/attach/text_segment_transformer \
		-I$B/FridaGum-prefix/src/FridaGum -I$W/third_party/spdlog/include \
		$src.cpp -o $src \
		$B/attach/text_segment_transformer/libbpftime_text_segment_transformer_core.a \
		-lpthread $B/third_party/spdlog/libspdlog.a \
		$B/FridaGum-prefix/src/FridaGum/libfrida-gum.a -lncurses
	echo "built $src"
done
