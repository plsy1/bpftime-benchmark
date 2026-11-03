import json, statistics
from pathlib import Path
d = Path(__file__).parent
rows = [json.loads(l) for l in (d / 'suite.jsonl').open()]
bs = ['dev-e5ba4aa7', 'dev-b7ded27a', 'dev-f9b92cee']
s = {b: {} for b in bs}
for r in rows:
    for k, v in r['samples'].items():
        s[r['build']].setdefault(k.replace('__bench_', ''), []).append(v)
print('ns per probe (1000 map operations), medians of 5 reps (50000 iters), cpu5\n')
print(f"{'case':26}{'e5ba4aa7':>11}{'b7ded27a':>11}{'f9b92cee':>11}{'vs b7ded':>10}   f9b92cee reps")
for c in s[bs[0]]:
    m = [statistics.median(s[b][c]) for b in bs]
    print(f"{c:26}" + ''.join(f"{x:11.0f}" for x in m) + f"{100*(m[2]/m[1]-1):+9.1f}%   {sorted(round(v) for v in s[bs[2]][c])}")
