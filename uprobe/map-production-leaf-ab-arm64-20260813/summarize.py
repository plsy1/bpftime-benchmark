#!/usr/bin/env python3
import csv
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MODES = (
    "base", "cache_control", "cached_handler", "direct_map", "percpu_array_direct",
    "percpu_array_fixed_cpu", "percpu_array_no_copy",
    "percpu_hash_lookup_no_find", "percpu_hash_update_no_find",
    "percpu_hash_update_no_copy",
)
FAMILY = {
    "array": {
        "cases": ("lookup_control", "update_control", "array_lookup", "array_update", "percpu_array_lookup", "percpu_array_update"),
        "control": {"array_lookup": "lookup_control", "array_update": "update_control", "percpu_array_lookup": "lookup_control", "percpu_array_update": "update_control"},
    },
    "hash": {
        "cases": ("lookup_control", "update_control", "hash_lookup", "hash_update", "percpu_hash_lookup", "percpu_hash_update"),
        "control": {"hash_lookup": "lookup_control", "hash_update": "update_control", "percpu_hash_lookup": "lookup_control", "percpu_hash_update": "update_control"},
    },
}


def read_run(path, cases):
    values = {}
    with path.open(errors="replace") as stream:
        for line in stream:
            if not line.startswith(cases):
                continue
            row = next(csv.DictReader(["case,order,iterations,total_ns,ns_per_invocation", line]))
            values[row["case"]] = float(row["ns_per_invocation"])
    missing = set(cases) - set(values)
    if missing:
        raise RuntimeError(f"{path}: missing {sorted(missing)}")
    return values


def stats(values):
    return statistics.mean(values), statistics.median(values), statistics.stdev(values), min(values), max(values)


net = {}
with (ROOT / "wall-raw.csv").open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(("family", "mode", "run", "case", "net_ns_per_helper"))
    for family, spec in FAMILY.items():
        for mode in MODES:
            runs = []
            for path in sorted((ROOT / "raw" / family).glob(f"{mode}-run??.txt")):
                values = read_run(path, spec["cases"])
                runs.append(values)
            if len(runs) != 5:
                raise RuntimeError(f"{family}/{mode}: expected 5 runs, got {len(runs)}")
            for case, control in spec["control"].items():
                key = (family, mode, case)
                net[key] = [(values[case] - values[control]) / 1000.0 for values in runs]
                for run, value in enumerate(net[key], 1):
                    writer.writerow((family, mode, run, case, f"{value:.9f}"))

with (ROOT / "wall-summary.csv").open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(("family", "mode", "case", "mean_ns_per_helper", "median", "sample_sd", "min", "max", "saving_vs_base"))
    for key, values in net.items():
        family, mode, case = key
        base = net[(family, "base", case)]
        saving = statistics.mean(base) - statistics.mean(values)
        writer.writerow((family, mode, case, *(f"{x:.9f}" for x in stats(values)), f"{saving:.9f}"))

effects = (
    ("ordinary array lookup", "array", "array_lookup", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("ordinary array lookup", "array", "array_lookup", "generic handler dispatch", "cached_handler", "direct_map"),
    ("ordinary array update", "array", "array_update", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("ordinary array update", "array", "array_update", "generic handler dispatch", "cached_handler", "direct_map"),
    ("per-CPU array lookup", "array", "percpu_array_lookup", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("per-CPU array lookup", "array", "percpu_array_lookup", "generic handler dispatch", "cached_handler", "direct_map"),
    ("per-CPU array lookup", "array", "percpu_array_lookup", "std::function wrapper", "base", "percpu_array_direct"),
    ("per-CPU array lookup", "array", "percpu_array_lookup", "sched_getcpu", "percpu_array_direct", "percpu_array_fixed_cpu"),
    ("per-CPU array update", "array", "percpu_array_update", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("per-CPU array update", "array", "percpu_array_update", "generic handler dispatch", "cached_handler", "direct_map"),
    ("per-CPU array update", "array", "percpu_array_update", "std::function wrapper", "base", "percpu_array_direct"),
    ("per-CPU array update", "array", "percpu_array_update", "sched_getcpu", "percpu_array_direct", "percpu_array_fixed_cpu"),
    ("per-CPU array update", "array", "percpu_array_update", "8-byte value copy", "percpu_array_direct", "percpu_array_no_copy"),
    ("per-CPU hash lookup", "hash", "percpu_hash_lookup", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("per-CPU hash lookup", "hash", "percpu_hash_lookup", "generic handler dispatch", "cached_handler", "direct_map"),
    ("per-CPU hash lookup", "hash", "percpu_hash_lookup", "Boost hash/find", "base", "percpu_hash_lookup_no_find"),
    ("per-CPU hash update", "hash", "percpu_hash_update", "SHM fd/variant lookup", "cache_control", "cached_handler"),
    ("per-CPU hash update", "hash", "percpu_hash_update", "generic handler dispatch", "cached_handler", "direct_map"),
    ("per-CPU hash update", "hash", "percpu_hash_update", "existing value copy", "base", "percpu_hash_update_no_copy"),
    ("per-CPU hash update", "hash", "percpu_hash_update", "Boost hash/find", "percpu_hash_update_no_copy", "percpu_hash_update_no_find"),
)

with (ROOT / "effects.csv").open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(("operation", "concrete_operation", "from_mode", "to_mode", "mean_saving_ns_per_helper", "paired_median", "paired_sd", "min", "max"))
    for operation, family, case, label, from_mode, to_mode in effects:
        paired = [a - b for a, b in zip(net[(family, from_mode, case)], net[(family, to_mode, case)])]
        writer.writerow((operation, label, from_mode, to_mode, *(f"{x:.9f}" for x in stats(paired))))

print((ROOT / "effects.csv").read_text(), end="")
