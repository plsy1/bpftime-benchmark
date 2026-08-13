#!/usr/bin/env python3
import csv
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ENVIRONMENTS = ("kernel", "bpftime")
CASES = (
    "empty", "lookup_control", "update_control", "array_lookup",
    "array_update", "percpu_array_lookup", "percpu_array_update",
)
CONTROL = {
    "array_lookup": "lookup_control",
    "percpu_array_lookup": "lookup_control",
    "array_update": "update_control",
    "percpu_array_update": "update_control",
}


def read_run(path):
    rows = {}
    with path.open(errors="replace") as handle:
        for line in handle:
            if not line.startswith(CASES):
                continue
            row = next(csv.DictReader(
                ["case,order,iterations,total_ns,ns_per_invocation", line]
            ))
            rows[row["case"]] = float(row["ns_per_invocation"])
    missing = set(CASES) - set(rows)
    if missing:
        raise RuntimeError(f"{path}: missing {sorted(missing)}")
    return rows


raw = {environment: [] for environment in ENVIRONMENTS}
for environment in ENVIRONMENTS:
    for path in sorted((ROOT / "raw" / environment).glob("victim-run??.txt")):
        raw[environment].append(read_run(path))
    if len(raw[environment]) != 5:
        raise RuntimeError(f"{environment}: expected 5 runs, got {len(raw[environment])}")

with (ROOT / "wall-raw.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("environment", "run", "case", "ns_per_invocation"))
    for environment in ENVIRONMENTS:
        for run, values in enumerate(raw[environment], 1):
            for case in CASES:
                writer.writerow((environment, run, case, f"{values[case]:.9f}"))

net = {environment: {} for environment in ENVIRONMENTS}
for environment in ENVIRONMENTS:
    for case, control in CONTROL.items():
        net[environment][case] = [
            (run[case] - run[control]) / 1000.0 for run in raw[environment]
        ]


def stats(values):
    return (
        statistics.mean(values), statistics.median(values),
        statistics.stdev(values), min(values), max(values),
    )


with (ROOT / "wall-summary.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("environment", "case", "mean_ns_per_helper",
                     "median_ns_per_helper", "sample_sd", "min", "max"))
    for environment in ENVIRONMENTS:
        for case in CONTROL:
            writer.writerow((environment, case, *(f"{value:.9f}" for value in stats(net[environment][case]))))

with (ROOT / "closure.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("case", "kernel_mean", "bpftime_mean", "bpftime_minus_kernel",
                     "gap_sample_sd", "kernel_control", "bpftime_control"))
    for case, control in CONTROL.items():
        kernel = net["kernel"][case]
        bpftime = net["bpftime"][case]
        gaps = [b - k for b, k in zip(bpftime, kernel)]
        writer.writerow((case, f"{statistics.mean(kernel):.9f}",
                         f"{statistics.mean(bpftime):.9f}",
                         f"{statistics.mean(gaps):.9f}",
                         f"{statistics.stdev(gaps):.9f}", control, control))

with (ROOT / "percpu-specific.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("operation", "kernel_percpu_minus_ordinary",
                     "bpftime_percpu_minus_ordinary", "percpu_specific_gap"))
    for operation in ("lookup", "update"):
        ordinary = f"array_{operation}"
        percpu = f"percpu_array_{operation}"
        kernel_delta = statistics.mean(net["kernel"][percpu]) - statistics.mean(net["kernel"][ordinary])
        bpftime_delta = statistics.mean(net["bpftime"][percpu]) - statistics.mean(net["bpftime"][ordinary])
        writer.writerow((operation, f"{kernel_delta:.9f}", f"{bpftime_delta:.9f}",
                         f"{bpftime_delta - kernel_delta:.9f}"))

print((ROOT / "closure.csv").read_text(), end="")


def read_perf(path, metric):
    with path.open() as handle:
        for row in csv.reader(handle):
            if len(row) >= 3 and row[2] == metric:
                return float(row[0])
    raise RuntimeError(f"{path}: missing {metric}")


pmu_rows = []
for environment in ENVIRONMENTS:
    for metric in ("cycles", "instructions"):
        for case, control in CONTROL.items():
            values = []
            for run in range(1, 4):
                stem = ROOT / "raw-pmu" / environment / f"{metric}-{case}-run{run:02d}"
                real = read_perf(Path(f"{stem}-real.perf.csv"), metric)
                ctl = read_perf(Path(f"{stem}-control.perf.csv"), metric)
                values.append((real - ctl) / (20000.0 * 1000.0))
            pmu_rows.append((environment, metric, case, values))

with (ROOT / "pmu-summary.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("environment", "metric", "case", "mean_per_helper",
                     "median_per_helper", "sample_sd", "min", "max"))
    for environment, metric, case, values in pmu_rows:
        writer.writerow((environment, metric, case,
                         *(f"{value:.9f}" for value in stats(values))))

pmu_index = {(environment, metric, case): values
             for environment, metric, case, values in pmu_rows}
with (ROOT / "pmu-closure.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("metric", "case", "kernel_mean", "bpftime_mean",
                     "bpftime_minus_kernel", "gap_sample_sd"))
    for metric in ("cycles", "instructions"):
        for case in CONTROL:
            kernel = pmu_index[("kernel", metric, case)]
            bpftime = pmu_index[("bpftime", metric, case)]
            gaps = [b - k for b, k in zip(bpftime, kernel)]
            writer.writerow((metric, case, f"{statistics.mean(kernel):.9f}",
                             f"{statistics.mean(bpftime):.9f}",
                             f"{statistics.mean(gaps):.9f}",
                             f"{statistics.stdev(gaps):.9f}"))
