#!/usr/bin/env python3
"""Stage costs of the sslsniff path ablation (ablation-<payload>.json from run_ablation.py).

impact(stage) = 1 - rps(stage) / rps(baseline of the same rep)           (pp, July method)
cost(stage)   = 1/rps(stage) - 1/rps(baseline of the same rep)           (us per request)
Stage n cost  = value(stage n) - value(stage n-1), per rep; mean and 95% t CI over reps.
"""
import json, statistics as st, sys
from pathlib import Path

T95 = {9: 2.262, 8: 2.306, 7: 2.365, 6: 2.447, 5: 2.571, 4: 2.776, 3: 3.182, 2: 4.303}
NAMES = {1: '1 probe trigger/dispatch', 2: '2 helpers/maps/metadata', 3: '3 bpf_probe_read_user', 4: '4 perf output'}

def ci(xs):
    m = st.mean(xs)
    h = T95.get(len(xs) - 1, 1.96) * st.stdev(xs) / len(xs) ** 0.5
    return m, h

root = Path(__file__).resolve().parent
for payload in sys.argv[1:] or ['16', '256k']:
    d = json.loads((root / f'ablation-{payload}.json').read_text())['samples']
    base = d['baseline']
    n = len(base)
    print(f'\n### {payload} payload ({n} reps)\n')
    print(f'baseline median {st.median(base):.1f} req/s = {1e6 / st.median(base):.1f} us/request\n')
    print('| Stage | kernel pp | bpftime pp | bpftime - kernel pp | kernel us/req | bpftime us/req |')
    print('|---|---:|---:|---:|---:|---:|')
    tot = {}
    for s in (1, 2, 3, 4):
        row = {}
        for t in ('kernel', '7ed339ad'):
            cur, prev = d[f'{t}@{s}'], d[f'{t}@{s - 1}'] if s > 1 else None
            pp = [100 * ((1 - cur[i] / base[i]) - ((1 - prev[i] / base[i]) if prev else 0)) for i in range(n)]
            us = [1e6 * ((1 / cur[i] - 1 / base[i]) - ((1 / prev[i] - 1 / base[i]) if prev else 0)) for i in range(n)]
            row[t] = (pp, us)
        diff = [row['7ed339ad'][0][i] - row['kernel'][0][i] for i in range(n)]
        cells = [f'{m:+.2f} +/- {h:.2f}' for m, h in (ci(row['kernel'][0]), ci(row['7ed339ad'][0]), ci(diff),
                                                         ci(row['kernel'][1]), ci(row['7ed339ad'][1]))]
        print(f'| {NAMES[s]} | ' + ' | '.join(cells) + ' |')
    print('| **total (stage 4 vs baseline)** | ' + ' | '.join(
        f'{ci(v)[0]:+.2f}' for v in (
            [100 * (1 - d['kernel@4'][i] / base[i]) for i in range(n)],
            [100 * (1 - d['7ed339ad@4'][i] / base[i]) for i in range(n)],
            [100 * ((1 - d['7ed339ad@4'][i] / base[i]) - (1 - d['kernel@4'][i] / base[i])) for i in range(n)],
            [1e6 * (1 / d['kernel@4'][i] - 1 / base[i]) for i in range(n)],
            [1e6 * (1 / d['7ed339ad@4'][i] - 1 / base[i]) for i in range(n)])) + ' |')
    print('\nMedian req/s: ' + ', '.join(f'{k} {st.median(v):.1f}' for k, v in d.items()))
