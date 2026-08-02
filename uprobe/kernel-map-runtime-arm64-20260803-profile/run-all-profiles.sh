#!/usr/bin/env bash
set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
for metric in cycles instructions l1d_loads llc_misses; do
    echo "$(date --iso-8601=seconds) profiling $metric"
    "$HERE/profile-array-update-fixedfreq.sh" "$metric"
done
echo "$(date --iso-8601=seconds) all profiles completed"

