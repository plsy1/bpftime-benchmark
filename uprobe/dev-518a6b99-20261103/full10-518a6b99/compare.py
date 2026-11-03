#!/usr/bin/env python3
"""This run (11/03 evening, dev 518a6b99: review fixes, hash seed, updates take a spare element)
against dev e5ba4aa7 (11/03 morning, dev-e5ba4aa7-20261103/full10-e5ba4aa7). ns per call, median of 10."""
import json, statistics as st
from pathlib import Path

root = Path(__file__).resolve().parent
up = root.parent.parent
new = json.load(open(root / 'full10' / 'benchmark-output.json'))
old = json.load(open(up / 'dev-e5ba4aa7-20261103' / 'full10-e5ba4aa7' / 'full10' / 'benchmark-output.json'))

def med(d, group, name):
    return st.median(d[group][name]['raw_values'])

print('| Operation | kernel 11/03 am | kernel 11/03 pm | change | e5ba4aa7 | 518a6b99 | change | e5ba4aa7 / kernel | 518a6b99 / kernel |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name in new['kernel_uprobe']:
    ko, kn = med(old, 'kernel_uprobe', name), med(new, 'kernel_uprobe', name)
    uo, un = med(old, 'userspace_uprobe', name), med(new, 'userspace_uprobe', name)
    print(f'| {name.removeprefix("__bench_")} | {ko:.1f} | {kn:.1f} | {kn / ko * 100 - 100:+.1f}% | '
          f'{uo:.1f} | {un:.1f} | {un / uo * 100 - 100:+.1f}% | {uo / ko:.2f}x | {un / kn:.2f}x |')
eo, en = med(old, 'embed', 'embed'), med(new, 'embed', 'embed')
print(f"\nembed: e5ba4aa7 {eo:.1f} ns, 518a6b99 {en:.1f} ns ({en / eo * 100 - 100:+.1f}%)")
