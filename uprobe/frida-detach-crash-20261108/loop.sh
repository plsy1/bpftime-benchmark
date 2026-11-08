#!/bin/bash
# loop.sh <label> <runs> <binary> [catch2 args...]
# Runs the test binary <runs> times (each bounded at 300 s), one log per run,
# and appends "label run rc diag..." to summary.tsv. Never runs inside the
# daily timer window (03:25-04:15 JST): it stops there.
set -u
L=$1; N=$2; BIN=$3; shift 3
R=/home/jetson/src/results/uprobe/frida-detach-crash-20261108
mkdir -p "$R/logs/$L"
for i in $(seq 1 "$N"); do
  hm=$(TZ=Asia/Tokyo date +%H%M)
  if [ "$hm" -ge 0325 ] && [ "$hm" -lt 0415 ]; then echo "stop: daily window at $hm"; exit 3; fi
  # Another session's measurements, builds or regressions go first.
  while ps -eo cmd | grep -E "go\.sh|measure_mt\.py|each\.sh|bpftime_runtime_tests|bpftime_[a-z_]*_tests|cmake --build|ab\.py|bpftrace|callloop|ctest" | grep -v grep | grep -v frida-detach-crash >/dev/null; do sleep 20; done
  log="$R/logs/$L/run$i.log"
  timeout -k 10 300 "$BIN" "$@" > "$log" 2>&1
  rc=$?
  # Catch2 console output: the case name follows a dashed line; a failure
  # prints "FAILED:" and its "child status N" message.
  cases=$(awk '/^-{20,}$/{getline n; if (n !~ /^[-\/.]/) name=n} /FAILED:/{f=name} /child status|fatal error/{print f" ["$0"]"}' "$log" | sed 's/  */ /g' | sort -u | tr '\n' ';')
  failed=$(grep -oE "child status [0-9]+" "$log" | tr '\n' ',')
  diag=$(grep -h "^DIAG" "$log" | head -3 | tr '\n' '|')
  printf '%s\t%d\t%d\t%s\t%s\t%s\n' "$L" "$i" "$rc" "$failed" "$cases" "$diag" >> "$R/summary.tsv"
  echo "$L run $i rc=$rc $failed $cases"
done
