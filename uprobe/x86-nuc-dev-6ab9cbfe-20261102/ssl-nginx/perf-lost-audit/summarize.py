#!/usr/bin/env python3
"""Table for the x86 perf event loss re-audit (run.sh): per set and leg, rounds with a reported
loss, lost events per request and events per request, from out/<set>-r<n>.json and the sslsniff
stderr next to it (libbpf's "lost N events on CPU #c" lines). Same columns as the Jetson
re-audit's summary (benchmark-results/jetson uprobe/perf-lost-audit-20261102).
"""
import json, re
from pathlib import Path

out = Path(__file__).resolve().parent / 'out'
LOST = re.compile(r'lost (\d+) events on CPU')
LEGS = ('kernel', 'official', 'a7d54d7e', 'dev-6ab9cbfe')

print('| Set | Leg | runs with loss | lost/request mean (min-max) | events/request mean | delivered |')
print('|---|---|---:|---:|---:|---:|')
for name, label in (('a16', '16 B, -c 10 -d 3'), ('a256k', '256 KB, -c 10 -d 3')):
    runs = sorted(out.glob(f'{name}-r*.json'), key=lambda p: int(p.stem.split('-r')[1]))
    for leg in LEGS:
        per, epr, ev, lo = [], [], 0, 0
        for p in runs:
            r = json.loads(p.read_text())[leg]
            lost = sum(int(m.group(1)) for line in open(f'{p}.{leg}.sslsniff.txt.stderr', errors='replace')
                       for m in [LOST.search(line)] if m)
            per.append(lost / r['requests'])
            epr.append(r['events_per_request'])
            ev += r['events']; lo += lost
        hit = sum(1 for x in per if x > 0)
        print(f'| {label} | {leg} | {hit}/{len(per)} | {sum(per) / len(per):.4f} ({min(per):.4f}-{max(per):.4f}) '
              f'| {sum(epr) / len(epr):.3f} | {100 * ev / (ev + lo):.4f}% |')
