#!/usr/bin/env python3
"""Print the ten-operation comparison from the archived full-suite results."""

import json
from pathlib import Path

ROOT = Path("/home/jetson/src/results/uprobe")
sources = {
    "initial": ROOT / "uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json",
    "previous": ROOT / "percpu-hash-flatten-full10-arm64-20261018/benchmark-output.json",
    "current": ROOT / "ordinary-hash-loop-20261018/full10/benchmark-output.json",
}
data = {label: json.loads(path.read_text()) for label, path in sources.items()}
operations = (
    ("Ordinary array lookup", "array_map_lookup", "Optimized"),
    ("Ordinary array update", "array_map_update", "Optimized"),
    ("Ordinary hash lookup", "hash_map_lookup", "Optimized"),
    ("Ordinary hash update", "hash_map_update", "Already faster"),
    ("Ordinary hash delete", "hash_map_delete", "Already faster"),
    ("Per-CPU array lookup", "per_cpu_array_map_lookup", "Optimized"),
    ("Per-CPU array update", "per_cpu_array_map_update", "Optimized"),
    ("Per-CPU hash lookup", "per_cpu_hash_map_lookup", "Optimized"),
    ("Per-CPU hash update", "per_cpu_hash_map_update", "Optimized"),
    ("Per-CPU hash delete", "per_cpu_hash_map_delete", "Optimized"),
)

print("# Full ten-round map-operation comparison\n")
print("Raw average ns per invocation divided by 1000 helpers; lower is better.")
print("This differs from the control-subtracted short A/B measurements.\n")
print("| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |")
print("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|")
for name, suffix, status in operations:
    key = "__bench_" + suffix
    values = {label: run["userspace_uprobe"][key]["avg"] / 1000
              for label, run in data.items()}
    initial_kernel = data["initial"]["kernel_uprobe"][key]["avg"] / 1000
    kernel = data["current"]["kernel_uprobe"][key]["avg"] / 1000
    initial, previous, current = (values[label] for label in ("initial", "previous", "current"))
    print(f"| {name} | {status} | {initial:.2f} | {initial / initial_kernel:.2f}× | "
          f"{previous:.2f} | {current:.2f} | {kernel:.2f} | {current / kernel:.2f}× | "
          f"{100 * (current / initial - 1):+.1f}% | {100 * (current / previous - 1):+.1f}% |")
print("\nSources:")
for label, path in sources.items():
    target = Path("../..") / path.relative_to(ROOT)
    print(f"\n- {label}: [{path.parent.name}]({target.as_posix()})")
