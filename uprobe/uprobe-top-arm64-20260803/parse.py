#!/usr/bin/env python3
import csv
import math
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RAW = ROOT / "raw"
X64_ROOT = ROOT.parent / "uprobe-top-x64-20260802"
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


def write_csv(name, data):
    with (ROOT / name).open("w", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=data[0].keys(), lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(data)


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
        samples = [(run[case] - base) / 1000 for run, base in zip(runs, empty)]
        derived.append(
            {
                "environment": environment,
                "case": case,
                **{
                    f"run{i}_net_ns_per_helper": value
                    for i, value in enumerate(samples, 1)
                },
                "mean_net_ns_per_helper": statistics.mean(samples),
                "median_net_ns_per_helper": statistics.median(samples),
                "stdev_net_ns_per_helper": statistics.stdev(samples),
                "min_net_ns_per_helper": min(samples),
                "max_net_ns_per_helper": max(samples),
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

with (X64_ROOT / "comparison.csv").open(newline="") as source:
    x64_rows = {row["case"]: row for row in csv.DictReader(source)}
cross_arch = []
for arm in comparison:
    case = arm["case"]
    x64 = x64_rows[case]
    arm_kernel = arm["kernel_median_net_ns_per_helper"]
    arm_bpftime = arm["bpftime_median_net_ns_per_helper"]
    x64_kernel = float(x64["kernel_median_net_ns_per_helper"])
    x64_bpftime = float(x64["bpftime_median_net_ns_per_helper"])
    kernel_arm_over_x64 = arm_kernel / x64_kernel
    bpftime_arm_over_x64 = arm_bpftime / x64_bpftime
    kernel_log_change = abs(math.log(kernel_arm_over_x64))
    bpftime_log_change = abs(math.log(bpftime_arm_over_x64))
    arm_winner = arm["lower_cost_path"]
    x64_winner = x64["lower_cost_path"]
    cross_arch.append(
        {
            "case": case,
            "arm64_kernel_median_net_ns_per_helper": arm_kernel,
            "x64_kernel_median_net_ns_per_helper": x64_kernel,
            "arm64_over_x64_kernel": kernel_arm_over_x64,
            "arm64_bpftime_median_net_ns_per_helper": arm_bpftime,
            "x64_bpftime_median_net_ns_per_helper": x64_bpftime,
            "arm64_over_x64_bpftime": bpftime_arm_over_x64,
            "arm64_bpftime_over_kernel": arm["bpftime_over_kernel"],
            "x64_bpftime_over_kernel": float(x64["bpftime_over_kernel"]),
            "arm64_lower_cost_path": arm_winner,
            "x64_lower_cost_path": x64_winner,
            "winner_flipped": "yes" if arm_winner != x64_winner else "no",
            "dominant_ratio_shift_component": (
                "bpftime numerator"
                if bpftime_log_change > kernel_log_change
                else "kernel denominator"
            ),
        }
    )

write_csv("summary.csv", rows)
write_csv("net-helper.csv", derived)
write_csv("comparison.csv", comparison)
write_csv("comparison-x64.csv", cross_arch)
