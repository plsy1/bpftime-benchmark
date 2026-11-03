import json, statistics
from pathlib import Path
d = Path(__file__).parent
rows = [json.loads(l) for l in (d / 'suite.jsonl').open()]
bs = ['dev-518a6b99', 'dev-7ad5a152']
s = {b: {} for b in bs}
for r in rows:
    for k, v in r['samples'].items():
        s[r['build']].setdefault(k.replace('__bench_', ''), []).append(v)
print('ns per probe (1000 map operations), medians of 5 reps (50000 iters), cpu5\n')
print(f"{'case':26}{'518a6b99':>11}{'7ad5a152':>11}{'change':>9}   7ad5a152 reps")
for c in s[bs[0]]:
    a, b = (statistics.median(s[x][c]) for x in bs)
    print(f"{c:26}{a:11.0f}{b:11.0f}{100*(b/a-1):+8.1f}%   {sorted(round(v) for v in s[bs[1]][c])}")
