#!/usr/bin/env python3
"""This run (11/02, dev 68f19c1f) against dev 6ab9cbfe (11/01,
dev-6ab9cbfe-20261101/full10-6ab9cbfe). ns per call, median of 10 rounds."""
import json, statistics as st
from pathlib import Path

root = Path(__file__).resolve().parent
up = root.parent.parent
new = json.load(open(root / 'full10' / 'benchmark-output.json'))
old = json.load(open(up / 'dev-6ab9cbfe-20261101' / 'full10-6ab9cbfe' / 'full10' / 'benchmark-output.json'))

def med(d, group, name):
    return st.median(d[group][name]['raw_values'])

print('| Operation | kernel 11/01 | kernel 11/02 | change | 6ab9cbfe 11/01 | 68f19c1f 11/02 | change | 6ab9cbfe / kernel | 68f19c1f / kernel |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name in new['kernel_uprobe']:
    ko, kn = med(old, 'kernel_uprobe', name), med(new, 'kernel_uprobe', name)
    uo, un = med(old, 'userspace_uprobe', name), med(new, 'userspace_uprobe', name)
    print(f'| {name.removeprefix("__bench_")} | {ko:.1f} | {kn:.1f} | {kn / ko * 100 - 100:+.1f}% | '
          f'{uo:.1f} | {un:.1f} | {un / uo * 100 - 100:+.1f}% | {uo / ko:.2f}x | {un / kn:.2f}x |')
eo, en = med(old, 'embed', 'embed'), med(new, 'embed', 'embed')
print(f"\nembed: 6ab9cbfe {eo:.1f} ns, 68f19c1f {en:.1f} ns ({en / eo * 100 - 100:+.1f}%)")
