#!/usr/bin/env bash
# (copy of reviews-20261105/fixes/regression/each.sh, via improve-20261106) Every runtime test case in a process of its own, as root, bounded at 300 s
# each, with a private global segment; prints failures. $1: the test binary.
set -u
T=$1
OUT=$(dirname "$0")
SHM=bpftime_each_$$
"$T" --list-tests --verbosity quiet 2>/dev/null | grep -v "^\[20" | grep -v "^$" > $OUT/cases.txt
: > $OUT/each.log
n=0; failed=0
while IFS= read -r name; do
  n=$((n+1))
  spec=$(printf '%s' "$name" | sed 's/,/\\,/g; s/\[/\\[/g')
  sudo -n timeout 300 env BPFTIME_GLOBAL_SHM_NAME=$SHM "$T" "$spec" > $OUT/case.out 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then
    failed=$((failed+1))
    echo "FAIL rc=$rc: $name" >> $OUT/each.log
    tail -25 $OUT/case.out >> $OUT/each.log
  fi
  sudo -n rm -f /dev/shm/$SHM
done < $OUT/cases.txt
rm -f $OUT/case.out
echo "cases $n failed $failed" | tee -a $OUT/each.log
