#!/usr/bin/env python3
"""Tables for the official-vs-dev GPU benchmark comparison: summarize.py > summary.md

Rows: the upstream case names. Columns per Orin run set (A official, B dev: 3 rounds; C dev with
the stock probe and the default victim; D official as-is): median us [min-max], ratio to the
case's own baseline in the same run (median over rounds), and a status from verify_runs.py's
checks. Upstream RTX 5090 / P40 rows are parsed from the committed result tables.
"""
import csv
import json
import re
import statistics
from collections import defaultdict
from pathlib import Path

R = Path("/home/jetson/src/results/gpu/compare-20261110")
UP = Path("/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/official/benchmark/gpu/micro")


def load_runs(run):
    out = {}
    for kind in ("micro", "examples"):
        p = R / "runs" / run / f"{kind}_vec_add_result.json"
        if not p.exists():
            continue
        d = json.loads(p.read_text())
        times = {t["name"]: t.get("avg_time_us") for t in d["tests"]}
        for t in d["tests"]:
            base = t.get("baseline") or ""
            out[(kind, t["name"])] = (t.get("avg_time_us"), times.get(base))
    return out


def checks():
    rows = {}
    for f in ("phase2-verify.csv",):
        p = R / f
        if not p.exists():
            continue
        for r in csv.DictReader(p.open()):
            rows[(r["run"], r["config"].replace("_vec_add", ""), r["case"])] = r
    return rows


def status(chk, run, kind, name, official):
    r = chk.get((run, kind, name))
    if r is None:
        return "?"
    if name.startswith("Baseline"):
        return "ok"
    if r["fail"]:
        if "Unable to initialize shared memory" in r["fail"] or "failed to create" in r["fail"]:
            return "FAIL: map 1502/1503 not created (needs CUDA IPC)"
        return "FAIL:" + r["fail"]
    if r["valid"] != "1":
        return "wrong result"
    if r["ended"] != "1":
        return "victim did not finish"
    if official:
        if int(r["off_record"] or 0) == 0 and "memtrace" not in name.lower() and "Memory" not in name:
            return "no probe recorded"
    else:
        if int(r["dev_patched"] or 0) == 0 and "memtrace" not in name.lower() and "Memory" not in name:
            return "no patched image"
    if kind == "examples" and not r["loader"]:
        return "not instrumented (loader printed nothing)"
    return "ok" + (f" ({r['loader']})" if r["loader"] else "")


def upstream(md):
    rows = {}
    p = UP / md
    if not p.exists():
        return rows
    for line in p.read_text().splitlines():
        m = re.match(r"\|\s*(.+?)\s*\|\s*(\S+)\s*\|\s*([\d.]+)\s*\|\s*(-|[\d.]+)\s*\|", line)
        if m:
            t = float(m.group(3)); b = m.group(4)
            rows[m.group(1)] = (t, (t / float(b)) if b != "-" else None)
    return rows


def fmt_set(vals):
    vals = [v for v in vals if v is not None]
    if not vals:
        return "-", None
    med = statistics.median(vals)
    if len(vals) == 1:
        return f"{med:,.1f}", med
    return f"{med:,.1f} [{min(vals):,.1f}-{max(vals):,.1f}]", med


def main():
    chk = checks()
    sets = {"A": ["A1", "A2", "A3"], "B": ["B1", "B2", "B3"], "C": ["C"], "D": ["D"]}
    data = {k: [load_runs(r) for r in v] for k, v in sets.items()}
    up = {"micro": (upstream("micro_vec_add_result.md"), upstream("micro_vec_add_result_p40.md")),
          "examples": (upstream("examples_vec_add_result.md"), upstream("examples_vec_add_result_p40.md"))}
    print("# Official GPU benchmark on the Orin: official 4708f409 (A) vs dev c115b33d (B)\n")
    for kind in ("micro", "examples"):
        names = []
        for runs in data.values():
            for d in runs:
                for (k, n) in d:
                    if k == kind and n not in names:
                        names.append(n)
        print(f"\n## {kind}_vec_add\n")
        print("| case | A official us | A ratio | A status | B dev us | B ratio | B status | C dev default us | C ratio | C status | D official as-is us | D status | 5090 us (ratio) | P40 us (ratio) |")
        print("|---|---:|---:|---|---:|---:|---|---:|---:|---|---:|---|---:|---:|")
        for n in names:
            cells = [n]
            for key in ("A", "B", "C", "D"):
                runs = data[key]
                ts = [d.get((kind, n), (None, None))[0] for d in runs]
                rs = [(d[(kind, n)][0] / d[(kind, n)][1]) if (kind, n) in d and d[(kind, n)][0] and d[(kind, n)][1] else None for d in runs]
                tstr, _ = fmt_set(ts)
                rstr, _ = fmt_set(rs)
                sts = sorted({status(chk, r, kind, n, key in ("A", "D")) for r in sets[key]})
                if key == "D":
                    cells += [tstr, " / ".join(sts)]
                else:
                    cells += [tstr, rstr if rstr != "-" else "-", " / ".join(sts)]
            for u in up[kind]:
                v = u.get(n)
                cells.append(f"{v[0]:,.1f} ({v[1]:.2f})" if v and v[1] else (f"{v[0]:,.1f}" if v else "-"))
            print("| " + " | ".join(cells) + " |")


main()
