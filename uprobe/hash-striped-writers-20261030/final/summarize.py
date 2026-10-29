#!/usr/bin/env python3
"""Step 2 final summary: per-call time per thread (us, medians of 3) and map cost = object minus
control on the same backend, for churn, shared counters and the tracking table."""
import collections, json, statistics as st
from pathlib import Path

D = Path(__file__).resolve().parent


def load(name, key):
    d = collections.defaultdict(list)
    rows = [json.loads(l) for l in open(D / name)]
    for r in rows:
        if r.get('ok'):
            d[key(r)].append(r['ops_per_sec'])
    bad = sum(1 for r in rows if not r.get('ok'))
    return {k: k[-1] / st.median(v) * 1e6 for k, v in d.items()}, len(rows), bad


def table(title, data, backends, obj, ctl, rest, threads=(1, 2, 4, 6)):
    print(f'\n{title}: map cost | total (us)')
    print('| Threads | ' + ' | '.join(f'{b} map' for b in backends) + ' | ' +
          ' | '.join(f'{b} total' for b in backends) + ' |')
    print('|---:|' + '---:|' * (2 * len(backends)))
    for t in threads:
        m = [data[(b, obj, *rest, t)] - data[(b, ctl, *rest, t)] for b in backends]
        tot = [data[(b, obj, *rest, t)] for b in backends]
        print(f'| {t} | ' + ' | '.join(f'{x:.2f}' for x in m) + ' | ' +
              ' | '.join(f'{x:.2f}' for x in tot) + ' |')


ch, n1, b1 = load('churn.jsonl', lambda r: (r['backend'], r['obj'], r['threads']))
table('Churn (insert at entry, lookup + delete at return), 4 B key, 8 B value', ch,
      ['kernel', 'c98951a4', 'cb3f29d2', 'f4fdcbcd'], 'churn-k4-v8.bpf.o', 'control.bpf.o', ())
cn, n2, b2 = load('count.jsonl', lambda r: (r['backend'], r['obj'], r['nkeys'], r['threads']))
for k in (1, 64):
    table(f'Shared counters, hash map, NKEYS {k}', cn, ['kernel', 'c98951a4', 'cb3f29d2', 'f4fdcbcd'],
          'count-hash.bpf.o', 'count-control.bpf.o', (k,))
tr, n3, b3 = load('track.jsonl', lambda r: (r['backend'], r['obj'], r['live'], r['threads']))
for live in (1000, 900000):
    table(f'Tracking table, {live:,} live entries (one op = lookup + delete + insert)', tr,
          ['kernel', 'c98951a4', 'cb3f29d2', 'f4fdcbcd'], 'track.bpf.o', 'track-control.bpf.o', (live,))
print(f'\nruns: churn {n1} ({b1} failed checks), counters {n2} ({b2}), tracking {n3} ({b3})')
