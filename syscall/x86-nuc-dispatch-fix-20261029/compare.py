#!/usr/bin/env python3
"""Before (dev 7a7d29b8, ../x86-nuc-breakdown-20261029) vs after (2e850c45, breakdown/)
the dispatch fix on x86, beside the Jetson numbers (copied from benchmark-results/jetson
syscall/breakdown-20261029 and syscall/dispatch-fix-20261029). Median ns per write,
the step over the layer below and that step in cycles at the measured clock (MHz).
Usage: compare.py  (prints Markdown)"""
import collections, json, statistics as st
from pathlib import Path
HERE = Path(__file__).resolve().parent
BEFORE = HERE.parent / 'x86-nuc-breakdown-20261029'
J_BEFORE = dict(none=352.6, rewrite=390.1, passthrough=390.5, **{'agent-other': 592.6,
                'agent-exit': 634.8, 'agent-count': 681.8, 'kernel-other': 367.4,
                'kernel-exit': 436.7, 'kernel-count': 523.6})
J_AFTER = dict(none=352.4, rewrite=389.9, passthrough=391.0, **{'agent-other': 418.8,
               'agent-exit': 498.5, 'agent-count': 600.0, 'kernel-other': 367.8,
               'kernel-exit': 438.1, 'kernel-count': 524.3})
ORDER = ['none', 'rewrite', 'passthrough', 'agent-other', 'agent-exit', 'agent-count',
         'kernel-other', 'kernel-exit', 'kernel-count']
BASE = {'rewrite': 'none', 'passthrough': 'rewrite', 'agent-other': 'passthrough',
        'agent-exit': 'agent-other', 'agent-count': 'agent-exit', 'kernel-other': 'none',
        'kernel-exit': 'kernel-other', 'kernel-count': 'kernel-exit'}


def medians(path):
    v = collections.defaultdict(list)
    for line in open(path):
        r = json.loads(line)
        assert r['ok'], r
        v[r['leg']].append(r['ns_per_write'])
    return {k: st.median(x) for k, x in v.items()}


def table(title, cols):
    print(f'\n### {title}\n')
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
    print('| bpftime agent-exit over none | ' + ' | '.join(
        f'+{m["agent-exit"] - m["none"]:.1f} ns, {(m["agent-exit"] - m["none"]) * mhz / 1000:.0f} cyc'
        for _, m, mhz in cols) + ' |')


print('Median ns per write (5 rounds), in brackets the step over the layer below and the step in cycles.')
table('1.7 GHz (x86) and Jetson 1.728 GHz', [
    ('x86 before', medians(BEFORE / '1.7ghz-noturbo/run.jsonl'), 1709),
    ('x86 after', medians(HERE / 'breakdown/1.7ghz-noturbo/run.jsonl'), 1709),
    ('Jetson before', J_BEFORE, 1728), ('Jetson after', J_AFTER, 1728)])
table('4.4 GHz (x86)', [
    ('x86 before', medians(BEFORE / '4.4ghz-turbo/run.jsonl'), 4359),
    ('x86 after', medians(HERE / 'breakdown/4.4ghz-turbo/run.jsonl'), 4346)])
