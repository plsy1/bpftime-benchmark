#!/usr/bin/env bash

set -uo pipefail

RESULT_DIR=/home/jetson/src/results/uprobe/ordinary-array-update-specialization-full-arm64-20261015
SOURCE_DIR=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD_DIR="$SOURCE_DIR/build-uprobe-array-update-specialization-20261015"
RUNNER_ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/array-update-runner/bpftime
BENCHMARK_SCRIPT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/runner/bpftime/benchmark.py
UPROBE_DIR="$SOURCE_DIR/benchmark/uprobe"
BPFTIMETOOL="$BUILD_DIR/tools/bpftimetool/bpftimetool"

archive_results() {
    status=$?

    printf '%s\n' "$status" > "$RESULT_DIR/exit-status.txt"
    date --iso-8601=seconds > "$RESULT_DIR/finished-at.txt"
    sudo -n /usr/sbin/nvpmodel -q > "$RESULT_DIR/nvpmodel-after.txt" 2>&1 || true
    sudo -n jetson_clocks --show > "$RESULT_DIR/jetson-clocks-after.txt" 2>&1 || true

    for name in benchmark-output.json results.md benchmark-logs.txt benchmark-logs-error.txt; do
        if [[ -f "$UPROBE_DIR/$name" ]]; then
            cp "$UPROBE_DIR/$name" "$RESULT_DIR/$name"
        fi
    done

    sudo -n "$BPFTIMETOOL" remove >> "$RESULT_DIR/cleanup.log" 2>&1 || true
    exit "$status"
}

trap archive_results EXIT

date --iso-8601=seconds > "$RESULT_DIR/started-at.txt"
git -C "$SOURCE_DIR" rev-parse HEAD > "$RESULT_DIR/base-commit.txt"
git -C "$SOURCE_DIR" diff > "$RESULT_DIR/source.diff"
cp "$BENCHMARK_SCRIPT" "$RESULT_DIR/benchmark.py"
sudo -n /usr/sbin/nvpmodel -q > "$RESULT_DIR/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks > "$RESULT_DIR/jetson-clocks-command.txt" 2>&1
sudo -n jetson_clocks --show > "$RESULT_DIR/jetson-clocks-before.txt" 2>&1
sudo -n "$BPFTIMETOOL" remove > "$RESULT_DIR/cleanup.log" 2>&1 || true

cd "$RUNNER_ROOT"
sudo -n taskset -c 5 env PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
    python3 "$BENCHMARK_SCRIPT" --iter 10 2>&1 | tee "$RESULT_DIR/runner.log"

