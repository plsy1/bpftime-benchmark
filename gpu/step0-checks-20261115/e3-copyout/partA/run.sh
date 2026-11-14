#!/bin/bash
# partA/run.sh OUTDIR: ordering matrix, writer-cap path check, DtoH latency
# and throughput. Writes OUTDIR/partA/{order,order-seqcap,bw}.jsonl and rc.txt.
# Expected wall: about 2 min (SECS=2). Total bound: 360 + 30 + 90 s.
set -u
E3=$(cd "$(dirname "$0")/.." && pwd)
B=$E3/build
O=${1:?usage: partA/run.sh OUTDIR}/partA
mkdir -p "$O"
SECS=${SECS:-2}
: > "$O/rc.txt"
# 2 areas x 2 chunk sizes x 6 layouts x 2 writers = 48 variants of SECS each
timeout -k 5 360 "$B/copyorder" order --seconds "$SECS" --areas 1M,16M --wpl 8,64 \
  --layouts after,before,farafter,farbefore,sepff,seppf --writers normal,reversed --out "$O/order.jsonl"
echo "order rc=$?" >> "$O/rc.txt"
# a tiny sequence cap: the writer must end by its cap, be relaunched and
# keep its sequence; checks the cap path and its counters
timeout -k 5 30 "$B/copyorder" order --seconds 0.5 --areas 1M --wpl 8 --layouts before \
  --writers normal --seq-cap 64 --out "$O/order-seqcap.jsonl"
echo "seqcap rc=$?" >> "$O/rc.txt"
# latency (200 copies, one at a time) and throughput (back to back), writer idle and running
timeout -k 5 90 "$B/copyorder" bw --sizes 64K,1M,16M --bw-writers none,running --lat-n 200 \
  --out "$O/bw.jsonl"
echo "bw rc=$?" >> "$O/rc.txt"
cat "$O/rc.txt"
