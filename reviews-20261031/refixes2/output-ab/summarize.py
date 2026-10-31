#!/usr/bin/env python3
"""ns per call per thread (1e9*threads/ops_per_sec), median of reps (min-max), vs the first backend."""
import json, sys, statistics, collections
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
bad = sum(1 for r in rows if not r.get('ok'))
g = collections.defaultdict(list)
backends = []
for r in rows:
    if r['backend'] not in backends:
        backends.append(r['backend'])
    g[(r['obj'], r['threads'], r['backend'])].append(1e9 * r['threads'] / r['ops_per_sec'])
print(f'runs {len(rows)} not ok {bad}')
for obj in ('out-control.bpf.o', 'out-perf.bpf.o', 'out-ringbuf.bpf.o'):
    for t in (1, 6):
        base = None
        cells = []
        for b in backends:
            v = g.get((obj, t, b))
            if not v:
                continue
            m = statistics.median(v)
            if base is None:
                base = m
            cells.append(f'{b:>14} {m:8.1f} ({min(v):.0f}-{max(v):.0f}) {100*(m/base-1):+5.1f}%')
        if cells:
            print(f'{obj.split("-")[1].split(".")[0]:8} t={t} | ' + ' | '.join(cells))
