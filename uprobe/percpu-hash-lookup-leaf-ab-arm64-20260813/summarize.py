#!/usr/bin/env python3
import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MODES = (
    "base", "percpu_hash_lookup_raw_key_copy",
    "percpu_hash_lookup_cached_hash", "percpu_hash_lookup_fixed4_equal",
    "percpu_hash_lookup_cached_hash_fixed4_equal",
)
CASES = ("lookup_control", "percpu_hash_lookup")
WALL_RE = re.compile(r"^(lookup_control|percpu_hash_lookup),[^,]*,[^,]*,[^,]*,([0-9.]+)$", re.M)

def stats(values):
    return statistics.mean(values), statistics.median(values), statistics.stdev(values), min(values), max(values)

wall = {}
wall_rows = []
for mode in MODES:
    wall[mode] = []
    for run, path in enumerate(sorted((ROOT / "raw" / "wall").glob(f"{mode}-run??.txt")), 1):
        found = {name: float(value) for name, value in WALL_RE.findall(path.read_text(errors="replace"))}
        if set(CASES) - set(found):
            raise RuntimeError(f"missing wall cases: {path}")
        net = (found["percpu_hash_lookup"] - found["lookup_control"]) / 1000.0
        wall[mode].append(net)
        wall_rows.append((mode, run, f"{net:.9f}"))

if wall:
    with (ROOT / "wall-raw.csv").open("w", newline="") as f:
        w = csv.writer(f); w.writerow(("mode", "run", "net_ns_per_helper")); w.writerows(wall_rows)
    with (ROOT / "wall-summary.csv").open("w", newline="") as f:
        w = csv.writer(f); w.writerow(("mode", "mean_ns_per_helper", "median", "sample_sd", "min", "max"))
        for mode in MODES:
            if len(wall[mode]) != 5: raise RuntimeError(f"{mode}: expected 5 wall runs")
            w.writerow((mode, *(f"{x:.9f}" for x in stats(wall[mode]))))

comparisons = (
    ("key vector assign above fixed memcpy", "base", "percpu_hash_lookup_raw_key_copy"),
    ("normal hasher above exact cached hash (normal equality)", "base", "percpu_hash_lookup_cached_hash"),
    ("generic vector equality above fixed4 equality (normal hash)", "base", "percpu_hash_lookup_fixed4_equal"),
    ("normal hasher above exact cached hash (fixed4 equality)", "percpu_hash_lookup_fixed4_equal", "percpu_hash_lookup_cached_hash_fixed4_equal"),
    ("generic vector equality above fixed4 equality (cached hash)", "percpu_hash_lookup_cached_hash", "percpu_hash_lookup_cached_hash_fixed4_equal"),
)

if wall and all(len(wall[m]) == 5 for m in MODES):
    with (ROOT / "wall-effects.csv").open("w", newline="") as f:
        w = csv.writer(f); w.writerow(("effect", "from_mode", "to_mode", "mean_saving_ns_per_helper", "median", "sample_sd", "min", "max"))
        for label, before, after in comparisons:
            delta = [a-b for a,b in zip(wall[before], wall[after])]
            w.writerow((label, before, after, *(f"{x:.9f}" for x in stats(delta))))

pmu = {}
pmu_rows = []
for mode in MODES:
    for run in range(1, 4):
        raw = {}
        for case in CASES:
            path = ROOT / "raw" / "pmu" / f"{mode}-{case}-run{run:02d}.perf.csv"
            if not path.exists(): continue
            metrics = {}
            for line in path.read_text().splitlines():
                row = line.split(",")
                if len(row) >= 3 and row[0].replace(".", "", 1).isdigit() and row[2] in ("cycles", "instructions"):
                    if len(row) >= 5 and row[4] and float(row[4]) < 99.9:
                        raise RuntimeError(f"multiplexed: {path}: {line}")
                    metrics[row[2]] = float(row[0])
            if set(metrics) != {"cycles", "instructions"}: raise RuntimeError(f"missing PMU metric: {path}")
            raw[case] = metrics
        if len(raw) == 2:
            for metric in ("cycles", "instructions"):
                net = (raw["percpu_hash_lookup"][metric] - raw["lookup_control"][metric]) / (20000 * 1000)
                pmu.setdefault((mode, metric), []).append(net)
                pmu_rows.append((mode, run, metric, f"{net:.9f}"))

if pmu:
    with (ROOT / "pmu-raw.csv").open("w", newline="") as f:
        w = csv.writer(f); w.writerow(("mode", "run", "metric", "net_per_helper")); w.writerows(pmu_rows)
    with (ROOT / "pmu-effects.csv").open("w", newline="") as f:
        w = csv.writer(f); w.writerow(("effect", "from_mode", "to_mode", "metric", "mean_saving_per_helper", "median", "sample_sd", "min", "max"))
        for label, before, after in comparisons:
            for metric in ("cycles", "instructions"):
                a, b = pmu[(before, metric)], pmu[(after, metric)]
                if len(a) != 3 or len(b) != 3: raise RuntimeError("expected 3 PMU runs")
                delta = [x-y for x,y in zip(a,b)]
                w.writerow((label, before, after, metric, *(f"{x:.9f}" for x in stats(delta))))

for name in ("wall-effects.csv", "pmu-effects.csv"):
    path = ROOT / name
    if path.exists(): print(path.read_text(), end="")
