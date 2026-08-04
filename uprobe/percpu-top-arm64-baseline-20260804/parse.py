#!/usr/bin/env python3
"""Parse paired ordinary/per-CPU cases from the official ARM64 victim runs."""

from __future__ import annotations

import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parent / "uprobe-top-arm64-20260803" / "raw"
PATTERN = re.compile(
    r"Benchmarking (\S+) in thread 1\nAverage time usage ([0-9.]+) ns"
)
ORDINARY = {
    "array_lookup": "__bench_array_map_lookup",
    "array_update": "__bench_array_map_update",
    "hash_lookup": "__bench_hash_map_lookup",
    "hash_update": "__bench_hash_map_update",
}
PERCPU = {
    "array_lookup": "__bench_per_cpu_array_map_lookup",
    "array_update": "__bench_per_cpu_array_map_update",
    "hash_lookup": "__bench_per_cpu_hash_map_lookup",
    "hash_update": "__bench_per_cpu_hash_map_update",
}


def read_run(environment: str, run: int) -> dict[str, float]:
    path = SOURCE / f"{environment}-run-{run}.txt"
    values = {
        name: float(value) for name, value in PATTERN.findall(path.read_text())
    }
    required = {"__bench_uprobe", *ORDINARY.values(), *PERCPU.values()}
    missing = required - values.keys()
    if missing:
        raise RuntimeError(f"{path}: missing {sorted(missing)}")
    return values


def stats(values: list[float]) -> dict[str, float]:
    return {
        "mean": statistics.mean(values),
        "median": statistics.median(values),
        "stdev": statistics.stdev(values),
        "min": min(values),
        "max": max(values),
    }


raw_rows = []
summary_rows = []
extra_rows = []

for environment in ("kernel", "bpftime"):
    runs = {run: read_run(environment, run) for run in range(1, 6)}
    for run, values in runs.items():
        empty = values["__bench_uprobe"]
        for case, ordinary_name in ORDINARY.items():
            for kind, name in (("ordinary", ordinary_name),
                               ("percpu", PERCPU[case])):
                raw = values[name]
                raw_rows.append({
                    "environment": environment,
                    "run": run,
                    "case": case,
                    "kind": kind,
                    "raw_ns_per_invocation": raw,
                    "empty_uprobe_ns_per_invocation": empty,
                    "net_ns_per_helper": (raw - empty) / 1000.0,
                })

    for case in ORDINARY:
        for kind, names in (("ordinary", ORDINARY), ("percpu", PERCPU)):
            values = [
                (runs[run][names[case]] - runs[run]["__bench_uprobe"])
                / 1000.0
                for run in range(1, 6)
            ]
            summary_rows.append({
                "environment": environment,
                "case": case,
                "kind": kind,
                **stats(values),
                **{f"run{run}": value for run, value in enumerate(values, 1)},
            })

        values = [
            ((runs[run][PERCPU[case]] - runs[run]["__bench_uprobe"])
             - (runs[run][ORDINARY[case]] - runs[run]["__bench_uprobe"]))
            / 1000.0
            for run in range(1, 6)
        ]
        extra_rows.append({
            "environment": environment,
            "case": case,
            "definition": "percpu net - ordinary net, same victim run",
            **stats(values),
            **{f"run{run}": value for run, value in enumerate(values, 1)},
        })


def write_csv(name: str, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    with (ROOT / name).open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


write_csv("raw-values.csv", raw_rows)
write_csv("summary.csv", summary_rows)
write_csv("paired-extra.csv", extra_rows)
