#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RESULTS = ROOT.parent


def rows(path: Path):
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


arm_jit = {}
for map_name, directory in (
    ("array", "array-path-arm-diagnosis-20260801"),
    ("hash", "hash-path-arm-diagnosis-20260801"),
):
    for row in rows(RESULTS / directory / "jit-helper-ab.csv"):
        if row["implementation"].endswith("-minus-noop"):
            arm_jit[(map_name, row["operation"])] = {
                "ns": float(row["mean_ns_per_helper"]),
                "cycles": float(row["delta_cycles_per_helper"]),
                "instructions": float(row["delta_instructions_per_helper"]),
            }

x64_jit = {}
for row in rows(ROOT / "jit-helper-ab.csv"):
    if row["stat"] == "median":
        x64_jit[(row["map"], row["operation"])] = {
            "ns": float(row["net_wall_ns"]),
            "cycles": float(row["net_cycles"]),
            "instructions": float(row["net_instructions"]),
        }

kernel = {"arm64": {}, "x64": {}}
for arch, directory in (
    ("arm64", "kernel-map-runtime-arm64-20260801"),
    ("x64", "kernel-map-runtime-x64-20260802"),
):
    for row in rows(RESULTS / directory / "summary.csv"):
        kernel[arch][row["operation"]] = float(row["mean_ns_per_helper"])

jit_fields = ["map", "operation"]
for metric in ("ns", "cycles", "instructions"):
    jit_fields += [f"arm64_{metric}_per_helper", f"x64_{metric}_per_helper",
                   f"x64_minus_arm64_{metric}_per_helper", f"x64_over_arm64_{metric}"]
jit_fields += ["arm64_kernel_ns_per_helper", "x64_kernel_ns_per_helper",
               "x64_minus_arm64_kernel_ns_per_helper", "x64_over_arm64_kernel_ns",
               "arm64_bpftime_over_kernel", "x64_bpftime_over_kernel"]

with (ROOT / "jit-cross-architecture.csv").open("w", newline="") as handle:
    writer = csv.DictWriter(handle, fieldnames=jit_fields)
    writer.writeheader()
    for map_name, operation in (("array", "lookup"), ("array", "update"),
                                ("hash", "lookup"), ("hash", "update")):
        arm = arm_jit[(map_name, operation)]
        x64 = x64_jit[(map_name, operation)]
        row = {"map": map_name, "operation": operation}
        for metric in ("ns", "cycles", "instructions"):
            row[f"arm64_{metric}_per_helper"] = arm[metric]
            row[f"x64_{metric}_per_helper"] = x64[metric]
            row[f"x64_minus_arm64_{metric}_per_helper"] = x64[metric] - arm[metric]
            row[f"x64_over_arm64_{metric}"] = x64[metric] / arm[metric]
        key = f"{map_name}_{operation}"
        arm_kernel = kernel["arm64"][key]
        x64_kernel = kernel["x64"][key]
        row["arm64_kernel_ns_per_helper"] = arm_kernel
        row["x64_kernel_ns_per_helper"] = x64_kernel
        row["x64_minus_arm64_kernel_ns_per_helper"] = x64_kernel - arm_kernel
        row["x64_over_arm64_kernel_ns"] = x64_kernel / arm_kernel
        row["arm64_bpftime_over_kernel"] = arm["ns"] / arm_kernel
        row["x64_bpftime_over_kernel"] = x64["ns"] / x64_kernel
        writer.writerow(row)


def direct_map(path: Path, arm: bool, map_name: str):
    result = {}
    for row in rows(path):
        if not arm and row["map"] != map_name:
            continue
        key = (row["operation"], row["layer"])
        if arm:
            result[key] = {
                "ns": float(row["net_ns_vs_control"]),
                "cycles": float(row["net_cycles_vs_control"]),
                "instructions": float(row["net_instructions_vs_control"]),
            }
        else:
            result[key] = {
                "ns": float(row["net_vs_control_wall_ns"]),
                "cycles": float(row["net_vs_control_cycles"]),
                "instructions": float(row["net_vs_control_instructions"]),
            }
    return result


layer_fields = ["map", "operation", "stage", "from_layer", "to_layer"]
for metric in ("ns", "cycles", "instructions"):
    layer_fields += [f"arm64_{metric}_per_op", f"x64_{metric}_per_op",
                     f"x64_minus_arm64_{metric}_per_op"]

with (ROOT / "layer-cross-architecture.csv").open("w", newline="") as handle:
    writer = csv.DictWriter(handle, fieldnames=layer_fields)
    writer.writeheader()
    for map_name, directory, chain in (
        ("array", "array-path-arm-diagnosis-20260801",
         (("map_implementation", "control", "l0"),
          ("handler", "l0", "l1"), ("shared_memory_fd", "l1", "l2"),
          ("final_helper", "l2", "l3"))),
        ("hash", "hash-path-arm-diagnosis-20260801",
         (("spin_lock", "control", "lock"), ("map_implementation", "lock", "l0"),
          ("handler", "l0", "l1"), ("shared_memory_fd", "l1", "l2"),
          ("final_helper", "l2", "l3"))),
    ):
        arm = direct_map(RESULTS / directory / "direct-layers.csv", True, map_name)
        x64 = direct_map(ROOT / "direct-layers.csv", False, map_name)
        for operation in ("lookup", "update"):
            for stage, previous, current in chain:
                row = {"map": map_name, "operation": operation, "stage": stage,
                       "from_layer": previous, "to_layer": current}
                for metric in ("ns", "cycles", "instructions"):
                    arm_delta = arm[(operation, current)][metric] - arm[(operation, previous)][metric]
                    x64_delta = x64[(operation, current)][metric] - x64[(operation, previous)][metric]
                    row[f"arm64_{metric}_per_op"] = arm_delta
                    row[f"x64_{metric}_per_op"] = x64_delta
                    row[f"x64_minus_arm64_{metric}_per_op"] = x64_delta - arm_delta
                writer.writerow(row)
