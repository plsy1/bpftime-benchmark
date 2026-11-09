#!/bin/bash
# fix-matrix.sh PREFIX [CASE...]
#   Reruns the examples-20261109 matrix with build-cuda (run.sh, one segment per run) and checks
#   each case's expected result after the fixes: probe events, the victim's own output, the
#   loader's exit status. Prints one line per case: PASS/FAIL, what was expected, what was seen.
#   Runs go to runs/PREFIX-<case>. With CASE arguments only those cases run.
#   Env passed through: SPDLOG_LEVEL (default info); EXTRA_VIC_ENV='K=V ...' added to every victim.
set -u
W=/home/jetson/src/bpftime-gpu-work
V=$W/benchmark/gpu/workload
E=$W/example/gpu
P=${1:?prefix}; shift
export SPDLOG_LEVEL=${SPDLOG_LEVEL:-info}
SUMMARY=$W/runs/$P-summary.txt
: > "$SUMMARY"

want() { [ ${#SEL[@]} -eq 0 ] && return 0; for c in "${SEL[@]}"; do [ "$c" = "$1" ] && return 0; done; return 1; }
SEL=("$@")

# run_case NAME EXDIR SRV_ENV SCMD VIC_ENV VCMD [NO_SMARCH]
run_case() { # ... [NO_SMARCH] [VICTIM_TIMEOUT_S]
  local name=$1 exdir=$2 senv=$3 scmd=$4 venv=$5 vcmd=$6 nosm=${7:-} vt=${8:-60}
  SRV_ENV="$senv" VIC_ENV="$venv ${EXTRA_VIC_ENV:-}" NO_SMARCH="$nosm" timeout 400 "$W/run.sh" "$P-$name" "$exdir" "$scmd" "$vcmd" 90 "$vt" > "$W/runs/$P-$name.runsh.txt" 2>&1
}
O() { echo "$W/runs/$P-$1"; }
events() { grep -o 'Total events collected: [0-9]*' "$(O $1)/server.log" | tail -1 | awk '{print $4}'; }
c1() { grep -o 'C\[1\] = [0-9.]*' "$(O $1)/client.log" | tail -1 | awk '{print $3}'; }
src() { grep -o 'server_rc=[0-9]*' "$(O $1)/meta.txt" | tail -1 | cut -d= -f2; }
crc() { grep -o 'client_rc=[0-9]*' "$(O $1)/meta.txt" | tail -1 | cut -d= -f2; }
globs() { grep -o 'g via pointer = [0-9]*, g via symbol = [0-9]*, managed m = [0-9]*' "$(O $1)/client.log" | tail -1 | grep -o '[0-9]*' | tr '\n' ' ' | sed 's/ $//'; }
report() { # NAME PASS(0/1) EXPECTED SEEN
  local r=FAIL; [ "$2" = 1 ] && r=PASS
  printf '%-22s %s  expected: %s | seen: %s\n' "$1" "$r" "$3" "$4" | tee -a "$SUMMARY"
}
krs() { # NAME VICTIM-CMD [NO_SMARCH] -- kernelretsnoop: 5 threads x 4 timed launches = 20 events
  local n=$1
  want "$n" || return 0
  run_case "$n" "$E/kernelretsnoop" 'BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16' ./kernelretsnoop '' "$2" "${3:-}"
  local ev=$(events $n) c=$(c1 $n) s=$(src $n) k=$(crc $n)
  local ok=0; [ "$ev" = 20 ] && [ "$c" = 3.00 ] && [ "$s" = 0 ] && [ "$k" = 0 ] && ok=1
  report "$n" $ok "20 events, C[1] 3.00, rc 0/0" "${ev:-none} events, C[1] ${c:-none}, rc ${k:-?}/${s:-?}"
}
krs krs-legacy "$V/vec_add_legacy 5 3 5 1"
krs krs-default "$V/vec_add 5 3 5 1"
krs krs-stripped "$V/vec_add_legacy_stripped 5 3 5 1"
krs krs-fs "$V/vec_add_legacy_fs 5 3 5 1"
krs krs-static-legacy "$V/vec_add_legacy_static 5 3 5 1"
krs krs-static-default "$V/vec_add_static 5 3 5 1"
krs krs-autodetect "$V/vec_add_legacy 5 3 5 1" 1

glob() { # NAME VICTIM: kernelretsnoop on vectorAdd<<<1,5>>> once = 5 events; globals 5 5 5
  local n=$1
  want "$n" || return 0
  run_case "$n" "$E/kernelretsnoop" 'BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16' ./kernelretsnoop '' "$2"
  local ev=$(events $n) g=$(globs $n) s=$(src $n)
  local ok=0; [ "$ev" = 5 ] && [ "$g" = "5 5 5" ] && [ "$s" = 0 ] && ok=1
  report "$n" $ok "5 events, globals 5 5 5, server rc 0" "${ev:-none} events, globals ${g:-none}, server rc ${s:-?}"
}
glob glob-shared "$W/gaps/globals_legacy"
glob glob-static "$W/gaps/subst/globals_legacy_static"

if want hmt-legacy; then # kprobe + kretprobe, HOST maps, printk per thread
  n=hmt-legacy
  run_case $n "$E/host_map_test" 'BPFTIME_SHM_MEMORY_MB=64' ./host_map_test 'BPFTIME_TRACE_PRINTK_OUTPUT=stdout' "$V/vec_add_legacy 10 5 10 1"
  c=$(c1 $n); s=$(src $n)
  calls=$(grep -o 'key=0  (call_count): total=[0-9]* (from [0-9]* threads)' "$(O $n)/server.log" | tail -1)
  thr=$(grep -o 'Thread ([0-9]*,0,0) entered kernel' "$(O $n)/client.log" | sort -u | wc -l)
  ok=0; [ "$c" = 3.00 ] && [ "$s" = 0 ] && echo "$calls" | grep -q 'total=60 (from 10 threads)' && ok=1
  report $n $ok "C[1] 3.00, call_count 60 from 10 threads, server rc 0" "C[1] ${c:-none}, ${calls:-no call_count}, rc ${s:-?}"
  ok=0; [ "$thr" = 10 ] && ok=1
  report $n-printk $ok "printk names 10 threads" "$thr threads"
fi

if want cc-legacy; then # kprobe + kretprobe, CPU hash maps through host calls, printk
  n=cc-legacy
  # printk's x is the block index: 10 blocks of 1 thread (one block of 10 prints x=0 only)
  run_case $n "$E/cuda-counter" 'BPFTIME_SHM_MEMORY_MB=64' ./cuda_probe 'BPFTIME_TRACE_PRINTK_OUTPUT=stdout' "$V/vec_add_legacy 10 5 1 10"
  c=$(c1 $n); s=$(src $n)
  xs=$(grep -o 'Exited (with tsp) _Z9vectorAddPKfS0_Pf x=[0-9]*' "$(O $n)/client.log" | sort -u | wc -l)
  ok=0; [ "$c" = 3.00 ] && [ "$s" = 0 ] && ok=1
  report $n $ok "C[1] 3.00, server rc 0" "C[1] ${c:-none}, rc ${s:-?}"
  ok=0; [ "$xs" = 10 ] && ok=1
  report $n-printk $ok "printk names x=0..9" "$xs distinct x"
fi

if want ts-legacy; then # per-thread keys through host calls
  n=ts-legacy
  run_case $n "$E/threadscheduling" 'BPFTIME_SHM_MEMORY_MB=64' ./threadscheduling '' "$V/vec_add_legacy 10 5 10 1"
  c=$(c1 $n); s=$(src $n)
  last=$(awk '/Thread-to-Hardware Mapping Samples/{buf=""} {buf=buf"\n"$0} END{print buf}' "$(O $n)/server.log" | grep -o '│  ( *[0-9]*, *[0-9]*, *[0-9]*)   │  ( *[0-9]*, *[0-9]*, *[0-9]*)' | sort -u | wc -l)
  ok=0; [ "$c" = 3.00 ] && [ "$s" = 0 ] && [ "$last" = 10 ] && ok=1
  report $n $ok "C[1] 3.00, 10 threads mapped, server rc 0" "C[1] ${c:-none}, $last threads, rc ${s:-?}"
fi

cg() { # NAME VICTIM: 1 <<<>>> launch + 5 graph launches = calls 6
  local n=$1
  want "$n" || return 0
  run_case "$n" "$E/cudagraph" 'BPFTIME_SHM_MEMORY_MB=64' ./cuda_probe 'BPFTIME_CUDAGRAPH_ITERS=5' "$2"
  local calls=$(grep -o 'calls: [0-9]*' "$(O $n)/server.log" | tail -1 | awk '{print $2}') s=$(src $n)
  local ok=0; [ "$calls" = 6 ] && [ "$s" = 0 ] && ok=1
  report "$n" $ok "calls 6, server rc 0" "calls ${calls:-none}, rc ${s:-?}"
}
cg cg-default ./vec_add_graph
cg cg-legacy ./vec_add_graph_legacy

if want kt-legacy; then # thread 0 writes one ringbuf event per launch; one launch a second, killed at 5 s
  n=kt-legacy
  run_case $n "$E/kernel_trace" 'BPFTIME_SHM_MEMORY_MB=256 BPFTIME_MAP_GPU_THREAD_COUNT=4096' ./kernel_trace '' ./vec_add_legacy '' 5
  ev=$(grep -o 'total events: [0-9]*' "$(O $n)/server.log" | tail -1 | awk '{print $3}'); s=$(src $n)
  ok=0; { [ "$ev" = 4 ] || [ "$ev" = 5 ]; } && [ "$s" = 0 ] && ok=1
  report $n $ok "4-5 events (one a second for 5 s), server rc 0" "${ev:-none} events, rc ${s:-?}"
fi

if want threadhist; then # map 1502 (CUDA IPC on discrete GPUs)
  n=threadhist
  # the victim launches 512 x 1024 threads; the map needs a slot for each
  # (beyond them the device wrote past the map: results gpu/fixes-20261109)
  run_case $n "$E/threadhist" 'BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=524288' ./threadhist '' ./vec_add
  t=$(grep -o 'Thread [0-9]*: [1-9][0-9]*' "$(O $n)/server.log" | awk '{print $2}' | sort -u | wc -l); s=$(src $n)
  ok=0; [ "$t" -ge 1 ] && [ "$s" = 0 ] && ok=1
  report $n $ok "threads with counts, server rc 0" "$t threads with counts, rc ${s:-?}"
fi
if want threadhist-small; then # map 1502 with the default thread count: threads beyond it are refused
  n=threadhist-small
  run_case $n "$E/threadhist" 'BPFTIME_SHM_MEMORY_MB=64' ./threadhist '' ./vec_add
  t=$(grep -o 'Thread [0-9]*: [1-9][0-9]*' "$(O $n)/server.log" | awk '{print $2}' | sort -u | wc -l); s=$(src $n); k=$(crc $n)
  w=$(grep -c 'is outside map' "$(O $n)/client.log")
  # the victim launches forever: 124 = still running when the harness stopped it
  ok=0; [ "$t" -ge 1 ] && [ "$s" = 0 ] && [ "$k" = 124 ] && [ "$w" = 1 ] && ok=1
  report $n $ok "threads with counts, 1 warning, rc 124/0" "$t threads with counts, $w warnings, rc ${k:-?}/${s:-?}"
fi
echo "summary: $SUMMARY"
