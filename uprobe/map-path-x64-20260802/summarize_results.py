#!/usr/bin/env python3
import csv
import re
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EVENTS = (
    "task-clock",
    "cycles",
    "instructions",
    "branches",
    "branch-misses",
    "cache-references",
    "cache-misses",
)


def mean_wall(path: Path, metric: str) -> float:
    values = []
    pattern = re.compile(rf"^round=\d+ .*\b{metric}=([0-9.]+)")
    for line in path.read_text().splitlines():
        match = pattern.search(line)
        if match:
            values.append(float(match.group(1)))
    if len(values) != 3:
        raise ValueError(f"expected three formal rounds in {path}, got {len(values)}")
    return statistics.mean(values)


def perf(path: Path, denominator: int) -> dict[str, float]:
    result = {}
    with path.open(newline="") as handle:
        for row in csv.reader(handle):
            if len(row) < 3 or row[2] not in EVENTS:
                continue
            if row[0].startswith("<"):
                raise ValueError(f"uncounted event {row[2]} in {path}")
            value = float(row[0].replace(" ", ""))
            if row[2] == "task-clock":
                value = value * 1_000_000 / denominator
            else:
                value /= denominator
            result[row[2]] = value
    missing = set(EVENTS) - result.keys()
    if missing:
        raise ValueError(f"missing perf events {sorted(missing)} in {path}")
    return result


def sample(name: str, denominator: int, metric: str) -> dict[str, float]:
    result = {"wall_ns": mean_wall(ROOT / "raw-stdout" / f"{name}.txt", metric)}
    result.update(perf(ROOT / "raw-perf" / f"{name}.csv", denominator))
    return result


def write_jit() -> None:
    metrics = ("wall_ns",) + EVENTS
    rows = []
    for map_name, denominator in (("array", 301_000_000), ("hash", 61_000_000)):
        for operation in ("lookup", "update"):
            pair_rows = []
            for pair in range(1, 6):
                prefix = f"jit-{map_name}-{operation}-pair{pair}"
                noop = sample(f"{prefix}-noop", denominator, "ns_per_helper")
                real = sample(f"{prefix}-real", denominator, "ns_per_helper")
                row = {"map": map_name, "operation": operation, "stat": "pair", "pair": pair}
                for metric in metrics:
                    row[f"noop_{metric}"] = noop[metric]
                    row[f"real_{metric}"] = real[metric]
                    row[f"net_{metric}"] = real[metric] - noop[metric]
                rows.append(row)
                pair_rows.append(row)
            for stat, function in (("median", statistics.median), ("min", min), ("max", max)):
                row = {"map": map_name, "operation": operation, "stat": stat, "pair": ""}
                for metric in metrics:
                    for kind in ("noop", "real", "net"):
                        key = f"{kind}_{metric}"
                        row[key] = function(item[key] for item in pair_rows)
                rows.append(row)
    fields = ["map", "operation", "stat", "pair"]
    fields += [f"{kind}_{metric}" for metric in metrics for kind in ("noop", "real", "net")]
    with (ROOT / "jit-helper-ab.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def write_direct() -> None:
    metrics = ("wall_ns",) + EVENTS
    rows = []
    layers_by_map = {
        "array": ("control", "l0", "l1", "l2", "l3"),
        "hash": ("control", "lock", "l0", "l1", "l2", "l3"),
    }
    for map_name, layers in layers_by_map.items():
        denominator = 301_000_000 if map_name == "array" else 31_000_000
        for operation in ("lookup", "update"):
            control = None
            previous = None
            for layer in layers:
                values = sample(f"direct-{map_name}-{operation}-{layer}", denominator, "ns_per_op")
                if control is None:
                    control = values
                row = {"map": map_name, "operation": operation, "layer": layer}
                for metric in metrics:
                    row[metric] = values[metric]
                    row[f"net_vs_control_{metric}"] = values[metric] - control[metric]
                    row[f"increment_vs_previous_{metric}"] = (
                        0.0 if previous is None else values[metric] - previous[metric]
                    )
                rows.append(row)
                previous = values
    fields = ["map", "operation", "layer"]
    fields += [key for metric in metrics for key in (
        metric,
        f"net_vs_control_{metric}",
        f"increment_vs_previous_{metric}",
    )]
    with (ROOT / "direct-layers.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


write_jit()
write_direct()
