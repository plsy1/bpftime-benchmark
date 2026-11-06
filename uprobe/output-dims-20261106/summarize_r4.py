#!/usr/bin/env python3
"""Tables of round 4 (go-r4.sh): r4 call duration percentiles, r5 variable sizes, r6 processes
against threads (paced and back to back), r7 many processes. Means over reps. -> summary-r4.md"""
import json, collections
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load(n):
    p = HERE / n
    return [json.loads(l) for l in p.open()] if p.exists() else []


def m(xs):
    xs = [x for x in xs if x is not None]
    return sum(xs) / len(xs) if xs else None


def f(x, s=1.0, d=1):
    return '-' if x is None else f'{x / s:.{d}f}'


out = ['# Round 4 summary (summarize_r4.py)', '']
rows = load('r4-call-tail.jsonl')
out += ['## R4: count_event call duration (ns), bursts of 100 at 4 us, every 10 ms', '',
        f'{len(rows)} runs, {sum(not r["ok"] for r in rows)} not ok.', '',
        '| obj | threads | backend | p50 | p90 | p99 | p99.9 | max |', '|---|---:|---|---:|---:|---:|---:|---:|']
g = collections.defaultdict(list)
for r in rows:
    g[(r['obj'], r['threads'], r['backend'])].append(r)
for k in sorted(g):
    rs = g[k]
    out.append(f'| {k[0]} | {k[1]} | {k[2]} | ' + ' | '.join(
        f(m([r.get(f"call_{p}") for r in rs]), 1, 0) for p in ('p50', 'p90', 'p99', 'p999', 'max')) + ' |')
out.append('')
for name, title in (('r5-varsize.jsonl', 'R5: variable sizes 72/520/1480/3992 B, matched arrival'),
                    ('r6-procs-paced.jsonl', 'R6: processes vs threads, bursts of 100 at 4 us'),
                    ('r7-many-procs.jsonl', 'R7: 16 and 64 processes, bursts of 10 at 4 us every 10 ms')):
    rows = load(name)
    out += [f'## {title}', '', f'{len(rows)} runs, {sum(not r["ok"] for r in rows)} not ok.', '',
            '| obj | burst | procs | threads | delivered k / b | first p50 us k / b | first p90 | all p99 us k / b |',
            '|---|---:|---:|---:|---|---|---|---|']
    g = collections.defaultdict(lambda: collections.defaultdict(list))
    for r in rows:
        g[(r['obj'], r['burst'], r.get('procs', 1), r['threads'])][r['backend']].append(r)
    for k in sorted(g):
        def pair(field, s=1000.0, pct=False):
            v = []
            for b in ('kernel', 'fix-final'):
                x = m([r.get(field) for r in g[k][b]])
                v.append('-' if x is None else (f'{x * 100:.1f}%' if pct else f(x, s)))
            return ' / '.join(v)
        out.append(f'| {k[0]} | {k[1]} | {k[2]} | {k[3]} | {pair("delivered", pct=True)} | '
                   f'{pair("first_p50")} | {pair("first_p90")} | {pair("lat_p99")} |')
    out.append('')
rows = load('r6-procs-steady.jsonl')
out += ['## R6 back to back: ns per call, 4 producers as threads or processes', '',
        f'{len(rows)} runs, {sum(not r["ok"] for r in rows)} not ok.', '',
        '| obj | layout | kernel ns/call | bpftime ns/call | delivered k / b |', '|---|---|---:|---:|---|']
g = collections.defaultdict(lambda: collections.defaultdict(list))
for r in rows:
    lay = f'{r.get("procs", 1)} proc x {r["threads"]} thr'
    g[(r['obj'], lay)][r['backend']].append(r)
for k in sorted(g):
    ns = [m([4 * 4e9 / r['total_ops'] for r in g[k][b] if r.get('total_ops')]) for b in ('kernel', 'fix-final')]
    dv = [m([r.get('delivered') for r in g[k][b]]) for b in ('kernel', 'fix-final')]
    out.append(f'| {k[0]} | {k[1]} | {f(ns[0])} | {f(ns[1])} | ' +
               ' / '.join('-' if x is None else f'{x * 100:.1f}%' for x in dv) + ' |')
text = '\n'.join(out) + '\n'
(HERE / 'summary-r4.md').write_text(text)
print(text)
