import json, statistics, sys
from pathlib import Path
d = Path(__file__).parent
rows = [json.loads(l) for l in (d / 'suite.jsonl').open()]
builds = ['dev-e5ba4aa7', 'dev-b7ded27a']
s = {b: {} for b in builds}
for r in rows:
    for k, v in r['samples'].items():
        s[r['build']].setdefault(k.replace('__bench_', ''), []).append(v)
print('ns per call, medians of 5 reps (50000 iters), cpu5\n')
print(f"{'case':30} {'e5ba4aa7':>10} {'b7ded27a':>10} {'change':>8}   b7ded27a reps")
for c in s[builds[0]]:
    a, b = (statistics.median(s[x][c]) for x in builds)
    print(f"{c:30} {a:10.1f} {b:10.1f} {100*(b/a-1):+7.1f}%   {sorted(round(v) for v in s[builds[1]][c])}")
