#!/usr/bin/env python3
import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MODES = ("base", "percpu_hash_delete_defer_reclaim")
PATTERN = re.compile(r"Benchmarking __bench_per_cpu_hash_map_delete.*?Average time usage ([0-9.]+) ns", re.S)
values = {}
rows = []
for mode in MODES:
    values[mode] = []
    for run, path in enumerate(sorted((ROOT / "raw" / "delete").glob(f"{mode}-run??.txt")), 1):
        match = PATTERN.search(path.read_text(errors="replace"))
        if not match:
            raise RuntimeError(f"missing delete value: {path}")
        per_helper = float(match.group(1)) / 1000.0
        values[mode].append(per_helper)
        rows.append((mode, run, f"{per_helper:.9f}"))
    if len(values[mode]) != 5:
        raise RuntimeError(f"{mode}: expected 5 runs")

with (ROOT / "delete-raw.csv").open("w", newline="") as f:
    w = csv.writer(f); w.writerow(("mode", "run", "ns_per_helper")); w.writerows(rows)
with (ROOT / "delete-summary.csv").open("w", newline="") as f:
    w = csv.writer(f)
    w.writerow(("mode", "mean_ns_per_helper", "median", "sample_sd", "min", "max"))
    for mode in MODES:
        v = values[mode]
        w.writerow((mode, *(f"{x:.9f}" for x in (statistics.mean(v), statistics.median(v), statistics.stdev(v), min(v), max(v)))))
    paired = [a - b for a, b in zip(values[MODES[0]], values[MODES[1]])]
    w.writerow(("synchronous_reclamation_saving", *(f"{x:.9f}" for x in (statistics.mean(paired), statistics.median(paired), statistics.stdev(paired), min(paired), max(paired)))))
print((ROOT / "delete-summary.csv").read_text(), end="")
