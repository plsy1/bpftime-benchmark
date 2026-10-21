#!/usr/bin/env python3
"""Global-data validation tables: full ten rounds vs the 177a1dc baseline, consuming runs."""
import json
from pathlib import Path

root=Path(__file__).resolve().parent
history=root.parent.parent
base=json.loads((history/'baseline-177a1dc-jit-llvm16-20261021/full10/benchmark-output.json').read_text())
cur=json.loads((root/'full10/benchmark-output.json').read_text())
ops=(('Ordinary array update','array_map_update'),('Ordinary hash lookup','hash_map_lookup'),
     ('Ordinary hash update','hash_map_update'),('Ordinary hash delete','hash_map_delete'),
     ('Per-CPU array update','per_cpu_array_map_update'),('Per-CPU hash lookup','per_cpu_hash_map_lookup'),
     ('Per-CPU hash update','per_cpu_hash_map_update'),('Per-CPU hash delete','per_cpu_hash_map_delete'),
     ('Ordinary array lookup (unused result; not a gain metric)','array_map_lookup'),
     ('Per-CPU array lookup (unused result; not a gain metric)','per_cpu_array_map_lookup'))
print('# bdd9835 global data: validation\n')
print('## Full ten rounds (raw ns/op)\n')
print('Baseline = 177a1dc 5466f0d full10 (different run). The original suite has no global data in its map programs, so no change is expected.\n')
print('| Operation | Baseline | Baseline kernel | Current | Same-run kernel | Current/kernel | vs. baseline |')
print('|---|---:|---:|---:|---:|---:|---:|')
for name,suffix in ops:
    key='__bench_'+suffix
    b,c=(d['userspace_uprobe'][key]['avg']/1000 for d in (base,cur))
    kb,k=(d['kernel_uprobe'][key]['avg']/1000 for d in (base,cur))
    print(f'| {name} | {b:.2f} | {kb:.2f} | {c:.2f} | {k:.2f} | {c/k:.2f}x | {100*(c/b-1):+.1f}% |')
for directory,label in (('ordinary-standard','Ordinary, standard workload'),('percpu-standard','Per-CPU, standard workload'),
                        ('ordinary-globals','Ordinary, libbpf-style globals workload'),('percpu-globals','Per-CPU, libbpf-style globals workload')):
    print(f'\n## Consuming: {label} (6 rounds, same run)\n')
    print('| Case | Kernel | Baseline 5466f0d | Current bdd9835 | Current/kernel | Current vs. baseline | Current wins |')
    print('|---|---:|---:|---:|---:|---:|---:|')
    for row in json.loads((root/directory/'summary.json').read_text()):
        if row['case']=='unused':
            continue
        print(f'| {row["case"]} | {row["kernel"]["median"]:.2f} | {row["previous"]["median"]:.2f} | {row["inline"]["median"]:.2f} | '
              f'{row["inline_vs_kernel"]:.2f}x | {row["inline_vs_previous_percent"]:+.1f}% | {row["paired_wins_vs_previous"]}/6 |')
