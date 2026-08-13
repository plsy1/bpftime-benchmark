#!/usr/bin/env python3
import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent
RESULTS = HERE.parent
ARRAY = RESULTS / "helper-map-closure-arm64-20260812"
HASH = RESULTS / "helper-hash-closure-arm64-20260812"


def rows(path):
    with path.open() as handle:
        return list(csv.DictReader(handle))


def index(path, key):
    return {tuple(row[name] for name in key): row for row in rows(path)}


array_top = index(ARRAY / "closure.csv", ("case",))
array_pmu = index(ARRAY / "pmu-closure.csv", ("metric", "case"))
hash_top = index(HASH / "closure.csv", ("case",))
hash_pmu = index(HASH / "pmu-closure.csv", ("metric", "case"))

top = []
for operation, source, pmu in (
    ("ordinary array lookup", array_top[("array_lookup",)], array_pmu),
    ("per-CPU array lookup", array_top[("percpu_array_lookup",)], array_pmu),
    ("ordinary array update", array_top[("array_update",)], array_pmu),
    ("per-CPU array update", array_top[("percpu_array_update",)], array_pmu),
    ("ordinary hash lookup", hash_top[("hash_lookup",)], hash_pmu),
    ("per-CPU hash lookup", hash_top[("percpu_hash_lookup",)], hash_pmu),
    ("ordinary hash update", hash_top[("hash_update",)], hash_pmu),
    ("per-CPU hash update", hash_top[("percpu_hash_update",)], hash_pmu),
):
    case = operation.replace("ordinary ", "").replace("per-CPU ", "percpu_").replace(" ", "_")
    top.append({
        "operation": operation,
        "kernel_ns_per_helper": float(source["kernel_mean"]),
        "bpftime_ns_per_helper": float(source["bpftime_mean"]),
        "gap_ns_per_helper": float(source["bpftime_minus_kernel"]),
        "gap_sample_sd": float(source["gap_sample_sd"]),
        "gap_cycles_per_helper": float(pmu[("cycles", case)]["bpftime_minus_kernel"]),
        "gap_instructions_per_helper": float(pmu[("instructions", case)]["bpftime_minus_kernel"]),
    })

with (HERE / "top-gap.csv").open("w", newline="") as handle:
    writer = csv.DictWriter(handle, fieldnames=top[0].keys())
    writer.writeheader()
    writer.writerows(top)

print("top-gap.csv generated")
