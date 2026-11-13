"""Per-kernel census from an nsys SQLite export.

Usage: python3 -I parse_census.py <run.sqlite> <own_entries.txt> <out.csv>
own_entries.txt: one mangled kernel name per line (entry functions of libggml-cuda.so,
from cuobjdump -symbols). A kernel is "own" when its mangled name is in that list.
Output columns: kernel (mangled), short name, own, launches, threads per launch min/max,
thread executions (sum of grid x block over launches), GPU time ns, share of GPU kernel time.
"""
import csv, sqlite3, sys
from collections import defaultdict

db, own_path, out = sys.argv[1:4]
own = set(l.strip() for l in open(own_path) if l.strip())
c = sqlite3.connect(db)
names = dict(c.execute('select id, value from StringIds'))
rows = c.execute('select mangledName, shortName, gridX*gridY*gridZ, blockX*blockY*blockZ, end-start '
                 'from CUPTI_ACTIVITY_KIND_KERNEL').fetchall()
agg = defaultdict(lambda: {'short': '', 'n': 0, 'tmin': None, 'tmax': 0, 'texec': 0, 'ns': 0})
for m, s, g, b, ns in rows:
    k = names.get(m, str(m))
    a = agg[k]
    a['short'] = names.get(s, str(s))
    t = g * b
    a['n'] += 1
    a['tmin'] = t if a['tmin'] is None else min(a['tmin'], t)
    a['tmax'] = max(a['tmax'], t)
    a['texec'] += t
    a['ns'] += ns
total = sum(a['ns'] for a in agg.values()) or 1
with open(out, 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['kernel', 'short', 'own', 'launches', 'threads_min', 'threads_max', 'thread_execs', 'gpu_ns', 'time_share'])
    for k, a in sorted(agg.items(), key=lambda kv: -kv[1]['ns']):
        w.writerow([k, a['short'], int(k in own), a['n'], a['tmin'], a['tmax'], a['texec'], a['ns'], f"{a['ns']/total:.4f}"])
own_ns = sum(a['ns'] for k, a in agg.items() if k in own)
print(f'{db}: kernels={len(agg)} launches={len(rows)} gpu_ms={total/1e6:.2f} own_share={own_ns/total:.4f} '
      f'thread_execs={sum(a["texec"] for a in agg.values())}')
