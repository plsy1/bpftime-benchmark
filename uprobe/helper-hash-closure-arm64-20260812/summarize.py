#!/usr/bin/env python3
import csv
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ENVS = ("kernel", "bpftime")
CASES = ("empty", "lookup_control", "update_control", "hash_lookup",
         "hash_update", "percpu_hash_lookup", "percpu_hash_update")
CONTROL = {"hash_lookup": "lookup_control", "percpu_hash_lookup": "lookup_control",
           "hash_update": "update_control", "percpu_hash_update": "update_control"}

def read(path):
    rows = {}
    with path.open(errors="replace") as handle:
        for line in handle:
            if not line.startswith(CASES): continue
            row = next(csv.DictReader(["case,order,iterations,total_ns,ns_per_invocation", line]))
            rows[row["case"]] = float(row["ns_per_invocation"])
    if set(rows) != set(CASES): raise RuntimeError(f"{path}: incomplete")
    return rows

raw = {env: [read(path) for path in sorted((ROOT / "raw" / env).glob("victim-run??.txt"))] for env in ENVS}
for env in ENVS:
    if len(raw[env]) != 5: raise RuntimeError(f"{env}: expected 5 runs")

net = {env: {case: [(run[case] - run[control]) / 1000.0 for run in raw[env]]
             for case, control in CONTROL.items()} for env in ENVS}

with (ROOT / "closure.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("case", "kernel_mean", "bpftime_mean", "bpftime_minus_kernel", "gap_sample_sd"))
    for case in CONTROL:
        kernel, bpftime = net["kernel"][case], net["bpftime"][case]
        gaps = [b - k for b, k in zip(bpftime, kernel)]
        writer.writerow((case, f"{statistics.mean(kernel):.9f}", f"{statistics.mean(bpftime):.9f}",
                         f"{statistics.mean(gaps):.9f}", f"{statistics.stdev(gaps):.9f}"))

with (ROOT / "percpu-specific.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("operation", "kernel_percpu_minus_ordinary", "bpftime_percpu_minus_ordinary", "percpu_specific_gap"))
    for operation in ("lookup", "update"):
        ordinary, percpu = f"hash_{operation}", f"percpu_hash_{operation}"
        kd = statistics.mean(net["kernel"][percpu]) - statistics.mean(net["kernel"][ordinary])
        bd = statistics.mean(net["bpftime"][percpu]) - statistics.mean(net["bpftime"][ordinary])
        writer.writerow((operation, f"{kd:.9f}", f"{bd:.9f}", f"{bd-kd:.9f}"))

print((ROOT / "closure.csv").read_text(), end="")

def read_perf(path, metric):
    with path.open() as handle:
        for row in csv.reader(handle):
            if len(row) >= 3 and row[2] == metric: return float(row[0])
    raise RuntimeError(f"{path}: missing {metric}")

with (ROOT / "pmu-closure.csv").open("w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(("metric", "case", "kernel_mean", "bpftime_mean", "bpftime_minus_kernel", "gap_sample_sd"))
    for metric in ("cycles", "instructions"):
        for case, control in CONTROL.items():
            values = {}
            for env in ENVS:
                values[env] = []
                for run in range(1, 4):
                    stem = ROOT / "raw-pmu" / env / f"{metric}-{case}-run{run:02d}"
                    real = read_perf(Path(f"{stem}-real.perf.csv"), metric)
                    ctl = read_perf(Path(f"{stem}-control.perf.csv"), metric)
                    values[env].append((real - ctl) / 10000000.0)
            gaps = [b-k for b, k in zip(values["bpftime"], values["kernel"])]
            writer.writerow((metric, case, f"{statistics.mean(values['kernel']):.9f}",
                             f"{statistics.mean(values['bpftime']):.9f}",
                             f"{statistics.mean(gaps):.9f}", f"{statistics.stdev(gaps):.9f}"))
