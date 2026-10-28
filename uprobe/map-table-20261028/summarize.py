#!/usr/bin/env python3
"""Medians per backend, object, live entries and threads. One op = one free + one alloc (three
map operations: lookup + delete at free, insert at alloc); time per op is per thread
(threads / ops per second). Map cost = track minus track-control on the same backend."""
import collections, json, statistics as st, sys

rows = [json.loads(l) for l in open(sys.argv[1] if len(sys.argv) > 1 else 'run1.jsonl')]
bad = [r for r in rows if not r.get('ok')]
d = collections.defaultdict(list)
for r in rows:
    if r.get('ok'):
        d[(r['backend'], r['obj'], r['live'], r['threads'])].append(r['ops_per_sec'])
threads = sorted({r['threads'] for r in rows})
lives = sorted({r['live'] for r in rows})
backends = [b for b in ('kernel', '09421773', 'official') if any(k[0] == b for k in d)]


def us(b, o, live, t):
    v = d.get((b, o, live, t))
    return t / st.median(v) * 1e6 if v else None


for live in lives:
    print(f'\nLive entries {live:,}: per-op time per thread (us)')
    print('| Threads | none | ' + ' | '.join(f'{b} total | {b} map | {b} probe w/o map' for b in backends) + ' |')
    print('|---:|---:|' + '---:|' * (3 * len(backends)))
    for t in threads:
        n = us('none', '-', live, t)
        cells = [f'{n:.3f}' if n else '-']
        for b in backends:
            tot, ctl = us(b, 'track.bpf.o', live, t), us(b, 'track-control.bpf.o', live, t)
            cells += [f'{tot:.3f}' if tot else '-',
                      f'{tot - ctl:.3f}' if tot and ctl else '-',
                      f'{ctl - n:.3f}' if ctl and n else '-']
        print(f'| {t} | ' + ' | '.join(cells) + ' |')
print(f'\nruns {len(rows)}, failed checks {len(bad)}')
for r in bad:
    print('FAILED', json.dumps(r)[:300])
