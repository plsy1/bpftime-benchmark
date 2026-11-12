#!/usr/bin/env python3
"""Tables of the AC3-in-dev rounds: summarize_ac3.py  (runs/A<W|D><round>/)
Per case: median over rounds of the benchmark's number (the per-launch mean, us) [range] for
O official 4708f409, D dev c115b33d, N dev aa6eb973; N vs D (ratio, Mann-Whitney p) and N vs O.
Only cases whose victim printed C[1] = 3.00 count."""
import re, glob, statistics, sys
from scipy.stats import mannwhitneyu
R = '/home/jetson/src/results/gpu/watcher-policy-20261112/dev/bench/runs'
d, order = {}, []
PFX = sys.argv[1] if len(sys.argv) > 1 else 'A'
for p in sorted(glob.glob(f'{R}/{PFX}[WD][0-9]*/micro_vec_add_bench.log')):
    s = p.split('/')[-2][1]
    parts = re.split(r"^.*Running test: (.+)$", open(p, errors='replace').read(), flags=re.M)
    for i in range(1, len(parts), 2):
        name = parts[i].strip()
        if name not in order: order.append(name)
        m = re.search(r"Average kernel time:\s+([\d.]+)", parts[i + 1])
        if m and 'C[1] = 3.00' in parts[i + 1]:
            d.setdefault((name, s), []).append(float(m.group(1)))
def cell(v):
    return f"{statistics.median(v):,.1f} [{min(v):,.1f}-{max(v):,.1f}]" if v else "-"
print("| case | W dev 46f5a7cb, WFE (us) | D dev 52126f33, AC3 (us) | D / W | p | rounds W/D |")
print("|---|---:|---:|---:|---:|---|")
for c in order:
    a, b = d.get((c, 'W'), []), d.get((c, 'D'), [])
    nd = f"{statistics.median(b) / statistics.median(a):.2f}" if a and b else "-"
    p = f"{mannwhitneyu(a, b).pvalue:.3f}" if len(a) > 1 and len(b) > 1 else "-"
    print(f"| {c} | {cell(a)} | {cell(b)} | {nd} | {p} | {len(a)}/{len(b)} |")
