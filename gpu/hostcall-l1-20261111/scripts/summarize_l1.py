#!/usr/bin/env python3
"""Tables of the layer-1 rounds: summarize_l1.py [PREFIX]  (runs/<PREFIX><D|N><round>/)
Per case: median over rounds of the benchmark's number (the run mean, us) [range] for D (dev
c115b33d) and N (dev with layer 1), N - D, N / D and the Mann-Whitney p of the run means. Only
cases whose victim printed C[1] = 3.00 count."""
import re, glob, sys, statistics
from scipy.stats import mannwhitneyu
R = '/home/jetson/src/results/gpu/hostcall-l1-20261111/runs'
prefix = sys.argv[1] if len(sys.argv) > 1 else 'R'
d, order = {}, []
for p in sorted(glob.glob(f'{R}/{prefix}[DN][0-9]*/micro_vec_add_bench.log')):
    s = p.split('/')[-2][len(prefix)]
    parts = re.split(r"^.*Running test: (.+)$", open(p, errors='replace').read(), flags=re.M)
    for i in range(1, len(parts), 2):
        name = parts[i].strip()
        if name not in order: order.append(name)
        m = re.search(r"Average kernel time:\s+([\d.]+)", parts[i + 1])
        if m and 'C[1] = 3.00' in parts[i + 1]:
            d.setdefault((name, s), []).append(float(m.group(1)))
print("| case | D dev c115b33d (us) | N layer 1 (us) | N - D (us) | N / D | p | rounds D/N |")
print("|---|---:|---:|---:|---:|---:|---|")
for c in order:
    a, b = d.get((c, 'D'), []), d.get((c, 'N'), [])
    if not a or not b: print(f"| {c} | missing | | | | | {len(a)}/{len(b)} |"); continue
    ma, mb = statistics.median(a), statistics.median(b)
    p = mannwhitneyu(a, b).pvalue
    print(f"| {c} | {ma:,.1f} [{min(a):,.1f}-{max(a):,.1f}] | {mb:,.1f} [{min(b):,.1f}-{max(b):,.1f}] | {mb - ma:+,.1f} | {mb / ma:.2f} | {p:.3f} | {len(a)}/{len(b)} |")
