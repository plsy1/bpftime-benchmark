#!/usr/bin/env python3
"""Counts the stress tests print (Catch2 -s "with message" lines: lookups, misses, torn copies),
min - max over the runs of each set in repeat/. Usage: repeat-counts.py > repeat/counts.md"""
import re
from pathlib import Path

R = Path(__file__).resolve().parent / 'repeat'
print('| set | runs | messages (min - max over runs) |')
print('|---|---:|---|')
for d in sorted(p for p in R.iterdir() if p.is_dir()):
    vals = {}
    for f in d.glob('run-*.log'):
        msgs = set(re.findall(r'with messages?:\n((?:  .*\n)+)', f.read_text()))
        for m in msgs:
            for k, v in re.findall(r'([a-z ]+?) (\d+)', m.replace('\n', ',')):
                vals.setdefault(k.strip(' ,'), []).append(int(v))
    print(f'| {d.name} | {len(list(d.glob("run-*.log")))} | ' + '; '.join(f'{k} {min(v)}-{max(v)}' for k, v in vals.items()) + ' |')
