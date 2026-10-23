#!/usr/bin/env python3
"""Paired private -> host effect per leg and the July interaction term
(tracer_host/tracer_private - 1) - (kernel_host/kernel_private - 1), mean and 95% t CI over reps."""
import json, statistics as st, sys
from pathlib import Path

T95 = {9: 2.262, 8: 2.306, 7: 2.365, 6: 2.447, 5: 2.571, 4: 2.776, 3: 3.182, 2: 4.303, 1: 12.706}

def ci(xs):
    m = st.mean(xs)
    if len(xs) < 2:
        return m, m, m
    h = T95.get(len(xs) - 1, 1.96) * st.stdev(xs) / len(xs) ** 0.5
    return m, m - h, m + h

d = json.loads((Path(__file__).resolve().parent / (sys.argv[1] if len(sys.argv) > 1 else 'recheck-1k.json')).read_text())
legs = d['legs']
s = {(x['rep'], x['netns'], x['leg']): x for x in d['samples']}
dirty = {x['rep'] for x in d['samples'] if x['foreign']}  # a kept leg overlapped a foreign build/benchmark
reps = sorted({x['rep'] for x in d['samples'] if all((x['rep'], n, l) in s for n in ('private', 'host') for l in legs)} - dirty)
print(f'payload {d["payload"]} B, {len(reps)} complete reps (excluded: {sorted(dirty)})\n')
print('| Leg | private req/s | host req/s | host vs private (95% CI) | reader cores p/h | nginx cores p/h |')
print('|---|---:|---:|---:|---:|---:|')
change = {}
for l in legs:
    p = [s[r, 'private', l]['rps'] for r in reps]
    h = [s[r, 'host', l]['rps'] for r in reps]
    change[l] = [h[i] / p[i] - 1 for i in range(len(reps))]
    m, lo, hi = ci(change[l])
    rc = [st.mean(s[r, n, l]['reader_cores'] for r in reps) for n in ('private', 'host')]
    nc = [st.mean(s[r, n, l]['nginx_cores'] for r in reps) for n in ('private', 'host')]
    print(f'| {l} | {st.median(p):.1f} | {st.median(h):.1f} | {100*m:+.2f}% [{100*lo:+.2f}, {100*hi:+.2f}] | '
          f'{rc[0]:.3f}/{rc[1]:.3f} | {nc[0]:.3f}/{nc[1]:.3f} |')
print('\n| Tracer | vs kernel, private | vs kernel, host | interaction vs kernel (95% CI) |')
print('|---|---:|---:|---:|')
for l in legs:
    if l in ('baseline', 'kernel'):
        continue
    rel = {n: st.median(s[r, n, l]['rps'] / s[r, n, 'kernel']['rps'] - 1 for r in reps) for n in ('private', 'host')}
    m, lo, hi = ci([change[l][i] - change['kernel'][i] for i in range(len(reps))])
    print(f'| {l} | {100*rel["private"]:+.2f}% | {100*rel["host"]:+.2f}% | {100*m:+.2f}pp [{100*lo:+.2f}, {100*hi:+.2f}] |')
m, lo, hi = ci([change['kernel'][i] - change['baseline'][i] for i in range(len(reps))])
print(f'\nkernel vs baseline interaction: {100*m:+.2f}pp [{100*lo:+.2f}, {100*hi:+.2f}]')
host = [x for x in d['samples'] if x['netns'] == 'host']
print(f'host INPUT packets/request: {st.mean(x["input_pkts_per_request"] for x in host):.2f}; '
      f'repeated legs: {sum(1 for x in d["samples"] if x["attempt"] > 0)}; foreign seen in kept legs: '
      f'{sum(1 for x in d["samples"] if x["foreign"])}')
