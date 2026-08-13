#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def read_effects(path, key, metric=None):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        if row["effect"] == key and (metric is None or row.get("metric") == metric):
            field = "mean_saving_ns_per_helper" if metric is None else "mean_saving_per_helper"
            return float(row[field])
    raise KeyError((path, key, metric))


def read_boundary(metric):
    with (ROOT / "find-boundary.csv").open(newline="") as stream:
        for row in csv.DictReader(stream):
            if row["metric"] == metric:
                return float(row["mean"])
    raise KeyError(metric)


wall_path = ROOT / "wall-effects.csv"
pmu_path = ROOT / "pmu-effects.csv"

labels = {
    "key": "key vector assign above fixed memcpy",
    "hash_normal": "normal hasher above exact cached hash (normal equality)",
    "equal_normal": "generic vector equality above fixed4 equality (normal hash)",
    "hash_fixed": "normal hasher above exact cached hash (fixed4 equality)",
    "equal_cached": "generic vector equality above fixed4 equality (cached hash)",
}


def components(metric):
    if metric == "ns/helper":
        effect = lambda name: read_effects(wall_path, labels[name])
    else:
        pmu_metric = metric.split("/")[0]
        effect = lambda name: read_effects(pmu_path, labels[name], pmu_metric)

    full_find = read_boundary(metric)
    hash_leaf = effect("hash_fixed")
    equality_leaf = effect("equal_cached")
    interaction = effect("hash_normal") - hash_leaf
    container_remainder = full_find - hash_leaf - equality_leaf - interaction
    return {
        "key_assign_over_fixed_memcpy": effect("key"),
        "complete_impl_find": full_find,
        "normal_hash_leaf": hash_leaf,
        "generic_equality_leaf": equality_leaf,
        "hash_equality_interaction": interaction,
        "boost_container_remainder": container_remainder,
    }


rows = []
for metric in ("ns/helper", "cycles/helper", "instructions/helper"):
    values = components(metric)
    full_find = values["complete_impl_find"]
    for component, value in values.items():
        share = ""
        if component not in ("key_assign_over_fixed_memcpy", "complete_impl_find"):
            share = f"{100.0 * value / full_find:.6f}"
        rows.append((component, metric, f"{value:.9f}", share))

with (ROOT / "leaf-attribution.csv").open("w", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(("component", "metric", "mean_effect", "share_of_complete_find_percent"))
    writer.writerows(rows)

print((ROOT / "leaf-attribution.csv").read_text(), end="")
