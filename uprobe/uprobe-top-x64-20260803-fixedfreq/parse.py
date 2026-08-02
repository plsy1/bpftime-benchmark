#!/usr/bin/env python3
import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RAW = ROOT / "raw"
CASES = (
    "__bench_uprobe",
    "__bench_array_map_lookup",
    "__bench_array_map_update",
    "__bench_hash_map_lookup",
    "__bench_hash_map_update",
)
PATTERN = re.compile(
    r"Benchmarking (\S+) in thread 1\nAverage time usage ([0-9.]+) ns"
)


def read_run(path):
    values = dict(PATTERN.findall(path.read_text()))
    missing = set(CASES) - values.keys()
    if missing:
        raise RuntimeError(f"{path}: missing {sorted(missing)}")
    return {case: float(values[case]) for case in CASES}


rows = []
derived = []
for environment in ("kernel", "bpftime"):
    runs = [read_run(RAW / f"{environment}-run-{i}.txt") for i in range(1, 6)]
    for case in CASES:
        samples = [run[case] for run in runs]
        rows.append(
            {
                "environment": environment,
                "case": case,
                **{f"run{i}_ns": value for i, value in enumerate(samples, 1)},
                "mean_ns": statistics.mean(samples),
                "median_ns": statistics.median(samples),
                "stdev_ns": statistics.stdev(samples),
                "min_ns": min(samples),
                "max_ns": max(samples),
            }
        )
    empty = [run["__bench_uprobe"] for run in runs]
    for case in CASES[1:]:
        net = [(run[case] - base) / 1000 for run, base in zip(runs, empty)]
        derived.append(
            {
                "environment": environment,
                "case": case,
                **{f"run{i}_net_ns_per_helper": value for i, value in enumerate(net, 1)},
                "mean_net_ns_per_helper": statistics.mean(net),
                "median_net_ns_per_helper": statistics.median(net),
                "stdev_net_ns_per_helper": statistics.stdev(net),
                "min_net_ns_per_helper": min(net),
                "max_net_ns_per_helper": max(net),
            }
        )

by_key = {(row["environment"], row["case"]): row for row in derived}
comparison = []
for case in CASES[1:]:
    kernel = by_key[("kernel", case)]["median_net_ns_per_helper"]
    bpftime = by_key[("bpftime", case)]["median_net_ns_per_helper"]
    comparison.append(
        {
            "case": case,
            "kernel_median_net_ns_per_helper": kernel,
            "bpftime_median_net_ns_per_helper": bpftime,
            "bpftime_minus_kernel_ns_per_helper": bpftime - kernel,
            "bpftime_over_kernel": bpftime / kernel,
            "lower_cost_path": "bpftime" if bpftime < kernel else "kernel",
        }
    )

for name, data in (
    ("summary.csv", rows),
    ("net-helper.csv", derived),
    ("comparison.csv", comparison),
):
    with (ROOT / name).open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=data[0].keys(), lineterminator="\n")
        writer.writeheader()
        writer.writerows(data)
