#!/usr/bin/env python3
"""Tables for the x86 NUC full ten rounds of 11/02: dev 6ab9cbfe against the 10/26 dev build
a7d54d7e measured in the same session (each against the kernel leg of its own round), plus
the 10/26 x86 values (a7d54d7e, official 177a1dc) and the Jetson 6ab9cbfe ratio.

Map rows: raw average ns per invocation / 1,000 operations (the probes loop 1,000 times),
as in the Jetson summaries. Other rows: raw ns per invocation.
Usage: summarize.py <run-dir name>
"""
import json, sys
from pathlib import Path

root = Path(__file__).resolve().parent
run = root / sys.argv[1]
d = {leg: json.loads((run / leg / 'benchmark-output.json').read_text()) for leg in ('dev-6ab9cbfe', 'a7d54d7e')}
old_dir = root.parent.parent / 'x86-nuc-full10-20261026/full10-4.4ghz-turbo'
old = {leg: json.loads((old_dir / leg / 'benchmark-output.json').read_text()) for leg in ('dev', 'official')}
# Copy of benchmark-results/jetson uprobe/dev-6ab9cbfe-20261101/full10-6ab9cbfe/full10/benchmark-output.json
jetson = json.loads((root / 'jetson-reference/dev-6ab9cbfe-20261101-full10-benchmark-output.json').read_text())

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
    print(f'| Operation | 6ab9cbfe {unit} | kernel (its round) | 6ab9cbfe/kernel | a7d54d7e {unit} | kernel (its round) '
          f'| a7d54d7e/kernel | 6ab9cbfe vs a7d54d7e | a7d54d7e 10/26 | official/kernel 10/26 | Jetson 6ab9cbfe/kernel |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
    for name, key in ops:
        nv, av = avg(d['dev-6ab9cbfe'], 'userspace_uprobe', key, scale), avg(d['a7d54d7e'], 'userspace_uprobe', key, scale)
        kn, ka = avg(d['dev-6ab9cbfe'], 'kernel_uprobe', key, scale), avg(d['a7d54d7e'], 'kernel_uprobe', key, scale)
        o = avg(old['dev'], 'userspace_uprobe', key, scale)
        of = avg(old['official'], 'userspace_uprobe', key, scale) / avg(old['official'], 'kernel_uprobe', key, scale)
        j = avg(jetson, 'userspace_uprobe', key, scale) / avg(jetson, 'kernel_uprobe', key, scale)
        print(f'| {name} | {nv:.2f} | {kn:.2f} | {nv/kn:.2f}x | {av:.2f} | {ka:.2f} | {av/ka:.2f}x | {100*(nv/av-1):+.1f}% '
              f'| {o:.2f} | {of:.2f}x | {j:.2f}x |')

print(f'# x86 NUC full ten rounds, dev 6ab9cbfe ({sys.argv[1]})\n')
print('Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 10 session; '
      'ratios use the kernel leg of the same session. a7d54d7e is the frozen 10/26 dev build, measured again in this session. '
      'Array lookup rows do not consume the returned value. '
      'Jetson column: benchmark-results/jetson uprobe/dev-6ab9cbfe-20261101 (CPU 5 at 1.728 GHz, LLVM 16).')
table('Map operations (ns per operation)', map_ops, 1000, 'ns/op')
table('Probe and helper overhead (ns per call)', other_ops, 1, 'ns')
print('\n## Kernel legs: this session against 10/26 (same workload binaries)\n')
print('| Operation | kernel (6ab9cbfe round) | kernel (a7d54d7e round) | kernel 10/26 (dev round) | rounds differ | vs 10/26 |')
print('|---|---:|---:|---:|---:|---:|')
for name, key in map_ops + other_ops:
    s = 1000 if (name, key) in map_ops else 1
    kn, ka = avg(d['dev-6ab9cbfe'], 'kernel_uprobe', key, s), avg(d['a7d54d7e'], 'kernel_uprobe', key, s)
    ko = avg(old['dev'], 'kernel_uprobe', key, s)
    print(f'| {name} | {kn:.2f} | {ka:.2f} | {ko:.2f} | {100*(ka/kn-1):+.1f}% | {100*(kn/ko-1):+.1f}% |')
print('\n## Run-to-run spread (coefficient of variation of the 10 samples, %)\n')
print('| Operation | 6ab9cbfe | a7d54d7e | kernel (6ab9cbfe round) | kernel (a7d54d7e round) |')
print('|---|---:|---:|---:|---:|')
for name, key in map_ops + other_ops:
    print(f'| {name} | {cv(d["dev-6ab9cbfe"], "userspace_uprobe", key):.1f} | {cv(d["a7d54d7e"], "userspace_uprobe", key):.1f} '
          f'| {cv(d["dev-6ab9cbfe"], "kernel_uprobe", key):.1f} | {cv(d["a7d54d7e"], "kernel_uprobe", key):.1f} |')
for leg in ('dev-6ab9cbfe', 'a7d54d7e'):
    for k, v in d[leg]['embed'].items():
        print(f'\nEmbedded ({leg}): {k} avg {v["avg"]:.2f}')
for k, v in old['dev']['embed'].items():
    print(f'\nEmbedded (a7d54d7e, 10/26): {k} avg {v["avg"]:.2f}')
