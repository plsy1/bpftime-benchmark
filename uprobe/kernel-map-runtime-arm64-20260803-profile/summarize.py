#!/usr/bin/env python3
import csv
import json
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
X64 = ROOT.parent / "kernel-map-runtime-x64-20260803-fixedfreq"
METRICS = ("cycles", "instructions", "l1d_loads", "llc_misses")


def write_csv(name, rows):
    with (ROOT / name).open("w", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=rows[0].keys(), lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)


profile_rows = []
runtime_rows = []
for metric in METRICS:
    directory = ROOT / f"raw-profile-{metric}"
    control = json.loads((directory / "control-profile.json").read_text())[0]
    real = json.loads((directory / "real-profile.json").read_text())[0]
    for role, value in (("control", control), ("real", real)):
        if value["metric"] != metric:
            raise RuntimeError(f"{metric}/{role}: unexpected metric")
        if value["run_cnt"] != 80000:
            raise RuntimeError(f"{metric}/{role}: unexpected run count")
        if value["enabled"] != value["running"]:
            raise RuntimeError(f"{metric}/{role}: event was multiplexed")
    control_per_run = control["value"] / control["run_cnt"]
    real_per_run = real["value"] / real["run_cnt"]
    profile_rows.append(
        {
            "metric": metric,
            "run_count": control["run_cnt"],
            "control_value": control["value"],
            "real_value": real["value"],
            "control_per_program_run": control_per_run,
            "real_per_program_run": real_per_run,
            "real_minus_control_per_helper": (
                real_per_run - control_per_run
            ) / 1000,
            "control_enabled": control["enabled"],
            "control_running": control["running"],
            "real_enabled": real["enabled"],
            "real_running": real["running"],
        }
    )

    with (directory / "raw.csv").open(newline="") as source:
        samples = [
            float(row["avg_ns_per_invocation"])
            for row in csv.DictReader(source)
            if row["operation"] == "array_update"
            and row["implementation"] == "real-minus-control"
        ]
    if len(samples) != 5:
        raise RuntimeError(f"{metric}: expected five runtime samples")
    runtime_rows.append(
        {
            "profile_metric": metric,
            **{f"round_{i}_ns_per_helper": value for i, value in enumerate(samples, 1)},
            "mean_ns_per_helper": statistics.mean(samples),
            "sample_stddev_ns": statistics.stdev(samples),
            "min_ns_per_helper": min(samples),
            "max_ns_per_helper": max(samples),
        }
    )

with (X64 / "profile-summary.csv").open(newline="") as source:
    x64 = {row["metric"]: row for row in csv.DictReader(source)}
comparison_rows = []
for arm in profile_rows:
    metric = arm["metric"]
    arm_value = arm["real_minus_control_per_helper"]
    x64_value = float(x64[metric]["real_minus_control_per_helper"])
    comparison_rows.append(
        {
            "metric": metric,
            "arm64_per_helper": arm_value,
            "x64_per_helper": x64_value,
            "arm64_minus_x64": arm_value - x64_value,
            "arm64_over_x64": arm_value / x64_value,
            "arm64_reduction_percent": (1 - arm_value / x64_value) * 100,
        }
    )

report = (ROOT / "raw-perf-record/perf-report.txt").read_text()
pattern = re.compile(
    r"^\s*([0-9.]+)%\s+([0-9]+)\s+.*?\[k\]\s+(\S+)\s*$", re.MULTILINE
)
reported = {symbol: (float(percent), int(samples)) for percent, samples, symbol in pattern.findall(report)}
perf_rows = []
for label, predicate in (
    ("array_map_update_elem", lambda symbol: symbol == "array_map_update_elem"),
    ("array_update_jit", lambda symbol: "kmr_array_update_real" in symbol),
    ("bpf_obj_memcpy", lambda symbol: symbol == "bpf_obj_memcpy"),
    ("bpf_obj_free_fields", lambda symbol: symbol == "bpf_obj_free_fields"),
    ("__memcpy_or_memcpy", lambda symbol: symbol in ("__memcpy", "memcpy")),
):
    matches = [value for symbol, value in reported.items() if predicate(symbol)]
    perf_rows.append(
        {
            "symbol_group": label,
            "overhead_percent": sum(value[0] for value in matches),
            "samples": sum(value[1] for value in matches),
            "interpretation": "observed" if matches else "zero samples in report",
        }
    )


def read_program_ids(directory):
    return dict(
        line.split("=", 1)
        for line in (directory / "program-ids.txt").read_text().splitlines()
    )


def count_jited_instructions(path):
    return sum(bool(re.match(r"^\s*[0-9a-f]+:", line)) for line in path.read_text().splitlines())


jit_rows = []
for architecture, directory in (
    ("arm64", ROOT / "raw-profile-cycles"),
    ("x64", X64 / "raw-profile-cycles"),
):
    ids = read_program_ids(directory)
    control_count = count_jited_instructions(
        directory / f"prog-{ids['control_id']}-jited.txt"
    )
    real_count = count_jited_instructions(
        directory / f"prog-{ids['real_id']}-jited.txt"
    )
    jit_rows.append(
        {
            "architecture": architecture,
            "control_static_jit_instructions": control_count,
            "real_static_jit_instructions": real_count,
            "real_minus_control_static_instructions": real_count - control_count,
        }
    )

arm_profile = {row["metric"]: row for row in profile_rows}
x64_profile = {
    metric: float(x64[metric]["real_minus_control_per_helper"])
    for metric in METRICS
}
derived_rows = []
for architecture, frequency_ghz, runtime_ns, cycles, instructions in (
    (
        "arm64",
        1.728,
        runtime_rows[0]["mean_ns_per_helper"],
        arm_profile["cycles"]["real_minus_control_per_helper"],
        arm_profile["instructions"]["real_minus_control_per_helper"],
    ),
    ("x64", 2.2, 32.877093, x64_profile["cycles"], x64_profile["instructions"]),
):
    derived_rows.append(
        {
            "architecture": architecture,
            "fixed_frequency_ghz": frequency_ghz,
            "runtime_ns_per_helper": runtime_ns,
            "runtime_times_frequency_cycles": runtime_ns * frequency_ghz,
            "profile_cycles_per_helper": cycles,
            "profile_instructions_per_helper": instructions,
            "net_instructions_per_cycle": instructions / cycles,
        }
    )

write_csv("profile-summary.csv", profile_rows)
write_csv("runtime-check.csv", runtime_rows)
write_csv("comparison-x64.csv", comparison_rows)
write_csv("perf-symbol-summary.csv", perf_rows)
write_csv("jit-static-summary.csv", jit_rows)
write_csv("derived-summary.csv", derived_rows)
