#!/usr/bin/env python3
"""Summary of run_correct.py output: requests/s per leg, system calls counted per request,
and the extra time per request and per nginx system call against no tracing."""
import collections, json, statistics as st, sys

LEGS = ['native', 'kernel_targeted', 'kernel_untargeted', 'bpftime_targeted',
        'bpftime_untargeted', 'transformer_only']
rps, per_req = collections.defaultdict(list), collections.defaultdict(list)
errors = 0
for line in open(sys.argv[1]):
    x = json.loads(line)
    if x.get('rps') is None or not x.get('nginx_alive'):
        errors += 1
        continue
    rps[x['leg']].append(x['rps'])
    if x.get('per_request') is not None:
        per_req[x['leg']].append(x['per_request'])
native = st.mean(rps['native'])
calls = st.mean(per_req['kernel_targeted'])
print('| Leg | runs | requests/s mean | sd | min-max | vs no tracing | +us per request | +ns per nginx system call | calls counted per request |')
print('|---|---:|---:|---:|---|---:|---:|---:|---|')
for leg in LEGS:
    v = rps[leg]
    m = st.mean(v)
    extra_us = 1e6 / m - 1e6 / native
    counted = per_req.get(leg)
    counted_s = f'{min(counted):.3f}-{max(counted):.3f}' if counted else '-'
    print(f'| {leg} | {len(v)} | {m:.0f} | {st.stdev(v):.0f} | {min(v):.0f}-{max(v):.0f} | '
          f'{100 * (m / native - 1):+.2f}% | {extra_us:+.2f} | {1000 * extra_us / calls:+.0f} | {counted_s} |')
print(f'\nruns not valid: {errors}; nginx system calls per request (kernel count): {calls:.3f}')
