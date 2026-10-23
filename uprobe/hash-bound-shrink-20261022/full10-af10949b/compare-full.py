#!/usr/bin/env python3
"""Userspace-only full run of af10949b against the previous complete runs."""
import json
import statistics
from pathlib import Path

root = Path(__file__).resolve().parent
history = root.parent.parent
data = {
    'baseline': json.loads((history / 'baseline-177a1dc-jit-llvm16-20261021/full10/benchmark-output.json').read_text()),
    'globals': json.loads((history / 'direct-map-helpers-20261022/full10-f26cc335/full10/benchmark-output.json').read_text()),
    'current': json.loads((root / 'full10/benchmark-output.json').read_text()),
}
ops = (('Ordinary array lookup', 'array_map_lookup'), ('Ordinary array update', 'array_map_update'),
       ('Ordinary hash lookup', 'hash_map_lookup'), ('Ordinary hash update', 'hash_map_update'),
       ('Ordinary hash delete', 'hash_map_delete'), ('Per-CPU array lookup', 'per_cpu_array_map_lookup'),
       ('Per-CPU array update', 'per_cpu_array_map_update'), ('Per-CPU hash lookup', 'per_cpu_hash_map_lookup'),
       ('Per-CPU hash update', 'per_cpu_hash_map_update'), ('Per-CPU hash delete', 'per_cpu_hash_map_delete'),
       ('uprobe', 'uprobe'), ('uretprobe', 'uretprobe'), ('uprobe + uretprobe', 'uprobe_uretprobe'),
       ('probe read', 'read'), ('probe write', 'write'))
print('# af10949b full ten rounds (userspace + embedded, no kernel arm)\n')
print('Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). '
      'Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = f26cc335 '
      '(`direct-map-helpers-20261022/full10-f26cc335`); both are different runs. The kernel arm was not run: '
      '"/kernel" uses the baseline run\'s kernel (not same-run). Array lookup rows do not consume the '
      'result and are not gain metrics.\n')
print('| Operation | Baseline ns | f26cc335 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. f26cc335 | /kernel (baseline run) |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
for name, suffix in ops:
    key = '__bench_' + suffix
    scale = 1 if suffix in ('uprobe', 'uretprobe', 'uprobe_uretprobe', 'read', 'write') else 1000
    b, g, c = (data[x]['userspace_uprobe'][key]['avg'] / scale for x in ('baseline', 'globals', 'current'))
    raw = [v / scale for v in data['current']['userspace_uprobe'][key]['raw_values']]
    k = data['baseline']['kernel_uprobe'][key]['avg'] / scale
    print(f'| {name} | {b:.2f} | {g:.2f} | {c:.2f} | {statistics.median(raw):.2f} | {min(raw):.2f}-{max(raw):.2f} | '
          f'{100*(c/b-1):+.1f}% | {100*(c/g-1):+.1f}% | {c/k:.2f}x |')
for label in ('embed',):
    for key, value in data['current'][label].items():
        b = data['baseline'][label][key]['avg']
        print(f'\nEmbedded `{key}`: current {value["avg"]:.2f}, baseline {b:.2f} ({100*(value["avg"]/b-1):+.1f}%).')
