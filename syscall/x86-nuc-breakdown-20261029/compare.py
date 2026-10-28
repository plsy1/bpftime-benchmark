#!/usr/bin/env python3
"""x86 (this dir's runs) vs Jetson (copied reference numbers): median ns per write per
leg, the step from the layer below, and that step in cycles at the measured clock.
Usage: compare.py  (prints Markdown)"""
import collections, json, statistics as st
from pathlib import Path
HERE = Path(__file__).resolve().parent
# Jetson Orin, syscall/breakdown-20261029 (benchmark-results/jetson), cpu5 1728000 kHz.
JETSON = dict(none=352.6, rewrite=390.1, passthrough=390.5, **{'agent-other': 592.6,
              'agent-exit': 634.8, 'agent-count': 681.8, 'kernel-other': 367.4,
              'kernel-exit': 436.7, 'kernel-count': 523.6})
# Measured busy clock of CPU 2 during each session (freq-log.txt median, MHz).
RUNS = [('x86 1.7 GHz', '1.7ghz-noturbo', 1709), ('x86 4.4 GHz', '4.4ghz-turbo', 4359),
        ('x86 4.4 GHz (1st)', '4.4ghz-turbo-onecpu', 4401)]
ORDER = ['none', 'rewrite', 'passthrough', 'agent-other', 'agent-exit', 'agent-count',
         'kernel-other', 'kernel-exit', 'kernel-count']
BASE = {'rewrite': 'none', 'passthrough': 'rewrite', 'agent-other': 'passthrough',
        'agent-exit': 'agent-other', 'agent-count': 'agent-exit', 'kernel-other': 'none',
        'kernel-exit': 'kernel-other', 'kernel-count': 'kernel-exit'}
cols = [('Jetson 1.73 GHz', JETSON, 1728)]
for label, d, mhz in RUNS:
    v = collections.defaultdict(list)
    for line in open(HERE / d / 'run.jsonl'):
        r = json.loads(line)
        assert r['ok'], r
        v[r['leg']].append(r['ns_per_write'])
    cols.append((label, {k: st.median(x) for k, x in v.items()}, mhz))
print('ns per write (median of 5), step over the layer below, and the step in cycles\n')
print('| Leg | ' + ' | '.join(c[0] for c in cols) + ' |')
print('|---|' + '---:|' * len(cols))
for leg in ORDER:
    cells = []
    for _, m, mhz in cols:
        if leg == 'none':
            cells.append(f'{m[leg]:.1f}')
        else:
            s = m[leg] - m[BASE[leg]]
            cells.append(f'{m[leg]:.1f} ({s:+.1f} ns, {s * mhz / 1000:+.0f} cyc)')
    print(f'| {leg} | ' + ' | '.join(cells) + ' |')
