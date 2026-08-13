#!/usr/bin/env python3
import csv
import math
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ARRAY = HERE.parent / "helper-map-closure-arm64-20260812"
HASH = HERE.parent / "helper-hash-closure-arm64-20260812"

subprocess.run([str(HERE / "analyze.py")], check=True)

for family in (ARRAY, HASH):
    for environment in ("kernel", "bpftime"):
        wall = list((family / "raw" / environment).glob("victim-run??.txt"))
        assert len(wall) == 5, (family, environment, len(wall))
        perf = list((family / "raw-pmu" / environment).glob("*.perf.csv"))
        assert len(perf) == 48, (family, environment, len(perf))
        for path in perf:
            found = False
            with path.open() as handle:
                for row in csv.reader(handle):
                    if len(row) >= 5 and row[2] in ("cycles", "instructions"):
                        value = float(row[0])
                        running = float(row[4])
                        assert math.isfinite(value) and value > 0, path
                        assert running == 100.0, (path, running)
                        found = True
            assert found, path

with (HERE / "top-gap.csv").open() as handle:
    top = list(csv.DictReader(handle))
assert len(top) == 8
for row in top:
    assert float(row["gap_sample_sd"]) < 0.5, row
    assert math.isfinite(float(row["gap_instructions_per_helper"])), row

for family in (ARRAY, HASH):
    environment = (family / "environment.txt").read_text()
    assert "kernel_links=0" in environment
    assert "bpftime_shm_exists=no" in environment

(HERE / "validation.txt").write_text(
    "PASS\n"
    "- 5 wall-time runs per engine and family\n"
    "- 3 paired PMU rounds per metric/case (48 perf files per engine and family)\n"
    "- all PMU events 100% running\n"
    "- all wall gap sample SD values below 0.5 ns/helper\n"
    "- final kernel link count 0 and BPFtime shared memory absent\n"
)
print((HERE / "validation.txt").read_text(), end="")
