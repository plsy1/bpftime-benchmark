#!/bin/bash
# step0 e9-hooks: analysis smoke on logs fabricated from the nsys census (no
# GPU, ~30 s). Each case states its expected verdict; prints PASS/FAIL per
# case and exits 1 on any FAIL. Writes smoke/fake-*.
set -u
E=/home/jetson/src/bpftime-gpu-work/step0/e9-hooks
S=/home/jetson/src/bpftime-gpu-work/llama/census
F=$E/smoke/fake
rm -rf $F && mkdir -p $F
cd $E
python3 -I make_fake_log.py - control-pass $F/control-pass > /dev/null
python3 -I make_fake_log.py - control-nonode $F/control-nonode > /dev/null
fail=0
# test mode control expected-verdict-prefix
while read -r t m ctl want; do
  out=$F/$t-$m-$ctl
  sq=$(python3 -I make_fake_log.py $S/$t-graph.sqlite $m $out | sed -n 's/^sqlite: //p')
  cargs=()
  [ "$ctl" != none ] && cargs=(--control $F/$ctl.tsv)
  python3 -I analyze_hooks.py --log $out.tsv --sqlite "$sq" --out $out --label fake-$t-$m-$ctl "${cargs[@]}" > /dev/null
  got=$(grep -m1 '^verdict:' $out.summary.txt | cut -c10-)
  case "$got" in "$want"*) r=PASS;; *) r=FAIL; fail=1;; esac
  echo "$r $t $m control=$ctl: want '$want', got '${got:0:150}'"
done <<'EOF'
pp512 complete control-pass YES
tg128 complete control-pass YES
pp512 graphs-off control-pass YES
tg128 graphs-off control-pass INCONCLUSIVE
pp512 unhooked-largest control-pass NO
tg128 capture-unhooked control-pass NO
pp512 blank-names control-pass INCONCLUSIVE
tg128 blank-names control-pass INCONCLUSIVE
tg128 unknown-capture control-pass INCONCLUSIVE
tg128 complete control-nonode INCONCLUSIVE
tg128 complete none INCONCLUSIVE
EOF
grep -h 'NO-direct' $F/tg128-capture-unhooked-control-pass.summary.txt | head -3 | cut -c1-150
rm -f $F/*.sqlite   # copies of the census, ~7 MB each
echo "smoke_analysis: $([ $fail = 0 ] && echo ALL PASS || echo FAILURES)"
exit $fail
