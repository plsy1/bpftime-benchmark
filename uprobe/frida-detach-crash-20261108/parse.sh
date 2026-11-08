#!/bin/bash
# parse.sh <label>: one line per run of logs/<label>: rc-independent view of
# which cases failed (child status) and the first DIAG line per crash.
for f in $(ls logs/$1/run*.log | sort -V); do
  r=$(basename $f .log)
  awk -v r=$r '/^-----/{getline n; if (n !~ /^[-\/.]/) name=n} /FAILED:/{f=name} /child status [0-9]+/{match($0,/child status [0-9]+/); print r"\tFAIL\t"f"\t"substr($0,RSTART,RLENGTH)} /^DIAG/{print r"\tDIAG\t"$0}' "$f"
done
