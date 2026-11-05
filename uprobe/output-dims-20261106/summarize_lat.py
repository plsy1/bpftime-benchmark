#!/usr/bin/env python3
"""Tables of the burst latency runs (l1-matched.jsonl, l2-backtoback.jsonl, l3-*.jsonl):
delivered fraction, burst time, and the delivery delay of the first record of each burst
(sent more than 1 ms after every record before it: the consumer's wakeup) and of all records.
Values are means over the reps; delays in us."""
import json, sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load(name):
    p = HERE / name
    if not p.exists():
        return []
    return [json.loads(l) for l in p.open() if l.strip()]


def mean(xs):
    xs = [x for x in xs if x is not None]
    return sum(xs) / len(xs) if xs else None


def fmt(x, scale=1.0, digits=1):
    return '-' if x is None else f'{x / scale:.{digits}f}'


def table(rows, title):
    out = [f'## {title}', '']
    bad = [r for r in rows if not r.get('ok')]
    out.append(f'{len(rows)} runs, {len(bad)} not ok.')
    out.append('')
    groups = defaultdict(lambda: defaultdict(list))
    for r in rows:
        key = (r['obj'].replace('out-', '').replace('.bpf.o', ''), r['burst'], r['period_ns'],
               r['consumer_ns'], r['threads'])
        groups[key][r['backend']].append(r)
    out.append('| path-size | burst | period ns | consumer ns | threads | delivered k / b | '
               'burst us k / b | first p50 k / b | first p90 k / b | first p99 k / b | '
               'all p50 k / b | all p99 k / b | firsts k / b |')
    out.append('|---|---:|---:|---:|---:|---|---|---|---|---|---|---|---|')
    for key in sorted(groups):
        g = groups[key]
        k, b = g.get('kernel', []), g.get('fix-final', [])

        def pair(field, scale=1000.0, digits=1, pct=False):
            vals = []
            for runs in (k, b):
                m = mean([r.get(field) for r in runs])
                if pct:
                    vals.append('-' if m is None else f'{m * 100:.1f}%')
                else:
                    vals.append(fmt(m, scale, digits))
            return ' / '.join(vals)

        out.append(f'| {key[0]} | {key[1]} | {key[2]} | {key[3]} | {key[4]} | '
                   f'{pair("delivered", pct=True)} | {pair("burst_us", 1.0, 0)} | '
                   f'{pair("first_p50")} | {pair("first_p90")} | {pair("first_p99")} | '
                   f'{pair("lat_p50")} | {pair("lat_p99")} | {pair("first_n", 1.0, 0)} |')
    out.append('')
    return out


def main():
    lines = ['# Burst latency summary (summarize_lat.py)', '',
             'k = kernel bpf, b = bpftime fix-final. Delays: CLOCK_MONOTONIC at the consumer '
             'callback minus the send time the workload passed to count_event. "first": records '
             'sent more than 1 ms after every record seen before them.', '']
    for name, title in (('l1-matched.jsonl', 'L1: matched arrival (calls 4 us apart by the clock)'),
                        ('l2-backtoback.jsonl', 'L2: back-to-back bursts'),
                        ('l3-nopad.jsonl', 'L3: ring buffer records that divide the ring')):
        rows = load(name)
        if rows:
            lines += table(rows, title)
    text = '\n'.join(lines) + '\n'
    (HERE / 'summary-lat.md').write_text(text)
    sys.stdout.write(text)


if __name__ == '__main__':
    main()
