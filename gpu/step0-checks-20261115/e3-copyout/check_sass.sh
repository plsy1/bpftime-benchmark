#!/bin/bash
# check_sass.sh: confirm that what the tests measure survived compilation.
# Reads build/keep*/ (PTX from nvcc -keep, SASS from cuobjdump -sass) and the
# shim's dynamic symbol table. Prints one line per check; exit 1 if any fails.
set -u
E3=$(cd "$(dirname "$0")" && pwd)
B=$E3/build
fail=0
# section of a SASS dump for one function
fn() { awk -v f="$2" '/Function : /{on=($3==f)} on' "$1"; }
# count of a fixed string in text on stdin
cnt() { grep -cF -- "$1" || true; }
check() { # name, got, op, want
  if [ "$3" = ge ] && [ "$2" -ge "$4" ]; then r=ok; elif [ "$3" = eq ] && [ "$2" -eq "$4" ]; then r=ok; else r=FAIL; fail=1; fi
  printf '%-4s %-62s %s (want %s %s)\n' "$r" "$1" "$2" "$3" "$4"
}

A_PTX=$B/keepA/copyorder.ptx; A_SASS=$B/keepA/copyorder.sass
W=$(fn "$A_SASS" _Z6writer2WP)
check "A ptx: payload st.global.u64" "$(cnt 'st.global.u64' < "$A_PTX")" ge 1
check "A ptx: flag st.volatile.global.u64 (normal + reversed)" "$(cnt 'st.volatile.global.u64' < "$A_PTX")" ge 2
check "A ptx: __threadfence = membar.gl" "$(cnt 'membar.gl;' < "$A_PTX")" ge 3
check "A ptx: __syncwarp = bar.warp.sync" "$(cnt 'bar.warp.sync' < "$A_PTX")" ge 2
check "A ptx: stop word ld.volatile.global.u32" "$(cnt 'ld.volatile.global.u32' < "$A_PTX")" ge 1
check "A ptx: %globaltimer deadline" "$(cnt '%globaltimer' < "$A_PTX")" ge 2
check "A sass writer: payload STG.E.64 (weak)" "$(echo "$W" | grep -c 'STG.E.64 \[')" ge 1
check "A sass writer: flag STG.E.64.STRONG.SYS" "$(echo "$W" | cnt 'STG.E.64.STRONG.SYS')" ge 2
check "A sass writer: MEMBAR.SC.GPU" "$(echo "$W" | cnt 'MEMBAR.SC.GPU')" ge 3
check "A sass writer: stop load LDG.E.STRONG.SYS" "$(echo "$W" | cnt 'LDG.E.STRONG.SYS')" ge 1
check "A sass writer: SR_GLOBALTIMER" "$(echo "$W" | cnt 'SR_GLOBALTIMER')" ge 2
check "A sass writer: exit counter RED/ATOM" "$(echo "$W" | grep -cE 'RED\.E\.ADD|ATOMG?\.E\.ADD')" ge 1
# Order in the code (address order of the straight-line loop body; read with
# the dump): P = weak payload STG.E.64, M = MEMBAR.SC.GPU, F = flag
# STG.E.64.STRONG.SYS. Normal writer: the last F has an M between it and the
# last P before it. Reversed writer: the first F has an M between it and the
# first P after it. Prints 1 if both hold.
ORD=$(echo "$W" | awk '
  /STG\.E\.64\.STRONG\.SYS/ { s = s "F"; next }
  /STG\.E\.64 \[/ { s = s "P"; next }
  /MEMBAR\.SC\.GPU/ { s = s "M"; next }
  END {
    lf = 0; for (i = 1; i <= length(s); i++) if (substr(s, i, 1) == "F") lf = i
    ff = index(s, "F"); ok = (lf > 0 && ff > 0 && lf != ff)
    lp = 0; for (i = 1; i < lf; i++) if (substr(s, i, 1) == "P") lp = i
    if (ok) ok = (lp > 0 && index(substr(s, lp, lf - lp), "M") > 0)
    fp = 0; for (i = ff + 1; i <= length(s); i++) if (substr(s, i, 1) == "P") { fp = i; break }
    if (ok) ok = (fp > 0 && index(substr(s, ff, fp - ff), "M") > 0)
    print ok + 0; print s > "/dev/stderr"
  }' 2> "$B/keepA/writer.order.txt")
check "A sass writer: order P..M..F (normal), F..M..P (reversed)" "$ORD" eq 1

C_PTX=$B/keepC/vmmexport.ptx; C_SASS=$B/keepC/vmmexport.sass
F=$(fn "$C_SASS" _Z4fillPymyi)
S=$(fn "$C_SASS" _Z6samplePVKymPy)
check "C ptx: fill wt store st.global.wt.u64" "$(cnt 'st.global.wt.u64' < "$C_PTX")" ge 1
check "C ptx: fill plain store st.global.u64" "$(cnt 'st.global.u64' < "$C_PTX")" ge 1
check "C ptx: fill sysfence membar.sys" "$(cnt 'membar.sys' < "$C_PTX")" ge 1
check "C sass fill: STG.E.64 (plain)" "$(echo "$F" | grep -c 'STG.E.64 \[')" ge 1
check "C sass fill: STG.E.64.STRONG.SYS (st.wt lowers to this)" "$(echo "$F" | cnt 'STG.E.64.STRONG.SYS')" ge 1
check "C sass fill: MEMBAR.SC.SYS" "$(echo "$F" | cnt 'MEMBAR.SC.SYS')" ge 1
check "C sass sample: LDG.E.64.STRONG.SYS (volatile reads)" "$(echo "$S" | cnt 'LDG.E.64.STRONG.SYS')" ge 2

for s in cudaStreamBeginCapture cudaStreamEndCapture cudaGraphInstantiate cudaGraphExecUpdate cudaGraphLaunch; do
  check "B shim exports $s" "$(nm -D --defined-only "$B/libstep0_copier.so" | grep -c " T $s\$")" eq 1
done
L=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/libggml-cuda.so
for s in cudaStreamBeginCapture cudaStreamEndCapture cudaGraphInstantiate cudaGraphExecUpdate cudaGraphLaunch; do
  check "B llama imports $s dynamically" "$(nm -D --undefined-only "$L" | grep -c " U $s@")" eq 1
done
check "B captest links shared cudart" "$(ldd "$B/captest" | grep -c libcudart.so)" ge 1
exit $fail
