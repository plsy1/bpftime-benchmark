#!/bin/bash
# step0 e5, compile-only part: which atomic forms compile at which -mcpu, and
# which BPF instruction each becomes (decoded from the bytes).
# Output: results/compile-only/matrix.csv, matrix.txt, per-case objdumps, and
# the objdumps of the three e5 programs as built (default -mcpu).
# No GPU, no bpftime. Wall: ~20 s.
set -u
D=/home/jetson/src/bpftime-gpu-work/step0/e5-atomics
O=$D/results/compile-only
mkdir -p "$O/objdump"
cd "$D" || exit 2
CASES="fadd64_used fadd64_unused fadd32_used fadd32_unused atomic_add_fetch64_used cas64 cas32 cas64_loop xchg64"
echo "compiler,mcpu,case,rc,error,atomics" > "$O/matrix.csv"
for cc in clang-15 clang-16; do
  command -v $cc >/dev/null || continue
  od=llvm-objdump-${cc#clang-}
  for m in default v1 v2 v3; do
    MF=""; [ $m != default ] && MF="-mcpu=$m"
    for c in $CASES; do
      o=$O/objdump/$cc.$m.$c
      timeout -k 5 60 $cc -O2 -g -target bpf $MF -DCASE_$c -c atomics_cases.bpf.c -o $o.o > $o.err 2>&1
      rc=$?
      err=$(grep -m1 -E 'error' $o.err | sed 's/^[^:]*:[0-9]*:[0-9]*: //; s/,/;/g; s/"//g')
      at=""
      if [ $rc = 0 ]; then
        $od -d $o.o > $o.dis 2>&1
        at=$(python3 -I "$D/decode_bpf_atomics.py" $o.dis | python3 -I -c 'import json,sys; print(" ".join("%s%d%s(%s)" % (a["op"], a["width"], "+fetch" if a["fetch"] else "", a["imm"]) for a in json.load(sys.stdin)))')
      fi
      echo "$cc,$m,$c,$rc,\"$err\",\"$at\"" >> "$O/matrix.csv"
    done
  done
done
# The e5 programs as built by the Makefile (clang-15, default -mcpu)
timeout 300 make e5_fadd.bpf.o e5_plain.bpf.o e5_cas.bpf.o > /dev/null 2>&1   # incremental
for p in fadd plain cas; do
  llvm-objdump-15 -d e5_$p.bpf.o > "$O/e5_$p.dis" 2>&1
  python3 -I "$D/decode_bpf_atomics.py" "$O/e5_$p.dis" > "$O/e5_$p.atomics.json"
done
# Which -mcpu tool Makefiles use (read, not run)
{
  echo "== bpftime example Makefiles (dev tree) with -mcpu:"
  grep -rl -- '-mcpu' /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate/example/*/Makefile /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate/example/gpu/*/Makefile 2>/dev/null || echo "(none)"
  echo "== bpftime example BPF compile rule (threadhist):"
  grep -n -- '-target bpf' /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate/example/gpu/threadhist/Makefile
  echo "== libbpf-tools (bcc v0.37.0) Makefile:"
  grep -n -- 'mcpu\|BPFCFLAGS\b\|-target bpf' /home/jetson/src/survey-sources/bcc-v0.37.0/libbpf-tools/Makefile
  echo "== clang-15 default BPF cpu:"
  clang-15 -target bpf -print-supported-cpus 2>&1 | grep -A8 'Available CPUs'
} > "$O/mcpu-in-makefiles.txt" 2>&1
python3 -I - "$O/matrix.csv" > "$O/matrix.txt" <<'EOF'
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
for r in rows:
    res = r['atomics'] if r['rc'] == '0' else 'FAIL: ' + r['error'][:70]
    print('%-9s %-8s %-24s %s' % (r['compiler'], r['mcpu'], r['case'], res))
EOF
cat "$O/matrix.txt"
