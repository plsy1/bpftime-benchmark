#!/usr/bin/env python3
"""ns per write, medians over the rounds: dev-b60b99c3 (before) vs fix-final (after) transformers."""
import json, statistics as st
from pathlib import Path
D = Path(__file__).resolve().parent
for name in ('before', 'after'):
    rows = [json.loads(l) for l in open(D / f'{name}.jsonl')]
    for leg in ('rewrite', 'agent-other', 'agent-exit'):
        v = sorted(r['ns_per_write'] for r in rows if r['leg'] == leg and r.get('ok'))
        print(f'{name:6} {leg:11} median {st.median(v):7.1f} ns  ({v[0]}-{v[-1]}, n={len(v)})')
