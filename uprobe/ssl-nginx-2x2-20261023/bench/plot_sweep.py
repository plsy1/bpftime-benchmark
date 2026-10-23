#!/usr/bin/env python3
"""Figures for the fair payload sweep (sweep-<size>.json from run_sweep.sh).

absolute_performance.png: same layout as the official draw_figture.py (grouped bars of
Requests/sec per file size), median of 5 reps, min-max whiskers, one bar per leg.
relative_to_kernel.png: per-rep paired ratio to the kernel leg of the same rep, median
with min-max band, for each tracer; 0% = kernel eBPF.
"""
import json, statistics as st
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
SIZES = ['16b', '1kb', '2kb', '4kb', '16kb', '128kb', '256kb']
LEGS = [('baseline', 'Baseline (no tracing)'), ('kernel', 'Kernel sslsniff'),
        ('official', 'bpftime official 177a1dc'), ('db3d475f', 'bpftime db3d475f')]
COLOR = {'baseline': '#2a78d6', 'kernel': '#eb6834', 'official': '#1baf7a', 'db3d475f': '#eda100'}
INK, MUTED, GRID = '#0b0b0b', '#52514e', '#e4e3df'

data = {s: json.loads((HERE / f'sweep-{s}.json').read_text())['samples'] for s in SIZES}
plt.rcParams.update({'font.size': 10, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
                     'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.spines.top': False,
                     'axes.spines.right': False})

# Absolute, official layout.
fig, ax = plt.subplots(figsize=(12, 6))
x = np.arange(len(SIZES))
width = 0.2
for i, (leg, label) in enumerate(LEGS):
    med = np.array([st.median(data[s][leg]) for s in SIZES])
    lo = med - np.array([min(data[s][leg]) for s in SIZES])
    hi = np.array([max(data[s][leg]) for s in SIZES]) - med
    ax.bar(x + (i - 1.5) * width, med, width * 0.9, label=label, color=COLOR[leg],
           yerr=[lo, hi], error_kw=dict(ecolor=MUTED, elinewidth=1, capsize=2))
ax.set_xlabel('File Size')
ax.set_ylabel('Requests/sec')
ax.set_title('Performance by File Size (fair scope, median of 5, min-max whiskers)', color=INK)
ax.set_xticks(x, SIZES)
ax.grid(axis='y', color=GRID, linewidth=0.8)
ax.set_axisbelow(True)
ax.legend(frameon=False)
fig.tight_layout()
fig.savefig(HERE / 'absolute_performance.png', dpi=150)

# Relative to kernel, paired per rep.
fig, ax = plt.subplots(figsize=(12, 5))
ax.axhline(0, color=COLOR['kernel'], linewidth=2, label='Kernel sslsniff (0%)')
rows = []
for leg, label in LEGS:
    if leg == 'kernel':
        continue
    rel = [[100 * (a / k - 1) for a, k in zip(data[s][leg], data[s]['kernel'])] for s in SIZES]
    med = [st.median(r) for r in rel]
    ax.fill_between(x, [min(r) for r in rel], [max(r) for r in rel], color=COLOR[leg], alpha=0.15, linewidth=0)
    ax.plot(x, med, color=COLOR[leg], linewidth=2, marker='o', markersize=6, label=label)
    for xi, m in zip(x, med):
        ax.annotate(f'{m:+.1f}%', (xi, m), textcoords='offset points', xytext=(0, 7), ha='center',
                    fontsize=8, color=INK)
    rows.append((label, med))
ax.set_xticks(x, SIZES)
ax.set_xlabel('File Size')
ax.set_ylabel('Requests/sec vs kernel eBPF (%)')
ax.set_title('Throughput relative to kernel eBPF (paired per rep; median, min-max band)', color=INK)
ax.grid(axis='y', color=GRID, linewidth=0.8)
ax.set_axisbelow(True)
ax.legend(frameon=False, loc='upper left', bbox_to_anchor=(1.0, 1.0))
fig.tight_layout()
fig.savefig(HERE / 'relative_to_kernel.png', dpi=150)

print('| Leg | ' + ' | '.join(SIZES) + ' |')
print('|---|' + '---:|' * len(SIZES))
for leg, label in LEGS:
    print(f'| {label} req/s | ' + ' | '.join(f'{st.median(data[s][leg]):.0f}' for s in SIZES) + ' |')
for label, med in rows:
    print(f'| {label} vs kernel | ' + ' | '.join(f'{m:+.1f}%' for m in med) + ' |')
