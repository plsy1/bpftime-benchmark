#!/bin/bash
# step0 e7-llama: analysis smoke on fabricated loader outputs (no GPU, ~2 s).
# Scenarios and expected verdicts come from smoke/make_fake_e7.py
# (expected.tsv); prints PASS/FAIL per scenario and exits 1 on any FAIL.
# Also checks check_ptx.py on the kept control SASS of a real run when one is
# given (CTRL_RUN=<run dir>): the real dump passes, a dump without helper
# 520's VOTE fails, and one without the probe's ATOM.ADD fails.
set -u
E=/home/jetson/src/bpftime-gpu-work/step0/e7-llama
O=$E/smoke/analysis
rm -rf $O && mkdir -p $O
python3 -I $E/smoke/make_fake_e7.py $O || exit 2
fail=0
while IFS=$'\t' read -r sc ov cr; do
  python3 -I $E/analyze.py $O/$sc > $O/$sc/stdout.txt 2>&1
  got_ov=$(grep -m1 '^overall:' $O/$sc/summary.txt | cut -c10-)
  got_cr=$(grep -m1 '^chunk_rule:' $O/$sc/summary.txt | cut -c13-)
  case "$got_ov" in "$ov"*) a=1;; *) a=0;; esac
  case "$got_cr" in "$cr"*) b=1;; *) b=0;; esac
  # no interpretation lists when the gate fails
  c=1
  if [ "$cr" = NONE ]; then
    python3 -I -c 'import json,sys; v=json.load(open(sys.argv[1]))["verdicts"]; sys.exit(0 if not v["full_warp_kernels"] and not v["low_mean_kernels"] and v["largest_C_with_unused_share_le_10pct_on_every_valid_kernel"] is None else 1)' $O/$sc/summary.json || c=0
  fi
  if [ $a = 1 ] && [ $b = 1 ] && [ $c = 1 ]; then r=PASS; else r=FAIL; fail=1; fi
  echo "$r $sc: overall='${got_ov:0:90}' chunk_rule='${got_cr:0:40}'"
done < $O/expected.tsv
if [ -n "${CTRL_RUN:-}" ] && [ -f "$CTRL_RUN/ptxcheck.sass" ]; then
  T=$O/sass && mkdir -p $T
  P=$CTRL_RUN/ptxcheck.sm_87.ptx
  sed -E 's/VOTEU?\.ANY (UR[0-9]+), UPT, PT/NOP/' $CTRL_RUN/ptxcheck.sass > $T/novote.sass
  sed -E 's/ATOM\.E\.ADD\.64/ATOM.E.EXCH.64/' $CTRL_RUN/ptxcheck.sass > $T/noadd.sass
  for c in real:$CTRL_RUN/ptxcheck.sass:0 novote:$T/novote.sass:1 noadd:$T/noadd.sass:1; do
    n=${c%%:*}; rest=${c#*:}; f=${rest%:*}; want=${rest##*:}
    python3 -I $E/check_ptx.py $P e7ctrl_partial $T/$n --sass $f > /dev/null; rc=$?
    [ $rc = $want ] && r=PASS || { r=FAIL; fail=1; }
    echo "$r check_ptx $n: rc=$rc (want $want)"
  done
fi
exit $fail
