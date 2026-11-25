#!/usr/bin/env python3
"""Medians per backend, object and thread count. Per-call time is per thread
(threads / total calls per second). Map cost = churn object minus control object on the same
backend and thread count: the three map operations of one call (insert, lookup, delete)."""
import collections, json, statistics as st, sys

rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run1.jsonl')]
bad = [r for r in rows if not r.get('ok')]
d = collections.defaultdict(list)
for r in rows:
    if r.get('ok'):
        d[(r['backend'], r['obj'], r['threads'])].append(r['ops_per_sec'])
threads = sorted({r['threads'] for r in rows})
backends = [b for b in ('kernel', '09421773', 'official') if any(k[0] == b for k in d)]
objs = sorted({k[1] for k in d if k[1] not in ('-', 'control.bpf.o')})


def us(b, o, t):
    v = d.get((b, o, t))
    return t / st.median(v) * 1e6 if v else None


print('Per-call time per thread (us), median of runs; none = no tracing')
print('| Threads | none | ' + ' | '.join(f'{b} control' for b in backends) + ' | ' +
      ' | '.join(f'{b} {o.split(".")[0]}' for o in objs for b in backends) + ' |')
print('|---:|' + '---:|' * (1 + len(backends) * (1 + len(objs))))
for t in threads:
    cells = [us('none', '-', t)] + [us(b, 'control.bpf.o', t) for b in backends] + \
            [us(b, o, t) for o in objs for b in backends]
    print(f'| {t} | ' + ' | '.join(f'{c:.3f}' if c else '-' for c in cells) + ' |')

print('\nMap cost per call (us, churn - control: insert + lookup + delete)')
print('| Threads | ' + ' | '.join(f'{b} {o.split(".")[0]}' for o in objs for b in backends) + ' |')
print('|---:|' + '---:|' * (len(backends) * len(objs)))
for t in threads:
    cells = []
    for o in objs:
        for b in backends:
            a, c = us(b, o, t), us(b, 'control.bpf.o', t)
            cells.append(f'{a - c:.3f}' if a and c else '-')
    print(f'| {t} | ' + ' | '.join(cells) + ' |')

print('\nProbe cost per call without maps (us, control - none)')
print('| Threads | ' + ' | '.join(backends) + ' |')
print('|---:|' + '---:|' * len(backends))
for t in threads:
    n = us('none', '-', t)
    print(f'| {t} | ' + ' | '.join(f'{us(b, "control.bpf.o", t) - n:.3f}' if us(b, 'control.bpf.o', t) else '-'
                                   for b in backends) + ' |')
print(f'\nruns {len(rows)}, failed checks {len(bad)}')
for r in bad:
    print('FAILED', json.dumps(r)[:300])
