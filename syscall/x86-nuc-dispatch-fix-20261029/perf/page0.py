#!/usr/bin/env python3
"""Share of perf samples whose IP is on page zero (zpoline's nop sled at 1-511 and
its jump at 0x200), from `perf script -F ip -G` on stdin. Usage:
  perf script -i X.perf.data -F ip -G | page0.py X"""
import sys
n = sled = stub = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    n += 1
    ip = int(line, 16)
    if ip < 0x200:
        sled += 1
    elif ip < 0x1000:
        stub += 1
print(f'{sys.argv[1]}: samples {n}, nop sled {sled} ({100 * sled / n:.1f}%), '
      f'jump at 0x200+ {stub} ({100 * stub / n:.1f}%)')
