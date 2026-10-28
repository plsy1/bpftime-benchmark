#!/usr/bin/env python3
"""Median ns per write per leg, and the step from the layer below."""
import collections, json, statistics as st, sys
rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run1.jsonl')]
d = collections.defaultdict(list)
for r in rows:
    if r.get('ok'):
        d[r['leg']].append(r['ns_per_write'])
order = ['none', 'rewrite', 'passthrough', 'agent-other', 'agent-exit', 'agent-count',
         'kernel-other', 'kernel-exit', 'kernel-count']
base = {'rewrite': 'none', 'passthrough': 'rewrite', 'agent-other': 'passthrough',
        'agent-exit': 'agent-other', 'agent-count': 'agent-exit', 'kernel-other': 'none',
        'kernel-exit': 'kernel-other', 'kernel-count': 'kernel-exit'}
print('| Leg | runs | median ns/write | min-max | step | over none |')
print('|---|---:|---:|---|---:|---:|')
med = {k: st.median(v) for k, v in d.items()}
for leg in order:
    if leg not in d:
        continue
    v = d[leg]
    step = f'+{med[leg] - med[base[leg]]:.1f} (vs {base[leg]})' if leg in base and base[leg] in med else '-'
    over = f'+{med[leg] - med["none"]:.1f}' if leg != 'none' else '-'
    print(f'| {leg} | {len(v)} | {med[leg]:.1f} | {min(v):.1f}-{max(v):.1f} | {step} | {over} |')
bad = [r for r in rows if not r.get('ok')]
print(f'\nruns {len(rows)}, not ok {len(bad)}')
for r in bad:
    print('NOT OK', json.dumps(r)[:200])
