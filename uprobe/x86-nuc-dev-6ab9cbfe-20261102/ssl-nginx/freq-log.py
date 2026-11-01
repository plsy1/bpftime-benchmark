#!/usr/bin/env python3
"""Once per second, print each listed CPU's busy fraction and busy MHz (APERF/MPERF/TSC
via /dev/cpu/N/msr), sampling from <own-cpu>. Usage: freq-log.py <own-cpu> <tsc-mhz> <cpu>...
"""
import os, struct, sys, time
own, tsc, cpus = int(sys.argv[1]), float(sys.argv[2]), [int(c) for c in sys.argv[3:]]
os.sched_setaffinity(0, {own})
fs = {c: open(f'/dev/cpu/{c}/msr', 'rb', buffering=0) for c in cpus}
def rd(c, reg):
    fs[c].seek(reg); return struct.unpack('<Q', fs[c].read(8))[0]
prev = {c: (rd(c, 0xe8), rd(c, 0xe7), rd(c, 0x10)) for c in cpus}
t0 = time.monotonic()
while True:
    time.sleep(1)
    parts = []
    for c in cpus:
        cur = (rd(c, 0xe8), rd(c, 0xe7), rd(c, 0x10))
        da, dm, dt = (x - y for x, y in zip(cur, prev[c]))
        prev[c] = cur
        parts.append(f'cpu{c} {dm / dt if dt else 0:.3f} {tsc * da / dm if dm else 0:.0f}')
    print(f'{time.monotonic() - t0:.0f} ' + ' | '.join(parts), flush=True)
