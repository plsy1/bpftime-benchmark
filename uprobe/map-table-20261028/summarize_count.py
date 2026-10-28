#!/usr/bin/env python3
"""Shared counters: medians per backend, map kind, NKEYS and threads. Per-call time per thread
(threads / calls per second); map cost = kind minus control on the same backend."""
import collections, json, statistics as st, sys

rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run1-count.jsonl')]
bad = [r for r in rows if not r.get('ok')]
d = collections.defaultdict(list)
for r in rows:
    if r.get('ok'):
        d[(r['backend'], r['obj'], r['nkeys'], r['threads'])].append(r['ops_per_sec'])
threads = sorted({r['threads'] for r in rows})
nkeys = sorted({r['nkeys'] for r in rows})
backends = [b for b in ('kernel', '09421773', 'c98951a4', 'official') if any(k[0] == b for k in d)]
kinds = ['hash', 'percpu-hash', 'percpu-array', 'array']


def us(b, o, n, t):
    v = d.get((b, o, n, t))
    return t / st.median(v) * 1e6 if v else None


for n in nkeys:
    print(f'\nNKEYS {n}: map cost per call (us, kind minus control), and probe cost w/o map')
    print('| Threads | ' + ' | '.join(f'{b} {k}' for k in kinds for b in backends) + ' | ' +
          ' | '.join(f'{b} probe w/o map' for b in backends) + ' |')
    print('|---:|' + '---:|' * (len(kinds) * len(backends) + len(backends)))
    for t in threads:
        cells = []
        for k in kinds:
            for b in backends:
                a, c = us(b, f'count-{k}.bpf.o', n, t), us(b, 'count-control.bpf.o', n, t)
                cells.append(f'{a - c:.3f}' if a and c else '-')
        none = us('none', '-', n, t)
        for b in backends:
            c = us(b, 'count-control.bpf.o', n, t)
            cells.append(f'{c - none:.3f}' if c and none else '-')
        print(f'| {t} | ' + ' | '.join(cells) + ' |')
print(f'\nruns {len(rows)}, failed checks {len(bad)}')
for r in bad:
    print('FAILED', json.dumps(r)[:300])
