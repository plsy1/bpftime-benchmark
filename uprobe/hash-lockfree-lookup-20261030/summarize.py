#!/usr/bin/env python3
"""Step 1 summary: per-call time per thread (us, medians of 3) and map cost = object minus control."""
import collections, json, statistics as st, sys
from pathlib import Path
D = Path(__file__).resolve().parent
B = ['kernel', 'c98951a4', '9c3e0345']

def load(name, key):
    d = collections.defaultdict(list)
    for l in open(D / name):
        r = json.loads(l)
        if r.get('ok'):
            d[key(r)].append(r['ops_per_sec'])
    return {k: r_[0] / st.median(v) * 1e6 for k, v in d.items() for r_ in [(k[-1],)]}

cnt = load('count.jsonl', lambda r: (r['backend'], r['obj'], r['nkeys'], r['threads']))
for n in (1, 64):
    print(f'\nShared counters, NKEYS {n}: hash map cost per call (us) | total per call (us)')
    print('| Threads | ' + ' | '.join(f'{b} map' for b in B) + ' | ' + ' | '.join(f'{b} total' for b in B) + ' |')
    print('|---:|' + '---:|' * 6)
    for t in (1, 2, 4, 6):
        m = [cnt[(b, 'count-hash.bpf.o', n, t)] - cnt[(b, 'count-control.bpf.o', n, t)] for b in B]
        tot = [cnt[(b, 'count-hash.bpf.o', n, t)] for b in B]
        print(f'| {t} | ' + ' | '.join(f'{x:.2f}' for x in m) + ' | ' + ' | '.join(f'{x:.2f}' for x in tot) + ' |')

ch = load('churn.jsonl', lambda r: (r['backend'], r['obj'], r['threads']))
print('\nChurn (insert at entry, lookup + delete at return), 4 B key, 8 B value: map cost | total (us)')
print('| Threads | ' + ' | '.join(f'{b} map' for b in B) + ' | ' + ' | '.join(f'{b} total' for b in B) + ' |')
print('|---:|' + '---:|' * 6)
for t in (1, 2, 4, 6):
    m = [ch[(b, 'churn-k4-v8.bpf.o', t)] - ch[(b, 'control.bpf.o', t)] for b in B]
    tot = [ch[(b, 'churn-k4-v8.bpf.o', t)] for b in B]
    print(f'| {t} | ' + ' | '.join(f'{x:.2f}' for x in m) + ' | ' + ' | '.join(f'{x:.2f}' for x in tot) + ' |')
