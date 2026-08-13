#!/usr/bin/env python3
import csv, re, statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MODES = ("base", "percpu_hash_lookup_no_find")
CASES = ("lookup_control", "percpu_hash_lookup")
RX = re.compile(r"^(lookup_control|percpu_hash_lookup),[^,]*,[^,]*,[^,]*,([0-9.]+)$", re.M)

def stats(v): return statistics.mean(v), statistics.median(v), statistics.stdev(v), min(v), max(v)

wall = {m: [] for m in MODES}
for mode in MODES:
    for path in sorted((ROOT / "raw/find-boundary/wall").glob(f"{mode}-run??.txt")):
        vals = {k: float(v) for k,v in RX.findall(path.read_text(errors="replace"))}
        wall[mode].append((vals["percpu_hash_lookup"] - vals["lookup_control"]) / 1000)
delta = [a-b for a,b in zip(wall[MODES[0]], wall[MODES[1]])]

pmu = {(m, metric): [] for m in MODES for metric in ("cycles", "instructions")}
for mode in MODES:
    for run in range(1,4):
        raw = {}
        for case in CASES:
            metrics = {}
            path = ROOT / f"raw/find-boundary/pmu/{mode}-{case}-run{run:02d}.perf.csv"
            for line in path.read_text().splitlines():
                row=line.split(",")
                if len(row)>=3 and row[0].replace(".","",1).isdigit() and row[2] in ("cycles","instructions"):
                    if len(row)>=5 and row[4] and float(row[4]) < 99.9: raise RuntimeError(f"multiplexed: {path}")
                    metrics[row[2]]=float(row[0])
            raw[case]=metrics
        for metric in ("cycles","instructions"):
            pmu[(mode,metric)].append((raw["percpu_hash_lookup"][metric]-raw["lookup_control"][metric])/(20000*1000))

with (ROOT/"find-boundary.csv").open("w",newline="") as f:
    w=csv.writer(f); w.writerow(("measurement","metric","mean","median","sample_sd","min","max"))
    w.writerow(("complete find production A/B","ns/helper",*(f"{x:.9f}" for x in stats(delta))))
    for metric in ("cycles","instructions"):
        d=[a-b for a,b in zip(pmu[(MODES[0],metric)],pmu[(MODES[1],metric)])]
        w.writerow(("complete find production A/B",f"{metric}/helper",*(f"{x:.9f}" for x in stats(d))))
print((ROOT/"find-boundary.csv").read_text(),end="")
