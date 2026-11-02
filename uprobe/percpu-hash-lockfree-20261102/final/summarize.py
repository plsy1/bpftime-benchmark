#!/usr/bin/env python3
"""Phase 2 final (a4ac0d31): total us per call per thread (medians of 3) and map cost (object minus control on
the same backend), kernel included for the counters; suite cases in ns per operation."""
import collections, json, statistics as st
from pathlib import Path

D = Path(__file__).resolve().parent
B = ['dev-68f19c1f', 'dev-a4ac0d31']
K = ['kernel'] + B


def load(name, key):
    d = collections.defaultdict(list)
    rows = [json.loads(l) for l in open(D / name)]
    for r in rows:
        if r.get('ok'):
            d[key(r)].append(r['ops_per_sec'])
    bad = sum(1 for r in rows if not r.get('ok'))
    return {k: k[-1] / st.median(v) * 1e6 for k, v in d.items()}, len(rows), bad


def table(title, data, obj, ctl, rest, backends, threads):
    print(f'\n{title}: total | map cost (us per call per thread)')
    print('| Threads | ' + ' | '.join(f'{b} total' for b in backends) + ' | change | ' +
          ' | '.join(f'{b} map' for b in backends) + ' |')
    print('|---:|' + '---:|' * (2 * len(backends) + 1))
    for t in threads:
        tot = [data[(b, obj, *rest, t)] for b in backends]
        m = [data[(b, obj, *rest, t)] - data[(b, ctl, *rest, t)] for b in backends]
        print(f'| {t} | ' + ' | '.join(f'{x:.3f}' for x in tot) +
              f' | {tot[-1] / tot[-2] * 100 - 100:+.1f}% | ' + ' | '.join(f'{x:.3f}' for x in m) + ' |')


cn, n3, b3 = load('count.jsonl', lambda r: (r['backend'], r['obj'], r['nkeys'], r['threads']))
for obj in ('count-percpu-hash.bpf.o', 'count-hash.bpf.o'):
    for k in (1, 64):
        table(f'Shared counters, {obj[6:-6]}, NKEYS {k}', cn, obj, 'count-control.bpf.o', (k,),
              K, (1, 2, 4, 6))
ch, n1, b1 = load('churn.jsonl', lambda r: (r['backend'], r['obj'], r['threads']))
table('Churn (hash), 4 B key, 8 B value', ch, 'churn-k4-v8.bpf.o', 'control.bpf.o', (), B, (1, 6))
tr, n2, b2 = load('track.jsonl', lambda r: (r['backend'], r['obj'], r['live'], r['threads']))
for live in (1000, 900000):
    table(f'Tracking table (hash), {live} live', tr, 'track.bpf.o', 'track-control.bpf.o', (live,),
          B, (1, 6))
print(f'\nruns: count {n3} ({b3} not ok), churn {n1} ({b1}), track {n2} ({b2}); "change" = '
      f'{B[1]} vs {B[0]}')

s = collections.defaultdict(lambda: collections.defaultdict(list))
for l in open(D / 'suite.jsonl'):
    r = json.loads(l)
    for k, v in r['samples'].items():
        s[r['build']][k].append(v)
order = ['a7d54d7e', 'dev-68f19c1f', 'dev-a4ac0d31']
print('\nOfficial suite, userspace leg, ns per map operation (medians of 5)\n')
print('| Case | ' + ' | '.join(order) + ' | a4ac0d31 vs 68f19c1f | a4ac0d31 vs a7d54d7e |')
print('|---|' + '---:|' * (len(order) + 2))
for name in s['a7d54d7e']:
    m = [st.median(s[b][name]) for b in order]
    print(f'| {name[8:]} | ' + ' | '.join(f'{x / 1000:.2f}' for x in m) +
          f' | {m[2] / m[1] * 100 - 100:+.1f}% | {m[2] / m[0] * 100 - 100:+.1f}% |')
