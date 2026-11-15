#!/usr/bin/env python3
"""Compare two C3 runs (the main session's two passes).

Usage: python3 -I compare_runs.py RUN_DIR_1 RUN_DIR_2 [OUT.md]
Reads each run's summary.csv and derived.json; prints (and optionally
writes) the per-item ratio of ns per thread-op (pass 2 / pass 1), the items
that moved by more than 10%, and both passes' decisions side by side.
"""
import csv
import json
import os
import statistics
import sys


def load(d):
    s = {r["id"]: r for r in csv.DictReader(open(os.path.join(d, "summary.csv")))}
    return s, json.load(open(os.path.join(d, "derived.json")))


def main():
    a, da = load(sys.argv[1])
    b, db = load(sys.argv[2])
    L = ["# C3: pass 1 (%s) vs pass 2 (%s)\n" % (sys.argv[1], sys.argv[2])]
    ratios, moved = [], []
    for i in sorted(a):
        x, y = a[i].get("ns_per_thread_op_med"), b.get(i, {}).get("ns_per_thread_op_med")
        if not x or not y or a[i]["case"] == "empty":
            continue
        x, y = float(x), float(y)
        if x <= 0:
            continue
        r = y / x
        ratios.append(r)
        if abs(r - 1) > 0.10:
            moved.append((i, x, y, r))
    if ratios:
        L.append("- %d items, pass 2 / pass 1 median %.3f (%.2f-%.2f); %d moved by more than 10%%" % (
            len(ratios), statistics.median(ratios), min(ratios), max(ratios), len(moved)))
    for i, x, y, r in moved:
        L.append("  - %s: %.4f -> %.4f ns (%.2f)" % (i, x, y, r))
    L.append("- run status: pass 1 %s; pass 2 %s" % (
        "valid" if da.get("run_valid") else "; ".join(da.get("run_problems", ["unknown"])),
        "valid" if db.get("run_valid") else "; ".join(db.get("run_problems", ["unknown"]))))
    L.append("- gate: pass 1 %s, pass 2 %s" % (da["gate"]["comparable_with_step0"], db["gate"]["comparable_with_step0"]))
    L.append("- decision (a) primary:\n  - pass 1: %s\n  - pass 2: %s" % (
        da["decision_a"]["primary"]["verdict"], db["decision_a"]["primary"]["verdict"]))
    for k in ("N_pct", "IC_pct", "I_pct", "K_pct", "M_pct", "F_pct", "store_pct", "E_pct"):
        L.append("  - %s: %.2f / %.2f" % (k, da["decision_a"]["primary"].get(k) or float("nan"),
                                          db["decision_a"]["primary"].get(k) or float("nan")))
    L.append("- decision (b): pass 1 %s\n  pass 2 %s" % (json.dumps(da["decision_b"]), json.dumps(db["decision_b"])))
    for s in da["decision_d"]:
        for k, v in da["decision_d"][s].items():
            w = db["decision_d"][s][k]
            L.append("- (d) %s %s: %s / %s" % (s, k, v["verdict"], w["verdict"]))
    text = "\n".join(L) + "\n"
    print(text)
    if len(sys.argv) > 3:
        open(sys.argv[3], "w").write(text)


if __name__ == "__main__":
    main()
