#!/usr/bin/env python3
import csv
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ENVS = ("kernel", "bpftime")
CASES = (
    "empty",
    "lookup_control",
    "update_control",
    "hash_lookup",
    "hash_update",
    "percpu_hash_lookup",
    "percpu_hash_update",
)
CONTROLS = {
    "hash_lookup": "lookup_control",
    "hash_update": "update_control",
    "percpu_hash_lookup": "lookup_control",
    "percpu_hash_update": "update_control",
}


def load_configs():
    with (ROOT / "configs.csv").open() as handle:
        return list(csv.DictReader(handle))


def read_victim(path):
    rows = {}
    with path.open(errors="replace") as handle:
        for line in handle:
            if not line.startswith(CASES):
                continue
            row = next(csv.DictReader(
                ["case,order,iterations,total_ns,ns_per_invocation", line]
            ))
            rows[row["case"]] = row
    if set(rows) != set(CASES):
        raise RuntimeError(f"{path}: incomplete cases {sorted(rows)}")
    return rows


def describe(values):
    return {
        "mean": statistics.mean(values),
        "median": statistics.median(values),
        "sample_sd": statistics.stdev(values),
        "min": min(values),
        "max": max(values),
    }


configs = load_configs()
raw_rows = []
net = {}
for config in configs:
    tag = config["tag"]
    net[tag] = {}
    for environment in ENVS:
        paths = sorted((ROOT / "raw" / "wall" / tag / environment).glob("victim-run??.txt"))
        if len(paths) != 5:
            raise RuntimeError(f"{tag}/{environment}: expected 5 runs, found {len(paths)}")
        runs = [read_victim(path) for path in paths]
        net[tag][environment] = {case: [] for case in CONTROLS}
        for run_index, run in enumerate(runs, 1):
            for case, control in CONTROLS.items():
                real = float(run[case]["ns_per_invocation"])
                ctl = float(run[control]["ns_per_invocation"])
                value = (real - ctl) / 1000.0
                net[tag][environment][case].append(value)
                raw_rows.append({
                    **config,
                    "environment": environment,
                    "run": run_index,
                    "case": case,
                    "real_ns_per_invocation": f"{real:.9f}",
                    "control_ns_per_invocation": f"{ctl:.9f}",
                    "net_ns_per_helper": f"{value:.9f}",
                })

