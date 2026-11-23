#!/usr/bin/env python3
"""E7c (x86 counterpart of Jetson E7c): cache/TLB counters per request on the nginx worker.
Usage: analyze_e7c.py <rows.jsonl>
Prints per-leg medians per request and the deltas the Jetson reported:
hook (S1 - B), output (S4 - B) and pinning (Odiag@S4 - Odiag@S4+NO_PIN; also per pin sequence = /2).
"""
import json, statistics as st, sys

rows = [json.loads(l) for l in open(sys.argv[1])]
per = {}
mux = 0
for r in rows:
    if not r.get('valid') or not r.get('perf_per_req'):
        continue
    if 'perf_multiplexed' in (r.get('flags') or []):
        mux += 1
    for ev, v in r['perf_per_req'].items():
        if v is not None:
            per.setdefault(r['leg'], {}).setdefault(ev.replace('cpu_core/', '').rstrip('/'), []).append(v)

events = sorted({e for d in per.values() for e in d})
med = {leg: {e: st.median(v) for e, v in d.items()} for leg, d in per.items()}
spread = {leg: {e: (max(v) - min(v)) / st.median(v) if st.median(v) else 0 for e, v in d.items()} for leg, d in per.items()}

print(f'rows with multiplexed counters: {mux}\n')
print('## Per request (median of reps; n per event in parentheses)\n')
print('| leg | ' + ' | '.join(events) + ' |')
print('|---|' + '---:|' * len(events))
for leg in med:
    print(f'| {leg} | ' + ' | '.join(f"{med[leg][e]:,.0f} ({len(per[leg][e])})" if e in med[leg] else '' for e in events) + ' |')
print('\nmax spread (max-min)/median over all legs and events: %.1f%%' % (100 * max(x for d in spread.values() for x in d.values())))

def delta(a, b):
    if a in med and b in med:
        return {e: med[a][e] - med[b][e] for e in events if e in med[a] and e in med[b]}

print('\n## Deltas per request\n')
steps = [('K@S1', 'B'), ('Odiag@S1', 'B'), ('D@S1', 'B'), ('K@S4', 'B'), ('Odiag@S4', 'B'), ('D@S4', 'B'),
         ('Odiag@S4', 'Odiag@S4+NO_PIN')]
print('| step | ' + ' | '.join(events) + ' |')
print('|---|' + '---:|' * len(events))
for a, b in steps:
    d = delta(a, b)
    if d:
        print(f'| {a} - {b} | ' + ' | '.join(f'{d[e]:+,.0f}' if e in d else '' for e in events) + ' |')
d = delta('Odiag@S4', 'Odiag@S4+NO_PIN')
if d:
    print('\nPer pin sequence (2 per request): ' + ', '.join(f'{e} {d[e] / 2:+,.0f}' for e in events))
