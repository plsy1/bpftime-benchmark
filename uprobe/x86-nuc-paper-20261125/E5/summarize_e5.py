#!/usr/bin/env python3
"""Summarize run_e5.py JSON lines: one line per (leg, side, scenario, point) with the fields
that mismatched (mismatch/n), ambiguous and missing counts; 0 mismatches as "<=3/n".
Usage: summarize_e5.py FILE.jsonl [--all]
"""
import json, sys
from collections import defaultdict

rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
# --all: one line per scenario over all points instead of per point
groups = defaultdict(list)
for r in rows:
    if r.get('kind') == 'leg':
        rcs = ','.join(f"{s}:{v.get('rc')}" for s, v in (r.get('targets') or {}).items())
        print(f"# leg {r['leg']} ok={r.get('ok')} {r.get('seconds')}s targets[{rcs}] "
              f"truth={r.get('truth_records')} k_calls={r.get('k_calls')} u_calls={r.get('u_calls')} "
              f"k[{r.get('k_tool')}] u[{r.get('u_tool')}] problems={r.get('problems')} "
              f"leftover={r.get('leftover_procs')}{r.get('leftover_shm')} {r.get('error', '')}")
    elif r.get('kind') == 'field':
        groups[(r['leg'], r['side'], r['scenario'], r['point'])].append(r)
    elif r.get('kind') == 'target_meta' and not r['text'].startswith(('pid ', 'done ')):
        print(f"# {r['leg']}: target {r['text']}")
    elif r.get('kind') == 'scenario' and (r['extra'] or r['multi_hit']):
        print(f"# {r['leg']} {r['side']} {r['scenario']}: extra {r['extra']} multi_hit {r['multi_hit']}")
all_points = '--all' in sys.argv
for (leg, side, scen, point), rs in groups.items():
    if (point == 'all') != all_points:
        continue
    n = max(r['n'] + r['ambiguous'] + r['missing'] for r in rs)
    bad = [f"{r['field']} {r['mismatch']}/{r['n']}" for r in rs if r['mismatch']]
    amb = sum(r['ambiguous'] for r in rs)
    miss = max(r['missing'] for r in rs)
    nmin = min(r['n'] for r in rs)
    verdict = ', '.join(bad) if bad else (f"all match (<= 3/{nmin})" if nmin else 'no data')
    print(f"{leg:6} {side} {scen:4} p{point} recs={n:<5} {verdict}" +
          (f"  ambiguous={amb}" if amb else '') + (f"  missing={miss}" if miss else ''))
