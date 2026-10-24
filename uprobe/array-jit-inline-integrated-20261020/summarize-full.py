#!/usr/bin/env python3
import json
from pathlib import Path

root=Path(__file__).resolve().parent
history=root.parent
data={
    'initial':json.loads((history/'uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json').read_text()),
    'previous':json.loads((history/'array-offset-branch-20261019/full10/benchmark-output.json').read_text()),
    'current':json.loads((root/'full10/benchmark-output.json').read_text()),
}
ops=(('Ordinary array lookup','array_map_lookup','JIT (separate consuming test)'),
     ('Ordinary array update','array_map_update','Optimized (C++)'),
     ('Ordinary hash lookup','hash_map_lookup','Optimized (C++)'),
     ('Ordinary hash update','hash_map_update','Already faster'),
     ('Ordinary hash delete','hash_map_delete','Already faster'),
     ('Per-CPU array lookup','per_cpu_array_map_lookup','Optimized (C++)'),
     ('Per-CPU array update','per_cpu_array_map_update','Optimized (C++)'),
     ('Per-CPU hash lookup','per_cpu_hash_map_lookup','Optimized (C++)'),
     ('Per-CPU hash update','per_cpu_hash_map_update','Optimized (C++)'),
     ('Per-CPU hash delete','per_cpu_hash_map_delete','Optimized (C++)'))
print('# Full ten-round regression comparison\n')
print('Raw average ns/invocation divided by 1,000 operations. Lower is better.\n')
print('Ordinary array lookup is excluded from historical gain claims: JIT can simplify its unused result. See the consuming-workload result below. Other operations retain their implementation path.\n')
print('| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |')
print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name,suffix,status in ops:
    key='__bench_'+suffix
    a,b,c=(data[x]['userspace_uprobe'][key]['avg']/1000 for x in ('initial','previous','current'))
    k0=data['initial']['kernel_uprobe'][key]['avg']/1000
    k=data['current']['kernel_uprobe'][key]['avg']/1000
    if suffix=='array_map_lookup':
        print(f'| {name} | {status} | {a:.2f} | {a/k0:.2f}x | {b:.2f} | Not comparable | {k:.2f} | Separate test | — | — |')
    else:
        print(f'| {name} | {status} | {a:.2f} | {a/k0:.2f}x | {b:.2f} | {c:.2f} | {k:.2f} | {c/k:.2f}x | {100*(c/a-1):+.1f}% | {100*(c/b-1):+.1f}% |')
print('\n## Consuming ordinary-array lookup (six paired rounds)\n')
print('Runtime keys, actual value reads and verified checksum; raw ns/element includes that extra work. Not comparable to the table above.\n')
print('| Case | Inline off | Inline on | Kernel | Inline/kernel | vs. off |')
print('|---|---:|---:|---:|---:|---:|')
for row in json.loads((root/'ab6-accepted/summary.json').read_text()):
    if row['case'] in ('hit','mixed'):
        print(f'| {row["case"]} | {row["off"]["median"]:.2f} | {row["inline"]["median"]:.2f} | {row["kernel"]["median"]:.2f} | {row["inline_vs_kernel"]:.2f}x | {row["inline_vs_off_percent"]:+.1f}% |')
