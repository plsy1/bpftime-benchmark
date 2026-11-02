#!/usr/bin/env python3
"""Per set and leg: runs with any reported loss, lost/request (mean, min, max), delivered fraction."""
import glob, json, os, statistics as st
R = os.path.dirname(os.path.abspath(__file__))
SETS = [('a16', '16 B, -c 10 -d 3'), ('c16', '16 B, -c 100 -d 10'), ('256k', '256 KB, -c 10 -d 3')]
LEGS = ['kernel', 'a7d54d7e', 'dev-6ab9cbfe', 'dev-68f19c1f']
print('| Set | Leg | runs with loss | lost/request mean (min-max) | events/request mean | delivered |')
print('|---|---|---:|---:|---:|---:|')
for key, label in SETS:
    pat = ['a256k', 'b256k'] if key == '256k' else [key]
    files = sorted(f for p in pat for f in glob.glob(f'{R}/out/{p}-r*.json'))
    for leg in LEGS:
        rs = [json.load(open(f))[leg] for f in files]
        lpr = [r['lost_per_request'] for r in rs]
        ev, lost = sum(r['events'] for r in rs), sum(r['lost'] for r in rs)
        print(f"| {label} | {leg} | {sum(r['lost'] > 0 for r in rs)}/{len(rs)} | "
              f"{st.mean(lpr):.4f} ({min(lpr):.4f}-{max(lpr):.4f}) | "
              f"{st.mean(r['events_per_request'] for r in rs):.3f} | {ev / (ev + lost):.4%} |")
