#!/usr/bin/env python3
"""Median req/s per leg for each fair-comparison round and both rounds pooled."""
import json, statistics as st, sys
from pathlib import Path
root = Path(__file__).resolve().parent
legs = ['baseline', 'kernel', 'official', 'official-noaff', 'opt-noaff-fork']
for payload in ('16', '256k'):
    rounds = {r: json.load(open(root / f'fair-{payload}-{r}.json'))['samples'] for r in ('r1', 'r2')}
    pooled = {l: rounds['r1'][l] + rounds['r2'][l] for l in legs}
    print(f'\n### {payload} payload\n')
    print('| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs official-noaff |')
    print('|---|---:|---:|---:|---:|---:|---:|')
    med = {l: st.median(pooled[l]) for l in legs}
    for l in legs:
        v = pooled[l]
        print(f'| {l} | {st.median(rounds["r1"][l]):.1f} | {st.median(rounds["r2"][l]):.1f} | {med[l]:.1f} | '
              f'{st.pstdev(v)/st.mean(v)*100:.1f}% | {med[l]/med["kernel"]*100-100:+.1f}% | '
              f'{med[l]/med["official-noaff"]*100-100:+.1f}% |')
