#!/usr/bin/env python3
"""Compare two C4 runs (e.g. the main session's two passes).

Usage: python3 -I compare_runs.py results/run-A results/run-B [OUT.md]
Cost: per item, run B's median ns per warp record / run A's (items valid in
both); the median ratio, the range, and the items that moved by more than
10%. Decisions: G* and the projection per shape, layout and protocol.
Correctness: each verdict string of run B against run A (same / differs).
"""
import csv
import json
import os
import statistics
import sys


def load_cost(run):
    p = os.path.join(run, "cost", "summary.csv")
    if not os.path.exists(p):
        return {}
    return {r["id"]: r for r in csv.DictReader(open(p))}


def load_json(run, *parts):
    p = os.path.join(run, *parts)
    return json.load(open(p)) if os.path.exists(p) else {}


def main():
    a, b = sys.argv[1], sys.argv[2]
    md = ["# C4: %s vs %s" % (os.path.basename(a.rstrip("/")), os.path.basename(b.rstrip("/"))), ""]
    ca, cb = load_cost(a), load_cost(b)
    ratios = []
    moved = []
    for k in sorted(set(ca) & set(cb)):
        if ca[k]["valid"] != "ok" or cb[k]["valid"] != "ok":
            continue
        x, y = float(ca[k]["ns_warp"]), float(cb[k]["ns_warp"])
        if x <= 0 or y <= 0:
            continue
        r = y / x
        ratios.append(r)
        if abs(r - 1) > 0.10:
            moved.append((k, x, y, r))
    if ratios:
        md += ["## Cost", "",
               "- %d items; B/A median %.3f (range %.2f-%.2f); %d moved by more than 10%%"
               % (len(ratios), statistics.median(ratios), min(ratios), max(ratios), len(moved)), ""]
        if moved:
            md += ["| item | A ns | B ns | B/A |", "|---|---:|---:|---:|"]
            md += ["| %s | %.2f | %.2f | %.2f |" % m for m in moved[:80]]
            md.append("")
    da, db = load_json(a, "cost", "derived.json"), load_json(b, "cost", "derived.json")
    if da and db:
        md += ["- rounds: A %s/%s%s, B %s/%s%s" % (
            da.get("rounds_done"), da.get("rounds_requested"), " PARTIAL" if da.get("partial") else "",
            db.get("rounds_done"), db.get("rounds_requested"), " PARTIAL" if db.get("partial") else ""), ""]
        md += ["## Cost decisions", "", "| shape | layout | protocol | A: G*, ns, % | B: G*, ns, % |", "|---|---|---|---|---|"]
        for shape, sd in da.get("shapes", {}).items():
            for layout, prot in sd.get("protocols", {}).items():
                for v, r in prot.items():
                    r2 = db.get("shapes", {}).get(shape, {}).get("protocols", {}).get(layout, {}).get(v, {})
                    f = lambda q: "%s, %.2f, %.1f" % (q.get("G_star"), q.get("ns", q.get("asymptote_ns", float("nan"))),
                                                       q.get("proj_pct", float("nan"))) if q else "-"
                    md.append("| %s | %s | %s | %s | %s |" % (shape, layout, v, f(r), f(r2)))
        md.append("")
    va, vb = load_json(a, "pubc", "verdicts.json"), load_json(b, "pubc", "verdicts.json")
    if va and vb:
        md += ["## Correctness verdicts", "",
               "- any protocol violation: A %s, B %s" % (va.get("any_protocol_violation"), vb.get("any_protocol_violation")),
               "- calls without a result: A %s, B %s" % (va.get("calls_without_result") or "none",
                                                         vb.get("calls_without_result") or "none"), ""]
        gb = {(g["layout"], g["G"]): g for g in vb.get("groups", [])}
        for g in va.get("groups", []):
            h = gb.get((g["layout"], g["G"]))
            if not h:
                md.append("- %s G=%d: only in A" % (g["layout"], g["G"]))
                continue
            for v, s in g["verdicts"].items():
                t = h["verdicts"].get(v)
                same = t is not None and s.split(":")[0] == t.split(":")[0]
                md.append("- %s G=%d %s: %s%s" % (g["layout"], g["G"], v, "same kind" if same else "DIFFERS",
                                                  "" if same else " (A: %s | B: %s)" % (s[:120], (t or "-")[:120])))
        md.append("")
    text = "\n".join(md) + "\n"
    if len(sys.argv) > 3:
        open(sys.argv[3], "w").write(text)
    print(text)


if __name__ == "__main__":
    main()
