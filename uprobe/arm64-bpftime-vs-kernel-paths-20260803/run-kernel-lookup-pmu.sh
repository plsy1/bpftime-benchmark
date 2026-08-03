#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
for operation in array_lookup hash_lookup; do
    for metric in cycles instructions; do
        "$DIR/profile-kernel-lookup-pmu.sh" "$operation" "$metric"
    done
done
