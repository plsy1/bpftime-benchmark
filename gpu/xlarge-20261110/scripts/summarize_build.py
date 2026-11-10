#!/usr/bin/env python3
"""Experiment tables: summarize_build.py PREFIX SETS
Runs are runs/<PREFIX><set><round>/ (PREFIX '' for experiment 1, B for 3, K for 4).
Per case and set: median over rounds of the run mean [range], median of the per-run fastest
launch, Mann-Whitney p of the run means against D and against O. Only valid runs
(victim printed C[1] = 3.00) count."""
import re, glob, sys, statistics
from scipy.stats import mannwhitneyu
R = '/home/jetson/src/results/gpu/xlarge-20261110/runs'
prefix, sets = sys.argv[1], sys.argv[2]
d = {}
for p in glob.glob(f'{R}/{prefix}[{sets}][0-9]*/micro_vec_add_bench.log'):
    s = p.split('/')[-2][len(prefix)]
    parts = re.split(r"^.*Running test: (.+)$", open(p, errors='replace').read(), flags=re.M)
    for i in range(1, len(parts), 2):
        m = re.search(r"Mean:\s+([\d.]+).*?Min:\s+([\d.]+)", parts[i + 1], re.S)
        if m and 'C[1] = 3.00' in parts[i + 1]:
            d.setdefault((parts[i].strip(), s), []).append((float(m.group(1)), float(m.group(2))))
for c in sorted({c for c, _ in d}):
    print(c)
    for s in sets:
        v = d.get((c, s), [])
        if not v: continue
        mean = [x[0] for x in v]; mn = [x[1] for x in v]
        pd = mannwhitneyu(mean, [x[0] for x in d[(c, 'D')]]).pvalue
        po = mannwhitneyu(mean, [x[0] for x in d[(c, 'O')]]).pvalue
        print(f"  {s}: {statistics.median(mean):8.1f} [{min(mean):.1f}-{max(mean):.1f}] fastest launch {statistics.median(mn):7.1f} n{len(v)}  p(vs D) {pd:.3f}  p(vs O) {po:.3f}")
