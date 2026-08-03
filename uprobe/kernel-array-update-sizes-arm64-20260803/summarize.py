#!/usr/bin/env python3
import csv
import json
import re
import statistics
from pathlib import Path

HERE = Path(__file__).resolve().parent
SIZES = (8, 16, 32, 64, 128, 256)
METRICS = ("cycles", "instructions", "l1d_loads", "llc_misses")

rows = list(csv.DictReader((HERE / "raw-wall" / "raw.csv").open()))
wall = {}
with (HERE / "wall-summary.csv").open("w", newline="") as out:
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(("value_size", "rounds", "mean_ns_per_helper",
                     "median", "stdev", "min", "max"))
    for size in SIZES:
        values = [float(r["avg_ns_per_invocation"])
                  for r in rows
                  if int(r["value_size"]) == size and
                  r["implementation"] == "real-minus-control"]
        if len(values) != 5:
            raise SystemExit(f"expected five net wall values for {size}, got {len(values)}")
        wall[size] = statistics.mean(values)
        writer.writerow((size, len(values), f"{wall[size]:.9f}",
                         f"{statistics.median(values):.9f}",
                         f"{statistics.stdev(values):.9f}",
                         f"{min(values):.9f}", f"{max(values):.9f}"))

profile = {}
with (HERE / "profile-summary.csv").open("w", newline="") as out:
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(("metric", "value_size", "control_run_cnt",
                     "real_run_cnt", "control_per_program",
                     "real_per_program", "net_per_helper"))
    for metric in METRICS:
        directory = HERE / f"raw-profile-{metric}-pairwise"
        for size in SIZES:
            control = json.loads((directory / f"{size}_ctrl.json").read_text())[0]
            real = json.loads((directory / f"{size}_real.json").read_text())[0]
            if control["enabled"] != control["running"] or real["enabled"] != real["running"]:
                raise SystemExit(f"PMU multiplex detected for {metric}/{size}")
            control_per = control["value"] / control["run_cnt"]
            real_per = real["value"] / real["run_cnt"]
            net = (real_per - control_per) / 1000.0
            profile[(metric, size)] = net
            writer.writerow((metric, size, control["run_cnt"], real["run_cnt"],
                             f"{control_per:.9f}", f"{real_per:.9f}",
                             f"{net:.9f}"))

with (HERE / "combined-summary.csv").open("w", newline="") as out:
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(("value_size", "wall_ns_per_helper", "cycles_per_helper",
                     "instructions_per_helper", "l1d_loads_per_helper",
                     "llc_misses_per_helper", "nominal_cycles_at_1_728GHz"))
    for size in SIZES:
        writer.writerow((size, f"{wall[size]:.9f}",
                         f"{profile[('cycles', size)]:.9f}",
                         f"{profile[('instructions', size)]:.9f}",
                         f"{profile[('l1d_loads', size)]:.9f}",
                         f"{profile[('llc_misses', size)]:.9f}",
                         f"{wall[size] * 1.728:.9f}"))

dump_dir = HERE / "raw-profile-instructions-pairwise"
xlated_pattern = re.compile(r"^\s*\d+:\s+\(")
jited_pattern = re.compile(r"^\s*[0-9a-f]+:\s+\S")
with (HERE / "jit-static-summary.csv").open("w", newline="") as out:
    writer = csv.writer(out, lineterminator="\n")
    writer.writerow(("value_size", "control_xlated_lines", "real_xlated_lines",
                     "xlated_delta", "control_jit_instructions",
                     "real_jit_instructions", "jit_delta"))
    for size in SIZES:
        counts = []
        for impl in ("ctrl", "real"):
            xlated = sum(bool(xlated_pattern.match(line)) for line in
                         (dump_dir / f"{size}_{impl}-xlated.txt").read_text().splitlines())
            jited = sum(bool(jited_pattern.match(line)) for line in
                        (dump_dir / f"{size}_{impl}-jited.txt").read_text().splitlines())
            counts.append((xlated, jited))
        writer.writerow((size, counts[0][0], counts[1][0],
                         counts[1][0] - counts[0][0], counts[0][1], counts[1][1],
                         counts[1][1] - counts[0][1]))