with (ROOT / "wall-raw.csv").open("w", newline="") as handle:
    fields = list(configs[0]) + [
        "environment", "run", "case", "real_ns_per_invocation",
        "control_ns_per_invocation", "net_ns_per_helper",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(raw_rows)

summary_rows = []
gap_rows = []
percpu_rows = []
for config in configs:
    tag = config["tag"]
    for case in CONTROLS:
        for environment in ENVS:
            stats = describe(net[tag][environment][case])
            summary_rows.append({
                **config,
                "environment": environment,
                "case": case,
                **{name: f"{value:.9f}" for name, value in stats.items()},
            })
        kernel = net[tag]["kernel"][case]
        bpftime = net[tag]["bpftime"][case]
        kernel_stats = describe(kernel)
        bpftime_stats = describe(bpftime)
        paired_gaps = [b - k for b, k in zip(bpftime, kernel)]
        gap_rows.append({
            **config,
            "case": case,
            "kernel_mean": f"{kernel_stats['mean']:.9f}",
            "kernel_median": f"{kernel_stats['median']:.9f}",
            "bpftime_mean": f"{bpftime_stats['mean']:.9f}",
            "bpftime_median": f"{bpftime_stats['median']:.9f}",
            "gap_of_means": f"{bpftime_stats['mean'] - kernel_stats['mean']:.9f}",
            "gap_of_medians": f"{bpftime_stats['median'] - kernel_stats['median']:.9f}",
            "mean_ratio": f"{bpftime_stats['mean'] / kernel_stats['mean']:.9f}",
            "paired_gap_sample_sd": f"{statistics.stdev(paired_gaps):.9f}",
        })
    for operation in ("lookup", "update"):
        ordinary = f"hash_{operation}"
        percpu = f"percpu_hash_{operation}"
        for environment in ENVS:
            values = [p - o for p, o in zip(
                net[tag][environment][percpu], net[tag][environment][ordinary]
            )]
            stats = describe(values)
            percpu_rows.append({
                **config,
                "environment": environment,
                "operation": operation,
                **{name: f"{value:.9f}" for name, value in stats.items()},
            })

with (ROOT / "wall-summary.csv").open("w", newline="") as handle:
    fields = list(configs[0]) + [
        "environment", "case", "mean", "median", "sample_sd", "min", "max",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(summary_rows)

with (ROOT / "gaps.csv").open("w", newline="") as handle:
    fields = list(configs[0]) + [
        "case", "kernel_mean", "kernel_median", "bpftime_mean",
        "bpftime_median", "gap_of_means", "gap_of_medians", "mean_ratio",
        "paired_gap_sample_sd",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(gap_rows)

with (ROOT / "percpu-specific.csv").open("w", newline="") as handle:
    fields = list(configs[0]) + [
        "environment", "operation", "mean", "median", "sample_sd", "min", "max",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(percpu_rows)

print((ROOT / "gaps.csv").read_text(), end="")


def read_perf(path, metric):
    with path.open() as handle:
        for row in csv.reader(handle):
            if len(row) >= 3 and row[2] == metric:
                if row[0].startswith("<"):
                    raise RuntimeError(f"{path}: event not counted: {row[0]}")
                return float(row[0])
    raise RuntimeError(f"{path}: missing {metric}")


def read_iterations(path, case):
    with path.open(errors="replace") as handle:
        for line in handle:
            if line.startswith(f"{case},"):
                return int(line.split(",")[2])
    raise RuntimeError(f"{path}: missing {case}")


with (ROOT / "pmu-endpoints.csv").open() as handle:
    endpoints = list(csv.DictReader(handle))

pmu_raw = []
for endpoint in endpoints:
    tag = endpoint["tag"]
    case = endpoint["case"]
    control = endpoint["control"]
    for environment in ENVS:
        raw_dir = ROOT / "raw" / "pmu" / tag / environment
        for metric in ("cycles", "instructions"):
            for run in range(1, 4):
                stem = raw_dir / f"{metric}-{case}-run{run:02d}"
                real_perf = read_perf(Path(f"{stem}-real.perf.csv"), metric)
                control_perf = read_perf(Path(f"{stem}-control.perf.csv"), metric)
                real_iterations = read_iterations(Path(f"{stem}-real.stdout"), case)
                control_iterations = read_iterations(Path(f"{stem}-control.stdout"), control)
                if real_iterations != control_iterations:
                    raise RuntimeError(f"{stem}: real/control iteration mismatch")
                denominator = real_iterations * 1000.0
                pmu_raw.append({
                    **endpoint,
                    "environment": environment,
                    "metric": metric,
                    "run": run,
                    "iterations": real_iterations,
                    "real_count": f"{real_perf:.3f}",
                    "control_count": f"{control_perf:.3f}",
                    "net_per_helper": f"{(real_perf - control_perf) / denominator:.9f}",
                })

with (ROOT / "pmu-raw.csv").open("w", newline="") as handle:
    fields = list(endpoints[0]) + [
        "environment", "metric", "run", "iterations", "real_count",
        "control_count", "net_per_helper",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(pmu_raw)

pmu_groups = {}
for row in pmu_raw:
    key = (row["tag"], row["case"], row["environment"], row["metric"])
    pmu_groups.setdefault(key, []).append(float(row["net_per_helper"]))

pmu_summary = []
pmu_gaps = []
for endpoint in endpoints:
    for metric in ("cycles", "instructions"):
        values_by_env = {}
        for environment in ENVS:
            values = pmu_groups[(endpoint["tag"], endpoint["case"], environment, metric)]
            values_by_env[environment] = values
            stats = describe(values)
            pmu_summary.append({
                **endpoint,
                "environment": environment,
                "metric": metric,
                **{name: f"{value:.9f}" for name, value in stats.items()},
            })
        kernel = values_by_env["kernel"]
        bpftime = values_by_env["bpftime"]
        gaps = [b - k for b, k in zip(bpftime, kernel)]
        pmu_gaps.append({
            **endpoint,
            "metric": metric,
            "kernel_mean": f"{statistics.mean(kernel):.9f}",
            "bpftime_mean": f"{statistics.mean(bpftime):.9f}",
            "gap_of_means": f"{statistics.mean(bpftime) - statistics.mean(kernel):.9f}",
            "paired_gap_sample_sd": f"{statistics.stdev(gaps):.9f}",
        })

with (ROOT / "pmu-summary.csv").open("w", newline="") as handle:
    fields = list(endpoints[0]) + [
        "environment", "metric", "mean", "median", "sample_sd", "min", "max",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(pmu_summary)

with (ROOT / "pmu-gaps.csv").open("w", newline="") as handle:
    fields = list(endpoints[0]) + [
        "metric", "kernel_mean", "bpftime_mean", "gap_of_means",
        "paired_gap_sample_sd",
    ]
    writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    writer.writerows(pmu_gaps)
