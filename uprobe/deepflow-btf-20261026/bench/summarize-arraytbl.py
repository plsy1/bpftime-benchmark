#!/usr/bin/env python3
"""Array-table variants (run-arraytbl-A, -B) next to run2 (DeepFlow unmodified):
medians of req/s, bpftime vs kernel, per-request uprobe cost against no-uprobe,
and the goroutine-id audit (events with a goroutine id per request)."""
import json, statistics as st, collections
runs = {'run2': 'run2/results.jsonl', 'A': 'run-arraytbl-A/results.jsonl', 'B': 'run-arraytbl-B/results.jsonl'}
d = collections.defaultdict(list); coro = collections.defaultdict(list); bad = collections.Counter()
for name, path in runs.items():
    for l in open(path):
        r = json.loads(l)
        k = (name, r['kind'], r['size'], r['leg'])
        d[k].append(r['req_per_sec'] or 0)
        coro[k].append(r.get('coroutine_events_per_request'))
        if not r.get('server_alive') or r.get('tracer_ready') is False or r.get('attached') is False:
            bad[name] += 1
med = lambda *k: st.median(d[k])
cost = lambda run, kind, size, leg: (1 / med(run, kind, size, leg) - 1 / med(run, kind, size, 'no-uprobe')) * 1e6
print('| Protocol | Size | run2 kernel | run2 bpftime | run2 vs | A kernel | A bpftime | A vs | B kernel | B bpftime | B vs |')
print('|---|---|' + '---:|' * 9)
for kind in ('https', 'http'):
    for size in (1024, 16384, 262144):
        cells = []
        for run in runs:
            k, u = med(run, kind, size, 'kernel-uprobe'), med(run, kind, size, 'user-uprobe')
            cells += [f'{k:,.0f}', f'{u:,.0f}', f'{u / k * 100 - 100:+.1f}%']
        print(f'| {kind} | {size // 1024} KB | ' + ' | '.join(cells) + ' |')
print('\nGo uprobe cost per request against no-uprobe (us), kernel / bpftime:')
print('| Protocol | Size | no-uprobe run2 / A / B | run2 | A | B |')
print('|---|---|---|---:|---:|---:|')
for kind in ('https', 'http'):
    for size in (1024, 16384, 262144):
        nu = ' / '.join(f"{med(r, kind, size, 'no-uprobe'):,.0f}" for r in runs)
        print(f'| {kind} | {size // 1024} KB | {nu} | ' + ' | '.join(
            f"{cost(r, kind, size, 'kernel-uprobe'):.1f} / {cost(r, kind, size, 'user-uprobe'):.1f}" for r in runs) + ' |')
print('\nEvents with a goroutine id per request, kernel-uprobe | user-uprobe (per rep):')
for kind in ('https', 'http'):
    for size in (1024, 16384, 262144):
        print(f'{kind} {size // 1024}KB ' + '  '.join(
            f"{r}: {coro[(r, kind, size, 'kernel-uprobe')]} | {coro[(r, kind, size, 'user-uprobe')]}" for r in runs))
print('\nfailed checks', dict(bad))
