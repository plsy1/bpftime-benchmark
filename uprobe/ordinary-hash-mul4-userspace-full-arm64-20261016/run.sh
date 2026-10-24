#!/usr/bin/env bash

set -uo pipefail

RESULT_DIR=/home/jetson/src/results/uprobe/ordinary-hash-mul4-userspace-full-arm64-20261016
SOURCE_DIR=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD_DIR="$SOURCE_DIR/build-uprobe-ordinary-hash-fast4-20261016"
RUNNER_ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/ordinary-hash-fast4-runner/bpftime
BENCHMARK_SCRIPT="$SOURCE_DIR/benchmark/uprobe/benchmark.py"
BPFTIMETOOL="$BUILD_DIR/tools/bpftimetool/bpftimetool"

archive_results() {
    status=$?
    printf '%s\n' "$status" > "$RESULT_DIR/exit-status.txt"
    date --iso-8601=seconds > "$RESULT_DIR/finished-at.txt"
    sudo -n "$BPFTIMETOOL" remove >> "$RESULT_DIR/cleanup.log" 2>&1 || true
    exit "$status"
}

trap archive_results EXIT

date --iso-8601=seconds > "$RESULT_DIR/started-at.txt"
git -C "$SOURCE_DIR" rev-parse HEAD > "$RESULT_DIR/base-commit.txt"
git -C "$SOURCE_DIR" status --short > "$RESULT_DIR/source-status.txt"
git -C "$SOURCE_DIR" diff > "$RESULT_DIR/source.diff"
sudo -n /usr/sbin/nvpmodel -q > "$RESULT_DIR/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks --show > "$RESULT_DIR/jetson-clocks-before.txt" 2>&1
sudo -n "$BPFTIMETOOL" remove > "$RESULT_DIR/cleanup.log" 2>&1 || true

cd "$RUNNER_ROOT"
sudo -n taskset -c 5 env PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
    python3 -c 'import asyncio,runpy,time; m=runpy.run_path("/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate/benchmark/uprobe/benchmark.py"); t=time.time(); print(asyncio.run(m["run_userspace_uprobe_test"](10))); print("WALL_SECONDS", time.time()-t)' \
    2>&1 | tee "$RESULT_DIR/runner.log"
