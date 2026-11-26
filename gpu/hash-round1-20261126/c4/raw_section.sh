#!/bin/bash
# Print the raw-output appendix of C4-report.md from logs/ (verbatim excerpts).
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D/logs"
echo "## Appendix: raw outputs"
echo
echo "Verbatim from \`logs/\`. Rule runs in full; other runs: the header, trial 0's summary lines and the final summary (each full log is in \`logs/<name>.log\`)."
echo
for n in r1-proto-128 r2-proto-48 r3-ctl1-plainclaim r4-ctl2-nobound-48 r5-ctl3-relaxpub; do
  echo "### $n (full)"
  echo '```'
  cat "$n.log"
  echo '```'
  echo
done
for n in r6-proto-128-reader r7-ctl3-relaxpub-reader r8-proto-128-prefill16 r9-proto-64 \
         t1-proto-128-x1000 t2-proto-48-x1000 t3-proto-64-x1000 t4-proto-128-prefill16-x300 \
         t5-proto-128-reader-x300 t6-relaxpub-128-reader-x300 t7-relaxpub-128-x1000 \
         t8-plainclaim-128-x200 t9-nobound-48-x200 t10-plainclaim-64-x1000 t11-plainclaim-48-x1000 \
         t12-plainnobound-128-x200 t13-plainnobound-48-x200 t14-proto-nowait-128-x300; do
  echo "### $n"
  echo '```'
  sed -n '1,4p' "$n.log"
  grep -E '^(=== trial 0|dir:|dir shape|totals|branches|kernel span|reader: start|per launch|E2BIG after|violations:)' "$n.log" | head -11
  grep -E '^  note:' "$n.log" | head -4
  sed -n '/^=== summary/,$p' "$n.log"
  echo '```'
  echo
done
