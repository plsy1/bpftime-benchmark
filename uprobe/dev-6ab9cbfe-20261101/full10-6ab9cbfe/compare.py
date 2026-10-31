#!/usr/bin/env python3
"""This run (11/01, dev 6ab9cbfe) against the last complete run on the BTF kernel (10/26,
a7d54d7e, btf-kernel-20261026/full10-a7d54d7e-kernel). ns per call, median of 10 rounds."""
import json, statistics as st
from pathlib import Path

root = Path(__file__).resolve().parent
up = root.parent.parent
new = json.load(open(root / 'full10' / 'benchmark-output.json'))
old = json.load(open(up / 'btf-kernel-20261026' / 'full10-a7d54d7e-kernel' / 'full10' / 'benchmark-output.json'))

def med(d, group, name):
    return st.median(d[group][name]['raw_values'])

print('| Operation | kernel 10/26 | kernel 11/01 | change | a7d54d7e 10/26 | 6ab9cbfe 11/01 | change | a7d54d7e / kernel | 6ab9cbfe / kernel |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name in new['kernel_uprobe']:
    ko, kn = med(old, 'kernel_uprobe', name), med(new, 'kernel_uprobe', name)
    uo, un = med(old, 'userspace_uprobe', name), med(new, 'userspace_uprobe', name)
    print(f'| {name.removeprefix("__bench_")} | {ko:.1f} | {kn:.1f} | {kn / ko * 100 - 100:+.1f}% | '
          f'{uo:.1f} | {un:.1f} | {un / uo * 100 - 100:+.1f}% | {uo / ko:.2f}x | {un / kn:.2f}x |')
eo, en = med(old, 'embed', 'embed'), med(new, 'embed', 'embed')
print(f"\nembed: a7d54d7e {eo:.1f} ns, 6ab9cbfe {en:.1f} ns ({en / eo * 100 - 100:+.1f}%)")
