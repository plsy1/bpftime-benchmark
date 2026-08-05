#!/usr/bin/env python3
"""Parse the matched ARM64 ordinary/per-CPU helper diagnostic.

The wall-time files use paired noop/control and real measurements.  The PMU
files use the same pairing: BPFtime is real-noop, while kernel BPF is the
program-profile value per invocation (real-control).  All reported helper
values divide one BPF program invocation by the 1000 helper calls in its
diagnostic loop.
"""

from __future__ import annotations

import csv
import json
import math
import re
import statistics
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent
WALL_BPFTIME = ROOT / "raw-bpftime"
WALL_KERNEL = ROOT / "raw-kernel" / "kernel-runtime.csv"
PMU_BPFTIME = ROOT / "raw-bpftime"
PMU_KERNEL = ROOT / "raw-perf"

OPS = ("array_lookup", "array_update", "hash_lookup", "hash_update")
KINDS = ("array", "percpu_array", "hash", "percpu_hash")
SHORT = {
    "kpa": "array",
    "kpp": "percpu_array",
    "kha": "hash",
    "khp": "percpu_hash",
}


def op_name(kind: str, op: str) -> str:
    return f"{kind}_{op}"


def stats(values: list[float]) -> dict[str, float]:
    if not values:
        return {"n": 0, "mean": math.nan, "median": math.nan,
                "stdev": math.nan, "min": math.nan, "max": math.nan}
    return {
        "n": len(values),
        "mean": statistics.fmean(values),
        "median": statistics.median(values),
        "stdev": statistics.stdev(values) if len(values) > 1 else 0.0,
        "min": min(values),
        "max": max(values),
    }


def wall_stats() -> tuple[dict[tuple[str, str], dict[str, list[float]]], list[dict]]:
    values: dict[tuple[str, str], dict[str, list[float]]] = defaultdict(
        lambda: defaultdict(list)
    )
    rows: list[dict] = []
    with WALL_KERNEL.open(newline="") as stream:
        for row in csv.DictReader(stream):
            kind = row["map_kind"]
            raw_operation = row["operation"]
            operation = "lookup" if raw_operation.endswith("_lookup") else "update"
            key = (kind, operation)
            impl = row["implementation"]
            value = float(row["avg_ns_per_invocation"])
            values[key]["kernel_" + impl].append(value)
            rows.append({"engine": "kernel", "map_kind": kind,
                         "operation": operation, "implementation": impl,
                         "round": row["round"], "ns_per_helper": value})

    pair_re = re.compile(
        r"^round=(\d+) noop_ns_per_helper=([0-9.eE+-]+) "
        r"real_ns_per_helper=([0-9.eE+-]+) net_ns_per_helper=([0-9.eE+-]+)$"
    )
    for path in sorted(WALL_BPFTIME.glob("*-pair.txt")):
        header = ""
        for line in path.read_text().splitlines():
            if line.startswith("map_kind="):
                header = line
                break
        fields = dict(item.split("=", 1) for item in header.split())
        kind, operation = fields["map_kind"], fields["operation"]
        key = (kind, operation)
        for line in path.read_text().splitlines():
            match = pair_re.match(line)
            if not match:
                continue
            round_no, noop, real, net = match.groups()
            noop, real, net = map(float, (noop, real, net))
            values[key]["bpftime_noop"].append(noop)
            values[key]["bpftime_real"].append(real)
            values[key]["bpftime_net"].append(net)
            rows.extend([
                {"engine": "bpftime", "map_kind": kind,
                 "operation": operation, "implementation": "noop",
                 "round": round_no, "ns_per_helper": noop},
                {"engine": "bpftime", "map_kind": kind,
                 "operation": operation, "implementation": "real",
                 "round": round_no, "ns_per_helper": real},
                {"engine": "bpftime", "map_kind": kind,
                 "operation": operation, "implementation": "real-minus-noop",
                 "round": round_no, "ns_per_helper": net},
            ])
    return values, rows


