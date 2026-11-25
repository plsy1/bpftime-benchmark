#!/usr/bin/env python3
"""I2 churn summary: per-call time per thread (us) = threads / ops_per_sec * 1e6, per backend, object and thread count;
median and mean with a t 95 % CI over reps; map cost = churn - control (difference of means, Welch-style CI from the two
sets of reps; reps are not paired across objects). Usage: summarize_i2.py churn.jsonl"""
import collections, json, math, statistics as st, sys
T = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776}
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
bad = [r for r in rows if not r.get('ok')]
d = collections.defaultdict(list)
for r in rows:
    if r.get('ok') and r.get('ops_per_sec'):
        d[(r['backend'], r['obj'], r['threads'])].append(r['threads'] / r['ops_per_sec'] * 1e6)
backends = [b for b in ('none', 'kernel', 'official', 'l3port', 'olatest', 'ol730x', 'dnew', 'dn730f') if any(k[0] == b for k in d)]
threads = sorted({k[2] for k in d})
def mci(v):
    m = st.fmean(v); h = T.get(len(v) - 1, 2) * st.stdev(v) / math.sqrt(len(v)) if len(v) > 1 else float('nan'); return m, h
print(f'runs {len(rows)}, failed checks {len(bad)}')
for obj in ('churn-k4-v8.bpf.o', 'control.bpf.o'):
    print(f'\n## {obj}: per-call time per thread, us, mean ± t95 half-width (n per cell)')
    print('| T | ' + ' | '.join(backends) + ' |'); print('|---:|' + '---:|' * len(backends))
    for t in threads:
        cells = []
        for b in backends:
            v = d.get((b, obj if b != 'none' else '-', t))
            cells.append(f'{mci(v)[0]:.3f} ± {mci(v)[1]:.3f} ({len(v)})' if v else '-')
        print(f'| {t} | ' + ' | '.join(cells) + ' |')
print('\n## map cost = churn − control, us per call (difference of means ± combined t95 half-width)')
print('| T | ' + ' | '.join(b for b in backends if b != 'none') + ' |'); print('|---:|' + '---:|' * (len(backends) - 1))
for t in threads:
    cells = []
    for b in backends:
        if b == 'none':
            continue
        a, c = d.get((b, 'churn-k4-v8.bpf.o', t)), d.get((b, 'control.bpf.o', t))
        (ma, ha), (mc, hc) = mci(a), mci(c)
        cells.append(f'{ma - mc:+.3f} ± {math.hypot(ha, hc):.3f}')
    print(f'| {t} | ' + ' | '.join(cells) + ' |')
