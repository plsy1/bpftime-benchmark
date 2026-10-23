#!/usr/bin/env bash
# Fair-scope payload sweep with the official draw_figture.py sizes (16b..256kb):
# run_fair.py per size, 5 reps, legs rotated, inside container ssl-nginx-2x2.
set -euo pipefail
LEGS=baseline,kernel,official,db3d475f
for size in 16b:16 1kb:1024 2kb:2048 4kb:4096 16kb:16384 128kb:131072 256kb:262144; do
    name=${size%%:*}; bytes=${size#*:}
    sudo -n docker exec ssl-nginx-2x2 python3 /bench/run_fair.py --payload "$bytes" --reps 5 \
        --legs "$LEGS" --out "/bench/sweep-$name.json" > "$(dirname "$0")/sweep-$name.log" 2>&1
    echo "$(date +%H:%M:%S) $name done"
done
