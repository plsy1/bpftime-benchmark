"""Medians of the multi-thread real-tool runs: ops/s per tool, backend and thread count."""
import json, statistics as st, sys
from collections import defaultdict
rows = [json.loads(l) for l in open(sys.argv[1])]
v = defaultdict(list); rep = defaultdict(list)
for r in rows:
    if r.get('ops_per_sec'):
        v[(r['tool'], r['backend'], r['threads'])].append(r['ops_per_sec'])
        if 'expected' in r and isinstance(r['expected'], int) and r['expected']:
            rep[(r['tool'], r['backend'], r['threads'])].append(r['reported'] / r['expected'])
tools = sorted({k[0] for k in v}); bs = []
for r in rows:
    if r['backend'] not in bs: bs.append(r['backend'])
ths = sorted({k[2] for k in v})
for t in tools:
    print(f'\n### {t}: traced workload ops/s (median), reported/expected\n')
    print('| threads | ' + ' | '.join(bs) + ' | 7ad5a152 / kernel | 7ad5a152 / a4ac0d31 |')
    print('|---:|' + '---:|' * (len(bs) + 2))
    for th in ths:
        m = {b: st.median(v[(t, b, th)]) if v.get((t, b, th)) else None for b in bs}
        cells = []
        for b in bs:
            if m[b] is None: cells.append('-'); continue
            rr = rep.get((t, b, th))
            cells.append(f'{m[b]/1e6:.2f}M' + (f' ({st.median(rr):.3f})' if rr else ''))
        a, k, o = m.get('dev-7ad5a152'), m.get('kernel'), m.get('dev-a4ac0d31')
        print(f'| {th} | ' + ' | '.join(cells) + f" | {a/k:.2f}x | {a/o:.2f}x |" if a and k and o else f'| {th} | ' + ' | '.join(cells) + ' | - | - |')
print(f'\nruns {len(rows)}, errors {sum(1 for r in rows if "error" in r)}')
