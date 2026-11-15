#!/bin/bash
# forms_check.sh OUTFILE: part (a), which injection forms ptxas accepts (CPU only).
# One line per form: name, ptxas rc, its error lines (if any).
set -u
D=$(cd "$(dirname "$0")/.." && pwd)
OUT=$1
: > "$OUT"
for f in "$D"/src/forms/*.ptx; do
	n=$(basename "$f" .ptx)
	log=$(/usr/local/cuda-13.2/bin/ptxas -arch=sm_87 -v "$f" -o "/dev/null" 2>&1)
	rc=$?
	echo "$n rc=$rc $(echo "$log" | grep -i 'error' | tr '\n' ' ')" >> "$OUT"
done
cat "$OUT"
