#!/usr/bin/env python3
"""Tables for the x86 NUC full ten rounds of 11/03: dev d5279469 (hash map rewritten as a
preallocated chained htab) against the frozen dev 42595149 build (the last version verified on
x86) in the same session, each leg against the kernel group of its own benchmark.py run, plus
the 6ab9cbfe value of the 11/02 ten rounds for scale and the Jetson 518a6b99 ratio
(benchmark-results/jetson uprobe/dev-518a6b99-20261103/full10-518a6b99, copied to
jetson-reference/).

Map rows: raw average ns per invocation / 1,000 operations (the probes loop 1,000 times).
Other rows: raw ns per invocation.
Usage: summarize.py <run-dir name>
"""
import json, statistics, sys
from pathlib import Path

root = Path(__file__).resolve().parent
run = root / sys.argv[1]
d = {leg: json.loads((run / leg / 'benchmark-output.json').read_text()) for leg in ('dev-d5279469', 'dev-42595149')}
full10 = json.loads((root.parent.parent / 'x86-nuc-dev-6ab9cbfe-20261102/full10/full10-4.4ghz-turbo/dev-6ab9cbfe'
                     / 'benchmark-output.json').read_text())
jetson = json.loads((root / 'jetson-reference/dev-518a6b99-20261103-full10-benchmark-output.json').read_text())

map_ops = (('Ordinary array lookup', 'array_map_lookup'), ('Ordinary array update', 'array_map_update'),
           ('Ordinary hash lookup', 'hash_map_lookup'), ('Ordinary hash update', 'hash_map_update'),
           ('Ordinary hash delete', 'hash_map_delete'), ('Per-CPU array lookup', 'per_cpu_array_map_lookup'),
           ('Per-CPU array update', 'per_cpu_array_map_update'), ('Per-CPU hash lookup', 'per_cpu_hash_map_lookup'),
           ('Per-CPU hash update', 'per_cpu_hash_map_update'), ('Per-CPU hash delete', 'per_cpu_hash_map_delete'))
other_ops = (('uprobe', 'uprobe'), ('uretprobe', 'uretprobe'), ('uprobe + uretprobe', 'uprobe_uretprobe'),
             ('probe read', 'read'), ('probe write', 'write'))


def avg(doc, group, key, scale):
    return doc[group]['__bench_' + key]['avg'] / scale


def med(doc, group, key, scale):
    return statistics.median(doc[group]['__bench_' + key]['raw_values']) / scale


def cv(doc, group, key):
    v = doc[group]['__bench_' + key]
    return 100 * v['std_dev'] / v['avg']


def table(title, ops, scale, unit):
    print(f'\n## {title}\n')
    print(f'| Operation | d5279469 {unit} | kernel (its round) | d5279469/kernel | 42595149 {unit} | kernel (its round) '
          f'| 42595149/kernel | d5279469 vs 42595149 | same, medians | 6ab9cbfe 11/02 | Jetson 518a6b99/kernel |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
    for name, key in ops:
        nv, ov = avg(d['dev-d5279469'], 'userspace_uprobe', key, scale), avg(d['dev-42595149'], 'userspace_uprobe', key, scale)
        kn, ko = avg(d['dev-d5279469'], 'kernel_uprobe', key, scale), avg(d['dev-42595149'], 'kernel_uprobe', key, scale)
        f = avg(full10, 'userspace_uprobe', key, scale)
        mc = med(d['dev-d5279469'], 'userspace_uprobe', key, scale) / med(d['dev-42595149'], 'userspace_uprobe', key, scale)
        j = avg(jetson, 'userspace_uprobe', key, scale) / avg(jetson, 'kernel_uprobe', key, scale)
        print(f'| {name} | {nv:.2f} | {kn:.2f} | {nv/kn:.2f}x | {ov:.2f} | {ko:.2f} | {ov/ko:.2f}x | {100*(nv/ov-1):+.1f}% '
              f'| {100*(mc-1):+.1f}% | {f:.2f} | {j:.2f}x |')


print(f'# x86 NUC full ten rounds, dev d5279469 ({sys.argv[1]})\n')
print('Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 10 session; '
      'ratios use the kernel group of the same leg. 42595149 is the frozen build of the 11/03 sanity check, '
      'measured again in this session. Array lookup rows do not consume the returned value. '
      'The suite\'s hash update rewrites existing keys. Sample 6 of the 42595149 leg is an outlier '
      'in the per-CPU rows (per-CPU array lookup 7.65 against 1.28 ns), hence the median column.')
table('Map operations (ns per operation)', map_ops, 1000, 'ns/op')
table('Probe and helper overhead (ns per call)', other_ops, 1, 'ns')
print('\n## Run-to-run spread (coefficient of variation of the 10 samples, %)\n')
print('| Operation | d5279469 | 42595149 | kernel (d5279469 round) | kernel (42595149 round) |')
print('|---|---:|---:|---:|---:|')
for name, key in map_ops + other_ops:
    print(f'| {name} | {cv(d["dev-d5279469"], "userspace_uprobe", key):.1f} | {cv(d["dev-42595149"], "userspace_uprobe", key):.1f} '
          f'| {cv(d["dev-d5279469"], "kernel_uprobe", key):.1f} | {cv(d["dev-42595149"], "kernel_uprobe", key):.1f} |')
for leg in ('dev-d5279469', 'dev-42595149'):
    for k, v in d[leg]['embed'].items():
        print(f'\nEmbedded ({leg}): {k} avg {v["avg"]:.2f}')
for k, v in full10['embed'].items():
    print(f'\nEmbedded (6ab9cbfe, 11/02 ten rounds): {k} avg {v["avg"]:.2f}')
