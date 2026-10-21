#!/usr/bin/env python3
"""Baseline table for codex/uprobe-official-177a1dc465ff (LLVM 16)."""
import json
from pathlib import Path

root=Path(__file__).resolve().parent
history=root.parent
data={
    'initial':json.loads((history/'uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json').read_text()),
    'previous':json.loads((history/'array-jit-default-delete-fix-20261021/full10/benchmark-output.json').read_text()),
    'current':json.loads((root/'full10/benchmark-output.json').read_text()),
}
ops=(('Ordinary array lookup','array_map_lookup','JIT (separate consuming test)'),
     ('Ordinary array update','array_map_update','Optimized (C++)'),
     ('Ordinary hash lookup','hash_map_lookup','Optimized (C++)'),
     ('Ordinary hash update','hash_map_update','Already faster; shared-path gains'),
     ('Ordinary hash delete','hash_map_delete','Already faster; shared-path gains'),
     ('Per-CPU array lookup','per_cpu_array_map_lookup','JIT (separate consuming test)'),
     ('Per-CPU array update','per_cpu_array_map_update','Optimized (C++)'),
     ('Per-CPU hash lookup','per_cpu_hash_map_lookup','Optimized (C++)'),
     ('Per-CPU hash update','per_cpu_hash_map_update','Optimized (C++)'),
     ('Per-CPU hash delete','per_cpu_hash_map_delete','Optimized (C++)'))
print('# 177a1dc baseline: full ten rounds\n')
print('Raw average ns/invocation divided by 1,000 operations. Lower is better. '
      'Current = codex/uprobe-official-177a1dc465ff 5466f0d, LLVM 16. '
      'Previous = array-jit-default-delete-fix-20261021 (c0939bd on 8da867d541f8, LLVM 16), '
      'whose kernel denominators are a different run.\n')
print('Both array lookup results are excluded from historical gain claims: the old workload does not consume the returned value. Use the independent consuming workloads below.\n')
print('| Operation | Status | Initial ns | Initial/kernel | Previous ns | Previous kernel ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |')
print('|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|')
for name,suffix,status in ops:
    key='__bench_'+suffix
    a,b,c=(data[x]['userspace_uprobe'][key]['avg']/1000 for x in ('initial','previous','current'))
    k0=data['initial']['kernel_uprobe'][key]['avg']/1000
    kb=data['previous']['kernel_uprobe'][key]['avg']/1000
    k=data['current']['kernel_uprobe'][key]['avg']/1000
    if suffix in ('array_map_lookup','per_cpu_array_map_lookup'):
        print(f'| {name} | {status} | {a:.2f} | {a/k0:.2f}x | {b:.2f} | {kb:.2f} | {c:.2f} (not comparable) | {k:.2f} | Separate test | — | — |')
    else:
        print(f'| {name} | {status} | {a:.2f} | {a/k0:.2f}x | {b:.2f} | {kb:.2f} | {c:.2f} | {k:.2f} | {c/k:.2f}x | {100*(c/a-1):+.1f}% | {100*(c/b-1):+.1f}% |')
for name,directory,rounds in (('Ordinary','ordinary-consuming',6),('Per-CPU','percpu-consuming',6)):
    print(f'\n## Consuming {name.lower()} array lookup ({rounds} paired rounds)\n')
    print('Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. '
          'Not comparable to the historical pure map-operation table. Previous = frozen c0939bd artifacts measured in this same run.\n')
    print('| Case | Previous c0939bd (8da) | Current 5466f0d (177a1dc) | Kernel | Current/kernel | vs. previous | Current wins |')
    print('|---|---:|---:|---:|---:|---:|---:|')
    for row in json.loads((root/directory/'summary.json').read_text()):
        if row['case'] in ('hit','mixed'):
            print(f'| {row["case"]} | {row["previous"]["median"]:.2f} | {row["inline"]["median"]:.2f} | {row["kernel"]["median"]:.2f} | '
                  f'{row["inline_vs_kernel"]:.2f}x | {row["inline_vs_previous_percent"]:+.1f}% | {row["paired_wins_vs_previous"]}/{rounds} |')
