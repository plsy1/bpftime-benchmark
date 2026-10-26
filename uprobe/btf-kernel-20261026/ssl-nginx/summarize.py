#!/usr/bin/env python3
"""Two-round fair comparison on the BTF kernel: kernel, official 177a1dc and a7d54d7e (fair-<payload>-r{1,2}.json)."""
import json, statistics as st
from pathlib import Path
root = Path(__file__).resolve().parent
legs = ['baseline', 'kernel', 'official', 'a7d54d7e']
for payload in ('16', '256k'):
    r = {x: json.load(open(root / f'fair-{payload}-{x}.json'))['samples'] for x in ('r1', 'r2')}
    pooled = {l: r['r1'][l] + r['r2'][l] for l in legs}
    med = {l: st.median(pooled[l]) for l in legs}
    print(f'\n### {payload} payload\n')
    print('| Leg | r1 median | r2 median | pooled median | CV | vs kernel | vs baseline | us/request over baseline |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|')
    for l in legs:
        v = pooled[l]
        print(f'| {l} | {st.median(r["r1"][l]):.1f} | {st.median(r["r2"][l]):.1f} | {med[l]:.1f} | '
              f'{st.pstdev(v)/st.mean(v)*100:.1f}% | {med[l]/med["kernel"]*100-100:+.1f}% | '
              f'{med[l]/med["baseline"]*100-100:+.1f}% | {1e6/med[l]-1e6/med["baseline"]:.1f} |')
    paired = [b / a - 1 for a, b in zip(pooled['kernel'], pooled['a7d54d7e'])]
    print(f'\na7d54d7e vs kernel, paired per rep: median {100*st.median(paired):+.2f}%, '
          f'{sum(x > 0 for x in paired)}/{len(paired)} faster')
