#!/usr/bin/env python3
"""Per-CPU scratch value: medians per backend, LEN and threads. Per-call time per thread
(threads / calls per second); scratch cost = scratch minus control on the same backend
(lookup, header writes and the copy of LEN bytes)."""
import collections, json, statistics as st, sys

rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run1-scratch.jsonl')]
bad = [r for r in rows if not r.get('ok')]
d = collections.defaultdict(list)
for r in rows:
    if r.get('ok'):
        d[(r['backend'], r['obj'], r['length'], r['threads'])].append(r['ops_per_sec'])
threads = sorted({r['threads'] for r in rows})
lengths = sorted({r['length'] for r in rows})
backends = [b for b in ('kernel', '09421773', 'official') if any(k[0] == b for k in d)]


def us(b, o, n, t):
    v = d.get((b, o, n, t))
    return t / st.median(v) * 1e6 if v else None


for n in lengths:
    print(f'\nLEN {n}: per-call time per thread (us)')
    print('| Threads | ' + ' | '.join(f'{b} total | {b} scratch | {b} probe w/o map' for b in backends) + ' |')
    print('|---:|' + '---:|' * (3 * len(backends)))
    for t in threads:
        none = us('none', '-', n, t)
        cells = []
        for b in backends:
            tot, ctl = us(b, 'scratch.bpf.o', n, t), us(b, 'scratch-control.bpf.o', n, t)
            cells += [f'{tot:.3f}' if tot else '-',
                      f'{tot - ctl:.3f}' if tot and ctl else '-',
                      f'{ctl - none:.3f}' if ctl and none else '-']
        print(f'| {t} | ' + ' | '.join(cells) + ' |')
print(f'\nruns {len(rows)}, failed checks {len(bad)}')
for r in bad:
    print('FAILED', json.dumps(r)[:300])
