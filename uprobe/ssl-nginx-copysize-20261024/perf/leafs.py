#!/usr/bin/env python3
"""Leaf-function breakdown of samples whose callchain contains a given entry point
(perf script -F ip,sym,dso output)."""
import collections, sys
path = sys.argv[1]
roots = sys.argv[2:] or ['bpf_perf_event_output', 'uprobe_listener_on_enter', 'uprobe_listener_on_leave',
                         '_gum_function_context_begin_invocation', '_gum_function_context_end_invocation']
samples = []
cur = []
for line in open(path, errors='replace'):
    s = line.strip()
    if not s:
        if cur: samples.append(cur)
        cur = []
        continue
    parts = s.split(' ', 1)
    cur.append(parts[1] if len(parts) > 1 else s)
if cur: samples.append(cur)
total = len(samples)
print('samples', total)
for r in roots:
    c = collections.Counter(st[0] for st in samples if any(r in f for f in st))
    k = sum(c.values())
    print(f'\n== {r}: {100 * k / total:.2f}% of samples; leaf breakdown')
    for leaf, n in c.most_common(14):
        print(f'   {100 * n / total:5.2f}%  {leaf[:115]}')
