#!/usr/bin/env bash
# E0 item 2: dev 9ccda7e8 runtime unit tests, one process per test case (root, so tail-call cases run too).
R=/home/y/src/bpftime-x86-runs/paper-20261124; B=/home/y/src/bpftime-paper/dev-9ccda7e8/build-unittest
T=$(find $B -name bpftime_runtime_tests -type f -perm -u+x | head -1)
O=$R/raw/E0-x86-unittests; mkdir -p $O
cd $(dirname $T)
$T --list-tests --verbosity quiet 2>/dev/null | grep -v -e "^\[20" -e "^$" > $O/cases.txt
pass=0; fail=0
while IFS= read -r c; do
  if sudo -n timeout 300 $T "$c" > $O/last.log 2>&1; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL: $c" >> $O/failures.txt; cp $O/last.log "$O/fail-$fail.log"; fi
done < $O/cases.txt
echo "cases=$(wc -l < $O/cases.txt) pass=$pass fail=$fail" | tee $O/summary.txt
sudo -n chown -R $(id -u):$(id -g) $O
