#!/usr/bin/env python3
"""Sum /proc/PID/smaps by mapping name: Rss, Pss, private (clean + dirty), shared, in KB.

Anonymous mappings are grouped as [anon]; usage: smaps_sum.py PID [label]."""
import collections, re, sys

pid = sys.argv[1]
rows = collections.defaultdict(lambda: collections.Counter())
name = None
for line in open(f'/proc/{pid}/smaps'):
    m = re.match(r'^[0-9a-f]+-[0-9a-f]+ \S+ \S+ \S+ \S+\s*(.*)$', line)
    if m:
        name = m.group(1).strip() or '[anon]'
        if name.startswith('/dev/shm/'):
            name = '/dev/shm/*'
        continue
    k, _, v = line.partition(':')
    if k in ('Rss', 'Pss', 'Private_Clean', 'Private_Dirty', 'Shared_Clean', 'Shared_Dirty'):
        rows[name][k] += int(v.split()[0])
tot = collections.Counter()
out = []
for n, c in rows.items():
    priv = c['Private_Clean'] + c['Private_Dirty']
    shared = c['Shared_Clean'] + c['Shared_Dirty']
    tot.update(dict(rss=c['Rss'], pss=c['Pss'], priv=priv, dirty=c['Private_Dirty'], shared=shared))
    out.append((c['Rss'], n, c['Pss'], priv, c['Private_Dirty'], shared))
print(f"{'rss':>8} {'pss':>8} {'private':>8} {'p_dirty':>8} {'shared':>8}  mapping")
for rss, n, pss, priv, dirty, shared in sorted(out, reverse=True):
    if rss >= 100:
        print(f'{rss:8d} {pss:8d} {priv:8d} {dirty:8d} {shared:8d}  {n}')
print(f"{tot['rss']:8d} {tot['pss']:8d} {tot['priv']:8d} {tot['dirty']:8d} {tot['shared']:8d}  TOTAL")
