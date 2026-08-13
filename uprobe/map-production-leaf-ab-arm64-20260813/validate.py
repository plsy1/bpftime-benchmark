#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def rows(name):
    return list(csv.DictReader((ROOT / name).open()))

assert len(rows("wall-raw.csv")) == 400
assert len(rows("wall-summary.csv")) == 80
assert len(rows("effects.csv")) == 20
assert len(rows("pmu-raw.csv")) == 180
assert len(rows("pmu-effects.csv")) == 40
assert len(rows("delete-raw.csv")) == 10
assert len(rows("delete-summary.csv")) == 3
assert len(rows("attribution.csv")) == 21

for r in rows("effects.csv"):
    assert float(r["mean_saving_ns_per_helper"]) > 0, r
for r in rows("pmu-effects.csv"):
    assert float(r["mean_saving_per_helper"]) > 0, r

for family in ("array", "hash"):
    for path in (ROOT / "raw" / family).glob("*-run??.txt"):
        text = path.read_text(errors="replace")
        assert "shm_open_type 1" in text, path
        assert "Segmentation fault" not in text, path
for path in (ROOT / "raw" / "delete").glob("*-run??.txt"):
    text = path.read_text(errors="replace")
    assert "shm_open_type 1" in text, path
    assert "Benchmarking __bench_per_cpu_hash_map_delete" in text, path
    assert "Segmentation fault" not in text, path

environment = (ROOT / "environment.txt").read_text()
assert "kernel_links=0" in environment
assert "bpftime_shm_exists=no" in environment
assert (ROOT / "source.patch").stat().st_size > 1000
print("PASS: counts, positive paired effects, agent attachment, cleanup, and source capture validated")
