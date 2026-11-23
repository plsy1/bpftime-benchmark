#!/usr/bin/env python3
"""Log the measured CPU's real frequency once per second from another CPU.

Reads APERF/MPERF/TSC of <cpu> through /dev/cpu/<cpu>/msr (needs root and the
msr module); prints elapsed seconds, busy fraction and busy MHz. Runs on
<own-cpu> (an E core) so it does not disturb the measurement.
Usage: freq-log.py <cpu> <own-cpu> <tsc-mhz>
"""
import os, struct, sys, time
cpu, own, tsc_mhz = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
os.sched_setaffinity(0, {own})
f = open(f'/dev/cpu/{cpu}/msr', 'rb', buffering=0)
def rd(reg):
    f.seek(reg)
    return struct.unpack('<Q', f.read(8))[0]
t0 = time.monotonic()
prev = (rd(0xe8), rd(0xe7), rd(0x10))
while True:
    time.sleep(1)
    cur = (rd(0xe8), rd(0xe7), rd(0x10))
    da, dm, dt = (c - p for c, p in zip(cur, prev))
    prev = cur
    busy = dm / dt if dt else 0.0
    mhz = tsc_mhz * da / dm if dm else 0.0
    print(f'{time.monotonic() - t0:.0f} {busy:.3f} {mhz:.0f}', flush=True)
