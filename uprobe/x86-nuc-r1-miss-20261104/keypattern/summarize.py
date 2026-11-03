#!/usr/bin/env python3
"""us per op (one delete + one insert) per thread, medians of the reps, and the map cost (the
object minus keymap-control on the same backend and thread count). x86 NUC copy (11/03) of Jetson
results uprobe/dev-518a6b99-20261103/keys/summarize.py: backends kernel and dev-d5279469
(keys.jsonl), then the addr pattern seeded against BPFTIME_HASH_ZERO_SEED=1 (keys-zeroseed.jsonl)."""
import collections, json, statistics as st
from pathlib import Path

D = Path(__file__).resolve().parent


def load(name):
    runs = collections.defaultdict(list)
    bad = total = 0
    for line in open(D / name):
        r = json.loads(line)
        total += 1
        if not r.get('ok') or not r.get('ops_per_sec'):
            bad += 1
            continue
        key = (r['backend'], r['obj'], r['pattern'], r['live'], r['threads'])
        runs[key].append(r['threads'] / r['ops_per_sec'] * 1e6)
    return {k: st.median(v) for k, v in runs.items()}, total, bad


def table(name, B, ratio):
    med, total, bad = load(name)
    ctl = {(b, t): med.get((b, 'keymap-control.bpf.o', 'addr', 5120, t)) for b in B for t in (1, 6)}
    configs = sorted({k[1:4] for k in med if k[1] != 'keymap-control.bpf.o'},
                     key=lambda c: (int(c[0].split('-m')[1].split('.')[0]), c[2], c[0], c[1]))
    num, den = ratio
    for t in (1, 6):
        print(f'\n### {name}, {t} thread(s): map cost, us per delete + insert (total in parentheses)\n')
        print('| capacity | load | key | pattern | ' + ' | '.join(B) + f' | {num} / {den} |')
        print('|---:|---:|---|---|' + '---:|' * (len(B) + 1))
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
            r = (f'{cost[num] / cost[den]:.2f}x' if num in cost and cost.get(den, 0) > 0.02 else '-')
            print(f'| {m} | {live / m:.0%} | {k} B | {pat} | ' + ' | '.join(cells) + f' | {r} |')
    print(f'\n{name}: runs {total}, not ok {bad}; control (us per op): ' +
          ', '.join(f'{b} {t}t {ctl[(b, t)]:.3f}' for b in B for t in (1, 6) if ctl.get((b, t))))


print('# x86 NUC key-pattern matrix, dev 7ad5a152 vs dev d5279469 (11/04)')
table('keys.jsonl', ['dev-d5279469', 'dev-7ad5a152'], ('dev-7ad5a152', 'dev-d5279469'))
if (D / 'keys-zeroseed.jsonl').exists():
    table('keys-zeroseed.jsonl', ['dev-d5279469', 'dev-d5279469-zeroseed'], ('dev-d5279469', 'dev-d5279469-zeroseed'))
