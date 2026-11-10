#!/usr/bin/env python3
"""Ablation tables: summarize_ablate.py > summary.md

Per case and set (O official, D dev, L late path, F 32 frames, G no gate, T official array/ringbuf
device code), each round gives the victim's mean and median over its timed iterations (the
'Detailed statistics' of the runner log). Per set: the median over rounds of each, and the
range of the round means. For each ablation X: shift = median(X) - median(D), the share of the
official-dev gap it closes = (X - D) / (O - D), and a two-sided Mann-Whitney p over the round
values of X against D (and of O against D).
"""
import lzma
import re
import statistics
import sys
from pathlib import Path
from scipy.stats import mannwhitneyu

R = Path("/home/jetson/src/results/gpu/ablate-20261110/runs")
SETS = ["O", "D", "L", "F", "G", "T"]
NAMES = {"O": "official", "D": "dev", "L": "dev, late path (811b841f off)", "F": "dev, 32 GPU stack frames (636501ff off)",
         "G": "dev, no probe gate (5c5312d7 off)", "T": "dev, official array-map and ringbuf-full device code (518f05b5 parts off)"}
# which ablation answers which rows (the others are shown for reference)
TARGET = {"L": "launch path", "F": "frame", "G": "gate", "T": "array map / ringbuf device code"}


def read_log(p):
    if p.with_suffix(p.suffix + ".xz").exists():
        return lzma.open(p.with_suffix(p.suffix + ".xz"), "rt", errors="replace").read()
    return p.read_text(errors="replace")


def stats(run, kind):
    p = R / run / f"{kind}_vec_add_bench.log"
    if not p.exists() and not p.with_suffix(".log.xz").exists():
        return {}
    log = read_log(p)
    out = {}
    parts = re.split(r"^.*Running test: (.+)$", log, flags=re.M)
    for i in range(1, len(parts), 2):
        sec = parts[i + 1]
        m = re.search(r"Mean:\s+([\d.]+).*?Median:\s+([\d.]+).*?Min:\s+([\d.]+).*?P95:\s+([\d.]+)", sec, re.S)
        if m:
            out[parts[i].strip()] = tuple(float(x) for x in m.groups())
    return out


def main():
    rounds = sorted({int(p.name[1:]) for p in R.iterdir() if re.fullmatch(r"[ODLFGT]\d+", p.name)})
    data = {}  # (kind, case, set) -> list of (mean, median, min, p95)
    for s in SETS:
        for r in rounds:
            for kind in ("micro", "examples"):
                for case, v in stats(f"{s}{r}", kind).items():
                    data.setdefault((kind, case, s), []).append(v)
    print(f"# Ablations on the Orin: which dev change moves each case ({len(rounds)} rounds)\n")
    for s in SETS:
        print(f"- **{s}**: {NAMES[s]}")
    for idx, label in ((0, "round means (the benchmark's number)"), (1, "round medians")):
        print(f"\n## {label}\n")
        print("Per set: median over rounds [range]. Ablation columns: shift vs D in us, share of the O-D gap closed, Mann-Whitney p vs D.\n")
        print("| kind | case | O | D | O-D (p) | " + " | ".join(f"{x}: value, shift, share, p" for x in "LFGT") + " |")
        print("|---|---|---:|---:|---:|" + "---:|" * 4)
        cases = []
        for (kind, case, s) in data:
            if (kind, case) not in cases:
                cases.append((kind, case))
        for kind, case in cases:
            def vals(s):
                return [v[idx] for v in data.get((kind, case, s), [])]
            o, d = vals("O"), vals("D")
            if not o or not d:
                continue
            mo, md = statistics.median(o), statistics.median(d)
            gap = mo - md
            p_od = mannwhitneyu(o, d).pvalue if len(o) > 2 and len(d) > 2 else float("nan")
            cells = [kind, case, f"{mo:,.1f} [{min(o):,.1f}-{max(o):,.1f}]", f"{md:,.1f} [{min(d):,.1f}-{max(d):,.1f}]",
                     f"{gap:+,.1f} ({p_od:.3f})"]
            for x in "LFGT":
                xv = vals(x)
                if not xv:
                    cells.append("-")
                    continue
                mx = statistics.median(xv)
                shift = mx - md
                share = f"{shift / gap:+.0%}" if abs(gap) > 0.5 and not case.startswith("Baseline") else "-"
                p = mannwhitneyu(xv, d).pvalue if len(xv) > 2 else float("nan")
                cells.append(f"{mx:,.1f}, {shift:+,.1f}, {share}, {p:.3f}")
            print("| " + " | ".join(cells) + " |")


main()
