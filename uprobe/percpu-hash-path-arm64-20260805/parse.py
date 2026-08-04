#!/usr/bin/env python3
"""Parse the per-CPU hash path wall-clock and PMU captures."""

from __future__ import annotations

import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
NS_RE = re.compile(r"ns_per_op=([0-9]+(?:\.[0-9]+)?)")
PMU_RE = re.compile(
    r"^([0-9]+(?:\.[0-9]+)?),([^,]*),(task-clock|cycles|instructions|branches|branch-misses),"
)
WALL_LAYERS = {
    "lookup": ("control", "sched", "key_copy", "hash_find", "value_select", "l0", "l1", "l2", "l3"),
    "update": ("control", "sched", "key_copy", "value_copy", "hash_find", "copy_existing", "l0", "l1", "l2", "l3"),
    "delete": ("control", "sched", "key_copy", "hash_find", "l0", "l1", "l2", "l3"),
}
PMU_LAYERS = {
    "lookup": WALL_LAYERS["lookup"],
    "update": WALL_LAYERS["update"],
}


def stat_row(values: list[float]) -> dict[str, float]:
    return {
        "n": len(values),
        "mean_ns_per_op": statistics.mean(values),
        "median_ns_per_op": statistics.median(values),
        "stdev_ns_per_op": statistics.stdev(values) if len(values) > 1 else 0.0,
        "min_ns_per_op": min(values),
        "max_ns_per_op": max(values),
    }


def parse_wall() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for operation, layers in WALL_LAYERS.items():
        for layer in layers:
            path = ROOT / "raw-wall" / f"{operation}-{layer}.txt"
            values = [float(m.group(1)) for m in NS_RE.finditer(path.read_text())]
            if len(values) != 5:
                raise RuntimeError(f"{path}: expected 5 wall rounds, got {len(values)}")
            row: dict[str, object] = {"operation": operation, "layer": layer}
            row.update(stat_row(values))
            rows.append(row)
    controls = {
        operation: next(
            r["mean_ns_per_op"]
            for r in rows
            if r["operation"] == operation and r["layer"] == "control"
        )
        for operation in WALL_LAYERS
    }
    for row in rows:
        row["delta_from_control_ns_per_op"] = (
            float(row["mean_ns_per_op"]) - float(controls[row["operation"]])
        )
    return rows


def parse_pmu() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for operation, layers in PMU_LAYERS.items():
        for layer in layers:
            path = ROOT / "raw-pmu" / f"{operation}-{layer}.txt"
            events: dict[str, float] = {}
            for line in path.read_text().splitlines():
                match = PMU_RE.match(line.strip())
                if match:
                    events[match.group(3)] = float(match.group(1))
            required = ("task-clock", "cycles", "instructions", "branches", "branch-misses")
            missing = set(required) - set(events)
            if missing:
                raise RuntimeError(f"{path}: missing PMU events {sorted(missing)}")
            operations = 50_000_000
            rows.append({
                "operation": operation,
                "layer": layer,
                "task_clock_msec": events["task-clock"],
                "cycles": events["cycles"],
                "instructions": events["instructions"],
                "branches": events["branches"],
                "branch_misses": events["branch-misses"],
                "cycles_per_op": events["cycles"] / operations,
                "instructions_per_op": events["instructions"] / operations,
                "branches_per_op": events["branches"] / operations,
                "branch_misses_per_op": events["branch-misses"] / operations,
                "ipc": events["instructions"] / events["cycles"],
            })
    return rows


def parse_ordinary_wall() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for operation in ("lookup", "update"):
        for layer in ("control", "l0", "l1", "l2", "l3"):
            path = ROOT / "raw-ordinary-wall" / f"{operation}-{layer}.txt"
            values = [float(m.group(1)) for m in NS_RE.finditer(path.read_text())]
            if len(values) != 5:
                raise RuntimeError(f"{path}: expected 5 wall rounds, got {len(values)}")
            row: dict[str, object] = {"operation": operation, "layer": layer}
            row.update(stat_row(values))
            rows.append(row)
    controls = {
        operation: next(
            r["mean_ns_per_op"]
            for r in rows
            if r["operation"] == operation and r["layer"] == "control"
        )
        for operation in ("lookup", "update")
    }
    for row in rows:
        row["delta_from_control_ns_per_op"] = (
            float(row["mean_ns_per_op"]) - float(controls[row["operation"]])
        )
    return rows


