#!/bin/bash
# partC/run.sh OUTDIR: POSIX-FD VMM export between two processes.
# Writes OUTDIR/partC/vmm.jsonl (one exporter, one importer and one parent
# line per variant, each preceded by a {"variant": ...} line) and rc.txt.
# 4 variants; expected wall about 30 s. Each bounded by timeout SECS + 70 s.
set -u
E3=$(cd "$(dirname "$0")/.." && pwd)
B=$E3/build
O=${1:?usage: partC/run.sh OUTDIR}/partC
mkdir -p "$O"
SECS=${SECS:-3}
: > "$O/rc.txt"
for v in plain:0 wt:0 sysfence:0 plain:1; do
  fill=${v%%:*}; rdma=${v#*:}
  name=fill-$fill-rdma$rdma
  printf '{"variant":"%s"}\n' "$name" >> "$O/vmm.jsonl"
  timeout -k 5 "$(awk -v s="$SECS" 'BEGIN{printf "%d", s + 70}')" "$B/vmmexport" --bytes 16777216 --seconds "$SECS" --update-us 2000 \
    --fill "$fill" --rdma "$rdma" --out "$O/vmm.jsonl" > "$O/$name.out" 2> "$O/$name.err"
  echo "$name rc=$?" | tee -a "$O/rc.txt"
done
