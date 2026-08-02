#!/usr/bin/env python3
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
METRICS = ("cycles", "instructions", "l1d_loads", "llc_misses")

rows = []
for metric in METRICS:
    directory = ROOT / f"raw-profile-{metric}"
    control = json.loads((directory / "control-profile.json").read_text())[0]
    real = json.loads((directory / "real-profile.json").read_text())[0]
    if control["metric"] != metric or real["metric"] != metric:
        raise RuntimeError(f"{metric}: unexpected metric name")
    if control["run_cnt"] != real["run_cnt"]:
        raise RuntimeError(f"{metric}: control/real run count mismatch")
    if control["enabled"] != control["running"]:
        raise RuntimeError(f"{metric}: control event was multiplexed")
    if real["enabled"] != real["running"]:
        raise RuntimeError(f"{metric}: real event was multiplexed")

    run_count = control["run_cnt"]
    control_per_run = control["value"] / run_count
    real_per_run = real["value"] / run_count
    rows.append(
        {
            "metric": metric,
            "run_count": run_count,
            "control_value": control["value"],
            "real_value": real["value"],
            "control_per_program_run": control_per_run,
            "real_per_program_run": real_per_run,
            "real_minus_control_per_helper": (real_per_run - control_per_run) / 1000,
            "control_enabled": control["enabled"],
            "control_running": control["running"],
            "real_enabled": real["enabled"],
            "real_running": real["running"],
        }
    )

with (ROOT / "profile-summary.csv").open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=rows[0].keys(), lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
