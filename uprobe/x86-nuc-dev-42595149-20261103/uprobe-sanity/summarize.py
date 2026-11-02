#!/usr/bin/env python3
"""Table for the x86 NUC uprobe sanity check of 11/03: dev 42595149 against the frozen dev
6ab9cbfe build in the same session (benchmark.py --iter 2, each leg against its own kernel
group), plus the 6ab9cbfe value of the 11/02 ten rounds for scale. Only to see whether a
number is off; two samples are not enough for a performance claim.

Map rows: raw average ns per invocation / 1,000 operations (the probes loop 1,000 times).
Other rows: raw ns per invocation.
Usage: summarize.py <run-dir name>
"""
import json, sys
from pathlib import Path

root = Path(__file__).resolve().parent
run = root / sys.argv[1]
d = {leg: json.loads((run / leg / 'benchmark-output.json').read_text()) for leg in ('dev-42595149', 'dev-6ab9cbfe')}
full10 = json.loads((root.parent.parent / 'x86-nuc-dev-6ab9cbfe-20261102/full10/full10-4.4ghz-turbo/dev-6ab9cbfe'
                     / 'benchmark-output.json').read_text())

map_ops = (('Ordinary array lookup', 'array_map_lookup'), ('Ordinary array update', 'array_map_update'),
           ('Ordinary hash lookup', 'hash_map_lookup'), ('Ordinary hash update', 'hash_map_update'),
           ('Ordinary hash delete', 'hash_map_delete'), ('Per-CPU array lookup', 'per_cpu_array_map_lookup'),
           ('Per-CPU array update', 'per_cpu_array_map_update'), ('Per-CPU hash lookup', 'per_cpu_hash_map_lookup'),
           ('Per-CPU hash update', 'per_cpu_hash_map_update'), ('Per-CPU hash delete', 'per_cpu_hash_map_delete'))
other_ops = (('uprobe', 'uprobe'), ('uretprobe', 'uretprobe'), ('uprobe + uretprobe', 'uprobe_uretprobe'),
             ('probe read', 'read'), ('probe write', 'write'))

def avg(doc, group, key, scale):
    return doc[group]['__bench_' + key]['avg'] / scale

def table(title, ops, scale, unit):
    print(f'\n## {title}\n')
    print(f'| Operation | 42595149 {unit} | kernel (its round) | 42595149/kernel | 6ab9cbfe {unit} | kernel (its round) '
          f'| 6ab9cbfe/kernel | 42595149 vs 6ab9cbfe | 6ab9cbfe 11/02 ten rounds |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
    for name, key in ops:
        nv, ov = avg(d['dev-42595149'], 'userspace_uprobe', key, scale), avg(d['dev-6ab9cbfe'], 'userspace_uprobe', key, scale)
        kn, ko = avg(d['dev-42595149'], 'kernel_uprobe', key, scale), avg(d['dev-6ab9cbfe'], 'kernel_uprobe', key, scale)
        f = avg(full10, 'userspace_uprobe', key, scale)
        print(f'| {name} | {nv:.2f} | {kn:.2f} | {nv/kn:.2f}x | {ov:.2f} | {ko:.2f} | {ov/ko:.2f}x | {100*(nv/ov-1):+.1f}% '
              f'| {f:.2f} |')

print(f'# x86 NUC uprobe sanity check, dev 42595149 ({sys.argv[1]})\n')
print('Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 2 session; '
      'ratios use the kernel group of the same leg. 6ab9cbfe is the frozen 11/02 build, measured again in this session. '
      'Array lookup rows do not consume the returned value. Two samples per value: a sanity check, not a performance result.')
table('Map operations (ns per operation)', map_ops, 1000, 'ns/op')
table('Probe and helper overhead (ns per call)', other_ops, 1, 'ns')
for leg in ('dev-42595149', 'dev-6ab9cbfe'):
    for k, v in d[leg]['embed'].items():
        print(f'\nEmbedded ({leg}): {k} avg {v["avg"]:.2f}')
for k, v in full10['embed'].items():
    print(f'\nEmbedded (6ab9cbfe, 11/02 ten rounds): {k} avg {v["avg"]:.2f}')
