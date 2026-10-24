#!/usr/bin/env python3
"""Leave-one-out split of stage 2 (split-16.json from run_ablation.py).

Cost of group X = 1/rps(stage 2) - 1/rps(stage 2 without X), per rep, in us per request (and the
same as throughput pp relative to that rep's baseline). Stage 2 total = stage 2 - stage 1.
The remainder (stage 2 total minus the four groups) covers event metadata stores and whatever
the groups share; leave-one-out costs are not strictly additive.
"""
import json, statistics as st, sys
from pathlib import Path

T95 = {9: 2.262, 8: 2.306, 7: 2.365, 6: 2.447, 5: 2.571, 4: 2.776, 3: 3.182, 2: 4.303}
GROUPS = [('nouid', 'bpf_get_current_uid_gid'), ('nometa', 'pid_tgid + ktime_get_ns + get_current_comm'),
          ('nohash', 'hash map update/lookup/delete (bufs, start_ns)'), ('nopcpu', 'per-CPU array lookup (ssl_data)')]

def ci(xs):
    return st.mean(xs), T95.get(len(xs) - 1, 1.96) * st.stdev(xs) / len(xs) ** 0.5

d = json.loads((Path(__file__).resolve().parent / (sys.argv[1] if len(sys.argv) > 1 else 'split-16.json')).read_text())['samples']
base = d['baseline']
n = len(base)
us = lambda leg: [1e6 / v for v in d[leg]]
pp = lambda leg: [100 * (1 - d[leg][i] / base[i]) for i in range(n)]
print(f'16 B, {n} reps; baseline median {st.median(base):.1f} req/s = {1e6 / st.median(base):.1f} us/request\n')
print('| Part | kernel us/req | bpftime us/req | kernel pp | bpftime pp | bpftime - kernel us/req |')
print('|---|---:|---:|---:|---:|---:|')
rows = {}
def row(name, a, b):
    vals = {}
    for t in ('kernel', 'db3d475f'):
        u = [us(f'{t}@{a}')[i] - us(f'{t}@{b}')[i] for i in range(n)]
        p = [pp(f'{t}@{a}')[i] - pp(f'{t}@{b}')[i] for i in range(n)]
        vals[t] = (u, p)
    diff = [vals['db3d475f'][0][i] - vals['kernel'][0][i] for i in range(n)]
    cells = [ci(vals['kernel'][0]), ci(vals['db3d475f'][0]), ci(vals['kernel'][1]), ci(vals['db3d475f'][1]), ci(diff)]
    print(f'| {name} | ' + ' | '.join(f'{m:+.2f} +/- {h:.2f}' for m, h in cells) + ' |')
    return {t: vals[t][0] for t in vals}
total = row('**stage 2 total** (2 - 1)', '2', '1')
parts = [row(label, '2', f'2-{key}') for key, label in GROUPS]
for t in ('kernel', 'db3d475f'):
    rest = [total[t][i] - sum(p[t][i] for p in parts) for i in range(n)]
    m, h = ci(rest)
    print(f'\nremainder ({t}): {m:+.2f} +/- {h:.2f} us/request')
print('\nMedian req/s: ' + ', '.join(f'{k} {st.median(v):.1f}' for k, v in d.items()))
