#!/usr/bin/env python3

import csv
import re
import statistics
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = ("empty", "loop_control", "hash_lookup")
COMPONENTS = {
    "nonhelper_loop": ("loop_control", "empty"),
    "helper_containing_path": ("hash_lookup", "loop_control"),
    "full_top_path": ("hash_lookup", "empty"),
}


def write_csv(name, header, rows):
    with (HERE / name).open("w", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)


def stats(values):
    return (
        statistics.mean(values),
        statistics.median(values),
        statistics.stdev(values),
        min(values),
        max(values),
    )


wall = {environment: {} for environment in ("kernel", "bpftime")}
wall_raw = []
for environment in wall:
    for path in sorted((HERE / "raw" / environment).glob("victim-run??.txt")):
        run = int(re.search(r"run(\d+)", path.name).group(1))
        values = {}
        with path.open() as stream:
            for row in csv.reader(stream):
                if row and row[0] in CASES:
                    case, order, iterations, total_ns, ns_per_invocation = row
                    values[case] = float(ns_per_invocation)
                    wall_raw.append((environment, run, int(order), case,
                                     int(iterations), int(total_ns),
                                     f"{float(ns_per_invocation):.9f}"))
        assert set(values) == set(CASES), (path, values)
        wall[environment][run] = values

write_csv("wall-raw.csv",
          ("environment", "run", "order", "case", "iterations", "total_ns",
           "ns_per_invocation"), wall_raw)

wall_summary = []
wall_components = {environment: {} for environment in wall}
for environment in wall:
    assert len(wall[environment]) == 6
    for case in CASES:
        values = [wall[environment][run][case] for run in sorted(wall[environment])]
        wall_summary.append((environment, "absolute", case, *
                             (f"{value:.9f}" for value in stats(values))))
    for component, (high, low) in COMPONENTS.items():
        values = [(wall[environment][run][high] - wall[environment][run][low]) /
                  1000.0 for run in sorted(wall[environment])]
        wall_components[environment][component] = values
        wall_summary.append((environment, "derived_ns_per_helper", component, *
                             (f"{value:.9f}" for value in stats(values))))

write_csv("wall-summary.csv",
          ("environment", "quantity_type", "quantity", "mean", "median",
           "sample_sd", "min", "max"), wall_summary)

decomposition = []
for component in COMPONENTS:
    kernel_values = wall_components["kernel"][component]
    bpftime_values = wall_components["bpftime"][component]
    gaps = [bpftime - kernel for bpftime, kernel in
            zip(bpftime_values, kernel_values)]
    k = stats(kernel_values)
    b = stats(bpftime_values)
    g = stats(gaps)
    decomposition.append((component, f"{k[0]:.9f}", f"{b[0]:.9f}",
                          f"{g[0]:.9f}", f"{g[1]:.9f}", f"{g[2]:.9f}",
                          f"{g[3]:.9f}", f"{g[4]:.9f}"))

write_csv("decomposition.csv",
          ("component", "kernel_mean_ns_per_helper", "bpftime_mean_ns_per_helper",
           "bpftime_minus_kernel_mean", "gap_median", "gap_sample_sd", "gap_min",
           "gap_max"), decomposition)

pmu = {metric: {environment: {} for environment in ("kernel", "bpftime")}
       for metric in ("cycles", "instructions")}
pmu_raw = []
for metric in pmu:
    for environment in pmu[metric]:
        for path in sorted((HERE / "raw-pmu" / environment).glob(
                f"{metric}-*-run??.perf.csv")):
            match = re.match(rf"{metric}-(.+)-run(\d+)\.perf\.csv", path.name)
            case, run = match.group(1), int(match.group(2))
            assert case in CASES
            data = [row for row in csv.reader(path.open())
                    if row and not row[0].startswith("#")]
            assert len(data) == 1 and data[0][2] == metric, (path, data)
            count = float(data[0][0])
            running_percent = float(data[0][4])
            assert running_percent == 100.0
            per_invocation = count / 100000.0
            pmu[metric][environment].setdefault(run, {})[case] = per_invocation
            pmu_raw.append((environment, metric, run, case, f"{count:.0f}",
                            f"{per_invocation:.9f}", f"{running_percent:.2f}"))

write_csv("pmu-raw.csv",
          ("environment", "metric", "run", "case", "raw_count",
           "count_per_invocation", "running_percent"), pmu_raw)

pmu_summary = []
for metric in pmu:
    for component, (high, low) in COMPONENTS.items():
        environment_values = {}
        for environment in ("kernel", "bpftime"):
            assert len(pmu[metric][environment]) == 3
            values = [(pmu[metric][environment][run][high] -
                       pmu[metric][environment][run][low]) / 1000.0
                      for run in sorted(pmu[metric][environment])]
            environment_values[environment] = values
            summary = stats(values)
            pmu_summary.append((metric, component, environment, *
                                (f"{value:.9f}" for value in summary)))
        gaps = [bpftime - kernel for bpftime, kernel in zip(
            environment_values["bpftime"], environment_values["kernel"])]
        summary = stats(gaps)
        pmu_summary.append((metric, component, "bpftime_minus_kernel", *
                            (f"{value:.9f}" for value in summary)))

write_csv("pmu-summary.csv",
          ("metric", "component", "environment_or_gap", "mean_per_helper",
           "median_per_helper", "sample_sd", "min", "max"), pmu_summary)

full_gap = next(row for row in decomposition if row[0] == "full_top_path")
loop_gap = next(row for row in decomposition if row[0] == "nonhelper_loop")
helper_gap = next(row for row in decomposition
                  if row[0] == "helper_containing_path")
assert abs(float(full_gap[3]) -
           (float(loop_gap[3]) + float(helper_gap[3]))) < 2e-9

write_csv("closure.csv",
          ("quantity", "ns_per_helper", "formula", "interpretation"), (
              ("matched_full_gap", full_gap[3],
               "(hash-empty)_bpftime - (hash-empty)_kernel",
               "complete top-level difference"),
              ("matched_nonhelper_gap", loop_gap[3],
               "(loop-empty)_bpftime - (loop-empty)_kernel",
               "loop/key-stack path; no map helper"),
              ("matched_helper_containing_gap", helper_gap[3],
               "(hash-loop)_bpftime - (hash-loop)_kernel",
               "map relocation/helper call/helper body in top context"),
              ("reconstructed_full_gap",
               f"{float(loop_gap[3]) + float(helper_gap[3]):.9f}",
               "matched_nonhelper_gap + matched_helper_containing_gap",
               "algebraic closure"),
              ("closure_error",
               f"{float(full_gap[3]) - float(loop_gap[3]) - float(helper_gap[3]):.9f}",
               "matched_full_gap - reconstructed_full_gap",
               "must be zero within output rounding"),
          ))
