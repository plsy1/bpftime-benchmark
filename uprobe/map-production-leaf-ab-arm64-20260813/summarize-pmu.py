#!/usr/bin/env python3
import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ITERATIONS = 20000
HELPERS = ITERATIONS * 1000
RE = re.compile(r"^([0-9][0-9.]*)[^,]*,([^,]+),")

values = {}
raw_rows = []
for path in sorted((ROOT / "raw" / "pmu").glob("*/*.perf.csv")):
    family = path.parent.name
    match = re.match(r"(.+)-([^-]+)-run(\d+)\.perf\.csv$", path.name)
    if not match:
        raise RuntimeError(f"bad filename: {path}")
    case, mode, run = match.groups()
    metrics = {}
    for line in path.read_text().splitlines():
        row = line.split(",")
        if len(row) >= 3 and row[0].replace(".", "", 1).isdigit() and row[2] in ("cycles", "instructions"):
            metrics[row[2]] = float(row[0]) / HELPERS
            if len(row) >= 5 and row[4] and float(row[4]) < 99.9:
                raise RuntimeError(f"multiplexed counter in {path}: {line}")
    if set(metrics) != {"cycles", "instructions"}:
        raise RuntimeError(f"missing counters in {path}: {metrics}")
    values[(family, case, mode, int(run))] = metrics
    for metric, value in metrics.items():
        raw_rows.append((family, case, mode, run, metric, f"{value:.9f}"))

comparisons = (
    ("array", "array_lookup", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("array", "array_lookup", "generic handler dispatch", "cached_handler", "direct_map"),
    ("array", "array_update", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("array", "array_update", "generic handler dispatch", "cached_handler", "direct_map"),
    ("array", "percpu_array_lookup", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("array", "percpu_array_lookup", "generic handler dispatch", "cached_handler", "direct_map"),
    ("array", "percpu_array_lookup", "std::function wrapper", "base", "percpu_array_direct"),
    ("array", "percpu_array_lookup", "sched_getcpu", "percpu_array_direct", "percpu_array_fixed_cpu"),
    ("array", "percpu_array_update", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("array", "percpu_array_update", "generic handler dispatch", "cached_handler", "direct_map"),
    ("array", "percpu_array_update", "std::function wrapper", "base", "percpu_array_direct"),
    ("array", "percpu_array_update", "sched_getcpu", "percpu_array_direct", "percpu_array_fixed_cpu"),
    ("array", "percpu_array_update", "8-byte value copy", "percpu_array_direct", "percpu_array_no_copy"),
    ("hash", "percpu_hash_lookup", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("hash", "percpu_hash_lookup", "generic handler dispatch", "cached_handler", "direct_map"),
    ("hash", "percpu_hash_lookup", "Boost hash/find", "base", "percpu_hash_lookup_no_find"),
    ("hash", "percpu_hash_update", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("hash", "percpu_hash_update", "generic handler dispatch", "cached_handler", "direct_map"),
    ("hash", "percpu_hash_update", "existing value copy", "base", "percpu_hash_update_no_copy"),
    ("hash", "percpu_hash_update", "Boost hash/find", "percpu_hash_update_no_copy", "percpu_hash_update_no_find"),
)

with (ROOT / "pmu-raw.csv").open("w", newline="") as f:
    w = csv.writer(f); w.writerow(("family", "case", "mode", "run", "metric", "value_per_helper")); w.writerows(raw_rows)

with (ROOT / "pmu-effects.csv").open("w", newline="") as f:
    w = csv.writer(f)
    w.writerow(("family", "case", "operation", "from_mode", "to_mode", "metric", "mean_saving_per_helper", "median", "sample_sd", "min", "max"))
    for family, case, label, before, after in comparisons:
        for metric in ("cycles", "instructions"):
            delta = [values[(family, case, before, run)][metric] - values[(family, case, after, run)][metric] for run in range(1, 4)]
            row = (statistics.mean(delta), statistics.median(delta), statistics.stdev(delta), min(delta), max(delta))
            w.writerow((family, case, label, before, after, metric, *(f"{x:.9f}" for x in row)))

print((ROOT / "pmu-effects.csv").read_text(), end="")
