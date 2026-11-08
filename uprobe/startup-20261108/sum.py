#!/usr/bin/env python3
"""Summary of startup-20261108 (writes summary.md): memleak start, processes starting at once,
steady-state output cost and real tools, the AOT cache build against the dev before it."""
import collections, json, re, statistics
from pathlib import Path

N = Path(__file__).resolve().parent
out = []
p = lambda *a: out.append(' '.join(str(x) for x in a))


def med(v):
    return statistics.median(v) if v else float('nan')


# 1. memleak -c
cells = collections.defaultdict(lambda: collections.defaultdict(list))
for line in (N / 'memleak-start/results.txt').read_text().splitlines():
    m = re.match(r'(\S+) rep \d+ rc (\d+) start_us (\d+) tool_setup_us (\d+)', line)
    if m:
        cells[m[1]]['start'].append(int(m[3]) / 1000)
        cells[m[1]]['setup'].append(int(m[4]) / 1000)
p('## 1. A process traced by memleak -c (10 runs; ms, p50 [min-max])\n')
p('| cell | exec -> main | tool set-up until the command starts |')
p('|---|---:|---:|')
for c in ('kernel', 'old', 'new', 'new-noaot'):
    d = cells.get(c)
    if not d:
        continue
    f = lambda v: f'{med(v):.1f} [{min(v):.1f}-{max(v):.1f}]'
    p(f'| {c} | {f(d["start"])} | {f(d["setup"])} |')

# 2. processes starting at once
rows = [json.loads(l) for l in (N / 'concurrent-start/burst.jsonl').read_text().splitlines() if l.strip()]
g = collections.defaultdict(list)
for r in rows:
    g[(r['backend'], r['k'])].append(r)
p('\n## 2. K processes starting at once, one program (ms; two passes a/b)\n')
p('| backend | K | child p50 | child max | child CPU p50 | round p50 | ok |')
p('|---|---:|---:|---:|---:|---:|---|')
for (b, k), rs in g.items():
    f = lambda key: '/'.join(f"{r.get(key, float('nan')):.1f}" for r in rs)
    p(f"| {b} | {k} | {f('child_wall_p50_ms')} | {f('child_wall_max_ms')} | {f('child_cpu_p50_ms')} | {f('round_wall_p50_ms')} | {all(r.get('ok') for r in rs)} |")

# 3. output cost, new / old
path = N / 'output-threads.jsonl'
if path.exists():
    rows = [json.loads(l) for l in path.read_text().splitlines() if l.strip()]
    g = collections.defaultdict(list)
    for r in rows:
        if r.get('ops_per_sec'):
            g[(r['obj'], r['threads'], r['backend'])].append(1e9 * r['threads'] / r['ops_per_sec'])
    p('\n## 3. Output cost per call (output-dims run.py, 3 x 6 s, ns per call per thread, median)\n')
    p('| program | threads | old | new | new/old |')
    p('|---|---:|---:|---:|---:|')
    for o, t in sorted({(o, t) for o, t, _ in g}):
        a = med(g[(o, t, 'dev-9c088441-probecheck')])
        b = med(g[(o, t, 'dev-9898ed19-probecheck')])
        p(f'| {o} | {t} | {a:.0f} | {b:.0f} | {b / a:.3f} |')
open(N / 'summary.md', 'w').write('\n'.join(out) + '\n')
print('\n'.join(out))
