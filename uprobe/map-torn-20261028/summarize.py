#!/usr/bin/env python3
"""Torn reads per million reads, per backend, object and threads (sums over reps)."""
import collections, json, sys

rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run1-torn.jsonl')]
bad = [r for r in rows if not r.get('ok')]
agg = collections.defaultdict(lambda: [0, 0, 0, 0])
for r in rows:
    if r.get('ok'):
        a = agg[(r['backend'], r['obj'], r['threads'])]
        a[0] += r['reads']; a[1] += r['torn']; a[2] += r['writes']; a[3] += 1
print('| Object | Threads | Backend | Runs | Reads | Writes | Torn | Torn per million reads |')
print('|---|---:|---|---:|---:|---:|---:|---:|')
for (b, o, t), (reads, torn, writes, n) in sorted(agg.items(), key=lambda kv: (kv[0][1], kv[0][2], kv[0][0])):
    print(f'| {o} | {t} | {b} | {n} | {reads:,} | {writes:,} | {torn} | {torn / reads * 1e6:.2f} |')
print(f'\nruns {len(rows)}, invalid {len(bad)}')
for r in bad:
    print('INVALID', json.dumps(r)[:300])
