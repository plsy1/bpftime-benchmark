#!/usr/bin/env python3
"""Kernel arm on the BTF kernel (this run) vs the stock kernel (10/21 complete run), and
a7d54d7e's userspace arm relative to each. Values are ns per call, median of 10 rounds."""
import json, statistics as st
from pathlib import Path

root = Path(__file__).resolve().parent
up = root.parent.parent
new = json.load(open(root / 'full10' / 'benchmark-output.json'))
old_kernel = json.load(open(up / 'baseline-177a1dc-jit-llvm16-20261021' / 'full10' / 'benchmark-output.json'))['kernel_uprobe']
old_user = json.load(open(up / 'realprog-rest-20261026' / 'full10-a7d54d7e' / 'full10' / 'benchmark-output.json'))['userspace_uprobe']

def med(group, name):
    return st.median(group[name]['raw_values'])

print('| Operation | kernel stock (10/21) | kernel BTF (10/26) | change | a7d54d7e (10/26 run) | a7d54d7e (10/26 01:52) | a7d54d7e / kernel stock | a7d54d7e / kernel BTF |')
print('|---|---:|---:|---:|---:|---:|---:|---:|')
for name in new['kernel_uprobe']:
    ko, kn = med(old_kernel, name), med(new['kernel_uprobe'], name)
    un, uo = med(new['userspace_uprobe'], name), med(old_user, name)
    print(f'| {name.removeprefix("__bench_")} | {ko:.1f} | {kn:.1f} | {kn / ko * 100 - 100:+.1f}% | '
          f'{un:.1f} | {uo:.1f} | {uo / ko:.2f}x | {un / kn:.2f}x |')
print(f"\nembed: {med(new['embed'], 'embed'):.1f} ns")
