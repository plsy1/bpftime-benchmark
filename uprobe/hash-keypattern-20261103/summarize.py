#!/usr/bin/env python3
"""us per op (one delete + one insert) per thread, medians of the reps, and the map cost (the
object minus keymap-control on the same backend and thread count)."""
import collections, json, statistics as st
from pathlib import Path

D = Path(__file__).resolve().parent
B = ['kernel', 'a7d54d7e', 'dev-a4ac0d31']
runs = collections.defaultdict(list)
bad = total = 0
for line in open(D / 'keys.jsonl'):
    r = json.loads(line)
    total += 1
    if not r.get('ok') or not r.get('ops_per_sec'):
        bad += 1
        continue
    key = (r['backend'], r['obj'], r['pattern'], r['live'], r['threads'])
    runs[key].append(r['threads'] / r['ops_per_sec'] * 1e6)
med = {k: st.median(v) for k, v in runs.items()}
ctl = {(b, t): med.get((b, 'keymap-control.bpf.o', 'addr', 5120, t)) for b in B for t in (1, 6)}
configs = sorted({k[1:4] for k in med if k[1] != 'keymap-control.bpf.o'},
                 key=lambda c: (int(c[0].split('-m')[1].split('.')[0]), c[2], c[0], c[1]))
for t in (1, 6):
    print(f'\n### {t} thread(s): map cost, us per delete + insert (total in parentheses)\n')
    print('| capacity | load | key | pattern | ' + ' | '.join(B) + ' | dev / kernel | dev / a7d54d7e |')
    print('|---:|---:|---|---|' + '---:|' * (len(B) + 2))
    for obj, pat, live in configs:
        m = int(obj.split('-m')[1].split('.')[0])
        k = obj.split('-k')[1].split('-')[0]
        cells, cost = [], {}
        for b in B:
            v = med.get((b, obj, pat, live, t))
            c = ctl.get((b, t))
            if v is None or c is None:
                cells.append('-')
                continue
            cost[b] = v - c
            cells.append(f'{v - c:.3f} ({v:.3f})')
        r1 = f"{cost['dev-a4ac0d31'] / cost['kernel']:.2f}x" if 'kernel' in cost and 'dev-a4ac0d31' in cost and cost['kernel'] > 0 else '-'
        r2 = f"{cost['dev-a4ac0d31'] / cost['a7d54d7e']:.2f}x" if 'a7d54d7e' in cost and 'dev-a4ac0d31' in cost and cost['a7d54d7e'] > 0 else '-'
        print(f'| {m} | {live / m:.0%} | {k} B | {pat} | ' + ' | '.join(cells) + f' | {r1} | {r2} |')
print(f'\nruns {total}, not ok {bad}; control (us per op): ' +
      ', '.join(f'{b} {t}t {ctl[(b, t)]:.3f}' for b in B for t in (1, 6) if ctl.get((b, t))))
