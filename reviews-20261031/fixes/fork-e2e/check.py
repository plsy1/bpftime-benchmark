#!/usr/bin/env python3
"""Events whose reported pid differs from the pid in their path."""
import re, sys, collections
ok = collections.Counter(); wrong = collections.Counter(); other = 0
for line in open(sys.argv[1], errors='replace'):
    m = re.match(r'\s*(\d+)\s+(\S+)\s+(-?\d+)\s+(-?\d+)\s+/tmp/fork-e2e-(\d+)\s*$', line)
    if not m:
        other += 'fork-e2e' in line
        continue
    pid, path_pid = int(m.group(1)), int(m.group(5))
    (ok if pid == path_pid else wrong)[path_pid] += 1
print(f'events with the right pid: {sum(ok.values())}, wrong pid: {sum(wrong.values())}, '
      f'garbled lines naming the path: {other}')
for p in sorted(set(ok) | set(wrong)):
    print(f'  path pid {p}: right {ok[p]} wrong {wrong[p]}')
