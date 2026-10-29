#!/usr/bin/env python3
"""Single-thread A/B (rerun after the daily job): per-call us (median of 3), map cost =
object minus control on the same build, plus all three reps; and perf stat per op."""
import collections, json, statistics as st
from pathlib import Path

D = Path(__file__).resolve().parent
B = ['c98951a4', 'stripe-tokens', 'stripe-view', 'stripe-line']


def load(name, key):
    d = collections.defaultdict(list)
    for r in map(json.loads, open(D / name)):
        if r.get('ok'):
            d[key(r)].append(1e6 / r['ops_per_sec'])
    return d


def show(title, d, obj, ctl, rest, threads):
    print(f'\n{title}: map cost (total), us; reps of total in brackets')
    for t in threads:
        print(f'  threads {t}')
        for b in B:
            o, c = d[(b, obj, *rest, t)], d[(b, ctl, *rest, t)]
            if not o or not c:
                continue
            print(f'    {b:14s} {st.median(o) - st.median(c):.3f} ({st.median(o):.3f})  '
                  f'[{", ".join(f"{x:.3f}" for x in sorted(o))}]')


ch = load('churn.jsonl', lambda r: (r['backend'], r['obj'], r['threads']))
show('Churn 4 B key, 8 B value', ch, 'churn-k4-v8.bpf.o', 'control.bpf.o', (), (1, 2, 6))
tr = load('track.jsonl', lambda r: (r['backend'], r['obj'], r['live'], r['threads']))
for live in (1000, 900000):
    show(f'Tracking table, {live:,} live', tr, 'track.bpf.o', 'track-control.bpf.o', (live,), (1,))
p = D / 'stat-track.jsonl'
if p.exists():
    print('\nperf stat, tracking 1K, 1 thread: instructions / cycles per op')
    for r in map(json.loads, open(p)):
        print(f"  {r['backend']:14s} {r.get('instructions_per_op', 0):.0f} / {r.get('cycles_per_op', 0):.0f}")