def parse_ordinary_pmu() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for operation in ("lookup", "update"):
        for layer in ("control", "l0", "l1", "l2", "l3"):
            path = ROOT / "raw-ordinary-pmu" / f"{operation}-{layer}.txt"
            events: dict[str, float] = {}
            for line in path.read_text().splitlines():
                match = PMU_RE.match(line.strip())
                if match:
                    events[match.group(3)] = float(match.group(1))
            required = ("task-clock", "cycles", "instructions", "branches", "branch-misses")
            missing = set(required) - set(events)
            if missing:
                raise RuntimeError(f"{path}: missing PMU events {sorted(missing)}")
            operations = 50_000_000
            rows.append({
                "operation": operation,
                "layer": layer,
                "task_clock_msec": events["task-clock"],
                "cycles": events["cycles"],
                "instructions": events["instructions"],
                "branches": events["branches"],
                "branch_misses": events["branch-misses"],
                "cycles_per_op": events["cycles"] / operations,
                "instructions_per_op": events["instructions"] / operations,
                "branches_per_op": events["branches"] / operations,
                "branch_misses_per_op": events["branch-misses"] / operations,
                "ipc": events["instructions"] / events["cycles"],
            })
    return rows


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    fields = list(rows[0])
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    wall = parse_wall()
    pmu = parse_pmu()
    ordinary_wall = parse_ordinary_wall()
    ordinary_pmu = parse_ordinary_pmu()
    write_csv(ROOT / "wall-summary.csv", wall)
    write_csv(ROOT / "pmu-summary.csv", pmu)
    write_csv(ROOT / "ordinary-wall-summary.csv", ordinary_wall)
    write_csv(ROOT / "ordinary-pmu-summary.csv", ordinary_pmu)
    with (ROOT / "key-results.csv").open("w", newline="") as stream:
        fields = ["operation", "layer", "mean_ns_per_op", "delta_from_control_ns_per_op", "instructions_per_op"]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        pmu_index = {(r["operation"], r["layer"]): r for r in pmu}
        for row in wall:
            if row["operation"] in PMU_LAYERS:
                pmu_row = pmu_index[(row["operation"], row["layer"])]
                writer.writerow({
                    "operation": row["operation"],
                    "layer": row["layer"],
                    "mean_ns_per_op": row["mean_ns_per_op"],
                    "delta_from_control_ns_per_op": row["delta_from_control_ns_per_op"],
                    "instructions_per_op": pmu_row["instructions_per_op"],
                })
            else:
                writer.writerow({
                    "operation": row["operation"],
                    "layer": row["layer"],
                    "mean_ns_per_op": row["mean_ns_per_op"],
                    "delta_from_control_ns_per_op": row["delta_from_control_ns_per_op"],
                    "instructions_per_op": "not measured",
                })

    per_wall = {(r["operation"], r["layer"]): r for r in wall}
    per_pmu = {(r["operation"], r["layer"]): r for r in pmu}
    ord_wall = {(r["operation"], r["layer"]): r for r in ordinary_wall}
    ord_pmu = {(r["operation"], r["layer"]): r for r in ordinary_pmu}
    with (ROOT / "shared-layer-comparison.csv").open("w", newline="") as stream:
        fields = [
            "operation", "layer", "ordinary_mean_ns_per_op", "percpu_mean_ns_per_op",
            "percpu_minus_ordinary_ns_per_op", "ordinary_instructions_per_op",
            "percpu_instructions_per_op", "percpu_minus_ordinary_instructions_per_op",
        ]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for operation in ("lookup", "update"):
            for layer in ("l0", "l1", "l2", "l3"):
                writer.writerow({
                    "operation": operation,
                    "layer": layer,
                    "ordinary_mean_ns_per_op": ord_wall[(operation, layer)]["mean_ns_per_op"],
                    "percpu_mean_ns_per_op": per_wall[(operation, layer)]["mean_ns_per_op"],
                    "percpu_minus_ordinary_ns_per_op": (
                        float(per_wall[(operation, layer)]["mean_ns_per_op"])
                        - float(ord_wall[(operation, layer)]["mean_ns_per_op"])
                    ),
                    "ordinary_instructions_per_op": ord_pmu[(operation, layer)]["instructions_per_op"],
                    "percpu_instructions_per_op": per_pmu[(operation, layer)]["instructions_per_op"],
                    "percpu_minus_ordinary_instructions_per_op": (
                        float(per_pmu[(operation, layer)]["instructions_per_op"])
                        - float(ord_pmu[(operation, layer)]["instructions_per_op"])
                    ),
                })


if __name__ == "__main__":
    main()
