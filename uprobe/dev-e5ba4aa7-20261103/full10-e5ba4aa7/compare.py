#!/usr/bin/env python3
"""This run (11/03, dev e5ba4aa7: hash map as preallocated elements in u32 chains) against dev
a4ac0d31 (11/02 evening, dev-a4ac0d31-20261102/full10-a4ac0d31). ns per call, median of 10."""
import json, statistics as st
from pathlib import Path

root = Path(__file__).resolve().parent
up = root.parent.parent
new = json.load(open(root / 'full10' / 'benchmark-output.json'))
old = json.load(open(up / 'dev-a4ac0d31-20261102' / 'full10-a4ac0d31' / 'full10' / 'benchmark-output.json'))

def med(d, group, name):
    return st.median(d[group][name]['raw_values'])

print('| Operation | kernel 11/02 pm | kernel 11/03 | change | a4ac0d31 11/02 pm | e5ba4aa7 11/03 | change | a4ac0d31 / kernel | e5ba4aa7 / kernel |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name in new['kernel_uprobe']:
    ko, kn = med(old, 'kernel_uprobe', name), med(new, 'kernel_uprobe', name)
    uo, un = med(old, 'userspace_uprobe', name), med(new, 'userspace_uprobe', name)
    print(f'| {name.removeprefix("__bench_")} | {ko:.1f} | {kn:.1f} | {kn / ko * 100 - 100:+.1f}% | '
          f'{uo:.1f} | {un:.1f} | {un / uo * 100 - 100:+.1f}% | {uo / ko:.2f}x | {un / kn:.2f}x |')
eo, en = med(old, 'embed', 'embed'), med(new, 'embed', 'embed')
print(f"\nembed: a4ac0d31 {eo:.1f} ns, e5ba4aa7 {en:.1f} ns ({en / eo * 100 - 100:+.1f}%)")
