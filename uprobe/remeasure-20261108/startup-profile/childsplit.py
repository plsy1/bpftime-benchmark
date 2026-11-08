#!/usr/bin/env python3
"""perf script text on stdin; argv[1] a function name (substring). For samples whose stack holds
it, count the frame called directly from it (its child), and how many samples hold it at all."""
import collections, sys
target = sys.argv[1]
total = held = 0
child = collections.Counter()
stack = []
def flush():
    global total, held
    if not stack:
        return
    total += 1
    # stack[0] is the leaf; find the outermost frame matching target
    idx = [i for i, f in enumerate(stack) if target in f]
    if idx:
        held += 1
        i = idx[-1]
        child[stack[i - 1] if i > 0 else '(self)'] += 1
for line in sys.stdin:
    line = line.rstrip('\n')
    if not line.strip():
        flush(); stack = []
        continue
    if line.startswith((' ', '\t')):
        parts = line.strip().split(None, 1)
        if len(parts) == 2:
            stack.append(parts[1].rsplit(' (', 1)[0])
    else:
        flush(); stack = []
flush()
print(f'samples {total}, holding {target}: {held} ({100*held/max(total,1):.1f}%)')
for f, n in child.most_common(14):
    print(f'{100*n/max(held,1):6.1f}%  {f[:150]}')
