#!/usr/bin/env python3
"""Retain the initial baseline and the immediately preceding accepted version."""
import json
from pathlib import Path

ROOT = Path('/home/jetson/src/results/uprobe')
SOURCES = {
    'initial': ROOT/'uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json',
    'previous': ROOT/'ordinary-array-lookup-cold-20261019/full10/benchmark-output.json',
    'current': ROOT/'percpu-array-lookup-cold-20261019/full10/benchmark-output.json',
}
OPERATIONS = (
    ('Ordinary array lookup','array_map_lookup','Optimized'),
    ('Ordinary array update','array_map_update','Optimized'),
    ('Ordinary hash lookup','hash_map_lookup','Optimized'),
    ('Ordinary hash update','hash_map_update','Already faster'),
    ('Ordinary hash delete','hash_map_delete','Already faster'),
    ('Per-CPU array lookup','per_cpu_array_map_lookup','Optimized'),
    ('Per-CPU array update','per_cpu_array_map_update','Optimized'),
    ('Per-CPU hash lookup','per_cpu_hash_map_lookup','Optimized'),
    ('Per-CPU hash update','per_cpu_hash_map_update','Optimized'),
    ('Per-CPU hash delete','per_cpu_hash_map_delete','Optimized'),
)
data = {label:json.loads(path.read_text()) for label,path in SOURCES.items()}
print('# Full ten-round map-operation comparison\n')
print('Raw average ns per invocation divided by 1000 helpers; lower is better.')
print('This differs from control-subtracted short A/B timings.\n')
print('| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |')
print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name,suffix,status in OPERATIONS:
    key='__bench_'+suffix
    initial,previous,current=(data[v]['userspace_uprobe'][key]['avg']/1000
                              for v in ('initial','previous','current'))
    initial_kernel=data['initial']['kernel_uprobe'][key]['avg']/1000
    kernel=data['current']['kernel_uprobe'][key]['avg']/1000
    print(f'| {name} | {status} | {initial:.2f} | {initial/initial_kernel:.2f}× | '
          f'{previous:.2f} | {current:.2f} | {kernel:.2f} | {current/kernel:.2f}× | '
          f'{100*(current/initial-1):+.1f}% | {100*(current/previous-1):+.1f}% |')
print('\nSources:\n')
for label,path in SOURCES.items():
    target=Path('../..')/path.relative_to(ROOT)
    print(f'- {label}: [{path.parent.name}]({target.as_posix()})')
