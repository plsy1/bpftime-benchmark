#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
"$DIR/run-wall-arm64.sh"
for metric in cycles instructions l1d_loads llc_misses; do
    "$DIR/profile-arm64.sh" "$metric"
done
python3 "$DIR/summarize.py"