def write_wall(values: dict[tuple[str, str], dict[str, list[float]]], rows: list[dict]) -> None:
    with (ROOT / "wall-raw.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=[
            "engine", "map_kind", "operation", "implementation",
            "round", "ns_per_helper",
        ], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    fields = ["engine", "map_kind", "operation", "measurement", "n",
              "mean_ns_per_helper", "median_ns_per_helper",
              "stdev_ns_per_helper", "min_ns_per_helper", "max_ns_per_helper"]
    with (ROOT / "wall-summary.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for (kind, operation), group in sorted(values.items()):
            for key, vals in sorted(group.items()):
                engine, measurement = key.split("_", 1)
                item = stats(vals)
                writer.writerow({
                    "engine": engine, "map_kind": kind,
                    "operation": operation, "measurement": measurement,
                    "n": item["n"],
                    "mean_ns_per_helper": f"{item['mean']:.9f}",
                    "median_ns_per_helper": f"{item['median']:.9f}",
                    "stdev_ns_per_helper": f"{item['stdev']:.9f}",
                    "min_ns_per_helper": f"{item['min']:.9f}",
                    "max_ns_per_helper": f"{item['max']:.9f}",
                })

    fields = ["map_kind", "operation", "bpftime_ordinary",
              "bpftime_percpu", "kernel_ordinary", "kernel_percpu",
              "bpftime_percpu_delta", "kernel_percpu_delta",
              "percpu_specific_gap"]
    with (ROOT / "wall-matched.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for operation in ("lookup", "update"):
            for base_kind in ("array", "hash"):
                pc_kind = "percpu_" + base_kind
                bpf_o = statistics.fmean(values[(base_kind, operation)]["bpftime_net"])
                bpf_p = statistics.fmean(values[(pc_kind, operation)]["bpftime_net"])
                ker_o = statistics.fmean(values[(base_kind, operation)]["kernel_real-minus-control"])
                ker_p = statistics.fmean(values[(pc_kind, operation)]["kernel_real-minus-control"])
                writer.writerow({
                    "map_kind": base_kind, "operation": operation,
                    "bpftime_ordinary": f"{bpf_o:.9f}",
                    "bpftime_percpu": f"{bpf_p:.9f}",
                    "kernel_ordinary": f"{ker_o:.9f}",
                    "kernel_percpu": f"{ker_p:.9f}",
                    "bpftime_percpu_delta": f"{bpf_p - bpf_o:.9f}",
                    "kernel_percpu_delta": f"{ker_p - ker_o:.9f}",
                    "percpu_specific_gap": f"{(bpf_p - bpf_o) - (ker_p - ker_o):.9f}",
                })


def parse_perf_file(path: Path, metric: str) -> float:
    pattern = re.compile(r"^([0-9]+(?:\.[0-9]+)?),," + re.escape(metric) + r",")
    for line in path.read_text().splitlines():
        match = pattern.match(line)
        if match:
            return float(match.group(1))
    raise RuntimeError(f"missing {metric} in {path}")


def pmu_bpftime() -> dict[tuple[str, str, str], dict[str, float]]:
    result: dict[tuple[str, str, str], dict[str, float]] = {}
    helpers = 50000.0 * 1000.0
    for path in sorted(PMU_BPFTIME.glob("*-noop.perf.txt")):
        stem = path.name.removesuffix("-noop.perf.txt")
        kind, operation = stem.rsplit("-", 1)
        result[(kind, operation, "noop")] = {
            metric: parse_perf_file(path, metric) / helpers
            for metric in ("cycles", "instructions")
        }
        real_path = PMU_BPFTIME / f"{kind}-{operation}-real.perf.txt"
        result[(kind, operation, "real")] = {
            metric: parse_perf_file(real_path, metric) / helpers
            for metric in ("cycles", "instructions")
        }
    return result


def kernel_name(name: str) -> tuple[str, str, str]:
    match = re.fullmatch(r"(kpa|kpp|kha|khp)_([lu])_(ctl|real)", name)
    if not match:
        raise RuntimeError(f"unrecognized kernel program name: {name}")
    prefix, operation, impl = match.groups()
    return SHORT[prefix], "lookup" if operation == "l" else "update", \
        "control" if impl == "ctl" else "real"


def pmu_kernel() -> dict[tuple[str, str, str, str], dict[str, float]]:
    result: dict[tuple[str, str, str, str], dict[str, float]] = {}
    for metric in ("cycles", "instructions"):
        for path in sorted((PMU_KERNEL / f"kernel-{metric}").glob("*.profile.json")):
            name = path.name.removesuffix(".profile.json")
            kind, operation, impl = kernel_name(name)
            item = json.loads(path.read_text())[0]
            if item["enabled"] != item["running"] or item["run_cnt"] <= 0:
                raise RuntimeError(f"invalid PMU multiplex/status: {path}")
            # bpftool value/run_cnt is one BPF program invocation.  The
            # diagnostic program performs 1000 identical helpers per invoke.
            result[(kind, operation, impl, metric)] = {
                "value_per_invocation": item["value"] / item["run_cnt"],
                "value_per_helper": item["value"] / item["run_cnt"] / 1000.0,
                "run_cnt": item["run_cnt"],
            }
    return result


def write_pmu() -> dict[tuple[str, str], dict[str, dict[str, float]]]:
    bpftime = pmu_bpftime()
    kernel = pmu_kernel()
    fields = ["engine", "metric", "map_kind", "operation", "implementation",
              "run_cnt", "raw_value", "value_per_invocation",
              "value_per_helper"]
    with (ROOT / "pmu-summary.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for (kind, operation, impl), vals in sorted(bpftime.items()):
            for metric, value in sorted(vals.items()):
                writer.writerow({"engine": "bpftime", "metric": metric,
                                 "map_kind": kind, "operation": operation,
                                 "implementation": impl, "run_cnt": "",
                                 "raw_value": f"{value * 50000 * 1000:.3f}",
                                 "value_per_invocation": "",
                                 "value_per_helper": f"{value:.9f}"})
        for (kind, operation, impl, metric), vals in sorted(kernel.items()):
            writer.writerow({"engine": "kernel", "metric": metric,
                             "map_kind": kind, "operation": operation,
                             "implementation": impl,
                             "run_cnt": vals["run_cnt"],
                             "raw_value": "",
                             "value_per_invocation": f"{vals['value_per_invocation']:.3f}",
                             "value_per_helper": f"{vals['value_per_helper']:.9f}"})

    fields = ["metric", "map_kind", "operation", "bpftime_ordinary",
              "bpftime_percpu", "kernel_ordinary", "kernel_percpu",
              "bpftime_percpu_delta", "kernel_percpu_delta",
              "percpu_specific_gap"]
    matched: dict[tuple[str, str], dict[str, dict[str, float]]] = {}
    with (ROOT / "pmu-matched.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for metric in ("cycles", "instructions"):
            for operation in ("lookup", "update"):
                for base_kind in ("array", "hash"):
                    pc_kind = "percpu_" + base_kind
                    bpf_o = bpftime[(base_kind, operation, "real")][metric] - bpftime[(base_kind, operation, "noop")][metric]
                    bpf_p = bpftime[(pc_kind, operation, "real")][metric] - bpftime[(pc_kind, operation, "noop")][metric]
                    ker_o = kernel[(base_kind, operation, "real", metric)]["value_per_helper"] - kernel[(base_kind, operation, "control", metric)]["value_per_helper"]
                    ker_p = kernel[(pc_kind, operation, "real", metric)]["value_per_helper"] - kernel[(pc_kind, operation, "control", metric)]["value_per_helper"]
                    row = {
                        "metric": metric, "map_kind": base_kind,
                        "operation": operation,
                        "bpftime_ordinary": f"{bpf_o:.9f}",
                        "bpftime_percpu": f"{bpf_p:.9f}",
                        "kernel_ordinary": f"{ker_o:.9f}",
                        "kernel_percpu": f"{ker_p:.9f}",
                        "bpftime_percpu_delta": f"{bpf_p - bpf_o:.9f}",
                        "kernel_percpu_delta": f"{ker_p - ker_o:.9f}",
                        "percpu_specific_gap": f"{(bpf_p - bpf_o) - (ker_p - ker_o):.9f}",
                    }
                    writer.writerow(row)
                    matched[(metric, op_name(base_kind, operation))] = {
                        key: float(value) for key, value in row.items()
                        if key not in ("metric", "map_kind", "operation")
                    }
    return matched


def main() -> None:
    values, rows = wall_stats()
    write_wall(values, rows)
    write_pmu()
    print("wrote wall-raw.csv wall-summary.csv wall-matched.csv pmu-summary.csv pmu-matched.csv")


if __name__ == "__main__":
    main()
