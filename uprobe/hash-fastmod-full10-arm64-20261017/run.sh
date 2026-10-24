#!/usr/bin/env bash
set -uo pipefail

RESULT_DIR=/home/jetson/src/results/uprobe/hash-fastmod-full10-arm64-20261017
SOURCE_DIR=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
RUNNER_ROOT=/tmp/bpftime-handler-fast-runner/bpftime
BUILD_DIR="$RUNNER_ROOT/build"
BPFTIMETOOL="$BUILD_DIR/tools/bpftimetool/bpftimetool"

archive_results()
{
	status=$?
	printf '%s\n' "$status" >"$RESULT_DIR/exit-status.txt"
	date --iso-8601=seconds >"$RESULT_DIR/finished-at.txt"
	sudo -n "$BPFTIMETOOL" remove >>"$RESULT_DIR/cleanup.log" 2>&1 || true
	if [[ $status -eq 0 ]]; then
		cp "$RUNNER_ROOT/benchmark/uprobe/benchmark-output.json" "$RESULT_DIR/"
		cp "$RUNNER_ROOT/benchmark/uprobe/results.md" "$RESULT_DIR/"
		cp "$RUNNER_ROOT/benchmark/uprobe/benchmark-logs.txt" "$RESULT_DIR/"
	fi
	exit "$status"
}
trap archive_results EXIT

date --iso-8601=seconds >"$RESULT_DIR/started-at.txt"
git -C "$SOURCE_DIR" rev-parse HEAD >"$RESULT_DIR/base-commit.txt"
git -C "$SOURCE_DIR" status --short >"$RESULT_DIR/source-status.txt"
git -C "$SOURCE_DIR" diff -- runtime >"$RESULT_DIR/source.diff"
sha256sum "$BUILD_DIR/runtime/agent/libbpftime-agent.so" \
	"$BUILD_DIR/runtime/syscall-server/libbpftime-syscall-server.so" \
	>"$RESULT_DIR/runtime-sha256.txt"
sudo -n /usr/sbin/nvpmodel -q >"$RESULT_DIR/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks --show >"$RESULT_DIR/jetson-clocks-before.txt" 2>&1
sudo -n "$BPFTIMETOOL" remove >"$RESULT_DIR/cleanup.log" 2>&1 || true

cd "$RUNNER_ROOT"
sudo -n taskset -c 5 env PYTHONUNBUFFERED=1 \
	BPFTIME_LOG_OUTPUT=console \
	/usr/bin/python3 -u benchmark/uprobe/benchmark.py --iter 10 \
	2>&1 | tee "$RESULT_DIR/runner.log"
exit "${PIPESTATUS[0]}"
