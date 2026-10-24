#!/usr/bin/env python3
"""Copy-size experiment at 256 KB (copysize-256k.json from run_ablation.py).

Legs <tracer>@s<stage>c<cap>: stage 3 = probe_read without output, stage 4 = full sslsniff; cap =
bytes read and output per event (16, 1024, 4096, 8192). Events per request are fixed at 18, so the
per-event cost is (1/rps - 1/rps_baseline) / 18 per rep. The slope of a least-squares line over the
four caps gives the extra cost per KB copied; "output" = stage 4 - stage 3 isolates the perf output.
"""
import json, statistics as st, sys
from pathlib import Path

EVENTS = 18
CAPS = [16, 1024, 4096, 8192]
T95 = {7: 2.365, 9: 2.262}
root = Path(__file__).resolve().parent
d = json.loads((root / (sys.argv[1] if len(sys.argv) > 1 else 'copysize-256k.json')).read_text())['samples']
base = d['baseline']
n = len(base)

def per_event(leg):
    return [(1e6 / d[leg][i] - 1e6 / base[i]) / EVENTS for i in range(n)]

def ci(xs):
    return st.mean(xs), T95.get(len(xs) - 1, 2.0) * st.stdev(xs) / len(xs) ** 0.5

def slope(ys):
    xs = [c / 1024 for c in CAPS]
    mx, my = st.mean(xs), st.mean(ys)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)

print(f'256 KB, {n} reps, {EVENTS} events/request; baseline median {st.median(base):.1f} req/s\n')
print('us per event over no tracing (mean +/- 95% CI)\n')
print('| Part | ' + ' | '.join(f'cap {c} B' for c in CAPS) + ' | slope us/KB (95% CI) |')
print('|---|' + '---:|' * (len(CAPS) + 1))
for t in ('kernel', 'bca2ebce'):
    rows = {
        f'{t} stage 3 (read, no output)': {c: per_event(f'{t}@s3c{c}') for c in CAPS},
        f'{t} stage 4 (full)': {c: per_event(f'{t}@s4c{c}') for c in CAPS},
    }
    rows[f'{t} output only (4 - 3)'] = {c: [a - b for a, b in zip(rows[f'{t} stage 4 (full)'][c],
                                                                rows[f'{t} stage 3 (read, no output)'][c])]
                                         for c in CAPS}
    for name, vals in rows.items():
        slopes = [slope([vals[c][i] for c in CAPS]) for i in range(n)]
        m, h = ci(slopes)
        print(f'| {name} | ' + ' | '.join('{:.2f} +/- {:.2f}'.format(*ci(vals[c])) for c in CAPS) +
              f' | {m:+.3f} +/- {h:.3f} |')
print('\nMedian req/s: ' + ', '.join(f'{k} {st.median(v):.1f}' for k, v in d.items()))
