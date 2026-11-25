#!/usr/bin/env bash
# Re-run the unit-test cases whose names contain commas (Catch2 treats ',' as OR; escape it).
R=/home/y/src/bpftime-x86-runs/paper-20261124; O=$R/raw/E0-x86-unittests; B=/home/y/src/bpftime-paper/dev-9ccda7e8/build-unittest
until grep -q E11-DONE $R/logs/after-ut2-e11.out; do sleep 30; done
T=$(find $B -name bpftime_runtime_tests -type f -perm -u+x | head -1); cd $(dirname $T)
: > $O/rerun.txt
sed -n 's/^FAIL: //p' $O/failures.txt | while IFS= read -r c; do
  esc=${c//,/\\,}
  sudo -n timeout 300 $T "$esc" > $O/rerun-last.log 2>&1; rc=$?
  skipped=$(grep -c "SKIPPED" $O/rerun-last.log)
  ran=$(grep -oE "test cases?: [0-9]+" $O/rerun-last.log | tail -1)
  echo "rc=$rc skipped=$skipped [$ran] $c" >> $O/rerun.txt
done
sudo -n chown -R $(id -u):$(id -g) $O; cat $O/rerun.txt; echo RERUN-DONE
