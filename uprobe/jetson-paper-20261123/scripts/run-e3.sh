#!/usr/bin/env bash
# E3 full10 microbenchmark (Jetson), structure of the x86 run-e3.sh:
#   bookend 0 -> run1 A -> run2 B -> bookend 2 -> run3 B -> run4 A -> bookend 4  (ABBA from the arguments)
# Usage: run-e3.sh <session-id> <arm> <arm> <arm> <arm>      e.g. J-E3-20261123 O D D O
# Runner per arm: runners/<arm>/bpftime (D-tree benchmark/, frozen test + uprobe, arm's agent,
# syscall server, embed and bpftimetool). Everything on cpu5 at 1.728 GHz (jetson_clocks).
set -euo pipefail
SID=${1:?session}; shift; ARMS=("$@"); [ ${#ARMS[@]} -eq 4 ]
R=$HOME/src/paper-runs-20261124
F=$R/raw/$SID; mkdir "$F"; LOG=$F/log; mkdir "$LOG"
CPU=5
CLAUDE_PIDS=()
finish()
{
    status=$?
    trap - EXIT
    sudo -n "$R/builds/D/bpftimetool" remove >>"$LOG/cleanup.log" 2>&1 || true
    for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 0-5 "$p" >/dev/null 2>&1 || true; done
    sudo -n chown -R "$(id -u):$(id -g)" "$F" "$R/runners" 2>/dev/null || true
    "$R/harness/check_env.sh" end "$LOG" >>"$LOG/check-end.txt" 2>&1 || { echo "end guard FAILED" >>"$LOG/warnings.txt"; status=98; }
    printf '%s\n' "$status" >"$LOG/exit-status.txt"
    date --iso-8601=seconds >"$LOG/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$LOG/started-at.txt"
printf '%s\n' "${ARMS[@]}" >"$LOG/order.txt"
"$R/harness/check_env.sh" start "$LOG" 130
(cd "$R/runners/D/bpftime/benchmark" && sha256sum --check --quiet "$R/runners/bench-sha256.txt")
for a in "${ARMS[@]}"; do
  B=$R/runners/$a/bpftime
  (cd "$B/benchmark" && sha256sum --check --quiet "$R/runners/bench-sha256.txt")
  for f in build/runtime/agent/libbpftime-agent.so build/runtime/syscall-server/libbpftime-syscall-server.so build/benchmark/simple-benchmark-with-embed-ebpf-calling; do
    [ "$(readlink -f "$B/$f")" = "$R/builds/$a/$(basename "$f")" ] || { echo "runner $a: $f not the frozen artifact"; exit 1; }
  done
  [ "$(sha256sum <"$B/kernel_bookend.py" | cut -d' ' -f1)" = 63d7542c77899d45bebcf59af32325db0a6395fc0b3a5039b0f11825dd072be3 ]
done
# this Claude session's process tree off cpu5
p=$$
while [ "$p" -gt 1 ]; do
    c=$(cat /proc/$p/comm 2>/dev/null || true)
    case $c in claude|node|2.1.*|ccd-cli*) CLAUDE_PIDS+=("$p");; esac
    p=$(awk '{print $4}' /proc/$p/stat)
done
for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 2 "$p" >>"$LOG/claude-taskset.txt" 2>&1 || true; done
taskset -pc 2 $$ >/dev/null

bookend() { # n
  echo "bookend $1 start $(date +%T)" >>"$LOG/steps.txt"
  (cd "$R/runners/${ARMS[0]}/bpftime" && sudo -n timeout 1800 taskset -c $CPU env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 \
      python3 -u kernel_bookend.py 10 "$F/bookend-$1.json") >"$LOG/bookend-$1.log" 2>&1
  echo "bookend $1 end   $(date +%T)" >>"$LOG/steps.txt"
}
fullrun() { # n arm
  local n=$1 a=$2 B=$R/runners/$2/bpftime
  sudo -n "$R/builds/$a/bpftimetool" remove >/dev/null 2>&1 || true
  test -z "$(find /dev/shm -mindepth 1 -name 'bpftime*')"
  date --iso-8601=seconds >"$F/run$n-$a.started-at"
  echo "run$n $a start $(date +%T)" >>"$LOG/steps.txt"
  (cd "$B" && sudo -n timeout 3600 taskset -c $CPU env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
      python3 -u benchmark/uprobe/benchmark.py --iter 10) >"$LOG/run$n-$a.log" 2>&1
  echo "run$n $a end   $(date +%T)" >>"$LOG/steps.txt"
  for x in benchmark-output.json results.md benchmark-logs.txt; do
    test "$B/benchmark/uprobe/$x" -nt "$F/run$n-$a.started-at"
    cp "$B/benchmark/uprobe/$x" "$F/run$n-$a-$x"
  done
  sudo -n "$R/builds/$a/bpftimetool" remove >/dev/null 2>&1 || true
  python3 -I - "$F/run$n-$a-benchmark-output.json" <<'PY'
import json, math, sys
r = json.load(open(sys.argv[1]))
for g, c in (('kernel_uprobe', 17), ('userspace_uprobe', 17), ('embed', 1)):
    assert len(r[g]) == c, (g, len(r[g]))
    for k, v in r[g].items():
        assert len(v['raw_values']) == 10, (g, k)
        assert all(math.isfinite(x) and x > 0 for x in v['raw_values']), (g, k)
print('validation ok', sys.argv[1])
PY
}
bookend 0
fullrun 1 "${ARMS[0]}"; fullrun 2 "${ARMS[1]}"
bookend 2
fullrun 3 "${ARMS[2]}"; fullrun 4 "${ARMS[3]}"
bookend 4
(cd "$R/runners/D/bpftime/benchmark" && sha256sum --check --quiet "$R/runners/bench-sha256.txt")
echo "E3 done"
