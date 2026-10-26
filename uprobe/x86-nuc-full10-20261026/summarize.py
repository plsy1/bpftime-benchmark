#!/usr/bin/env python3
"""Tables for the x86 NUC full ten rounds: dev branch and official 177a1dc, each
against the kernel leg of its own round, plus the Jetson a7d54d7e ratio.

Map rows: raw average ns per invocation / 1,000 operations (the probes loop
1,000 times), as in the Jetson summaries. Other rows: raw ns per invocation.
Usage: summarize.py <run-dir name>
"""
import json, sys
from pathlib import Path

root = Path(__file__).resolve().parent
run = root / sys.argv[1]
d = {leg: json.loads((run / leg / 'benchmark-output.json').read_text()) for leg in ('dev', 'official')}
jetson = json.loads((root.parent / 'btf-kernel-20261026/full10-a7d54d7e-kernel/full10/benchmark-output.json').read_text())

map_ops = (('Ordinary array lookup', 'array_map_lookup'), ('Ordinary array update', 'array_map_update'),
           ('Ordinary hash lookup', 'hash_map_lookup'), ('Ordinary hash update', 'hash_map_update'),
           ('Ordinary hash delete', 'hash_map_delete'), ('Per-CPU array lookup', 'per_cpu_array_map_lookup'),
           ('Per-CPU array update', 'per_cpu_array_map_update'), ('Per-CPU hash lookup', 'per_cpu_hash_map_lookup'),
           ('Per-CPU hash update', 'per_cpu_hash_map_update'), ('Per-CPU hash delete', 'per_cpu_hash_map_delete'))
other_ops = (('uprobe', 'uprobe'), ('uretprobe', 'uretprobe'), ('uprobe + uretprobe', 'uprobe_uretprobe'),
             ('probe read', 'read'), ('probe write', 'write'))

def avg(doc, group, key, scale):
    return doc[group]['__bench_' + key]['avg'] / scale

def cv(doc, group, key):
    v = doc[group]['__bench_' + key]
    return 100 * v['std_dev'] / v['avg']

def table(title, ops, scale, unit):
    print(f'\n## {title}\n')
    print(f'| Operation | dev {unit} | kernel (dev round) | dev/kernel | official {unit} | kernel (official round) | official/kernel '
          f'| dev vs official | kernel rounds differ | Jetson a7d54d7e/kernel |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
    for name, key in ops:
        dv, ov = avg(d['dev'], 'userspace_uprobe', key, scale), avg(d['official'], 'userspace_uprobe', key, scale)
        kd, ko = avg(d['dev'], 'kernel_uprobe', key, scale), avg(d['official'], 'kernel_uprobe', key, scale)
        j = avg(jetson, 'userspace_uprobe', key, scale) / avg(jetson, 'kernel_uprobe', key, scale)
        print(f'| {name} | {dv:.2f} | {kd:.2f} | {dv/kd:.2f}x | {ov:.2f} | {ko:.2f} | {ov/ko:.2f}x | {100*(dv/ov-1):+.1f}% '
              f'| {100*(ko/kd-1):+.1f}% | {j:.2f}x |')

print(f'# x86 NUC full ten rounds ({sys.argv[1]})\n')
print('Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 10 session; '
      'ratios use the kernel leg of the same session. Array lookup rows do not consume the returned value '
      '(the JIT inlines the lookup in the dev branch); see the Jetson consuming tests before claiming gains there. '
      'Jetson column: btf-kernel-20261026/full10-a7d54d7e-kernel (CPU 5 at 1.728 GHz, LLVM 16).')
table('Map operations (ns per operation)', map_ops, 1000, 'ns/op')
table('Probe and helper overhead (ns per call)', other_ops, 1, 'ns')
print('\n## Run-to-run spread (coefficient of variation of the 10 samples, %)\n')
print('| Operation | dev | official | kernel (dev round) | kernel (official round) |')
print('|---|---:|---:|---:|---:|')
for name, key in map_ops + other_ops:
    print(f'| {name} | {cv(d["dev"], "userspace_uprobe", key):.1f} | {cv(d["official"], "userspace_uprobe", key):.1f} '
          f'| {cv(d["dev"], "kernel_uprobe", key):.1f} | {cv(d["official"], "kernel_uprobe", key):.1f} |')
for leg in ('dev', 'official'):
    e = d[leg]['embed']
    for k, v in e.items():
        print(f'\nEmbedded ({leg}): {k} avg {v["avg"]:.2f}')
