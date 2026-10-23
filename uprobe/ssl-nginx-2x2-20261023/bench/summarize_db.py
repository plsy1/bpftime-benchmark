#!/usr/bin/env python3
"""Two-round fair comparison including db3d475f (fair-db-<payload>-r{1,2}.json)."""
import json, statistics as st
from pathlib import Path
root = Path(__file__).resolve().parent
legs = ['baseline', 'kernel', 'official', 'opt-noaff-fork', 'db3d475f']
for payload in ('16', '256k'):
    r = {x: json.load(open(root / f'fair-db-{payload}-{x}.json'))['samples'] for x in ('r1', 'r2')}
    pooled = {l: r['r1'][l] + r['r2'][l] for l in legs}
    med = {l: st.median(pooled[l]) for l in legs}
    print(f'\n### {payload} payload\n')
    print('| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs a90fa447 |')
    print('|---|---:|---:|---:|---:|---:|---:|')
    for l in legs:
        v = pooled[l]
        print(f'| {l} | {st.median(r["r1"][l]):.1f} | {st.median(r["r2"][l]):.1f} | {med[l]:.1f} | '
              f'{st.pstdev(v)/st.mean(v)*100:.1f}% | {med[l]/med["kernel"]*100-100:+.1f}% | '
              f'{med[l]/med["opt-noaff-fork"]*100-100:+.1f}% |')
